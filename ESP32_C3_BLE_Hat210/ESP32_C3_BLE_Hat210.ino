/* 
  Justins Hat210 V3.3 Code - https://emcot.world/The_Disney_Hat_(Hat210)
  Designed for an ESP32c3 with SD Card Attachment
  Author: Justin Gehring
  http://emcot.world/
  
  Based on Neil Kolban example for IDF: https://github.com/nkolban/esp32-snippets/blob/master/cpp_utils/tests/BLE%20Tests/SampleScan.cpp
  Ported to Arduino ESP32 by Evandro Copercini
*/

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "NimBLEDevice.h"
#include <NimBLEAdvertisedDevice.h>
#include "NimBLEBeacon.h"
#include <SPI.h>
#include <SD.h>
#include "FS.h"
#include <ESP32Time.h>

#define PIN D6
#define N_LEDS 60
#define ENDIAN_CHANGE_U16(x) ((((x)&0xFF00) >> 8) + (((x)&0xFF) << 8))

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


int scanTime = 1;  //In seconds
int sequenceNumber = 0;
int mode = 1;  
int dressmode = 0; //Default to Kid mode (details at: https://emcot.world/index.php/Disney_Bluetooth_Wands)
bool blackout = false;

int lastmode = 0;
int timer = 0;

BLEScan *pBLEScan; //Old code but compatible

int selected_color = 0;
int last_color = 0;

ESP32Time rtc(3600);  //Realtime clock for logging
const int chipSelect = 21;
char savefilename[30] = "/datalog_00.txt";
int session_number = 0;

File dataFile;

//Hat and Dress are GRB
Adafruit_NeoPixel strip = Adafruit_NeoPixel(N_LEDS, PIN, NEO_GRB + NEO_KHZ800);

//https://emcot.world/Disney_MagicBand%2B_Bluetooth_Codes
uint32_t pallet_array[] = { 
  strip.Color(0, 255, 255),      //0 = Cyan
  strip.Color(30, 255, 255),     //1 = Purple
  strip.Color(0, 0, 255),        //2 = Blue
  strip.Color(0, 0, 128),        //3 = Midnight Blue
  strip.Color(0, 0, 255),        //4 = Blue 
  strip.Color(255, 60, 255),     //5 = bright purple
  strip.Color(153, 0, 153),      //6 = lavender
  strip.Color(153, 0, 153),      //7 = purple
  strip.Color(195, 122, 123),      //8 = pink
  strip.Color(195, 122, 123),     //9 = pink
  strip.Color(195, 122, 123),      //10 = pink
  strip.Color(195, 122, 123),      //11 = pink
  strip.Color(195, 122, 123),      //12 = pink
  strip.Color(195, 122, 123),      //13 = pink
  strip.Color(195, 122, 123),      //14 = pink
  strip.Color(255, 60, 0),       //15 = Yellow Orange
  strip.Color(255, 255, 128),    //16 Off Yellow
  strip.Color(255, 200, 50),     //17 Yellow ORange
  strip.Color(51, 255, 51), 	 //18 Lime Greenstrip.Color(51, 255, 51), 
  strip.Color(255, 50, 0),       //19 Orange
  strip.Color(255, 30, 0),       //20 Red Orange?
  strip.Color(255, 0, 0),        //21 Red 
  strip.Color(0, 255, 255),		 //22 cyan
  strip.Color(0, 255, 255),		  //23 cyan
  strip.Color(0, 255, 255),		//24 cyan
  strip.Color(0, 255, 0),         //25 Green
  strip.Color(0, 255, 51),  		//26 Lime Green
  strip.Color(255, 255, 255),		//27 White
  strip.Color(255, 255, 255),		//28 white
  strip.Color(0, 0, 0),				//29 Off
  strip.Color(0, 0, 0),				//unique
  strip.Color(255, 255, 255)      //Random                   
};

// This is used by the bluetooth wand.
// More colors can be added.
uint32_t mode_array[] = {
  strip.Color(0,0,255),
  strip.Color(0,255,0),
  strip.Color(0,255,255),
  strip.Color(255,255,0),
  strip.Color(255,0,255),
  strip.Color(255,255,255),
  strip.Color(255, 0, 0),     //17 Yellow ORange
  strip.Color(50, 50, 50),                                   //18 Lime Greenstrip.Color(51, 255, 51), 
  strip.Color(255, 99, 33),       //19 Orange
  strip.Color(255, 102, 0)
};  

// Temporary Variables for pattern colors
uint32_t temp_color_array[] = { strip.Color(0, 0, 0), strip.Color(0, 0, 0), strip.Color(0, 0, 0), strip.Color(0, 0, 0), strip.Color(0, 0, 0) };
int temp_start = 0;
int temp_x = 0;
int temp_y = 0;
int temp_z = 0;
  
int led1 = 0;
int led2 = 0;
int led3 = 0;
int led4 = 0;
int led5 = 0;

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
void appendFile(fs::FS &fs, const char * path, const char * message){
    Serial.printf("Appending to file: %s\n", path);

    File file = fs.open(path, FILE_APPEND);
    if(!file){
        Serial.println("Failed to open file for appending");
        return;
    }
    if(file.print(message)){
        Serial.println("Message appended");
    } else {
        Serial.println("Append failed");
    }
    file.close();
}




//This is the Bluetooth scanner... Listening for all the fun stuff.
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  

void solid_color_function(uint8_t param1, uint8_t param2, uint8_t param3){
  // 6f0ef5 = 011011110000111011110101
  // 6f0ef1 = 011011110000111011110001
  // 210efb = 001000010000111011111011
  // 110eef = 000100010000111011101111
  int color = param3 & 0x1f;
  mode = 2;
  Serial.printf("Found Color: %i", color);
  Serial.printf("\n");
  for (int i = 0; i < N_LEDS; i++) {
    strip.setPixelColor(i, pallet_array[color]);
  }
  strip.show();
  delay(3000);
}

void e908_function(uint8_t param1, uint8_t param2, uint8_t param3,uint8_t param4, uint8_t param5){
    int r1 = (param3 >> 2) * 4;
    int g1 = (param4 >> 2) * 4;
    int b1 = (param5 >> 2) * 4;
    int time = param2 * 60;

    mode = 2;
    for (int i = 0; i < N_LEDS; i++) {
        strip.setPixelColor(i, strip.Color(r1, g1, b1));
    }
    strip.show();
    Serial.printf("E908: %i %i %i Time: %i", r1,g1,b1,time);
    delay(time);

}


void e909_function(uint8_t param1, uint8_t param2, uint8_t param3,uint8_t param4, uint8_t param5, uint8_t param6,uint8_t param7){
  //8301e100e90900050fa7b1a7b1a7b0
  //8301e100e909000f0fafa4b9a4b9b0
  int color[5];
  color[2] = param3 & 0x1f;  //Middle
  color[1] = param4 & 0x1f;  //Top Right
  color[3]= param5 & 0x1f;  //Bottom Right
  color[4] = param6 & 0x1f;  //Bottom Left
  color[0] = param7 & 0x1f;  //Top Left
  mode = 2;
  Serial.printf("Found Color: %i %i %i %i %i", color[0],color[1],color[2],color[3],color[4]);
 
  //Hat Color Scheme
  //Front of Hat is color 2
  //Ears are alternating Left Right
  for (int i = 0; i < 20; i++){
    strip.setPixelColor(i, pallet_array[color[2]]);
  }
  for (int i = 0; i < 15; i++){
    if (i%2 == 0){
      strip.setPixelColor(19+i, pallet_array[color[0]]);
      strip.setPixelColor(34+i, pallet_array[color[1]]);
    } else {
      strip.setPixelColor(19+i, pallet_array[color[4]]);
      strip.setPixelColor(34+i, pallet_array[color[3]]);
    }
  }
  strip.show();
}

void e911_function(uint8_t param1, uint8_t param2, uint8_t param3,uint8_t param4, uint8_t param5, uint8_t param6,uint8_t param7,uint8_t param8,uint8_t param9,uint8_t param10,uint8_t param11,uint8_t param12){
  mode = 2;
  int color[5];
  color[0] = param3 & 0x1f;
  color[1] = param4 & 0x1f;
  Serial.printf("Found E91 Color: %i %i", color[0], color[1]);

  int red1 = pallet_array[color[0]] >> 16 & 0xff;
  int green1 = pallet_array[color[0]] >> 8 & 0xff;
  int blue1 = pallet_array[color[0]] >> 0 & 0xff;
  int red2 = pallet_array[color[1]] >> 16 & 0xff;
  int green2 = pallet_array[color[1]] >> 8 & 0xff;
  int blue2 = pallet_array[color[1]] >> 0 & 0xff;

  int red_step = (red2 - red1) / 30;
  int green_step = (green2 - green1) / 30;
  int blue_step = (blue2 - blue1) / 30;
  for (int temp_y = 0; temp_y < 4; temp_y++){
    for (int temp_x = 0; temp_x < 30; temp_x++){
      uint32_t current_color = strip.Color(red1 + (temp_x * red_step), green1 + (temp_x * green_step), blue1 + (temp_x * blue_step));
      uint32_t current_color2 = strip.Color(red2 - (temp_x * red_step), green2 - (temp_x * green_step), blue2 - (temp_x * blue_step));
    
      for (int i = 1; i < 19; i++){
        if (i % 2 == 0){
          strip.setPixelColor(i, current_color);
        } else {
          strip.setPixelColor(i, current_color2);
        }
      }
      for (int i= 19; i< 34; i++){
        strip.setPixelColor(i, current_color2);
      }
      for (int i= 34; i< 60; i++){
 
        strip.setPixelColor(i, current_color);
      }
      strip.show();
      delay(20);
    }
    for (int temp_x = 30; temp_x > 0; temp_x--){
      uint32_t current_color = strip.Color(red1 + (temp_x * red_step), green1 + (temp_x * green_step), blue1 + (temp_x * blue_step));
      uint32_t current_color2 = strip.Color(red2 - (temp_x * red_step), green2 - (temp_x * green_step), blue2 - (temp_x * blue_step));
    
      for (int i = 1; i < 19; i++){
        if (i % 2 == 0){
          strip.setPixelColor(i, current_color);
        } else {
          strip.setPixelColor(i, current_color2);
        }
      }
      for (int i= 19; i< 34; i++){
        strip.setPixelColor(i, current_color2);
      }
      for (int i= 34; i< 60; i++){
 
        strip.setPixelColor(i, current_color);
      }
      strip.show();
      delay(20);
    }
  }
}

void e914_function(uint8_t param1, uint8_t param2, uint8_t param3,uint8_t param4, uint8_t param5, uint8_t param6,uint8_t param7,uint8_t param8,uint8_t param9,uint8_t param10,uint8_t param11,uint8_t param12){
  mode = 2;
  int color[5];
  color[0] = param3 & 0x1f;
  color[1] = param4 & 0x1f;
  Serial.printf("Found E914 Color: %i %i", color[0], color[1]);

  int red1 = pallet_array[color[0]] >> 16 & 0xff;
  int green1 = pallet_array[color[0]] >> 8 & 0xff;
  int blue1 = pallet_array[color[0]] >> 0 & 0xff;
  int red2 = pallet_array[color[1]] >> 16 & 0xff;
  int green2 = pallet_array[color[1]] >> 8 & 0xff;
  int blue2 = pallet_array[color[1]] >> 0 & 0xff;

  int red_step = (red2 - red1) / 30;
  int green_step = (green2 - green1) / 30;
  int blue_step = (blue2 - blue1) / 30;
  for (int temp_y = 0; temp_y < 4; temp_y++){
    for (int temp_x = 0; temp_x < 30; temp_x++){
      uint32_t current_color = strip.Color(red1 + (temp_x * red_step), green1 + (temp_x * green_step), blue1 + (temp_x * blue_step));
      uint32_t current_color2 = strip.Color(red2 - (temp_x * red_step), green2 - (temp_x * green_step), blue2 - (temp_x * blue_step));
    
      for (int i = 1; i < 19; i++){
        if (i % 2 == 0){
          strip.setPixelColor(i, current_color);
        } else {
          strip.setPixelColor(i, current_color2);
        }
      }
      for (int i= 19; i< 34; i++){
        strip.setPixelColor(i, current_color2);
      }
      for (int i= 34; i< 60; i++){
 
        strip.setPixelColor(i, current_color);
      }
      strip.show();
      delay(20);
    }
    for (int temp_x = 30; temp_x > 0; temp_x--){
      uint32_t current_color = strip.Color(red1 + (temp_x * red_step), green1 + (temp_x * green_step), blue1 + (temp_x * blue_step));
      uint32_t current_color2 = strip.Color(red2 - (temp_x * red_step), green2 - (temp_x * green_step), blue2 - (temp_x * blue_step));
    
      for (int i = 1; i < 19; i++){
        if (i % 2 == 0){
          strip.setPixelColor(i, current_color);
        } else {
          strip.setPixelColor(i, current_color2);
        }
      }
      for (int i= 19; i< 34; i++){
        strip.setPixelColor(i, current_color2);
      }
      for (int i= 34; i< 60; i++){
 
        strip.setPixelColor(i, current_color);
      }
      strip.show();
      delay(20);
    }
  }
}


void e90c_function(uint8_t param1, uint8_t param2, uint8_t param3,uint8_t param4, uint8_t param5, uint8_t param6,uint8_t param7,uint8_t param8){
      //LOTS TO DECODE HERE NOT SURE WHAT ALL THIS DOES
      //e100e90c 000f0f b1b9b5b1a2307b7db0
      //e100e90c 000f0f 5d465bf00532374895
      //Not Working: e100e90c000f0f5d465bf00532374895  Blink
      //83 01 e1 00 e9 0c 00 0f0f b2b2b2b2b2 -307b7db0
  int color[5];
  color[2] = param3 & 0x1f;  //Middle
  color[1] = param4 & 0x1f;  //Top Right
  color[3] = param5 & 0x1f;  //Bottom Right
  color[4] = param6 & 0x1f;  //Bottom Left
  color[0] = param7 & 0x1f;  //Top Left

  mode = 2;
  Serial.printf("Found E90C Color: %i %i %i %i %i", color[0],color[1],color[2],color[3],color[4]);
  Serial.printf("\n");
  int randomon = 0;
  for (int k = 0; k < 40; k++){
    for (int j = 0; j <= 4; j++){
      for (int i = ((N_LEDS / 5) * j); i < ((N_LEDS / 5) * j) + (N_LEDS / 5); i++){
          //strip.setPixelColor(i, pallet_array[color[i % 5]]);
          strip.setPixelColor(i, pallet_array[color[(j + k) % 5]]);
      }
    }
    
    strip.show();
    delay(400);
  }
  for (int i = 0; i < N_LEDS; i++) {
    strip.setPixelColor(i, pallet_array[30]);
  }
  strip.show();
}


void e90f_function(uint8_t param1, uint8_t param2, uint8_t param3,uint8_t param4, uint8_t param5, uint8_t param6,uint8_t param7,uint8_t param8,uint8_t param9,uint8_t param10,uint8_t param11,uint8_t param12){
  mode = 2;
  int color[5];
  color[0] = param3 & 0x1f;
  color[1] = param4 & 0x1f;
  Serial.printf("Found E90f Color: %i %i", color[0], color[1]);
  for (int j=0; j<30; j++){
    for (int i = 1; i < 50; i++){
      if (i % 2 == 0){
        strip.setPixelColor(i, pallet_array[color[0]]);
      } else {
        strip.setPixelColor(i, pallet_array[30]);
      }
    }
    strip.show();
    delay(100);
    for (int i = 1; i < 60; i++){
      if (i % 2 == 0){
        strip.setPixelColor(i, pallet_array[30]);
      } else {
        strip.setPixelColor(i, pallet_array[color[0]]);
      }
    }
    strip.show();
    delay(100);
  }     
}

void e90e_function(uint8_t param1, uint8_t param2, uint8_t param3,uint8_t param4, uint8_t param5, uint8_t param6,uint8_t param7,uint8_t param8,uint8_t param9,uint8_t param10,uint8_t param11,uint8_t param12){
    mode = 2;
    int color[5];
    //e90e has 2 sub functions it seems, but we're not sure why. Not sure if param2 or param12 is the magic (or something else) 2 works for now.
    if (param2 == 0xd2){
        //2 6-bit hex colors
        //first color middle
        //second color outline
        // e100e90e00 23 d255 fc00fc d258 fc3900 78b0      //orange outline purple center
        int r1 = (param4 >> 2) * 4;
        int g1 = (param5 >> 2) * 4;
        int b1 = (param6 >> 2) * 4;
        
        int r2 = (param9 >> 2) * 4;
        int g2 = (param10 >> 2) * 4;
        int b2 = (param11 >> 2) * 4;
        int time = param2 * 60;
      for (int i = 0; i < N_LEDS; i++) {
        if (i % 5 == 0){
          strip.setPixelColor(i, strip.Color(r1, g1, b1));
        } else {
          strip.setPixelColor(i, strip.Color(r2, g2, b2));
        }
      }
      strip.show();
      //Serial.printf("Color: %i %i %i %i %i %i Time: %i", r1,g1,b1,r2,g2,b2,time);
      delay(time);


    } else if (param2 == 0x0f) {
      
      color[2] = param3 & 0x1f;  //Middle
      color[1] = param4 & 0x1f;  //Top Right
      color[3]= param5 & 0x1f;  //Bottom Right
      color[4] = param6 & 0x1f;  //Bottom Left
      color[0] = param7 & 0x1f;  //Top Left
      mode = 2;
      Serial.printf("Found Color: %i %i %i %i %i", color[0],color[1],color[2],color[3],color[4]);
      Serial.printf("\n");
      int randomon = 0;
      for (int k = 0; k < 20; k++){
        for (int j = 0; j <= 4; j++){
          randomon = random(0,3);
            for (int i = ((N_LEDS / 5) * j); i < ((N_LEDS / 5) * j) + (N_LEDS / 5); i++){
            //strip.setPixelColor(i, pallet_array[color[i % 5]]);
              if (randomon == 0){
              strip.setPixelColor(i, pallet_array[color[j]]);
              } else {
                strip.setPixelColor(i, pallet_array[30]);
              }
            }
        }
        strip.show();
        delay(200);
      
      }
    }
      for (int i = 0; i < N_LEDS; i++) {
        strip.setPixelColor(i, pallet_array[30]);
      }
      strip.show();
}

  void save_text(BLEAdvertisedDevice *advertisedDevice){
    sequenceNumber++;
     std::string strManufacturerData = advertisedDevice->getManufacturerData();
      uint8_t cManufacturerData[200];
      strManufacturerData.copy((char *)cManufacturerData, strManufacturerData.length(), 0);
    File file = SD.open(savefilename, FILE_APPEND);
    if(!file){
        Serial.println("Failed to open file for appending");
        //writeFile(SD, "/datalog_camera.txt", "Hello");
        return;
    } else {


    }
    file.printf(rtc.getTime().c_str());
    file.printf(": %i %s ", sequenceNumber, advertisedDevice->getAddress().toString().c_str());
    for (int i = 2; i < strManufacturerData.length(); i++) {
      //Serial.printf("%02x", cManufacturerData[i]);
      file.printf("%02x", cManufacturerData[i]);
    }
    file.printf("\n");
    Serial.printf("\n");
    file.close();
    digitalWrite(LED_BUILTIN, LOW);
    delay(100);
    digitalWrite(LED_BUILTIN, HIGH);
    delay(100);
    digitalWrite(LED_BUILTIN, LOW);
  }

  void onResult(BLEAdvertisedDevice *advertisedDevice) {

    //Anything that doesn't have this, we should just ignore.
    if (advertisedDevice->haveManufacturerData() == true) {
      std::string strManufacturerData = advertisedDevice->getManufacturerData();
      //Serial.printf("Length Of Data: %i", strManufacturerData.length());
      uint8_t cManufacturerData[200];
      strManufacturerData.copy((char *)cManufacturerData, strManufacturerData.length(), 0);


      //Is this Justins Wand?
      if (cManufacturerData[0] == 0x42 && cManufacturerData[1] == 0x01) {
        sequenceNumber++;
        save_text(advertisedDevice);
        //Debug Data Can be commented out.
        Serial.printf("Found JJWand: ", strManufacturerData.length());
        Serial.printf("A %i %s ", sequenceNumber, advertisedDevice->getAddress().toString().c_str());
        for (int i = 2; i < strManufacturerData.length(); i++) {
          Serial.printf("%02x", cManufacturerData[i]);
        }

        
        if (strManufacturerData.length() > 4) {
          if (cManufacturerData[2] == 0) {
            if (cManufacturerData[3] == 5) {
              blackout = true;
              dressmode = 1;   
              
            } else if (cManufacturerData[3] == 6) {
              blackout = false;
              dressmode = 0;   
              mode = 0;
            } else if (cManufacturerData[3] == 0) {     //Solid Colors
              mode = 0;
              selected_color = cManufacturerData[4];
              Serial.printf("Selected Color Is: %i", selected_color);
            } else if (cManufacturerData[3] == 1) {     //Patterns
              mode = 0;
              selected_color = cManufacturerData[4];
              
              //mode = cManufacturerData[3];
              //selected_color = cManufacturerData[4];      
              //Serial.printf("Selected Pattern Is: %i", selected_color);                          
            } else if (cManufacturerData[3] == 3) {     //Unlock
              mode = 0;
              dressmode = 0;                 
            } 
          }
        } else {
          Serial.printf("Invalid Code Detected");
        }

        Serial.printf("\n");
      /////////////////////////// Disney Codes
      } else if (cManufacturerData[0] == 0x83 && cManufacturerData[1] == 0x01 && cManufacturerData[4] == 0xe9) {
        sequenceNumber++;
        save_text(advertisedDevice);
        Serial.printf("Found Disney: ", strManufacturerData.length());
        Serial.printf("A %i %s ", sequenceNumber, advertisedDevice->getAddress().toString().c_str());
        for (int i = 2; i < strManufacturerData.length(); i++) {
          Serial.printf("%02x", cManufacturerData[i]);
        }
        Serial.printf("\n");
        
      } else if (cManufacturerData[0] == 0x83 && cManufacturerData[1] == 0x01) {
        Serial.printf("Found OTHER Disney: ", strManufacturerData.length());
        Serial.printf("B %i %s ", sequenceNumber, advertisedDevice->getAddress().toString().c_str());
        save_text(advertisedDevice);
        for (int i = 2; i < strManufacturerData.length(); i++) {
          Serial.printf("%02x", cManufacturerData[i]);
        }
        Serial.printf("\n");
      }
    }
  return;
  }
};


void setup() {
  delay(1000);  // prevents usb driver crash on startup, do not omit this
    Serial.begin(115200);
  rtc.setTime(0,0,1,1,1,2023);


   Serial.print("Initializing SD card...");

  // see if the card is present and can be initialized:
  if (!SD.begin(chipSelect)) {
    Serial.println("Card failed, or not present");
    // don't do anything more:
    //while (1);
    sleep(10);
  }
  sleep(1);


//  Serial.println("card initialized.");
//dataFile = SD.open("datalog.txt", FILE_WRITE);
 //Serial.println("Card open.");


  //vNopDelayMS(1000); // prevents usb driver crash on startup, do not omit this


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

}

long mytime;
long mytime2;
int failures = 0;
void loop() {
  timer++;
  
  digitalWrite(LED_BUILTIN, LOW);
  //Simple check to make sure scanner never stops.
  if (timer % 100 == 0) {  
    if(pBLEScan->isScanning() == false) {
      Serial.printf("New Scan Started \n");      
      pBLEScan->start(0, nullptr, true);
    }
  }

if (timer % 10000 == 1){
    File file = SD.open(savefilename, FILE_APPEND);
    if(!file){
        failures++;
        Serial.println("Failed to open file for appending");
        sprintf(savefilename, "/datalog%d.txt", failures);
        writeFile(SD, savefilename, "Hello");
        return;
    } else {


    }

    
}
if (timer == 19999){
  timer = 0;
}
  if(!SD.begin(chipSelect)){
        digitalWrite(LED_BUILTIN, HIGH);
        Serial.println("Card Mount Failed");
        //sleep(2);
    }
}
