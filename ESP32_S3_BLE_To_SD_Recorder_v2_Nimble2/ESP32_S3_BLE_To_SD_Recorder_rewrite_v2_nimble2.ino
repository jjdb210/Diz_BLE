/*
  BLE To SD Recorder
  Rewritten to reduce SD-card write stalls and scanner interruptions.

  Original concept by Justin Gehring
  Based on Neil Kolban example for IDF and Arduino ESP32 NimBLE scanning.
*/

#include <Arduino.h>
#include "NimBLEDevice.h"
#include <NimBLEAdvertisedDevice.h>
#include "NimBLEBeacon.h"
#include <SPI.h>
#include <SD.h>
#include <ESP32Time.h>
#include "FS.h"

ESP32Time rtc(3600);  // Used to track time between messages.

const int chipSelect = 21;
bool serialdebug = true;

char savefilename[30] = "/datalog_00.txt";
uint32_t session_number = 0;
File file;

#define PIN D6
#define N_LEDS 60

static constexpr uint32_t scanTimeMs = 30 * 1000;  // NimBLE 2.x uses milliseconds.

NimBLEScan *pBLEScan = nullptr;

volatile uint32_t sequenceNumber = 0;
volatile uint32_t droppedRecords = 0;
volatile uint32_t fileOpenFailures = 0;

struct LogRecord {
  char timeText[16];
  uint32_t session;
  uint32_t sequence;
  char address[18];
  uint8_t data[200];
  uint16_t length;
  uint8_t typeTag;  // 1=JJWand, 2=Disney, 3=Other Disney
};

QueueHandle_t logQueue = nullptr;

int listDir(fs::FS &fs, const char *dirname, uint8_t levels) {
  File root = fs.open(dirname);
  if (!root) {
    Serial.println("Failed to open directory");
    return 9999;
  }
  if (!root.isDirectory()) {
    Serial.println("Not a directory");
    return 9999;
  }

  int filecount = 0;
  File entry = root.openNextFile();
  while (entry) {
    filecount++;
    if (entry.isDirectory()) {
      Serial.print("  DIR : ");
      Serial.println(entry.name());
      if (levels) {
        listDir(fs, entry.path(), levels - 1);
      }
    } else {
      Serial.print("  FILE: ");
      Serial.print(entry.name());
      Serial.print("  SIZE: ");
      Serial.println(entry.size());
    }
    entry = root.openNextFile();
  }
  return filecount;
}

void writeFile(fs::FS &fs, const char *path, const char *message) {
  Serial.printf("Writing file: %s\n", path);

  File localFile = fs.open(path, FILE_WRITE);
  if (!localFile) {
    Serial.println("Failed to open file for writing");
    return;
  }
  if (localFile.print(message)) {
    Serial.println("File written");
  } else {
    Serial.println("Write failed");
  }
  localFile.close();
}

bool initSDCard() {
  if (!SD.begin(chipSelect)) {
    if (serialdebug) {
      Serial.println("Card failed, or not present");
    }
    return false;
  }
  return true;
}

void buildNewFilename() {
  int dircount = listDir(SD, "/", 1);
  snprintf(savefilename, sizeof(savefilename), "/datalog%d_%lu.txt", dircount, (unsigned long)(fileOpenFailures % 100000UL));
}

bool openLogFile() {
  if (file) {
    file.close();
  }

  file = SD.open(savefilename, FILE_APPEND);
  if (!file) {
    fileOpenFailures++;
    if (serialdebug) {
      Serial.printf("Failed to open file for appending: %s\n", savefilename);
    }
    return false;
  }

  if (serialdebug) {
    Serial.printf("Opened log file: %s\n", savefilename);
  }
  return true;
}

bool ensureLogFileOpen() {
  if (file) {
    return true;
  }

  if (!initSDCard()) {
    return false;
  }

  if (!openLogFile()) {
    buildNewFilename();
    return openLogFile();
  }

  return true;
}

void writerTask(void *parameter) {
  LogRecord rec;
  uint32_t lastFlushMs = millis();
  uint16_t bufferedRecords = 0;

  (void)parameter;

  for (;;) {
    if (xQueueReceive(logQueue, &rec, pdMS_TO_TICKS(250)) == pdTRUE) {
      if (!ensureLogFileOpen()) {
        vTaskDelay(pdMS_TO_TICKS(100));
        continue;
      }

      char line[700];
      int pos = snprintf(
        line,
        sizeof(line),
        "%s: %lu %lu %s ",
        rec.timeText,
        (unsigned long)rec.session,
        (unsigned long)rec.sequence,
        rec.address
      );

      if (pos < 0) {
        pos = 0;
      }

      for (uint16_t i = 2; i < rec.length && pos < (int)sizeof(line) - 3; i++) {
        int wrote = snprintf(line + pos, sizeof(line) - pos, "%02x", rec.data[i]);
        if (wrote <= 0) {
          break;
        }
        pos += wrote;
      }

      if (pos < (int)sizeof(line) - 2) {
        line[pos++] = '\n';
      }
      line[pos] = '\0';

      size_t written = file.write((const uint8_t *)line, (size_t)pos);
      if (written != (size_t)pos) {
        if (serialdebug) {
          Serial.println("SD write failed, attempting recovery");
        }
        file.close();
        buildNewFilename();
        vTaskDelay(pdMS_TO_TICKS(50));
        ensureLogFileOpen();
      } else {
        bufferedRecords++;
        digitalWrite(LED_BUILTIN, (rec.sequence % 2U) ? LOW : HIGH);
      }
    }

    uint32_t now = millis();
    if (file && (bufferedRecords >= 25 || (now - lastFlushMs) >= 500)) {
      file.flush();
      bufferedRecords = 0;
      lastFlushMs = now;
    }
  }
}

class MyAdvertisedDeviceCallbacks : public NimBLEScanCallbacks {
  private:
    bool enqueueRecord(const NimBLEAdvertisedDevice *advertisedDevice, const uint8_t *data, size_t len, uint8_t typeTag) {
      LogRecord rec;
      memset(&rec, 0, sizeof(rec));

      rec.session = session_number;
      rec.sequence = ++sequenceNumber;
      rec.typeTag = typeTag;
      rec.length = (uint16_t)((len > sizeof(rec.data)) ? sizeof(rec.data) : len);

      String nowText = rtc.getTime();
      nowText.toCharArray(rec.timeText, sizeof(rec.timeText));

      String addrText = advertisedDevice->getAddress().toString().c_str();
      addrText.toCharArray(rec.address, sizeof(rec.address));

      memcpy(rec.data, data, rec.length);

      BaseType_t ok = xQueueSend(logQueue, &rec, 0);
      if (ok != pdTRUE) {
        droppedRecords++;
        return false;
      }

      return true;
    }

    void printDebug(const char *label, const NimBLEAdvertisedDevice *advertisedDevice, const uint8_t *data, size_t len, uint32_t seqPrefix) {
      if (!serialdebug) {
        return;
      }

      Serial.printf("%s (%u bytes): ", label, (unsigned int)len);
      Serial.printf("%lu %s ", (unsigned long)seqPrefix, advertisedDevice->getAddress().toString().c_str());
      for (size_t i = 2; i < len; i++) {
        Serial.printf("%02x", data[i]);
      }
      Serial.printf("\n");
    }

  public:
    void onResult(const NimBLEAdvertisedDevice *advertisedDevice) override {
      if (!advertisedDevice->haveManufacturerData()) {
        return;
      }

      std::string strManufacturerData = advertisedDevice->getManufacturerData();
      size_t len = strManufacturerData.length();
      if (len < 2) {
        return;
      }

      uint8_t cManufacturerData[200];
      size_t copyLen = (len > sizeof(cManufacturerData)) ? sizeof(cManufacturerData) : len;
      memcpy(cManufacturerData, strManufacturerData.data(), copyLen);

      // Justin's wand: 0x4201
      if (copyLen >= 2 && cManufacturerData[0] == 0x42 && cManufacturerData[1] == 0x01) {
        uint32_t nextSeq = sequenceNumber + 1;
        if (enqueueRecord(advertisedDevice, cManufacturerData, copyLen, 1)) {
          printDebug("Found JJWand", advertisedDevice, cManufacturerData, copyLen, nextSeq);
        }
        return;
      }

      if (copyLen >= 5 && cManufacturerData[0] == 0x83 && cManufacturerData[1] == 0x01 && cManufacturerData[4] == 0xe9) {
        uint32_t nextSeq = sequenceNumber + 1;
        if (enqueueRecord(advertisedDevice, cManufacturerData, copyLen, 2)) {
          printDebug("Found Disney", advertisedDevice, cManufacturerData, copyLen, nextSeq);
        }
        return;
      }

      if (copyLen >= 2 && cManufacturerData[0] == 0x83 && cManufacturerData[1] == 0x01) {
        uint32_t nextSeq = sequenceNumber + 1;
        if (enqueueRecord(advertisedDevice, cManufacturerData, copyLen, 3)) {
          printDebug("Found OTHER Disney", advertisedDevice, cManufacturerData, copyLen, nextSeq);
        }
        return;
      }
    }
  public:
    void onScanEnd(const NimBLEScanResults &results, int reason) override {
      (void)results;

      if (serialdebug) {
        Serial.printf("Scan ended, reason = %d; restarting scan\n", reason);
      }

      NimBLEScan *scan = NimBLEDevice::getScan();
      if (scan != nullptr) {
        scan->start(scanTimeMs, false, true);
      }
    }
};

static MyAdvertisedDeviceCallbacks myScanCallbacks;

void setup() {
  delay(1000);  // Prevents USB driver crash on startup, do not omit.

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  if (serialdebug) {
    Serial.begin(115200);
    delay(50);
    Serial.println();
    Serial.println("Initializing SD card...");
  }

  session_number = (uint32_t)random(1, 9999999);
  rtc.setTime(0, 0, 1, 1, 1, 2023);

  if (initSDCard()) {
    buildNewFilename();
    openLogFile();
  }

  logQueue = xQueueCreate(256, sizeof(LogRecord));
  if (logQueue == nullptr) {
    if (serialdebug) {
      Serial.println("Failed to create log queue");
    }
  } else {
    xTaskCreatePinnedToCore(writerTask, "writerTask", 8192, nullptr, 1, nullptr, 1);
  }

  // Must be called before NimBLEDevice::init()
  NimBLEDevice::setScanFilterMode(CONFIG_BTDM_SCAN_DUPL_TYPE_DATA);
  NimBLEDevice::setScanDuplicateCacheSize(10);
  NimBLEDevice::init("");

  pBLEScan = NimBLEDevice::getScan();
  pBLEScan->setScanCallbacks(&myScanCallbacks, true);
  pBLEScan->setActiveScan(false);   // Active scan requires response from target; do not use for dresses.
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);
  pBLEScan->setMaxResults(0);       // Callback only.
  pBLEScan->start(scanTimeMs, false, true);

  if (serialdebug) {
    Serial.printf("Session number: %lu\n", (unsigned long)session_number);
    Serial.printf("Logging to: %s\n", savefilename);
    Serial.printf("Scan time setting (ms): %lu\n", (unsigned long)scanTimeMs);
  }
}

void loop() {
  static uint32_t lastScanCheckMs = 0;
  static uint32_t lastStatusMs = 0;

  uint32_t now = millis();

  // Make sure the BLE scanner never stops.
  if ((now - lastScanCheckMs) >= 250) {
    lastScanCheckMs = now;
    if (pBLEScan != nullptr && !pBLEScan->isScanning()) {
      if (serialdebug) {
        Serial.println("New Scan Started");
      }
      pBLEScan->start(scanTimeMs, false, true);
    }
  }

  // Periodic status output.
  if (serialdebug && (now - lastStatusMs) >= 5000) {
    lastStatusMs = now;
    UBaseType_t queued = (logQueue != nullptr) ? uxQueueMessagesWaiting(logQueue) : 0;
    Serial.printf(
      "Status - seq:%lu queued:%u dropped:%lu fileFailures:%lu scanning:%s\n",
      (unsigned long)sequenceNumber,
      (unsigned int)queued,
      (unsigned long)droppedRecords,
      (unsigned long)fileOpenFailures,
      (pBLEScan != nullptr && pBLEScan->isScanning()) ? "yes" : "no"
    );
  }

  delay(10);
}
