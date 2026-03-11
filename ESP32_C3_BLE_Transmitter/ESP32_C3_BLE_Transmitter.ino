#ifndef _SIMPLE_BLE_H_
#define _SIMPLE_BLE_H_

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_bt.h"

#include "Arduino.h"

#include <Adafruit_NeoPixel.h>
#include "esp32-hal-log.h"


#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_defs.h"
#include "esp_bt_main.h"

#define MAX_MANUFACTURER_DATA_SIZE 100
#define MAX_SERVICE_DATA_SIZE 11

// What pin are the LEDs on the dress (D10 is NOT the same as 10). and how many are there?
#define PIN    D6
#define N_LEDS 10  

Adafruit_NeoPixel strip = Adafruit_NeoPixel(N_LEDS, PIN, NEO_BGR + NEO_KHZ800);

const int SHORT_PRESS_TIME = 500; // 500 milliseconds
const int BUTTON1_PIN = D4;
const int BUTTON2_PIN = D1;

int BUTTON1_LastState = LOW;
int BUTTON2_LastState = LOW;
int BUTTON1_CurrentState = LOW;
int BUTTON2_CurrentState = LOW;
bool BUTTON1_Pressed = false;
bool BUTTON2_Pressed = false;
unsigned long BUTTON1_PressedTime = 0;
unsigned long BUTTON2_PressedTime = 0;

int lasttime = 0;

bool radio_on = false;

int mode = 0; 
//  0 = Primary/Starlight Mode (32 steps)
//  1 = Dress Mode             (32 steps)
//  2 = Magicband Mode         (32 steps)

int last_mode = 0; // Used to Track Interal Updates
int mode_step = 0;

uint32_t pallet_array[] = { 
  strip.Color(0, 255, 255),      //0 = Cyan
  strip.Color(30, 255, 255),     //1 = Purple
  strip.Color(0, 0, 255),        //2 = Blue
  strip.Color(0, 0, 128),        //3 = Midnight Blue
  strip.Color(0, 0, 255),        //4 = Blue 
  strip.Color(255, 60, 255),     //5 = bright purple
  strip.Color(153, 0, 153),      //6 = lavender
  strip.Color(153, 0, 153),      //7 = purple
  strip.Color(153, 0, 153),      //8 = pink
  strip.Color(153, 0, 153),     //9 = pink
  strip.Color(153, 0, 153),      //10 = pink
  strip.Color(153, 0, 153),      //11 = pink
  strip.Color(153, 0, 153),     //12 = pink
  strip.Color(153, 0, 153),      //13 = pink
  strip.Color(255, 50, 50),      //14 = pink
  strip.Color(255, 60, 0),       //15 = Yellow Orange
  strip.Color(255, 255, 114),    //16 Off Yellow
  strip.Color(200, 180, 0),     //17 Yellow
  strip.Color(200, 210, 0), 	 //18 Lime Greenstrip.Color(51, 255, 51), 
  strip.Color(255, 50, 0),       //19 Orange
  strip.Color(255, 30, 0),       //20 Red Orange?
  strip.Color(255, 0, 0),        //21 Red 
  strip.Color(0, 255, 255),		 //22 cyan
  strip.Color(0, 255, 180),		  //23 cyan WITH A HINT OF GREEN
  strip.Color(0, 200, 120),		//24 cyan
  strip.Color(0, 255, 0),         //25 Green
  strip.Color(50, 255, 0),  		//26 Lime Green
  strip.Color(255, 255, 255),		//27 White
  strip.Color(255, 255, 255),		//28 white
  strip.Color(0, 0, 0),				//29 Off
  strip.Color(255, 255, 255),				//MB Default Color (Unique)
  strip.Color(255, 255, 255)      //Random                   
}; 

uint32_t mode_array[] = {
  strip.Color(0,0,255),
  strip.Color(0,255,0),
  strip.Color(255,0,0),
  strip.Color(255,255,0),
  strip.Color(255,0,255),
  strip.Color(255,255,255),
  strip.Color(0, 255, 255),     //17 Yellow ORange
  strip.Color(50, 50, 50),                                   //18 Lime Greenstrip.Color(51, 255, 51), 
  strip.Color(255, 99, 33),       //19 Orange
  strip.Color(255, 102, 0),
  strip.Color(195, 122, 123), 
  strip.Color(195, 192, 123), 
  strip.Color(195, 122, 192), 
  strip.Color(195, 0, 123), 
  strip.Color(195, 0, 96), 
  strip.Color(125, 0, 123), 

  strip.Color(0,0,128),
  strip.Color(0,128,0),
  strip.Color(128,0,0),
  strip.Color(128,128,0),
  strip.Color(128,0,128),
  strip.Color(128,128,128),
  strip.Color(0, 128, 128),     //17 Yellow ORange
  strip.Color(25, 25, 25),                                   //18 Lime Greenstrip.Color(51, 255, 51), 
  strip.Color(128, 99, 33),       //19 Orange
  strip.Color(128, 102, 0),
  strip.Color(90, 122, 123), 
  strip.Color(90, 192, 123), 
  strip.Color(90, 122, 192), 
  strip.Color(90, 0, 123), 
  strip.Color(90, 0, 96), 
  strip.Color(60, 0, 123), 
};     

//Keeps track of how long to transmit
int transmit_times = 0;
const int transmit_length = 5;


struct ble_gap_adv_params_s;

class SimpleBLE {
    public:

        SimpleBLE(void);
        ~SimpleBLE(void);

        /**
         * Start BLE Advertising
         *
         * @param[in] localName  local name to advertise
         *
         * @return true on success
         *
         */
        bool begin(String localName=String());

        /**
         * Advertises data on Manufacturer Data field
         *
         * @param[in] data  String with the message to be transmitted
         *
         * @return true on success
         *
         */
        bool advertise(String data);

        /**
         * Advertises data on Manufacturer Data field
         *
         * @param[in] data  byte array with the message to be transmitted
         *
         * @param[in] size  size of the byte array
         *
         * @return true on success
         *
         */
        bool advertise(byte* data, int size);

        /**
         * Advertises data on Service Data field
         *
         * @param[in] data  String with the message to be transmitted
         *
         * @return true on success
         *
         */
        bool serviceAdvertise(String data);

        /**
         * Advertises data on Service Data field
         *
         * @param[in] data  byte array with the message to be transmitted
         *
         * @param[in] size  size of the byte array
         *
         * @return true on success
         *
         */
        bool serviceAdvertise(byte* data, int size);

        //bool advertise(byte* data_man, int size_man, byte* data_ser, int size_ser);

        //bool advertise(String data_man, String data_ser); 

        /**
         * Stop BLE Advertising
         *
         * @return none
         */
        void end(void);

        private:
            void clearAdvertiseData();

            void fillManufacturerData(byte* data, int size);

            void fillServiceData(byte* data, int size);


        

    private:
        String local_name;
    private:

};

#endif



esp_ble_adv_data_t adv_data; // data that will be advertised
byte dataBuffer[100];
byte dataBuffer2[100];


// Standard parameters
static esp_ble_adv_data_t _adv_config = {
        .set_scan_rsp        = false,
        .include_name        = false,
        .include_txpower     = false,
        /*.min_interval        = 512,
        .max_interval        = 1024,*/
        .appearance          = 0,
        .manufacturer_len    = 0,
        .p_manufacturer_data = NULL,
        .service_data_len    = 0,
        .p_service_data      = NULL,
        .service_uuid_len    = 0,
        .p_service_uuid      = NULL,
        .flag                = (ESP_BLE_ADV_FLAG_NON_LIMIT_DISC|ESP_BLE_ADV_FLAG_BREDR_NOT_SPT)
};



// 
static esp_ble_adv_params_t _adv_params = {
        .adv_int_min         = 512,
        .adv_int_max         = 1024,
        .adv_type            = ADV_TYPE_NONCONN_IND,         // Excelent description of this parameter here: https://www.esp32.com/viewtopic.php?t=2267
        .own_addr_type       = BLE_ADDR_TYPE_PUBLIC,
        .peer_addr           = {0x00, },
        .peer_addr_type      = BLE_ADDR_TYPE_PUBLIC,
        .channel_map         = ADV_CHNL_ALL,
        .adv_filter_policy   = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

static void _on_gap(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param){
    if(event == ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT){
        esp_ble_gap_start_advertising(&_adv_params);
    }
}

static bool _init_gap(const char * name, esp_ble_adv_data_t* adv_data){
    if(!btStarted() && !btStart()){
        log_e("btStart failed");
        return false;
    }
    esp_bluedroid_status_t bt_state = esp_bluedroid_get_status();
    if(bt_state == ESP_BLUEDROID_STATUS_UNINITIALIZED){
        if (esp_bluedroid_init()) {
            log_e("esp_bluedroid_init failed");
            return false;
        }
    }
    if(bt_state != ESP_BLUEDROID_STATUS_ENABLED){
        if (esp_bluedroid_enable()) {
            log_e("esp_bluedroid_enable failed");
            return false;
        }
    }

    esp_err_t err;

    err = esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, ESP_PWR_LVL_P9);
    if (err != ESP_OK) {
        log_e("esp_ble_tx_power_set(ADV) failed");
        return false;
    }

    // Optional: also set default BLE TX power for other BLE uses
    err = esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, ESP_PWR_LVL_P9);
    if (err != ESP_OK) {
        log_e("esp_ble_tx_power_set(DEFAULT) failed");
        return false;
    }


    if(esp_ble_gap_set_device_name(name)){
        log_e("gap_set_device_name failed");
        return false;
    }
    if(esp_ble_gap_config_adv_data(adv_data)){
        log_e("gap_config_adv_data failed");
        return false;
    }
    if(esp_ble_gap_register_callback(_on_gap)){
        log_e("gap_register_callback failed");
        return false;
    }
    return true;
}

static bool _stop_gap()
{
    if(btStarted()){
        esp_bluedroid_disable();
        esp_bluedroid_deinit();
        btStop();
    }
    return true;
}

/*
 * BLE Arduino
 *
 * */


SimpleBLE::SimpleBLE()
{
    local_name = "esp32";
    adv_data = {
        .set_scan_rsp        = false,
        .include_name        = false,
        .include_txpower     = false,
        .appearance          = 0,
        .manufacturer_len    = 0,
        .p_manufacturer_data = NULL,  //manufacturer data is what we will use to broadcast our info
        .service_data_len    = 0,
        .p_service_data      = NULL,
        .service_uuid_len    = 0,
        .p_service_uuid      = NULL,
        .flag                = (ESP_BLE_ADV_FLAG_BREDR_NOT_SPT|(0x1 << 1))
    };
}

SimpleBLE::~SimpleBLE(void)
{
    clearAdvertiseData();
    _stop_gap();
}

bool SimpleBLE::begin(String localName)
{
    if(localName.length()){
        local_name = localName;
    }
    return _init_gap(local_name.c_str(), &_adv_config);
}

void SimpleBLE::end()
{
    _stop_gap();
}

bool SimpleBLE::advertise(String data) {
    data.getBytes(dataBuffer, data.length()+1);
    return advertise(dataBuffer, data.length());
}

bool SimpleBLE::advertise(byte* data, int size) {
    clearAdvertiseData();
    fillManufacturerData(data, size);
    return _init_gap(local_name.c_str(), &adv_data);
}

bool SimpleBLE::serviceAdvertise(String data) {
    data.getBytes(dataBuffer, data.length()+1);
    return serviceAdvertise(dataBuffer, data.length());
}

bool SimpleBLE::serviceAdvertise(byte* data, int size) {
    clearAdvertiseData();
    fillServiceData(data, size);
    return _init_gap(local_name.c_str(), &adv_data);
}

/* Advertising both Manufacturer Data and Service Data was not possible, I will continue testing and improving 
bool SimpleBLE::advertise(String data_man, String data_ser) {
    data_man.getBytes(dataBuffer, data_man.length()+1);
    data_ser.getBytes(dataBuffer2, data_ser.length()+1);
    return advertise(dataBuffer, data_man.length(), dataBuffer2, data_ser.length());
}

bool SimpleBLE::advertise(byte* data_man, int size_man, byte* data_ser, int size_ser) {
    clearAdvertiseData();
    fillManufacturerData(data_man, size_man);
    fillServiceData(data_ser, size_ser);
    return _init_gap(local_name.c_str(), &adv_data);
} */

void SimpleBLE::clearAdvertiseData() {
    if(adv_data.p_manufacturer_data != NULL) {
        free(adv_data.p_manufacturer_data);
        adv_data.p_manufacturer_data = NULL;
        adv_data.manufacturer_len = 0;
    }
    if(adv_data.p_service_data != NULL) {
        free(adv_data.p_service_data);
        adv_data.p_service_data = NULL;
        adv_data.service_data_len = 0;
    }
}

void SimpleBLE::fillManufacturerData(byte* data, int size) {
    if(size > MAX_MANUFACTURER_DATA_SIZE)
        size = MAX_MANUFACTURER_DATA_SIZE;
    adv_data.p_manufacturer_data = (uint8_t *) malloc(size*sizeof(uint8_t));
    adv_data.manufacturer_len = size;
    memcpy(adv_data.p_manufacturer_data, data, size);
}

void SimpleBLE::fillServiceData(byte* data, int size) {
    if(size > MAX_SERVICE_DATA_SIZE)
        size = MAX_SERVICE_DATA_SIZE;
    adv_data.p_service_data = (uint8_t *) malloc(size*sizeof(uint8_t));
    adv_data.service_data_len = size;
    memcpy(adv_data.p_service_data, data, size);
}


std::string advertisingdata;

SimpleBLE ble;

void setup() {
  strip.begin(); 
  strip.setBrightness(50);
  for (int i=0; i<N_LEDS; i++){
    strip.setPixelColor(i, strip.Color(255, 0, 0));    
  }
  strip.show();
  
  //Allow Serial Control
  Serial.begin(115200);
  
  ble.begin("");  //sets the device name
  advertisingdata = "";
  //Sample Code: 8301e90800f40fa0a4b9b9a4

  //Make the Buttons High
  pinMode(BUTTON1_PIN, INPUT_PULLUP);
  pinMode(BUTTON2_PIN, INPUT_PULLUP);

}


void loop() {

  //Flags and the Like (for later) 020106ff
  //0183 should be 8301 (lil endian for biz code)
  //LEN will be string size, up to 27.
  int len = 27;
  byte raw[128];

  //Check the buttons
  int BUTTON1_CurrentState = digitalRead(BUTTON1_PIN);
  int BUTTON2_CurrentState = digitalRead(BUTTON2_PIN); 
  int temp_status = 0;
  int time = millis();
  
  if (BUTTON1_CurrentState == LOW && BUTTON1_LastState == HIGH){
      BUTTON1_PressedTime = time;   
  } 
  if (BUTTON2_CurrentState == LOW  && BUTTON2_LastState == HIGH){
      BUTTON2_PressedTime = time;    
  }

  if (BUTTON1_CurrentState == HIGH && BUTTON1_LastState == LOW){
      temp_status = time;    
      if (temp_status - BUTTON1_PressedTime > SHORT_PRESS_TIME){
        Serial.println("Button 1 Long Press");  
        mode++;
        mode_step = 0;
        last_mode = -1;
        if (mode > 2){
          mode = 0;
        }
      } else {
        Serial.println("Button 1 Short Press"); 
        //asdf
        last_mode = -1;
        mode_step++;
        if (mode_step >= 32){
          mode_step = 0;
        }
      } 
  } 

// A short press resets the transmit timer to start transmitting whatever "code" is on file with the wand.
// A long press will enable or disable blackout mode.
  if (BUTTON2_CurrentState == HIGH  && BUTTON2_LastState == LOW){
      temp_status = time;    
      if (temp_status - BUTTON2_PressedTime > 2000){
        Serial.println("Button 2 Long Press (Ignore)"); 
        transmit_times = -5;
        mode = 0;
        last_mode = -1;
        mode_step = 29;
        radio_on = false; 
      } else {
        Serial.println("Button 2 Short Press"); 
        //transmit
        radio_on = false;
        transmit_times = 0;              
      }

            //Mode 0 - Starlight Mode
      if (mode == 0){
        char hexChar[2];
        sprintf(hexChar,"%02x", mode_step);
        advertisingdata = std::string("8301cf0b00c42922efa819f22b62") + hexChar;

      //Mode 1 - Dress Mode
      } else if (mode == 1){
        char hexChar[2];
        sprintf(hexChar,"%02x", mode_step);
        advertisingdata = std::string("42010000") + hexChar;


      } else if (mode == 2){
      //Mode 2  - Magicband Mode
          const std::string baseCodes[38] = {
            "8301e200e90c00b10fb9b4a4a1ad307bfcb0",
              "8301e200e91400060f4d5d58f44882d0651cd14602ed307bffb0",
              "8301e200e910006f4897d00e80d146091930d04e00b0",
              "8301e200e91200cf0fbca4bca4bc30d037f4d246001978b0",
              "8301e200e911004f0f515558f44882d146023ed06505b0",
              "8301e200e90e00010fb1b1b1b1b159190248aeb0",
              "8301e200e910006f4897d00e80d146091930d04e00b0",
              "8301e200e91200cf0fbca4bca4bc30d037f4d246001978b0",
              "8301e200e911004f0f515558f44882d146023ed06505b0",
              "8301e200e90e00010fb1b1b1b1b159190248aeb0",
              "8301e200e910006f4897d00e80d146091930d04e00b0",
              "8301e200e91200cf0fbca4bca4bc30d037f4d246001978b0",
              "8301e200e911004f0f515558f44882d146023ed06505b0",
              "8301e200e90e00010fb1b1b1b1b159190248aeb0",
              "8301e200e910006f4897d00e80d146091930d04e00b0",
              "8301e200e91200cf0fbca4bca4bc30d037f4d246001978b0",
              "8301e200e911004f0f515558f44882d146023ed06505b0",
              "8301e200e90e00010fb1b1b1b1b159190248aeb0",
              "8301e200e910006f4897d00e80d146091930d04e00b0",
              "8301e200e91200cf0fbca4bca4bc30d037f4d246001978b0",
              "8301e200e911004f0f515558f44882d146023ed06505b0",
              "8301e200e90e00010fb1b1b1b1b159190248aeb0",
              "8301e200e910006f4897d00e80d146091930d04e00b0",
              "8301e200e91200cf0fbca4bca4bc30d037f4d246001978b0",
              "8301e200e911004f0f515558f44882d146023ed06505b0",
              "8301e200e90e00010fb1b1b1b1b159190248aeb0",
              "8301e200e910006f4897d00e80d146091930d04e00b0",
              "8301e200e91200cf0fbca4bca4bc30d037f4d246001978b0",
              "8301e200e911004f0f515558f44882d146023ed06505b0",
              "8301e200e90e00010fb1b1b1b1b159190248aeb0",
              "8301e200e910006f4897d00e80d146091930d04e00b0",
              "8301e200e91200cf0fbca4bca4bc30d037f4d246001978b0",
              "8301e200e911004f0f515558f44882d146023ed06505b0",
              "8301e200e90e00010fb1b1b1b1b159190248aeb0",
              "8301e200e910006f4897d00e80d146091930d04e00b0",
              "8301e200e91200cf0fbca4bca4bc30d037f4d246001978b0",
              "8301e200e911004f0f515558f44882d146023ed06505b0",
              "8301e200e90e00010fb1b1b1b1b159190248aeb0"
          };
          advertisingdata = baseCodes[mode_step];

      }

  }
  BUTTON1_LastState = BUTTON1_CurrentState;
  BUTTON2_LastState = BUTTON2_CurrentState;


  ///// Lets get some logic in play. Only do this when we need to.
  if (mode != last_mode){
     strip.setBrightness(50);
     for (int i=1; i<N_LEDS; i++){
       if (mode == 0){
         strip.setPixelColor(i, pallet_array[mode_step]);  
       } else if (mode == 1){
         strip.setPixelColor(i, mode_array[mode_step]);
       }  
     }


  }

  strip.setPixelColor(0, mode_array[mode]);
  //strip.setPixelColor(5, mode_array[mode]);

  last_mode = mode;
  strip.show();















//We are only going to do this once per second (give or take).
if ((time - lasttime) > 500){
  //This is a fun tool to override the advertising data via serial.
  ////////////////// Could be removed when wand is final ////////////////
  while (Serial.available() > 0){
   //Create a place to hold the incoming message
    static char message[100];
    static unsigned int message_pos = 0;
    char inByte = Serial.read();
    //Message coming in (check not terminating character) and guard for over message size
    if ( inByte != '\n' && (message_pos < 100) ){
     //Add the incoming byte to our message
      message[message_pos] = inByte;
      message_pos++;
    } else {
     //Add null character to string
     advertisingdata = message;
     transmit_times = 0;
     message_pos = 0;
     }
  }
 ////////////////////////////////////////////////////////////////////////
 ////////////////////////////////////////////////////////////////////////

  // Switches our hex code to raw binary.
  len = advertisingdata.length(); 
  for(char i = 0; i < len; i++){
    byte extract;
    char a = advertisingdata[2*i];
    char b = advertisingdata[2*i + 1];
    extract = convertCharToHex(a)<<4 | convertCharToHex(b);
    raw[i] = extract;
  }

  //Are we in a place to transmit
    if (transmit_times < transmit_length){   
      if (radio_on == false){
        Serial.printf("Radio ON!");
        strip.setBrightness(75);
        ble.begin("");
        ble.advertise(raw, len);
        radio_on = true;
      }
      Serial.printf("Transmitting: %d ", time);
      Serial.println(advertisingdata.c_str()); 
      transmit_times++;
    } else {
      if (radio_on){
        strip.setBrightness(50);
        strip.show();
        radio_on = false;
        ble.end();
      }
    }
    
    lasttime = time;
  } // End Time If
}




char convertCharToHex(char ch)
{
  char returnType;
  switch(ch)
  {
    case '0':
    returnType = 0;
    break;
    case  '1' :
    returnType = 1;
    break;
    case  '2':
    returnType = 2;
    break;
    case  '3':
    returnType = 3;
    break;
    case  '4' :
    returnType = 4;
    break;
    case  '5':
    returnType = 5;
    break;
    case  '6':
    returnType = 6;
    break;
    case  '7':
    returnType = 7;
    break;
    case  '8':
    returnType = 8;
    break;
    case  '9':
    returnType = 9;
    break;
    case  'a':
    returnType = 10;
    break;
    case  'b':
    returnType = 11;
    break;
    case  'c':
    returnType = 12;
    break;
    case  'd':
    returnType = 13;
    break;
    case  'e':
    returnType = 14;
    break;
    case  'f' :
    returnType = 15;
    break;
    default:
    returnType = 0;
    break;
  }
  return returnType;
}