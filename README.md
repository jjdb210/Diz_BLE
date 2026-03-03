# Diz_BLE

Contains various scripts for use with Arduino / ESP32 based chips that allows Disney BLE signals to be recorded, received, and sent. 

Full Details about the reverse engineering can be found here:
https://emcot.world/Disney_MagicBand%2B_Bluetooth_Codes

Often worked on at http://twitch.tv/jjdb210


## ESP32_C3_BLE_Dress100 

Code for a 100 to 150 pixel based dress (spiral). Receiver only.

## ESP32_C3_BLUE_Hat210

Code for a hat that has 50 lights, 2 ears, and the rest are in the hat itself. This hat also as an SD reordered.

## ESP32_S3_BLE_To_SD_Recorder

Code for a simple recording device. Does a better job of recording than the hat (working towards being faster and more reliable recorder)

## ESP32_C3_BLE_Transmitter

Code for a wand with 5 to 10 LEDS that is designed for sending BLE codes. Primary role is to send starlight codes, but also sends custom codes to dresses, and a third option for sending magic band codes. Also supports sending commands via serial via the console interface.

## Legacy_Code

Older Code being archived from initial devices.

## Sample Raw Data Recordings

These are recordings either taken by Wireshark, or by the SD recorder. Useful for seeing actual codes and timestamps of various shows (or if attempting to reproduce a show).

