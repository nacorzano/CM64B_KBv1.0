 __   __          
|  |_|  |--.-----.
|   _|     |  -__|
|____|__|__|_____|
 ______     ______     __   __     ______     ______   __  __     ______     ______     __  __    
/\  ___\   /\  __ \   /\ "-.\ \   /\  ___\   /\__  _\ /\ \/\ \   /\  __ \   /\  == \   /\ \_\ \   
\ \___  \  \ \  __ \  \ \ \-.  \  \ \ \____  \/_/\ \/ \ \ \_\ \  \ \  __ \  \ \  __<   \ \____ \  
 \/\_____\  \ \_\ \_\  \ \_\\"\_\  \ \_____\    \ \_\  \ \_____\  \ \_\ \_\  \ \_\ \_\  \/\_____\ 
  \/_____/   \/_/\/_/   \/_/ \/_/   \/_____/     \/_/   \/_____/   \/_/\/_/   \/_/ /_/   \/_____/ 
//////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                                  //
//    Firmware for the Sanctuary Keyboard                                                           //
//    Designed by: Foster Phillips, Lego_Rocket on many social media                                //
//    https://linktr.ee/Lego_Rocket                                                                 //
//    Primarily used to run the firmware behind the Sanctuary                                       //
//    Keyboard and kits to be sold at one point                                                     //
//      Firmware will be open source - hardware will be closed source                               //
//    This File is the first to compile - it does NOT include the main() functions                  //
//      That is only going to be in the _Main file, to allow for easy changing of Matrix functions  //
//                                                                                                  //
//////////////////////////////////////////////////////////////////////////////////////////////////////



//Libraries used
#include <BleKeyboard.h>        //Primary heavyweight for sending keystrokes
#include <EEPROM.h>             //Used for storage of MAC address selection

#include "CM64B_HW.h"  //Definitions file, for all the hardware - removes and simplifies from this file



//Setup keyboard
BleKeyboard Kbd("CM64B","NACH",100);



//  Code to "store" devices, so that the keyboard can switch connections on the fly ////////////////////////////////////////////////////////////////////////
//Adapted from : https://github.com/Cemu0/ESP32_USBHOST_TO_BLE_KEYBOARD/blob/main/src/USBHIDBootBLEKbd.cpp
//Primarily stores the selected MAC address in EEPROM storage
const int maxdevice = 3;
uint8_t MACAddress[maxdevice][6] = 
{
  {0x35, 0xAF, 0xA4, 0x07, 0x0B, 0x66},
  {0x31, 0xAE, 0xAA, 0x47, 0x0D, 0x61},
  {0x31, 0xAE, 0xAC, 0x42, 0x0A, 0x31}
  
};

//Basically just change the selected ID and reset - MAC address can only be changed before the keyboard start, so write to store selection, until changed again
  //Take in device number, and set the EEPROM to the selected - selects what address to shift to, instead of iterating to that address
void changeID(int DevNum)
{
    //Serial.println("changing MAC...");

    //Make sure the selection is valid
    if(DevNum < maxdevice)
    {
      //Write and commit to storage, reset ESP 32
      EEPROM.write(0,DevNum);
      EEPROM.commit();
      //esp_reset();
      esp_sleep_enable_timer_wakeup(1);
      esp_deep_sleep_start(); 
    }
}
}