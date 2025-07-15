/* 
  BLE To SD Recorder
  Designed for an ESP32s3 with SD Card Attachment
  Author: Justin Gehring
  http://emcot.world/
  
  Based on Neil Kolban example for IDF: https://github.com/nkolban/esp32-snippets/blob/master/cpp_utils/tests/BLE%20Tests/SampleScan.cpp
  Ported to Arduino ESP32 by Evandro Copercini
*/

#include <Arduino.h>
//#include <Adafruit_NeoPixel.h>
#include "NimBLEDevice.h"
#include <NimBLEAdvertisedDevice.h>
#include "NimBLEBeacon.h"
#include <SPI.h>
#include <SD.h>
#include <ESP32Time.h>

ESP32Time rtc(3600); 			//Used to track time between messages.
const int chipSelect = 21;
bool serialdebug = true;

char savefilename[30] = "/datalog_00.txt";  //Starting File Name
int session_number = 0;
File file; //Main file being written.


File dataFile;
#include "FS.h"
#define PIN D6
#define N_LEDS 60

//Hat and Dress are GRB
//Adafruit_NeoPixel strip = Adafruit_NeoPixel(N_LEDS, PIN, NEO_GRB + NEO_KHZ800);

#define ENDIAN_CHANGE_U16(x) ((((x)&0xFF00) >> 8) + (((x)&0xFF) << 8))

hw_timer_t *My_timer = NULL;

int listDir(fs::FS &fs, const char * dirname, uint8_t levels){
    //Serial.printf("Listing directory: %s\n", dirname);
    File root = fs.open(dirname);
    if(!root){
        Serial.println("Failed to open directory");
        return 9999;
    }
    if(!root.isDirectory()){
        Serial.println("Not a directory");
        return 9999;
    }
    int filecount = 0;
    File file = root.openNextFile();
    while(file){
        filecount++;
        if(file.isDirectory()){
            Serial.print("  DIR : ");
            Serial.println(file.name());
            if(levels){
                listDir(fs, file.path(), levels -1);
            }
        } else {
            Serial.print("  FILE: ");
            Serial.print(file.name());
            Serial.print("  SIZE: ");
            Serial.println(file.size());
        }
        file = root.openNextFile();
    }
    return filecount;
}


// Use to create the file.
void writeFile(fs::FS &fs, const char * path, const char * message){
    Serial.printf("Writing file: %s\n", path);

    File file = fs.open(path, FILE_WRITE);
    if(!file){
        Serial.println("Failed to open file for writing");
        return;
    }
    if(file.print(message)){
        Serial.println("File written");
    } else {
        Serial.println("Write failed");
    }
    file.close();
}


int scanTime = 1;  //In seconds
int sequenceNumber = 0;
//int mode = 1;  
//int dressmode = 0; //Default to Kid mode (details at: https://emcot.world/index.php/Disney_Bluetooth_Wands)
//bool blackout = false;

//int lastmode = 0;

int timer = 0;

BLEScan *pBLEScan; //Old code but compatible

#define BYTE_TO_BINARY_PATTERN "%c%c%c%c%c%c%c%c "
#define BYTE_TO_BINARY(byte) \
  (byte & 0x80 ? '1' : '0'), \
    (byte & 0x40 ? '1' : '0'), \
    (byte & 0x20 ? '1' : '0'), \
    (byte & 0x10 ? '1' : '0'), \
    (byte & 0x08 ? '1' : '0'), \
    (byte & 0x04 ? '1' : '0'), \
    (byte & 0x02 ? '1' : '0'), \
    (byte & 0x01 ? '1' : '0')


//This is the Bluetooth scanner... Listening for all the fun stuff.
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
	void save_text(BLEAdvertisedDevice *advertisedDevice){
		sequenceNumber++;
		
		std::string strManufacturerData = advertisedDevice->getManufacturerData();
		uint8_t cManufacturerData[200];
		
		strManufacturerData.copy((char *)cManufacturerData, strManufacturerData.length(), 0);
		
		//File file = SD.open(savefilename, FILE_APPEND);
		
		if(!file){
			if (serialdebug) Serial.println("Failed to open file for appending");
			return;
		}
		
		file.printf(rtc.getTime().c_str());
		file.printf(": %i %i %s ", session_number, sequenceNumber, advertisedDevice->getAddress().toString().c_str());
		for (int i = 2; i < strManufacturerData.length(); i++) {
			file.printf("%02x", cManufacturerData[i]);
		}
		file.printf("\n");
		if (serialdebug) Serial.printf("\n");
		//file.close();
		
		if (sequenceNumber % 2 == 1){
			digitalWrite(LED_BUILTIN, LOW);
		} else {
			digitalWrite(LED_BUILTIN, HIGH);
		}
	}

  void onResult(BLEAdvertisedDevice *advertisedDevice) {

    //Anything that doesn't have this, we should just ignore.
    if (advertisedDevice->haveManufacturerData() == true) {
      std::string strManufacturerData = advertisedDevice->getManufacturerData();
      uint8_t cManufacturerData[200];
      strManufacturerData.copy((char *)cManufacturerData, strManufacturerData.length(), 0);


      //Is this Justins Wand? 0x4201
		if (cManufacturerData[0] == 0x42 && cManufacturerData[1] == 0x01) {
			save_text(advertisedDevice);
			
			if (serialdebug == true){
				Serial.printf("Found JJWand: ", strManufacturerData.length());
				Serial.printf("A %i %s ", sequenceNumber, advertisedDevice->getAddress().toString().c_str());
				for (int i = 2; i < strManufacturerData.length(); i++) {
					Serial.printf("%02x", cManufacturerData[i]);
				}	
			}

 
		} else if (cManufacturerData[0] == 0x83 && cManufacturerData[1] == 0x01 && cManufacturerData[4] == 0xe9  ) {
		
			save_text(advertisedDevice);
			
			if (serialdebug == true){
				Serial.printf("Found Disney: ", strManufacturerData.length());
				Serial.printf("A %i %s ", sequenceNumber, advertisedDevice->getAddress().toString().c_str());
				for (int i = 2; i < strManufacturerData.length(); i++) {
					Serial.printf("%02x", cManufacturerData[i]);
				}
				Serial.printf("\n");
			}
		} else if (cManufacturerData[0] == 0x83 && cManufacturerData[1] == 0x01) {
			save_text(advertisedDevice);
			
			if (serialdebug == true){
				Serial.printf("Found OTHER Disney: ", strManufacturerData.length());
				Serial.printf("B %i %s ", sequenceNumber, advertisedDevice->getAddress().toString().c_str());
				for (int i = 2; i < strManufacturerData.length(); i++) {
					Serial.printf("%02x", cManufacturerData[i]);
				}
				Serial.printf("\n");
			}
		}
	return;
  }
}
};


int failures = 0;	//Used to create new files when something errors, but might be overwriting... 
int dircount = 0;

void setup() {
	delay(1000);  // prevents usb driver crash on startup, do not omit this
	session_number = random(1,9999999);
	//initialize Failures and Time HERE!


	if (serialdebug){
		Serial.begin(115200);
		Serial.print("Initializing SD card...");
	}
  
	if (!SD.begin(chipSelect)) {
		if (serialdebug) Serial.println("Card failed, or not present");
		// don't do anything more but wait to try again.
		sleep(10);
	}
  	
	rtc.setTime(0,0,1,1,1,2023);
  dircount = listDir(SD,"/",1);
  sprintf(savefilename, "/datalog%d_%d.txt", dircount, failures);
	//writeFile(SD, savefilename, "Hello");
	sleep(1);

	//BLE Scanner Setup Starts Now!
/** *Optional* Sets the filtering mode used by the scanner in the BLE controller.
 *
 *  Can be one of:
 *  CONFIG_BTDM_SCAN_DUPL_TYPE_DEVICE (0) (default)
 *  Filter by device address only, advertisements from the same address will be reported only once.
 *
 *  CONFIG_BTDM_SCAN_DUPL_TYPE_DATA (1)
 *  Filter by data only, advertisements with the same data will only be reported once,
 *  even from different addresses.
 *
 *  CONFIG_BTDM_SCAN_DUPL_TYPE_DATA_DEVICE (2)
 *  Filter by address and data, advertisements from the same address will be reported only once,
 *  except if the data in the advertisement has changed, then it will be reported again.
 *
 *  Can only be used BEFORE calling NimBLEDevice::init.
*/
	NimBLEDevice::setScanFilterMode(CONFIG_BTDM_SCAN_DUPL_TYPE_DATA);
/** *Optional* Sets the scan filter cache size in the BLE controller.
 *  When the number of duplicate advertisements seen by the controller
 *  reaches this value it will clear the cache and start reporting previously
 *  seen devices. The larger this number, the longer time between repeated
 *  device reports. Range 10 - 1000. (default 20)
 *
 *  Can only be used BEFORE calling NimBLEDevice::init.
 */
	NimBLEDevice::setScanDuplicateCacheSize(10);
	NimBLEDevice::init("");
	pBLEScan = BLEDevice::getScan();  //create new scan
	pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks()); //Whats The True do here Justin?
	pBLEScan->setActiveScan(false);           // Active scan requires response from target. DO NOT USE for dresses.
	pBLEScan->setInterval(100);                // How often the scan occurs / switches channels; in milliseconds,
	pBLEScan->setWindow(99);                  // How long to scan during the interval; in milliseconds.
	pBLEScan->setMaxResults(0);               // do not store the scan results, use callback only.  

	//End Setup
}

long mytime;
long mytime2;


//Loop Is:
// Making sure the BLE Scanner is going
// Making sure we are able to write to disk.
void loop() {
	timer++;

	//Simple check to make sure scanner never stops.
	if (timer % 100 == 0) {  
		if(pBLEScan->isScanning() == false) {
			if (serialdebug) Serial.printf("New Scan Started \n");      
			pBLEScan->start(0, nullptr, true);
		}
	}

	//Check to insure SD Card is still open.
	if (timer % 10000 == 1){
		file = SD.open(savefilename, FILE_APPEND);
		if(!file){
			failures++;
			Serial.println("Failed to open file for appending");
		  dircount = listDir(SD,"/",1);
		sprintf(savefilename, "/datalog%d_%d.txt", dircount, failures);
			//writeFile(SD, savefilename, "Hello");
			return;
		} 
	}
	
	if (timer == 19999){
	  timer = 0;
	}
	
	if(!SD.begin(chipSelect)){
        digitalWrite(LED_BUILTIN, HIGH);
        if (serialdebug) Serial.println("Card Mount Failed");
    }
}
