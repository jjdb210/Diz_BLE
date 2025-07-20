/*
   Based on Neil Kolban example for IDF: https://github.com/nkolban/esp32-snippets/blob/master/cpp_utils/tests/BLE%20Tests/SampleScan.cpp
   Ported to Arduino ESP32 by Evandro Copercini
   Changed to a beacon scanner to report iBeacon, EddystoneURL and EddystoneTLM beacons by beegee-tokyo
*/

#include <Arduino.h>

#include "NimBLEDevice.h"
#include <NimBLEAdvertisedDevice.h>
#include "NimBLEBeacon.h"

/*
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <BLEBeacon.h>
*/

#include <Adafruit_NeoPixel.h>




// What pin are the LEDs on the dress (D10 is NOT the same as 10). and how many are there?
//dress!! 
#define PIN D10
#define N_LEDS 100
#define DRESS_ID 1  //Each dress needs an ID (1-5)

//Initiliaze the dress strip.
Adafruit_NeoPixel strip = Adafruit_NeoPixel(N_LEDS, PIN, NEO_GRB + NEO_KHZ800);

#define ENDIAN_CHANGE_U16(x) ((((x)&0xFF00) >> 8) + (((x)&0xFF) << 8))

int scanTime = 1;  //In seconds
int sequenceNumber = 0;
int mode = 0;  
int dressmode = 0; //Default to Kid mode (details at: https://emcot.world/index.php/Disney_Bluetooth_Wands)
bool blackout = false;

int lastmode = 0;
int timer = 0;

BLEScan *pBLEScan; //Old code but compatible

#include <ESP32Time.h>
ESP32Time rtc(3600);

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

//
int selected_color = 1;
int last_color = 0;

uint32_t pallet_array[] = { 
  strip.Color(0, 255, 255),      //0 = Cyan
  strip.Color(30, 255, 255),      //1 = Purple
  strip.Color(0, 0, 255),        //2 = Blue
  strip.Color(0, 0, 128),        //3 = Midnight Blue
  strip.Color(0, 0, 255),        //4 = Blue 
  strip.Color(255, 60, 255),      //5 = bright purple
  strip.Color(153, 0, 153),       //6 = 
  strip.Color(90, 0, 200),         //7
  strip.Color(153, 0, 153),        //8
  strip.Color(153, 0, 153),        //9
  strip.Color(153, 0, 153),       //10
  strip.Color(153, 0, 153),      //11
  strip.Color(153, 0, 153),       //12   13 20   25    4 1
  strip.Color(153, 0, 153),      //13 uurplr
  strip.Color(153, 0, 153),        //14   255,170,0 = Good Yellow
  strip.Color(255, 60, 0),       //15 off YUellow was 255 255 33
  strip.Color(255, 255, 128),      //16 Off Yellow
  strip.Color(255, 200, 50),     //17 Yellow ORange
  strip.Color(255, 255, 0),                                   //18 Lime Greenstrip.Color(51, 255, 51), 
  strip.Color(255, 30, 0),       //19 Orange
  strip.Color(255, 10, 0),        //20 Red Orange?
  strip.Color(255, 0, 0),         //21 Red 
  strip.Color(0, 255, 255),
  strip.Color(0, 255, 255),
  strip.Color(0, 255, 255),
  strip.Color(0, 255, 0),         //25 Green
  strip.Color(0, 0, 255),  //27
  strip.Color(255, 255, 255),
  strip.Color(255, 255, 255),
  strip.Color(0, 0, 0),
  strip.Color(0, 0, 0),
  strip.Color(255, 255, 255)      //32 White                  
};


uint32_t mode_array[] = {strip.Color(0,0,255),
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


uint32_t temp_color_array[] = { strip.Color(0, 0, 0), strip.Color(0, 0, 0), strip.Color(0, 0, 0), strip.Color(0, 0, 0), strip.Color(0, 0, 0) };

int temp_start = 0;
int temp_x = 0;
int temp_y = 0;
int temp_z = 0;


hw_timer_t *My_timer = NULL;


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

void e906_function(uint8_t param1, uint8_t param2, uint8_t param3,uint8_t param4, uint8_t param5){
    mode = 2;
    int color1 = param4 & 0x1f;
    int color2 = param5 & 0x1f;
    int color3 = param3 & 0x1f;
    int color4 = param2 & 0x1f;
    Serial.printf("e908: %i %i %i %i", color1, color2, color3, color4);
    for (int i = 0; i < N_LEDS; i++) {
      if (i % 2 == 0){
        strip.setPixelColor(i, pallet_array[color1]);
      } else {
        strip.setPixelColor(i, pallet_array[color2]);
      }
    }
    strip.show();
    delay(3000);
}

//Single 6bit color
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

//5 Color Pallet
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
 for (int j = 0; j <= 4; j++){
 for (int i = ((N_LEDS / 5) * j); i < ((N_LEDS / 5) * j) + (N_LEDS / 5); i++){
    //strip.setPixelColor(i, pallet_array[color[i % 5]]);
    strip.setPixelColor(i, pallet_array[color[j]]);
 }
 }
  strip.show();
  delay(3000);
}


void e911_function(uint8_t param1, uint8_t param2, uint8_t param3,uint8_t param4, uint8_t param5, uint8_t param6,uint8_t param7,uint8_t param8,uint8_t param9,uint8_t param10,uint8_t param11,uint8_t param12){
  mode = 2;
  //2 colors
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
    
      for (int i = 1; i < N_LEDS; i++){
        if (i % 2 == 0){
          strip.setPixelColor(i, current_color);
        } else {
          strip.setPixelColor(i, current_color2);
        }
      }
      
      strip.show();
      delay(20);
    }
    for (int temp_x = 30; temp_x > 0; temp_x--){
      uint32_t current_color = strip.Color(red1 + (temp_x * red_step), green1 + (temp_x * green_step), blue1 + (temp_x * blue_step));
      uint32_t current_color2 = strip.Color(red2 - (temp_x * red_step), green2 - (temp_x * green_step), blue2 - (temp_x * blue_step));
    
      for (int i = 1; i < N_LEDS; i++){
        if (i % 2 == 0){
          strip.setPixelColor(i, current_color);
        } else {
          strip.setPixelColor(i, current_color2);
        }
      }
      strip.show();
      delay(20);
    }
  }
}

void e912_function(uint8_t param1, uint8_t param2, uint8_t param3,uint8_t param4, uint8_t param5, uint8_t param6,uint8_t param7,uint8_t param8,uint8_t param9,uint8_t param10,uint8_t param11,uint8_t param12){
  mode = 2;
  //2 colors
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
    
      for (int i = 1; i < N_LEDS; i++){
        if (i % 2 == 0){
          strip.setPixelColor(i, current_color);
        } else {
          strip.setPixelColor(i, current_color2);
        }
      }
      
      strip.show();
      delay(20);
    }
    for (int temp_x = 30; temp_x > 0; temp_x--){
      uint32_t current_color = strip.Color(red1 + (temp_x * red_step), green1 + (temp_x * green_step), blue1 + (temp_x * blue_step));
      uint32_t current_color2 = strip.Color(red2 - (temp_x * red_step), green2 - (temp_x * green_step), blue2 - (temp_x * blue_step));
    
      for (int i = 1; i < N_LEDS; i++){
        if (i % 2 == 0){
          strip.setPixelColor(i, current_color);
        } else {
          strip.setPixelColor(i, current_color2);
        }
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
    
      for (int i = 1; i < N_LEDS; i++){
        if (i % 2 == 0){
          strip.setPixelColor(i, current_color);
        } else {
          strip.setPixelColor(i, current_color2);
        }
      }
      strip.show();
      delay(20);
    }
    for (int temp_x = 30; temp_x > 0; temp_x--){
      uint32_t current_color = strip.Color(red1 + (temp_x * red_step), green1 + (temp_x * green_step), blue1 + (temp_x * blue_step));
      uint32_t current_color2 = strip.Color(red2 - (temp_x * red_step), green2 - (temp_x * green_step), blue2 - (temp_x * blue_step));
    
      for (int i = 1; i < N_LEDS; i++){
        if (i % 2 == 0){
          strip.setPixelColor(i, current_color);
        } else {
          strip.setPixelColor(i, current_color2);
        }
      }

      strip.show();
      delay(20);
    }
  }
}

//
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


void e90e_function(uint8_t param1, uint8_t param2, uint8_t param3,uint8_t param4, uint8_t param5, uint8_t param6,uint8_t param7,uint8_t param8,uint8_t param9,uint8_t param10,uint8_t param11,uint8_t param12){
    //8301e100e90e00 130f b9b5b5b1b3 590ffb 48ae b0
    //8301e100e90e00 110f a3a3a3a3a3 591902 48ae b0
    //e100e90e00 e1d2 550027fcd258005f2878b0
    //e100e90e00 11d2 55fc4bfcd25800fc0078b0 

    //8301e100e90e0011d255fc4bfcd25800fc0068b0
    //                                    ^
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
      Serial.printf("Color: %i %i %i %i %i %i Time: %i", r1,g1,b1,r2,g2,b2,time);
      delay(time);
      //8301 e100 e90e 0024 d2 552c007fd25800003078 b0
      //8301e100e90e0024d2552c007fd25800003078b0

 //     e100e90e0023d255fc00fcd258fc390078b0    //Orange Outline
  //    e100e90e0024d2 552c007f d2 5800003 078b0    //Blue Outline, purple center
      
    
      //////////// Not In View (Errors?)
//      e100e90e00 24 d256 2c007f d246 000030 78b0      //Red Outline Green Center
//      e100e90e00 24 d256 2c007f d256 000030 78b0      //Red Outline, no center
//      e100e90e00 24 d256 2c007f d258 000030 78b0      //Blue OUtline Green Center
      ////////////Not IN View
      
        //e100e90e0024d255fc00fcd25800003078b0 
 //     e100e90e00 24 d255 fcfcfc d258 fcfcfc 78b0      //Blue outline Purple Center
 //     e100e90e00 23 d255 fc00fc d258 fc3900 78b0      //orange outline purple center
 //     e200e90e00 21 d255 fc0000 d258 fc2500 78b0      //orange outline red center
 //     e100e90e00 11 d255 fc4bfc d258 00fc00 78b0      //green outline white center
  //    e200e90e00 7b d255 0000fc d258 2c001b 78b0      //purple outline, blue center
   //   e200e90e00 65 d255 fc5000 d258 fc0066 78b0      //pink outline, yellow center
   //   e200e90e00 21 d255 fc7f00 d258 fc0000 78b0      //red outline, yellow center
  //    e200e90e00 68 d255 fc69fc d258 1e2dfc 78b0      //Skyeblue outline, white center

//99fcfc cyan
//fc99fc purple/magenta
//fcfc99 yellow
//99fc99 green
//


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

void e90f_function(uint8_t param1, uint8_t param2, uint8_t param3,uint8_t param4, uint8_t param5, uint8_t param6,uint8_t param7,uint8_t param8,uint8_t param9,uint8_t param10,uint8_t param11,uint8_t param12){
  mode = 2;
  int color[5];
  color[0] = param3 & 0x1f;
  color[1] = param4 & 0x1f;
  Serial.printf("Found E90f Color: %i %i", color[0], color[1]);
  for (int j=0; j<30; j++){
    for (int i = 1; i < N_LEDS; i++){
      if (i % 2 == 0){
        strip.setPixelColor(i, pallet_array[color[0]]);
      } else {
        strip.setPixelColor(i, pallet_array[30]);
      }
    }
    strip.show();
    delay(100);
    for (int i = 1; i < N_LEDS; i++){
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


void e910_function(uint8_t param1, uint8_t param2, uint8_t param3,uint8_t param4, uint8_t param5, uint8_t param6,uint8_t param7,uint8_t param8,uint8_t param9,uint8_t param10,uint8_t param11,uint8_t param12){
  mode = 2;
  int color[5];
  color[0] = param3 & 0x1f;
  color[1] = param4 & 0x1f;
  Serial.printf("Found E910 Color: %i %i", color[0], color[1]);
  for (int j=0; j<5; j++){
    for (int i = 1; i < N_LEDS; i++){
      if (i % 2 == 0){
        strip.setPixelColor(i, pallet_array[color[0]]);
      } else {
        strip.setPixelColor(i, pallet_array[color[1]]);
      }
    }
    strip.show();
    delay(1000);
    for (int i = 1; i < N_LEDS; i++){
      if (i % 5 == 0){
        strip.setPixelColor(i, pallet_array[30]);
      } else {
        strip.setPixelColor(i, pallet_array[30]);
      }
    }
    strip.show();
    delay(300);
  }     
}



  void onResult(BLEAdvertisedDevice *advertisedDevice) {

    //Anything that doesn't have this, we should just ignore.
    if (advertisedDevice->haveManufacturerData() == true) {
      std::string strManufacturerData = advertisedDevice->getManufacturerData();
      uint8_t cManufacturerData[100];
      strManufacturerData.copy((char *)cManufacturerData, strManufacturerData.length(), 0);


      //Is this Justins Wand?
      if (cManufacturerData[0] == 0x42 && cManufacturerData[1] == 0x01) {
        sequenceNumber++;

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




        
      } else if (cManufacturerData[0] == 0x83 && cManufacturerData[1] == 0x01 && cManufacturerData[4] == 0xe9) {
        //Is this Disney?Disney Code 
        sequenceNumber++;
        Serial.printf("Found Disney: ", strManufacturerData.length());
        Serial.printf("A %i %s ", sequenceNumber, advertisedDevice->getAddress().toString().c_str());
        for (int i = 2; i < strManufacturerData.length(); i++) {
          Serial.printf("%02x", cManufacturerData[i]);
        }
        Serial.printf("\n");
        //e9 05 - Solid Color Function?
        //00 11 0e ef b0
        
        if (cManufacturerData[5] == 0x05){      //e905 - Single Color Function 
          solid_color_function(cManufacturerData[7],cManufacturerData[8],cManufacturerData[9]);
        } else if (cManufacturerData[5] == 0x06){ // Possibly 2 color function
          e906_function(cManufacturerData[6],cManufacturerData[7],cManufacturerData[8],cManufacturerData[9],cManufacturerData[10]);
        }else if (cManufacturerData[5] == 0x08){ // Possibly 2 color function
          e908_function(cManufacturerData[6],cManufacturerData[7],cManufacturerData[10],cManufacturerData[11],cManufacturerData[12]);
        } else if (cManufacturerData[5] == 0x09){ // 5 color function
          e909_function(cManufacturerData[7],cManufacturerData[8],cManufacturerData[9],cManufacturerData[10],cManufacturerData[11],cManufacturerData[12],cManufacturerData[13]);
        } else if (cManufacturerData[5] == 0x0b){ // Circle Animation?

        } else if (cManufacturerData[5] == 0x0c){ //Cycling Colors
          e90c_function(cManufacturerData[7],cManufacturerData[8],cManufacturerData[9],cManufacturerData[10],cManufacturerData[11],cManufacturerData[12],cManufacturerData[13],cManufacturerData[14]);
        } else if (cManufacturerData[5] == 0x0e){ //NEXT ONE
          e90e_function(cManufacturerData[7],cManufacturerData[8],cManufacturerData[9],cManufacturerData[10],cManufacturerData[11],cManufacturerData[12],cManufacturerData[13],cManufacturerData[14],cManufacturerData[15],cManufacturerData[16],cManufacturerData[17],cManufacturerData[18]);
        } else if (cManufacturerData[5] == 0x0f){ //NEXT ONE
          e90f_function(cManufacturerData[7],cManufacturerData[8],cManufacturerData[9],cManufacturerData[10],cManufacturerData[11],cManufacturerData[12],cManufacturerData[13],cManufacturerData[14],cManufacturerData[15],cManufacturerData[16],cManufacturerData[17],cManufacturerData[18]);
        } else if (cManufacturerData[5] == 0x10){ //Alternating Colors
          e910_function(cManufacturerData[7],cManufacturerData[8],cManufacturerData[9],cManufacturerData[10],cManufacturerData[11],cManufacturerData[12],cManufacturerData[13],cManufacturerData[14],cManufacturerData[15],cManufacturerData[16],cManufacturerData[17],cManufacturerData[18]);
        } else if (cManufacturerData[5] == 0x11){ //Circle with Vibration?
          e911_function(cManufacturerData[7],cManufacturerData[8],cManufacturerData[9],cManufacturerData[10],cManufacturerData[11],cManufacturerData[12],cManufacturerData[13],cManufacturerData[14],cManufacturerData[15],cManufacturerData[16],cManufacturerData[17],cManufacturerData[18]);
        } else if (cManufacturerData[5] == 0x12){ //
          e912_function(cManufacturerData[7],cManufacturerData[8],cManufacturerData[9],cManufacturerData[10],cManufacturerData[11],cManufacturerData[12],cManufacturerData[13],cManufacturerData[14],cManufacturerData[15],cManufacturerData[16],cManufacturerData[17],cManufacturerData[18]);
        
        } else if (cManufacturerData[5] == 0x13){ //More Animation

        }else if (cManufacturerData[5] == 0x14){ //More Animation
          e914_function(cManufacturerData[7],cManufacturerData[8],cManufacturerData[9],cManufacturerData[10],cManufacturerData[11],cManufacturerData[12],cManufacturerData[13],cManufacturerData[14],cManufacturerData[15],cManufacturerData[16],cManufacturerData[17],cManufacturerData[18]);
        
        }
      } 
    }

    return;
  }
};



void setup() {
  delay(1000);  // prevents usb driver crash on startup, do not omit this
  Serial.begin(9600);
  strip.begin();
  strip.setBrightness(40);
  for (int i = 0; i < N_LEDS; i++) {
    if (i % 2 == 1){
      strip.setPixelColor(i, strip.Color(0, 0, 255));
    } else {
      strip.setPixelColor(i, strip.Color(255, 0, 0));         
    }
  }
  strip.show();
  delay(2000);
  mode = 0;
  selected_color = 9;


  //vNopDelayMS(1000); // prevents usb         ,bm,./driver crash on startup, do not omit this


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

void loop() {
  timer++;
  //mytime = rtc.getEpoch();
  //Simple check to make sure scanner never stops.
  if (timer % 100 == 0) {  
    if(pBLEScan->isScanning() == false) {
      Serial.printf("New Scan Started \n");      
      pBLEScan->start(0, nullptr, true);
    }
  }
  if (timer == 19999){
    timer = 0;
  }
  if (blackout){
      strip.setBrightness(0);
      for (int i = 0; i < N_LEDS; i++) {
        strip.setPixelColor(i, strip.Color(0, 0, 0));
      }
      strip.show();
  } else if (mode == 0) {                                          //Solid Color Command
      //strip.setBrightness(50);
      
      
      if (selected_color == 10){
        if (mode != lastmode || selected_color != last_color) {
          temp_color_array[0] = pallet_array[21];
          temp_color_array[1] = pallet_array[2];
          temp_color_array[2] = pallet_array[0];
          temp_color_array[3] = pallet_array[17];
          temp_color_array[4] = pallet_array[25];

          temp_start = 0;
        }
        if (timer % 200 == 0) {
          temp_start++;
          if (temp_start > 4) temp_start = 0;
          
        }
        for (int j = 0; j < 5; j++) {
          for (int i = j * (N_LEDS / 5); i < (j * (N_LEDS / 5)) + (N_LEDS / 5); i++) {
            strip.setPixelColor(i, temp_color_array[(j + temp_start) % 5]);
          }
        }

        strip.show();

       } else if (selected_color == 11){
        //Christmas 1
        if (mytime != mytime2){
           if (mytime % 3 == 0){
             for (int i = 1; i < N_LEDS; i++){
               if (i % 3 == 0){
               strip.setPixelColor(i, strip.Color(255,0,0));
               } else if (i % 3 == 1){
               strip.setPixelColor(i, strip.Color(0,255,0));
               } else {
                 strip.setPixelColor(i, strip.Color(255,255,255));
               }
             }
           } if (mytime % 3 == 1){
               for (int i = 1; i < N_LEDS; i++){
               if (i % 3 == 1){
               strip.setPixelColor(i, strip.Color(255,0,0));
               } else if (i % 3 == 2){
               strip.setPixelColor(i, strip.Color(0,255,0));
               } else {
                 strip.setPixelColor(i, strip.Color(255,255,255));
               }
             }           
             
             
             
          } else {
             for (int i = 1; i < N_LEDS; i++){
               if (i % 3 == 2){
               strip.setPixelColor(i, strip.Color(255,0,0));
               } else if (i % 3 == 0){
               strip.setPixelColor(i, strip.Color(0,255,0));
               } else {
                 strip.setPixelColor(i, strip.Color(255,255,255));
               }
             }
           }
        }
        strip.show();

      }else if (selected_color == 12){
               //Christmas 1
        for (int i = 1; i < 18; i++){
          strip.setPixelColor(i, strip.Color(0,0,0));
        }
        if (mytime != mytime2){
           if (mytime % 2 == 0){
             for (int i = 18; i < 34; i++){
               strip.setPixelColor(i, strip.Color(255,0,0));
             }
             for (int i = 34; i < 60; i++){
               strip.setPixelColor(i, strip.Color(0,255,0));
             }
           } else {
             for (int i = 18; i < 34; i++){
               strip.setPixelColor(i, strip.Color(0,255,0));
             }
             for (int i = 34; i < 60; i++){
               strip.setPixelColor(i, strip.Color(255,0,0));
             }
           }
        }
        strip.show();
        

      }else if (selected_color == 13){
                //Christmas 1
        if (mytime != mytime2){
           if (mytime % 2 == 0){
             for (int i = 0; i < N_LEDS; i++){
               if (i % 2 == 0){
                 strip.setPixelColor(i, strip.Color(255,255,255));
               } else {
                 strip.setPixelColor(i, strip.Color(0,255,0));                 
               }
             }
           } else {
             for (int i = 0; i < N_LEDS; i++){
               if (i % 2 == 0){
                 strip.setPixelColor(i, strip.Color(0,255,0));
               } else {
                 strip.setPixelColor(i, strip.Color(255,255,255));                 
               }
             }
           }
        }
        strip.show();
        

      }else if (selected_color == 14){
        //Christmas 1
                        //Christmas 1
        if (mytime != mytime2){
           if (mytime % 2 == 0){
             for (int i = 0; i < N_LEDS; i++){
               if (i % 2 == 0){
                 strip.setPixelColor(i, strip.Color(255,255,255));
               } else {
                 strip.setPixelColor(i, strip.Color(255,0,0));                 
               }
             }
           } else {
             for (int i = 0; i < N_LEDS; i++){
               if (i % 2 == 0){
                 strip.setPixelColor(i, strip.Color(255,0,0));
               } else {
                 strip.setPixelColor(i, strip.Color(255,255,255));                 
               }
             }
           }
        }
        strip.show();

      }else if (selected_color == 15){
                       //Christmas 1
        if (mytime != mytime2){
           if (mytime % 2 == 0){
             for (int i = 0; i < N_LEDS; i++){
               if (i % 2 == 0){
                 strip.setPixelColor(i, strip.Color(255,0,0));
               } else {
                 strip.setPixelColor(i, strip.Color(0,255,0));                 
               }
             }
           } else {
             for (int i = 0; i < N_LEDS; i++){
               if (i % 2 == 0){
                 strip.setPixelColor(i, strip.Color(0,255,0));
               } else {
                 strip.setPixelColor(i, strip.Color(255,0,0));                 
               }
             }
           }
        }
        strip.show();
        

      }else if (selected_color == 16){
        //Christmas 1
        if (mytime != mytime2){
        for (int i = 0; i < N_LEDS; i++){
          strip.setPixelColor(i, strip.Color(0,0,120));
        }
        strip.setPixelColor(random(1,N_LEDS), strip.Color(255,255,255));
        strip.setPixelColor(random(1,N_LEDS), strip.Color(255,255,255));

        strip.setPixelColor(random(1,N_LEDS), strip.Color(255,255,255));
        strip.show();
        }
      } else if (selected_color == 17){
        //Christmas 1
        

      }else if (selected_color == 19){
        //Christmas 1
        

      } else if (selected_color == 20){
        //Christmas 1
        

      }  else if (selected_color == 18){
        //Christmas 1
        

      }  else if (selected_color == 8){
        if (timer % 1000 == 0){
              temp_x++;
              if (temp_x > N_LEDS * 3){
                temp_x = 0;
                temp_y++;
              }       
              if (temp_y > 6){
                temp_y = 0;              
              }
              strip.setPixelColor(temp_x, pallet_array[temp_y]);
              strip.show();            
          }
      } else if (selected_color == 9){
          temp_color_array[0] = pallet_array[21];
          temp_color_array[1] = pallet_array[2];
          temp_color_array[2] = pallet_array[0];
          temp_color_array[3] = pallet_array[17];
          temp_color_array[4] = pallet_array[25];
        
        if (timer % 100000 == 0){
            temp_x++;
            if (temp_x > 30){
              temp_x = 0;
              temp_y++;
              if (temp_y > 4){              
                temp_y = 0;
              }
              temp_z = temp_y + 1;
              if (temp_z > 4){
                temp_z = 0;
              }       
            }       

            int red1 = temp_color_array[temp_y] >> 16 & 0xff;
            int green1 = temp_color_array[temp_y] >> 8 & 0xff;
            int blue1 = temp_color_array[temp_y] >> 0 & 0xff;
            int red2 = temp_color_array[temp_z] >> 16 & 0xff;
            int green2 = temp_color_array[temp_z] >> 8 & 0xff;
            int blue2 = temp_color_array[temp_z] >> 0 & 0xff;

            int red_step = (red2 - red1) / 30;
            int green_step = (green2 - green1) / 30;
            int blue_step = (blue2 - blue1) / 30;

            uint32_t current_color = strip.Color(red1 + (temp_x * red_step), green1 + (temp_x * green_step), blue1 + (temp_x * blue_step));
            for (int i = 0; i < N_LEDS; i++){
              strip.setPixelColor(i, current_color);
            }
            strip.show();            
      } 
       
      


    } else {
        for (int i = 0; i < N_LEDS; i++) {
          strip.setPixelColor(i, mode_array[selected_color]);
        }
        strip.show();
        delay(50);  
      }
  
    lastmode = mode;
    last_color = selected_color;
  }
   mytime2 = mytime;
}