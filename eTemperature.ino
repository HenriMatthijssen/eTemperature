// =========================================================================================
// 
// eTemperature v1.07  Software for reading/displaying temperature and send it to Domoticz
//
//                     See 'Blog Henri Matthijssen' for instructions for the hardware and setup
//                     http://matthijsseninfo.nl
//
//                     (c) 2020 Henri Matthijssen (henri@matthijsseninfo.nl)
//
//                     Please do not distribute without permission of the original author and 
//                     respect his time and work spent on this.
//
// =========================================================================================
//
// Please follow instructions below to compile this code with the Arduino IDE:
//
// Please make sure that you have set next line in 'Additional Board Manager URL's' of 
// Preferences:
//
// http://arduino.esp8266.com/stable/package_esp8266com_index.json
//
// Then goto Boards Manager (‘Tools > Board’) and install the ‘esp8266 by ESP8266 Community’ entry.
// Finally set your board to 'NodeMCU 1.0 (ESP-12E Module)' in Tools menu. Make sure you use
// Version 2.4.2 (not higher)
//
// Further make sure that in the Libraries Manager (Sketch > Include Library > Manage Libraries...'
// you have installed next libraries:
//
// 'Adafruit BME280 Library' van Adafruit
// 'ESP8266 and ESP32 Oled Driver for SSD1306 display from Dianiel Eichhorn, Frabrice Weinberg
// 'DHT sensor library for ESPx' from beegee_tokyo
// 'LedControl' from Eberhard Fahle
//
// =========================================================================================
// 
// First time you installed the software the ESP8266 will startup in Access-Point (AP) mode.
// Connect with you WiFi to this AP using next information:
// 
// AP ssid           : eTemperature
// AP password       : eTemperature
//
// Enter IP-address 192.168.4.1 in your web-browser. This will present you with dialog to fill
// the credentials of your home WiFi Network. Fill your home SSID and password and press the
// submit button. Your ESP8266 will reboot and connect automatically to your home WiFi Network.
// Now find the assigned IP-address to the ESP8266 and use that again in your web-browser.
//
// Before you can use the Web-Interface you have to login with a valid user-name
// and password on location: http://ip-address/login (cookie is valid for 24 hours).
//
// Default user-name : admin
// Default password  : notdodo
//
// In the Web-Interface you can change the default user-name / password (among other
// settings like calibration).
//
// When using the API you always need to supply a valid value for the API key
// (default value=27031969). The API has next format in the URL:
//
// http://ip-address/api?action=xxx&value=yyy&api=zzz
//
// Currently next API actions are supported:
//
// You can upgrade the firmware with the (hidden) firmware upgrade URL:
// http://ip-address/upgradefw
// (you first needed to be logged in for the above URL to work)
//
// You can erase all settings by clearing the EEPROM with next URL:
// http://ip-address/erase
// (you first needed to be logged in for the above URL to work)
//
// =========================================================================================

// Current version of the eTemperature software
float g_current_version = 1.07;

// -----------------------------------------------------------------------------------------
// INCLUDES
// -----------------------------------------------------------------------------------------

// For ESP8266
#include <ESP8266WiFi.h>                            // ESP8266 WiFi
#include <ESP8266WebServer.h>                       // ESP8266 Web Server
#include <EEPROM.h>                                 // EEPROM
#include <SPI.h>                                    // Serial Peripheral Interrface
#include <FS.h>                                     // File system

//#include "Base64.h"                                 // Base64 encoding

#include <BME280I2C.h>                              // Library for reading out BME280 sensor
#include <Wire.h>                                   // Library for Wire
#include "SSD1306Wire.h"                            // Library for controlling Oled Display
#include <DHTesp.h>                                 // Library for reading out DHT22 sensor
#include "LedControl.h"                             // Library for controlling Max7219 Display

// -----------------------------------------------------------------------------------------
// DEFINES
// -----------------------------------------------------------------------------------------

// Defines for EEPROM map
#define LENGTH_SSID                            32   // Maximum length SSID
#define LENGTH_PASSWORD                        64   // Maximum length Password for SSID
#define LENGTH_HOSTNAME                        15   // Maximum length hostname
#define LENGTH_API_KEY                         64   // Maximum length API Key

#define LENGTH_WEB_USER                        64   // Maximum lenght of web-user
#define LENGTH_WEB_PASSWORD                    64   // Maximum lenght of web-password

#define LENGTH_DOMOTICZ_SERVER                 64   // Maximum lenght of Domodicz Server Name
#define LENGTH_DOMOTICZ_USER                   64   // Maximum lenght of Domoticz User
#define LENGTH_DOMOTICZ_PASSWORD               64   // Maximum lenght of Domoticz Password
#define LENGTH_FINGERPRINT                    128   // Maximum lenght of fingerprint of secure connection

#define ALLOCATED_EEPROM_BLOCK               1024   // Size of EEPROM block that you claim
#define BASE64_LEN                            512   // Used for encoding user-name / password of Domoticz 

//
// Defines for used pins on ESP8266 for controlling motor
//
// Remark: for NodeMCU the hard-coded pin-numbers can be found in file
// \variants\nodemcu\pins_arduino
//
// In this file you will find:
//
// #define PIN_WIRE_SDA (4)
// #define PIN_WIRE_SCL (5)
//
// static const uint8_t SDA = PIN_WIRE_SDA;
// static const uint8_t SCL = PIN_WIRE_SCL;
//
// #define LED_BUILTIN 16
//
//  static const uint8_t D0   = 16;
//  static const uint8_t D1   = 5;
//  static const uint8_t D2   = 4;
//  static const uint8_t D3   = 0;
//  static const uint8_t D4   = 2;
//  static const uint8_t D5   = 14;
//  static const uint8_t D6   = 12;
//  static const uint8_t D7   = 13;
//  static const uint8_t D8   = 15;
//  static const uint8_t D9   = 3;
//  static const uint8_t D10  = 1;
//

#define BUILTIN_LED1             D4       // Builtin LED of ESP8266 is on GPIO2 (=D4)

// Miscellaneous defines
#define g_epsilon           0.00001       // Used for comparing fractional values

// Default language
#define DEFAULT_LANGUAGE          1       // Default language is Dutch (=1), 0=English

// -----------------------------------------------------------------------------------------
// CONSTANTS
// -----------------------------------------------------------------------------------------

// ESP8266 settings
const char*       g_AP_ssid              = "eTemperature";  // Default Access Point ssid
const char*       g_AP_password          = "eTemperature";  // Default Password for AP ssid

const char*       g_default_host_name    = "eTemperature";  // Default Hostname
const char*       g_default_api_key      = "27031969";   		// Default API key

const char*       g_default_web_user     = "admin";      // Default Web User
const char*       g_default_web_password = "notdodo";    // Default Web Password

// LED character definitions (Max7219)

const static byte led_digits[][8] = {
  { 0x00, 0x00, 0x7E, 0x81, 0x81, 0x81, 0x7E, 0x00}, // Character '0'
  { 0x00, 0x00, 0x04, 0x82, 0xFF, 0x80, 0x00, 0x00}, // Character '1'
  { 0x00, 0x00, 0xE2, 0x91, 0x91, 0x91, 0x8E, 0x00}, // Character '2'
  { 0x00, 0x00, 0x42, 0x89, 0x89, 0x89, 0x76, 0x00}, // Character '3'
  { 0x00, 0x00, 0x1F, 0x10, 0x10, 0xFF, 0x10, 0x00}, // Character '4'
  { 0x00, 0x00, 0x8F, 0x89, 0x89, 0x89, 0x71, 0x00}, // Character '5'
  { 0x00, 0x00, 0x7E, 0x89, 0x89, 0x89, 0x71, 0x00}, // Character '6'
  { 0x00, 0x00, 0x01, 0x01, 0xF9, 0x05, 0x03, 0x00}, // Character '7'
  { 0x00, 0x00, 0x76, 0x89, 0x89, 0x89, 0x76, 0x00}, // Character '8'
  { 0x00, 0x00, 0x8E, 0x91, 0x91, 0x91, 0x7E, 0x00}, // Character '9'
};

const static byte led_special_characters[][8] = {
  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, // Character ' '
  { 0x00, 0x00, 0x00, 0x00, 0x90, 0x00, 0x00, 0x00}, // Character ':'
  { 0x00, 0x00, 0x00, 0x10, 0x10, 0x10, 0x10, 0x00}, // Character '-'
  { 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00}, // Character '.'
  { 0x10, 0x10, 0x10, 0x04, 0x82, 0xFF, 0x80, 0x00}, // Character '!'  => -1 (for -1x)
};

const static byte led_chars[][8] = {
  { 0x00, 0x00, 0xFE, 0x11, 0x11, 0x11, 0xFE, 0x00}, // Character 'A'
  { 0x00, 0x00, 0xFF, 0x89, 0x89, 0x89, 0x76, 0x00}, // Character 'B'
  { 0x00, 0x00, 0x7E, 0x81, 0x81, 0x81, 0x42, 0x00}, // Character 'C'
  { 0x00, 0x00, 0xFF, 0x81, 0x81, 0x81, 0x7E, 0x00}, // Character 'D'
  { 0x00, 0x00, 0xFF, 0x89, 0x89, 0x89, 0x81, 0x00}, // Character 'E'
  { 0x00, 0x00, 0xFF, 0x09, 0x09, 0x09, 0x01, 0x00}, // Character 'F'
  { 0x00, 0x00, 0x7E, 0x81, 0x81, 0x91, 0x72, 0x00}, // Character 'G'
  { 0x00, 0x00, 0xFF, 0x08, 0x08, 0x08, 0xFF, 0x00}, // Character 'H'
  { 0x00, 0x00, 0x00, 0x81, 0xFF, 0x81, 0x00, 0x00}, // Character 'I'
  { 0x00, 0x00, 0x60, 0x80, 0x80, 0x80, 0x7F, 0x00}, // Character 'J'
  { 0x00, 0x00, 0xFF, 0x18, 0x24, 0x42, 0x81, 0x00}, // Character 'K'
  { 0x00, 0x00, 0xFF, 0x80, 0x80, 0x80, 0x80, 0x00}, // Character 'L'
  { 0x00, 0x00, 0xFF, 0x02, 0x04, 0x02, 0xFF, 0x00}, // Character 'M'
  { 0x00, 0x00, 0xFF, 0x02, 0x04, 0x08, 0xFF, 0x00}, // Character 'N'
  { 0x00, 0x00, 0x7E, 0x81, 0x81, 0x81, 0x7E, 0x00}, // Character 'O'
  { 0x00, 0x00, 0xFF, 0x11, 0x11, 0x11, 0x0E, 0x00}, // Character 'P'
  { 0x00, 0x00, 0x7E, 0x81, 0x81, 0xA1, 0x7E, 0x80}, // Character 'Q'
  { 0x00, 0x00, 0xFF, 0x11, 0x31, 0x51, 0x8E, 0x00}, // Character 'R'
  { 0x00, 0x00, 0x46, 0x89, 0x89, 0x89, 0x72, 0x00}, // Character 'S'
  { 0x00, 0x00, 0x01, 0x01, 0xFF, 0x01, 0x01, 0x00}, // Character 'T'
  { 0x00, 0x00, 0x7F, 0x80, 0x80, 0x80, 0x7F, 0x00}, // Character 'U'
  { 0x00, 0x00, 0x3F, 0x40, 0x80, 0x40, 0x3F, 0x00}, // Character 'V'
  { 0x00, 0x00, 0x7F, 0x80, 0x60, 0x80, 0x7F, 0x00}, // Character 'W'
  { 0x00, 0x00, 0xE3, 0x14, 0x08, 0x14, 0xE3, 0x00}, // Character 'X'
  { 0x00, 0x00, 0x03, 0x04, 0xF8, 0x04, 0x03, 0x00}, // Character 'Y'
  { 0x00, 0x00, 0xE1, 0x91, 0x89, 0x85, 0x83, 0x00}, // Character 'Z'
};

// -----------------------------------------------------------------------------------------
// GLOBALS
// -----------------------------------------------------------------------------------------

// Webserver
ESP8266WebServer  server(80);                       // Listen port Webserver
WiFiClientSecure  client;                           // Client connection to Domoticz

unsigned int      g_start_eeprom_address = 0;       // Start offset in EEPROM

// Structure used for EEPROM read/write
struct {
  
  char  ssid              [ LENGTH_SSID      + 1 ]            = "";
  char  password          [ LENGTH_PASSWORD  + 1 ]            = "";

  float version                                               = 0.0;

  char  hostname          [ LENGTH_HOSTNAME  + 1 ]            = "";
  char  apikey            [ LENGTH_API_KEY   + 1 ]            = "";
  int   apikey_set                                            = 0;

  int   language                                              = DEFAULT_LANGUAGE;

  char  web_user          [ LENGTH_WEB_USER      + 1 ]        = "";
  char  web_password      [ LENGTH_WEB_PASSWORD  + 1 ]        = "";

  char  domoticz_server   [ LENGTH_DOMOTICZ_SERVER      + 1 ] = "";
  int   domoticz_port                                         = 443;
  char  domoticz_user     [ LENGTH_DOMOTICZ_USER        + 1 ] = "";
  char  domoticz_password [ LENGTH_DOMOTICZ_PASSWORD    + 1 ] = "";

  char  fingerprint       [ LENGTH_FINGERPRINT + 1 ]          = "";

  int   idx_virtual_sensor                                    = 0;
  int   update_frequency                                      = 30;  

  int   sensor_type                                           = 1;      // 0 = DHT22, 1 = BME280
  
  int   dht22_data                                            = 4;      // GPIO4

  int   display_type                                          = 1;      // 0 = MAX719, 1 = Oled 128x64, 2 = None

  int   max7219_data                                          = 13;     // GPIO13 (D7)
  int   max7219_clk                                           = 14;     // GPIO14 (D5)
  int   max7219_cs                                            = 12;     // GPIO12 (D6)
  int   max7219_brightness                                    = 8;      // Medium brightness

  int   oled_address                                          = 0x3c;   // 0x3c
  int   oled_sda                                              = 4;      // GPIO4 (D2)
  int   oled_scl                                              = 5;      // GPIO5 (D1)
  int   oled_brightness                                       = 4;      // Medium brightness
  
  float calibration_temperature                               = 0.0;
  float calibration_humidity                                  = 0.0;
  float calibration_pressure                                  = 0.0;

} g_data;

// For upgrading firmware
File    					g_upload_file;
String  					g_filename;

// Running in Access Point (AP) Mode (=0) or Wireless Mode (=1). Default start with AP Mode
int     					g_wifi_mode = 0;

// BME280 
// Wire SDA to GPIO4 (=D2) and SCL to GPIO5 (=D1)
BME280I2C         g_bme;

// OLED display (address, SDA, SCL)
// Wire SDA to GPIO4 (=D2) and SCL to GPIO5 (=D1)
SSD1306Wire       g_ssd1306 = SSD1306Wire(0x3c, 4, 5);

// Temperature sensor (DH22)
DHTesp            g_dht;

// Ledcontrol (data, clk, cs, no_leds)  // default 13,14,5,2 (for Max7219 Display)
LedControl        g_lc = LedControl(0, 0, 0, 0);

// Globals containing measured temperature, humidity and pressure (if available)
float             g_temperature = 0.0;
float             g_humidity    = 0.0;
float             g_pressure    = 0.0;

// -----------------------------------------------------------------------------------------
// LANGUAGE STRINGS
// -----------------------------------------------------------------------------------------
const String l_configure_wifi_network[] = {
    "Configure WiFi Network",
    "Configureer WiFi Netwerk",
  };
const String l_enter_wifi_credentials[] = {
    "Provide Network SSID and password",
    "Vul Netwerk SSID en wachtwoord in",
  };
const String l_network_ssid_label[] = {
    "Network SSID       : ",
    "Netwerk SSID       : ",
  };
const String l_network_password_label[] = {
    "Network Password   : ",
    "Netwerk Wachtwoord : ",
  };
  
const String l_network_ssid_hint[] = {
    "network SSID",
    "netwerk SSID",
  };
const String l_network_password_hint[] = {
    "network password",
    "netwerk wachtwoord",  
  };

const String l_login_screen[] = {
    "eTemperature Login", 
    "eTemperature Login" ,
  };
const String l_provide_credentials[] = { 
    "Please provide a valid user-name and password", 
    "Geef een correcte gebruikersnaam en wachtwoord" ,
  };
const String l_user_label[] = {
    "User       : ",
    "Gebruiker  : ", 
  };
const String l_password_label[] = {
    "Password   : ",
    "Wachtwoord : ",
  };  
  
const String l_username[] = {
    "user-name",
    "gebruikersnaam",
  };
const String l_password[] = {
    "password",
    "wachtwoord",
  };

const String l_settings_screen[] = {
    "eTemperature Settings", 
    "eTemperature Instellingen",
  };
const String l_provide_new_credentials[] = { 
    "Please provide a new user-name, password and API key", 
    "Geef een nieuwe gebruikersnaam, wachtwoord en API key",
  };

const String l_new_user_label[] = {
    "New User                : ",
    "Nieuwe Gebruiker        : ", 
  };
const String l_new_password_label[] = {
    "New Password            : ",
    "Nieuw Wachtwoord        : ",
  };  
const String l_new_api_key[] = {
    "New API Key             : ",
    "Nieuwe API Key          : ",
  };
  
const String l_change_language_hostname[] = {
    "Change language and Hostname",
    "Verander taal en hostnaam"
  };
const String l_language_label[] = {
    "New Language            : ",
    "Nieuwe Taal             : ",
  };
const String l_hostname_label[] = {
    "New Hostname            : ",
    "Nieuwe Hostnaam         : ",
  };
const String l_language_english[] = {
    "English",
    "Engels",
  };
const String l_language_dutch[] = {
    "Dutch",
    "Nederlands",
  };
const String l_change_settings[] = {
    "Change settings",
    "Verander instellingen",
  };

const String l_login_again [] = {
    "After pressing [Submit] you possibly need to login again",
    "Na het drukken van [Submit] moet je mogelijk opnieuw inloggen",
  };

const String l_version[] = {
    "Version",
    "Versie",
  };

const String l_hostname[] = {
    "Hostname ",
    "Hostnaam ",
  };

const String l_btn_logout[] = {
    "Logout",
    "Uitloggen",
  };
const String l_btn_settings[] = {
    "Settings",
    "Instellingen",
  };
const String l_btn_stop_updating[] = {
    "Stop updating",
    "Stop updaten",
  };
const String l_btn_start_updating[] = {
    "Start updating",
    "Start updaten",
  };

const String l_after_upgrade [] = {
    "After pressing 'Upgrade' please be patient. Wait until device is rebooted.",
    "Wees geduldig na het drukken van 'Upgrade'. Wacht totdat het device opnieuw is opgestart.",
  };

const String l_temperature [] = {
    "Temperature : ",
    "Temperatuur : ",
  };
const String l_degrees [] = {
    "degrees",
    "graden",    
  };
const String l_humidity [] = {
    "Humidity    : ",
    "Vochtigheid : ",
  };
const String l_pressure[] = {
    "Pressure    : ",
    "Druk        : ",
  };

const String l_change_domoticz_settings[] = {
  "Change specific Domoticz Server settings",
  "Verander specifieke Domoticz Server instellingen"
};

const String l_domoticz_server_label[] = {
    "Domoticz Server         : ",
    "Domoticz Server         : ",
  };
const String l_domoticz_port_label[] = {
    "Domoticz Port           : ",
    "Domoticz Poort          : ",
  };

const String l_fingerprint_label[] = {
    "Fingerprint             : ",
    "Fingerprint             : ",
  };

const String l_idx_virtual_sensor_label[] = {
    "Index Virtual Sensor    : ",
    "Index Virtuele Sensor   : ",
  };

const String l_change_hardware_settings[] = {
    "Set Hardware related settings for connected devices",
    "Configureer hardware gerelateerde instellingen van aangesloten devices"
  };

const String l_sensor_type_label[] = {
    "Sensor Type             : ",
    "Sensor Type             : ",
  };
const String l_sensor_type_dht22[] = {
    "DHT22",
    "DHT22",
  };
const String l_sensor_type_bme280[] = {
    "BME280",
    "BME280",
  };

const String l_pin_dht22_label[] = {
    "GPIO for DHT22 Data     : ",
    "GPIO voor DHT22 Data    : ",
  };

const String l_display_type_label[] = {
    "Display Type            : ",
    "Display Type            : ",
  };
const String l_display_type_max7219[] = {
    "Max7219",
    "Max7219",
  };
const String l_display_type_oled[] = {
    "Oled 128x64",
    "Oled 128x64",
  };
const String l_display_type_none[] = {
    "None",
    "Geen",
  };

const String l_max7219_pin_data_label[] = {
    "Max7219 GPIO for Data   : ",
    "Max7219 GPIO voor Data  : ",
  };
const String l_max7219_pin_clk_label[] = {
    "Max7219 GPIO for Clk    : ",
    "Max7219 GPIO voor Clk   : ",
  };
const String l_max7219_pin_cs_label[] = {
    "Max7219 GPIO for CS     : ",
    "Max7219 GPIO voor CS    : ",
  };
const String l_max7219_brightness_label[] = {
    "Max7219 Brightness      : ",
    "Max7219 Helderheid      : ",
  };

const String l_oled_address_label[] = {
    "Oled Address            : ",
    "Oled Adres              : ",
  };
const String l_decimal_label[] = {
    "(decimal)",
    "(decimaal)",
  };
const String l_oled_pin_sda_label[] = {
    "Oled GPIO for SDA       : ",
    "Oled GPIO voor SDA      : ",
  }; 
const String l_oled_pin_scl_label[] = {
    "Oled GPIO for SCL       : ",
    "Oled GPIO voor SCL      : ",
  };
const String l_oled_brightness_label[] = {
    "Oled Brightness         : ",
    "Oled Helderheid         : ",
  }; 

const String l_change_calibration_settings[] = {
    "Change calibration settings",
    "Verander calibratie instellingen"
  };

const String l_update_frequency[] = {
    "Update frequency        : ",
    "Update frequentie       : ",
  };
  
const String l_calibration_temperature_label[] = {
    "Calibration Temperature : ",
    "Calibratie Temperatuur  : ",
  };
const String l_calibration_humidity_label[] = {
    "Calibration Humidity    :",
    "Calibratie Vochtigheid  : ",
  };
const String l_calibration_pressure_label[] = {
    "Calibration Pressure    :",
    "Calibratie Druk         : ",
  };

const String l_update_frequency_message[] = {
    "Update frequency to Domoticz Server is ",
    "Update frequentie naar Domoticz Server is ",
  };
const String l_seconds[] = {
    "second(s)",
    "seconde(n)",
  };

// -----------------------------------------------------------------------------------------
// IMPLEMENTATION OF METHODS    
// -----------------------------------------------------------------------------------------

// -----------------------------------------------------------------------------------------
// setup
// Default setup() method called once by Arduino
// -----------------------------------------------------------------------------------------
void setup() 
{
  // Setup serial port for your Serial Monitor
  Serial.begin(115200);

  // Initialize SPI
  SPI.begin();

  // Initialize Wire
  Wire.begin();
  
  // Initialize EEPROM before you can use it 
  // (should be enough to fit g_data structure)
  EEPROM.begin( ALLOCATED_EEPROM_BLOCK );

  // Read current contents of EEPROM
  EEPROM.get( g_start_eeprom_address, g_data );

  // Set internal LED as Output
  pinMode(BUILTIN_LED1, OUTPUT);

  // Switch off internal LED
  digitalWrite(BUILTIN_LED1, HIGH);

  delay(200);

  Serial.println("");
  Serial.println("");
  Serial.println("Initializing");
  Serial.println("");

  // If current major version differs from EEPROM, write back new default values
  if ( (int)g_data.version != (int)g_current_version )
  {
    Serial.print("Major version in EEPROM ('");
    Serial.print(g_data.version);
    Serial.print("') differs with this current version ('");
    Serial.print(g_current_version);
    Serial.println("')"), 
    Serial.println("Setting default values in EEPROM.");
    Serial.println("");

    // Write back new default values
    strcpy (g_data.ssid,               "");
    strcpy (g_data.password,           "");
    
    strcpy (g_data.hostname,           g_default_host_name);
    g_data.version                   = g_current_version;
    strcpy (g_data.apikey,             g_default_api_key);
    g_data.apikey_set                = 0;

    strcpy(g_data.web_user,            g_default_web_user);
    strcpy(g_data.web_password,        g_default_web_password);
    
    g_data.language                  = DEFAULT_LANGUAGE;   

    strcpy(g_data.domoticz_server,     "Mijn_Server");
    g_data.domoticz_port             = 443;
    strcpy(g_data.domoticz_user,       "Gebruiker");
    strcpy(g_data.domoticz_password,   "Wachtwoord");
    
    strcpy(g_data.fingerprint,         "A0 F6 0E A2 00 D1 EF D8 73 00 40 B3 00 61 8C 56 ED F0 1A 22");
    
    g_data.idx_virtual_sensor        = 1;
    g_data.update_frequency          = 5;

    g_data.sensor_type               = 1;       // 0 = DHT22, 1 = BME280 (default)
    
    g_data.dht22_data                = 4;       // GPIO4

    g_data.display_type              = 1;       // 0 = MAX719, 1 = Oled 128x64 (default)

    g_data.max7219_data              = 13;      // GPIO13
    g_data.max7219_clk               = 14;      // GPIO14
    g_data.max7219_cs                = 5;       // GPIO5
    g_data.max7219_brightness        = 8;       // Medium Brightness
    
    g_data.oled_address              = 0x3c;    // 0x3c
    g_data.oled_sda                  = 4;       // GPIO4
    g_data.oled_scl                  = 5;       // GPIO5
    g_data.oled_brightness           = 4;       // Medium Brightness
  
    g_data.calibration_temperature   = 0.0;
    g_data.calibration_humidity      = 0.0;   
    g_data.calibration_pressure      = 0.0;   
  }

  // Set current version
  g_data.version = g_current_version;

  // Store values into EEPROM
  EEPROM.put( g_start_eeprom_address, g_data );
  EEPROM.commit();

  // Display current set hostname
  Serial.print("EEPROM Hostname                 : ");
  Serial.println(g_data.hostname);

  // Set Hostname
  WiFi.hostname( g_data.hostname );    

  // Display current 'eTemperature' version
  Serial.print("EEPROM Version                  : ");
  Serial.println(g_data.version);

  // Display current Web-Page Language Setting
  Serial.print("EEPROM Language                 : ");

  if (g_data.language == 0)
  {
    Serial.println(l_language_english[g_data.language]);
  }
  else
  {
    Serial.println(l_language_dutch[g_data.language]);
  }
  
  // Display current API key
  Serial.print("EEPROM API key                  : ");
  Serial.println(g_data.apikey);
  Serial.print("EEPROM API key set              : ");
  Serial.println(g_data.apikey_set);

  Serial.println("");

  // Display set web-user & web-password
  Serial.print("EEPROM Web User                 : ");
  Serial.println(g_data.web_user);
  Serial.print("EEPROM Web Password             : ");
  Serial.println(g_data.web_password);
  Serial.println("");

  // Display stored SSID & password
  Serial.print("EEPROM ssid                     : ");
  Serial.println(g_data.ssid);
  Serial.print("EEPROM password                 : ");
  Serial.println(g_data.password);
  Serial.println();

  Serial.print("EEPROM Domoticz Server          : ");
  Serial.println(g_data.domoticz_server); 
  Serial.print("EEPROM Domoticz Port            : ");
  Serial.println(g_data.domoticz_port); 
  Serial.print("EEPROM Domoticz User            : ");
  Serial.println(g_data.domoticz_user); 
  Serial.print("EEPROM Domoticz Password        : ");
  Serial.println(g_data.domoticz_password); 
  Serial.println();
  
  Serial.print("EEPROM Fingerprint              : ");
  Serial.println(g_data.fingerprint); 
  Serial.println();

  Serial.print("EEPROM idx virtual sensor       : ");
  Serial.println(g_data.idx_virtual_sensor); 
  Serial.print("EEPROM Update frequency         : ");
  Serial.println(g_data.update_frequency); 
  Serial.println();

  // Display current chosen Sensor Type Setting
  Serial.print("EEPROM Sensor Type              : ");
  if (g_data.sensor_type == 0)
  {
    Serial.println(l_sensor_type_dht22[g_data.language]);
  }
  else
  {
    Serial.println(l_sensor_type_bme280[g_data.language]);
  }

  Serial.print("EEPROM DHT22 Data               : ");
  Serial.println(g_data.dht22_data);
  Serial.println();

  // Display current chosen Display Type Setting
  Serial.print("EEPROM Display Type             : ");
  switch (g_data.display_type)
  {
    case 0:  Serial.println(l_display_type_max7219[g_data.language]);
             break;
    case 1:  Serial.println(l_display_type_oled[g_data.language]);
             break;
    default: Serial.println(l_display_type_none[g_data.language]);
             break;             
  }
  Serial.println();

  Serial.print("EEPROM Max7219 Data             : ");
  Serial.println(g_data.max7219_data);
  Serial.print("EEPROM Max7219 Clk              : ");
  Serial.println(g_data.max7219_clk);
  Serial.print("EEPROM Max7219 CS               : ");
  Serial.println(g_data.max7219_cs);
  Serial.print("EEPROM Max7219 Brightness       : ");
  Serial.println(g_data.max7219_brightness);
  Serial.println();

  Serial.print("EEPROM Oled Address             : 0x");
  Serial.println(g_data.oled_address, HEX);
  Serial.print("EEPROM Oled SDA                 : ");
  Serial.println(g_data.oled_sda);
  Serial.print("EEPROM Oled SCL                 : ");
  Serial.println(g_data.oled_scl);
  Serial.print("EEPROM Oled Brightness          : ");
  Serial.println(g_data.oled_brightness);
  Serial.println();
  
  Serial.print("EEPROM Calibration Temperature  : ");
  Serial.println(g_data.calibration_temperature); 
  Serial.print("EEPROM Calibration Humidity     : ");
  Serial.println(g_data.calibration_humidity); 
  Serial.print("EEPROM Calibration Pressure     : ");
  Serial.println(g_data.calibration_pressure); 
  Serial.println();

  // AP or WiFi Connect Mode

  // No WiFi configuration, then default start with AP mode
  if ( (strlen(g_data.ssid) == 0) || (strlen(g_data.password) == 0) )
  {
    Serial.println("No WiFi configuration found, starting in AP mode");
    Serial.println("");

    Serial.print("SSID          : '");
    Serial.print(g_AP_ssid);
    Serial.println("'");
    Serial.print("Password      : '");
    Serial.print(g_AP_password);
    Serial.println("'");

    // Ask for WiFi network to connect to and password
    g_wifi_mode = 0;
    
    WiFi.mode(WIFI_AP);
    WiFi.softAP(g_AP_ssid, g_AP_password);

    Serial.println("");
    Serial.print("Connected to  : '");
    Serial.print(g_AP_ssid);
    Serial.println("'");
    Serial.print("IP address    : ");
    Serial.println(WiFi.softAPIP());
  }
  // WiFi configuration found, try to connect
  else
  {
    int i = 0;

    Serial.print("Connecting to : '");
    Serial.print(g_data.ssid);
    Serial.println("'");

    WiFi.mode(WIFI_STA);
    WiFi.begin(g_data.ssid, g_data.password);

    while (WiFi.status() != WL_CONNECTED && i < 31)
    {
      delay(1000);
      Serial.print(".");
      ++i;
    }

    // Unable to connect to WiFi network with current settings
    if (WiFi.status() != WL_CONNECTED && i >= 30)
    {
      WiFi.disconnect();

      g_wifi_mode = 0;

      delay(1000);
      Serial.println("");

      Serial.println("Couldn't connect to network :( ");
      Serial.println("Setting up access point");
      Serial.print("SSID     : '");
      Serial.print(g_AP_ssid);
      Serial.println("'");
      Serial.print("Password : ");
      Serial.println(g_AP_password);
      
      // Ask for WiFi network to connect to and password
      WiFi.mode(WIFI_AP);
      WiFi.softAP(g_AP_ssid, g_AP_password);
      
      Serial.print("Connected to  : '");
      Serial.println(g_AP_ssid);
      Serial.println("'");
      IPAddress myIP = WiFi.softAPIP();
      Serial.print("IP address    : ");
      Serial.println(myIP);
    }
    // Normal connecting to WiFi network
    else
    {
      g_wifi_mode = 1;

      Serial.println("");
      Serial.print("Connected to  : '");
      Serial.print(g_data.ssid);
      Serial.println("'");
      Serial.print("IP address    : ");
      Serial.println(WiFi.localIP());
    }
    // Disable sleep Mode
    // WiFi.setSleepMode(WIFI_NONE_SLEEP);
  }

  // If connected to AP mode, display simple WiFi settings page
  if ( g_wifi_mode == 0)
  {
      Serial.println("");
      Serial.println("Connected in AP mode thus display simple WiFi settings page");
      server.on("/", handle_wifi_html);
  }
  // If connected in WiFi mode, display the eTemperature Web-Interface
  else
  {
    Serial.println("");
    Serial.println("Connected in WiFi mode thus display eTemperature Web-Interface");
    server.on ( "/", handle_root_ajax ); 
  }

  // Setup supported URL's for this ESP8266
  server.on("/erase", handle_erase );                       // Erase EEPROM
  server.on("/api", handle_api);                            // API of eTemperature

  server.on("/wifi_ajax", handle_wifi_ajax);                // Handle simple WiFi dialog
  
  server.on("/login", handle_login_html);                   // Login Screen
  server.on("/login_ajax", handle_login_ajax);              // Handle Login Screen

  server.on("/settings", handle_settings_html);             // Change Settings
  server.on("/settings_ajax", handle_settings_ajax);        // Handle Change Settings

  server.on("/upgradefw", handle_upgradefw_html);           // Upgrade firmware
  server.on("/upgradefw2", HTTP_POST, []() 
  {
    server.sendHeader("Connection", "close");
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    ESP.restart();
  }, []() 
  {
    HTTPUpload& upload = server.upload();
    
    if (upload.status == UPLOAD_FILE_START)
    {
      g_filename = upload.filename;
      Serial.setDebugOutput(true);
      
      Serial.printf("Starting upgrade with filename: %s\n", upload.filename.c_str());
      Serial.printf("Please be patient...\n");
      
      uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x01000) & 0xFFFFF000;

      // Start with max available size
      if ( !Update.begin( maxSketchSpace) ) 
      { 
          Update.printError(Serial);
      }
    }
    else if (upload.status == UPLOAD_FILE_WRITE)
    {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize)
        {
          Update.printError(Serial);
        }
    }
    else if (upload.status == UPLOAD_FILE_END)
    {
        if (Update.end(true)) //true to set the size to the current progress
        {
          Serial.printf("Upgrade Success: %u\nRebooting...\n", upload.totalSize);
        }
        else
        {
          Update.printError(Serial);
        }
        Serial.setDebugOutput(false);
    }
    yield();
  });

  // List of headers to be recorded  
  const char * headerkeys[] = {"User-Agent","Cookie"} ;
  size_t headerkeyssize = sizeof(headerkeys)/sizeof(char*);

  // Ask server to track these headers
  server.collectHeaders(headerkeys, headerkeyssize );

  // DHT22 configured
  if (g_data.sensor_type == 0)
  {  
    Serial.println("Initialize DHT22 Sensor");
    
    // Initialize DHT libarary
    g_dht.setup(g_data.dht22_data, DHTesp::DHT22);      // Connect DHT sensor configured GPIO (default GPIO4)
  }
  // BME280 configured
  else
  {
    Serial.println("Initialize BEM280 Sensor");
    
    if (g_bme.begin())
    {
      switch(g_bme.chipModel())
      {
        case BME280::ChipModel_BME280:
          Serial.println("Found BME280 sensor");
          break;
        case BME280::ChipModel_BMP280:
          Serial.println("Found BMP280 sensor, but no Humidity available");
          break;
        default:
          Serial.println("Found UNKNOWN sensor");
          break;
      }
    }
    else
    {
      Serial.println("Could not find BME280 sensor");
    }
  }

  // Max7219 Display configured
  if (g_data.display_type == 0)
  {
    Serial.println("Initialize Max8217 Display");
    
    // Initialize the Max8217 Display: Ledcontrol (data, clk, cs, number_of_leds)
    g_lc = LedControl(g_data.max7219_data, g_data.max7219_clk, g_data.max7219_cs, 2);

    // Wake-up
    g_lc.shutdown(0, false);                          // Wake-up LED 0
    g_lc.shutdown(1, false);                          // Wake-up LED 1

    // Set brightness
    g_lc.setIntensity(0, g_data.max7219_brightness);  // LED Brightness to Medium
    g_lc.setIntensity(1, g_data.max7219_brightness);  // LED Brightness to Medium

    // Clear display
    g_lc.clearDisplay(0);                             // Clear LED 0 Display
    g_lc.clearDisplay(1);                             // Clear LED 1 Display
  
    // Display 'OK' on LED
    led_display_character('O', 1);
    led_display_character('K', 0);
  }  
  // Oled Display configured
  else if (g_data.display_type == 1)
  {
    Serial.println("Initialize Oled display");
    
    // Initialize Oled display
    // Default: Address is 0x3c, SDA on GPIO4 (=D2) and SCL on GPIO5 (=D1)
    g_ssd1306 = SSD1306Wire(g_data.oled_address, g_data.oled_sda, g_data.oled_scl);

    // Initialize the display    
    g_ssd1306.init();
    
    // Set display on
    g_ssd1306.displayOn();

    // Set brightness
    g_ssd1306.setBrightness(g_data.oled_brightness);

    // Clear display
    g_ssd1306.clear();
  
    // Set Font
    g_ssd1306.setFont( ArialMT_Plain_24 );
  }
  // No display configured
  else
  {
    ;
  }
  
  // Start HTTP server
  server.begin();
  Serial.println("HTTP server started");  
}

// -----------------------------------------------------------------------------------------
// loop ()
// Here you find the main code which is run repeatedly
// -----------------------------------------------------------------------------------------
void loop() 
{
  // Handle Client
  server.handleClient();

  // Flash internal LED
  flash_internal_led();

  // DHT22 sensor
  if (g_data.sensor_type == 0)
  {
    // Retrieve Temperature Measurements
    delay(g_dht.getMinimumSamplingPeriod());
    
    g_temperature = g_dht.getTemperature() + g_data.calibration_temperature;
    g_humidity    = g_dht.getHumidity()    + g_data.calibration_humidity;
  }
  // BME280 sensor
  else
  {
    float temp(NAN), hum(NAN), pres(NAN);
  
    BME280::TempUnit tempUnit(BME280::TempUnit_Celsius);
    BME280::PresUnit presUnit(BME280::PresUnit_hPa);

    // Retrieve Temperature Measurements
    g_bme.read(pres, temp, hum, tempUnit, presUnit);    
    
    g_temperature = temp + g_data.calibration_temperature;
    g_humidity    = hum  + g_data.calibration_humidity;
    g_pressure    = pres + g_data.calibration_pressure;
  }

  // Check if valid readings are received
  if (isnan(g_temperature))
  {
    g_temperature = 0.0;
  }
  if (isnan(g_humidity))
  {
    g_humidity    = 0.0;
  }
  if (isnan(g_pressure))
  {
    g_pressure    = 0.0;
  }

  // Display the retrieved temperature measurement on the LED display (if configured)
  display_temperature ( g_temperature, g_humidity, g_pressure );

  // Send data to configured Domoticz Server
  send_data_to_domoticz( g_data.domoticz_server, g_data.domoticz_port, g_data.fingerprint,
                         g_data.domoticz_user, g_data.domoticz_password,
                         g_data.idx_virtual_sensor, g_temperature, g_humidity, g_pressure);
 
  // Delay configured time (in seconds);
  for (int i = 0; i < (g_data.update_frequency * 2); i++)
  {
    // Delay 500 ms
    delay( 500 );

    // Handle Client
    server.handleClient();    
  }

  // Check if WiFi is still connected
  if ( 
       ( g_wifi_mode == 1)
       &&
       (WiFi.status() != WL_CONNECTED )
     )
  {
    wifi_reconnect();
  }
}

// -----------------------------------------------------------------------------------------
// display_temperature
// Display the retrieved temperature measurements on the configured display
// -----------------------------------------------------------------------------------------
void display_temperature( float temperature, float humidity, float pressure )
{
  // Max7219 Display configured
  if (g_data.display_type == 0)
  {
    int negative = 0;
    if (temperature < 0)
    {
      negative = 1;
      temperature *= -1;
    }
  
    // Extract digits for LED 0 and LED 1
    int ones = ((int)temperature % 10);         // extract ones from temperature
    int tens = (((int)temperature / 10) % 10);  // extract tens from temperature
  
    // Display 'ones' on LED 0
    led_display_character(('0' + ones), 0);
  
    if (negative == 0)
    {
      // Display 'tens' on LED 1
      led_display_character(('0' + tens), 1);
    }
    else
    {
      // -10 degrees or lower?
      if (temperature > 10)
      {
        // Display '-1' on LED 1
        led_display_character('!', 1);
      }
      else
      {
        // Display '-' on LED1
        led_display_character('-', 1);
      }
    }
  }
  // Oled Display Configured
  else if (g_data.display_type == 1)
  {
    // Clear display
    g_ssd1306.clear();    

    // Set brightness
    g_ssd1306.setBrightness(g_data.oled_brightness);

    // Set text Alightment
    g_ssd1306.setTextAlignment(TEXT_ALIGN_CENTER);
      
    // Line 1: Display Temperature (with precision 1)
    String output_temperature = String(temperature,1) + String(" C");
    g_ssd1306.drawString( 64, 4, output_temperature );

    // DHT: Line 2: Display humidity
    if (g_data.sensor_type == 0)
    {
      String output_humidity = String(humidity,1) + String("%");
      g_ssd1306.drawString( 64, 32, output_humidity );       
      
    }
    // BME280: Line 2: Display Pressure
    else if (g_data.sensor_type == 1)
    {
      String output_pressure = String(pressure,1) + String(" hPa");
      g_ssd1306.drawString( 64, 32, output_pressure );       
    }

    // Finally display data
    g_ssd1306.display();    
  }
  // No Display configured
  else
  {
    ;
  }
}

// -----------------------------------------------------------------------------------------
// send_data_to_domoticz
// Send temperature measurements to configured Domoticz Server
// (see https://www.domoticz.com/wiki/Domoticz_API/JSON_URL's#Temperature)
//
// Temperature/Humidity: 
// /json.htm?type=command&param=udevice&idx=IDX&nvalue=0&svalue=TEMP;HUM;HUM_STAT
//
// Temperature/Humidity/Barometer
// /json.htm?type=command&param=udevice&idx=IDX&nvalue=0&svalue=TEMP;HUM;HUM_STAT;BAR;BAR_FOR
//
// Temperature/Barometer
// /json.htm?type=command&param=udevice&idx=IDX&nvalue=0&svalue=TEMP;BAR;BAR_FOR;ALTITUDE
// -----------------------------------------------------------------------------------------
void send_data_to_domoticz(
                    const char* domoticz_server,            
                    int         domoticz_port,
                    const char* fingerprint,
                    const char* domoticz_web_user,
                    const char* domoticz_web_password,
                    int         idx, 
                    float       temperature, 
                    float       humidity,
                    float       pressure)
{
//    char user_password[512];
//    char user_password_base64[512];
//
//    // Start fresh
//    memset(user_password,        0, sizeof(user_password));
//    memset(user_password_base64, 0, sizeof(user_password_base64));
//
//    // base64 encode the user:password
//    String helper = String(domoticz_web_user) + ":" + String(domoticz_web_password);
//    helper.toCharArray(user_password, 512);
//    base64_encode(user_password_base64, user_password, strlen(user_password));

    // Connect to configured Domoticz Server/Port
    client.stop();        
//    client.setFingerprint(fingerprint);   // Only possible with esp8266 above version 2.4.2 (but seems to have a bug)
    
    if (client.connect( domoticz_server, domoticz_port)) 
    {
        // Connect succeeded
        Serial.println("Connection to Domoticz Server succeeded");
        Serial.println();

        // Prepare buffer with JSON for supplying temperature readings
        String buffer;
        buffer  = "GET /json.htm?type=command&param=udevice&idx=";
        buffer += idx;
        buffer += "&nvalue=0&svalue=";

        // If DHT22: TEMP;HUM;0
        if (
            (g_data.sensor_type == 0) 
           )
        {
          buffer += temperature;
          buffer += ";";
          buffer += humidity;
          buffer += ";0";
        }
        // If BME280: TEMP;HUM;0;BAR;0
        else if ( (g_data.sensor_type == 1)&&(g_bme.chipModel() == BME280::ChipModel_BME280) )
        {
          buffer += temperature;
          buffer += ";";
          buffer += humidity;
          buffer += ";0;";         
          buffer += pressure;
          buffer += ";0";
        }
        // If BMP280: TEMP;BAR;0;0
        else if ( (g_data.sensor_type == 1)&&(g_bme.chipModel() == BME280::ChipModel_BMP280) )
        {
          buffer += temperature;
          buffer += ";";
          buffer += pressure;
          buffer += ";0;0";
        }
        buffer += " HTTP/1.1\r\n";

        buffer += "Host: ";
        buffer += domoticz_server;
        buffer += ":";
        buffer += String(domoticz_port);
        buffer += "\r\n";

//        buffer += "Authorization: Basic ";
//        buffer += user_password_base64;
//        buffer += "\r\n";
    
        buffer += "User-Agent: Arduino\r\n";
        buffer += "Connection: close\r\n\r\n";

        // Log buffer (JSON)
        Serial.print(buffer);      

        // Send buffer (JSON) to connected Domoticz Server
        client.print(buffer);

        // Handle possible responses from client
        while(client.connected()) 
        {
           while(client.available()) 
           {
              Serial.write(client.read());
           }
        }
        client.stop();
    }  
    else
    {
        // Connection failed
        Serial.println("Connection to Domoticz Server failed");
    }
}

// -----------------------------------------------------------------------------------------
// wifi_reconnect
// Automatically reconnect to your known WiFi network in case you got disconnected
// -----------------------------------------------------------------------------------------
void wifi_reconnect()
{
  // Try to disconnect WiFi
  Serial.println("WiFi disconnected");
  WiFi.disconnect(true);

  // Starting WiFi again
  Serial.println("Starting WiFi again");

  Serial.print("Connecting to : '");
  Serial.print(g_data.ssid);
  Serial.println("'");

  WiFi.mode(WIFI_STA);
  WiFi.begin(g_data.ssid, g_data.password);

  int i = 0;
  while (WiFi.status() != WL_CONNECTED && i < 31)
  {
    delay(500);
    Serial.print(".");
    ++i;
  }
  if (WiFi.status() == WL_CONNECTED)
  {
      Serial.println("");
      Serial.print("Connected to  : '");
      Serial.print(g_data.ssid);
      Serial.println("'");
      Serial.print("IP address    : ");
      Serial.println(WiFi.localIP());
  }
}

// -----------------------------------------------------------------------------------------
// handle_api
// API commands you can send to the Webserver in format: action, value, api. 
// (because of security reasons the 'api' is mandatory)
//
// Example: http://ip-address/api?action=set_host&value=my_host
//
// Currently next API actions are supported:
//
// ESP8266 Functions:
// reboot, value              // Reboot in case 'value' = "true"
// reset, value               // Erase EEPROM in case 'value' == "true"
//
// Miscellaneous Functions:
// set_api, value             // Set new API key to 'value'
// set_host, value            // Set hostname to 'value'
// set_language, value        // Set Web-Interface Language (0=English, 1=Dutch)
//
// -----------------------------------------------------------------------------------------
void handle_api()
{
  // Get variables for all commands (action,value,api)
  String action = server.arg("action");
  String value = server.arg("value");
  String api = server.arg("api");

  char buffer[256];
  sprintf( buffer, "\nWebserver API called with parameters (action='%s', value='%s', api='%s')\n\n", action.c_str(), value.c_str(), api.c_str() );

  Serial.print( buffer );

  // First check if user wants to set new API key (only possible when not already set)
  if ( (action == "set_api") && (g_data.apikey_set == 0) && (strcmp(api.c_str(), g_default_api_key)) )
  {
    Serial.println("Handle 'set_api' action");

    // Flash internal LED
    flash_internal_led();

    // Make sure that API key is not bigger as 64 characters (= LENGTH_API_KEY)
    if ( strlen(api.c_str()) > LENGTH_API_KEY )
    {
      char buffer[256];
      sprintf( buffer, "NOK (API key '%s' is bigger then '%d' characters)", api.c_str(), LENGTH_API_KEY );
      
      server.send ( 501, "text/html", buffer);
      delay(200);
    }
    else
    {
      Serial.print("Setting API key to value: '");
      Serial.print(value);
      Serial.println("'");
  
      strcpy( g_data.apikey, value.c_str() );
      g_data.apikey_set = 1;
  
      // replace values in EEPROM
      EEPROM.put( g_start_eeprom_address, g_data);
      EEPROM.commit();
  
      char buffer[256];
      sprintf( buffer, "OK (API key set to value '%s')", g_data.apikey );
      
      server.send ( 200, "text/html", buffer);
      delay(200);
    }
  }

  // API key valid?
  if (strcmp (api.c_str(), g_data.apikey) != 0 )
  {
    char buffer[256];
    sprintf( buffer, "NOK (you are not authorized to perform an action without a valid API key, provided api-key '%s')", api.c_str() );
    
    server.send ( 501, "text/html", buffer);
    delay(200);
  }
  else
  {
    // Action: reboot
    if (action == "reboot")
    {
      Serial.println("Handle 'reboot' action");

      server.send ( 200, "text/html", "OK");
      delay(500);

      // Flash internal LED
      flash_internal_led();

      if ( strcmp(value.c_str(), "true") == 0 )
      {
        Serial.println("Rebooting ESP8266");
        ESP.restart();
      }
    }
    // Action: reset
    else if (action == "reset")
    {
      Serial.println("Handle 'reset' action");

      server.send ( 200, "text/html", "OK");
      delay(200);

      if ( strcmp(value.c_str(), "true") == 0 )
      {
        // Flash internal LED
        flash_internal_led();
        
        Serial.println("Clearing EEPROM values of ESP8266");

        // Fill used EEPROM block with 00 bytes
        for (unsigned int i = g_start_eeprom_address; i < (g_start_eeprom_address + ALLOCATED_EEPROM_BLOCK); i++)
        {
          EEPROM.put(i, 0);
        }    
        // Commit changes to EEPROM
        EEPROM.commit();
  
        ESP.restart();
      }
    }
    // Action: set_host
    else if (action == "set_host")
    {
      Serial.println("Handle 'set_host' action");

      // Make sure that value is supplied for hostname
      if ( value == "" )
      {
        Serial.println("No hostname supplied for 'set_host' action");
        
        server.send ( 501, "text/html", "NOK (no hostname supplied for 'set_host' action)");
        delay(200);
      }
      else
      {
        // Flash internal LED
        flash_internal_led();
        
        // Set new Hostname
        strcpy( g_data.hostname, value.c_str() );
  
        // Replace values in EEPROM
        EEPROM.put( g_start_eeprom_address, g_data);
        EEPROM.commit();
  
        Serial.print("Setting hostname to: '");
        Serial.print(value);
        Serial.println("'");
  
        // Set Hostname
        WiFi.hostname(value);         

        char buffer[256];
        sprintf( buffer, "OK (hostname set to value '%s')", value.c_str() );
        
        server.send ( 200, "text/html", buffer);
        delay(200);
      }
    }
    // Action: set_language
    else if (action == "set_language")
    {
      Serial.println("Handle 'set_language' action");

      // Make sure that value is supplied for hostname
      if (value == "")
      {
        Serial.println("No language supplied for 'set_language' action, falling back to English");

        g_data.language = 0;
        
        // Replace values in EEPROM
        EEPROM.put( g_start_eeprom_address, g_data);
        EEPROM.commit();
       
        server.send ( 501, "text/html", "NOK (no language supplied for 'set_language' action, falling back to English");
        delay(200);
      }
      else if ( (value.toInt() < 0) || (value.toInt() > 1) )
      {
        Serial.println("Unknown language supplied for 'set_language' action, falling back to English");

        g_data.language = 0;
        
        // Replace values in EEPROM
        EEPROM.put( g_start_eeprom_address, g_data);
        EEPROM.commit();
       
        server.send ( 501, "text/html", "NOK (unknown language supplied for 'set_language' action, falling back to English");
        delay(200);
      }
      else
      {
        // Flash internal LED
        flash_internal_led();

        g_data.language = value.toInt();
        
        // Replace values in EEPROM
        EEPROM.put( g_start_eeprom_address, g_data);
        EEPROM.commit();

        char buffer [256];
        sprintf( buffer, "OK (language set to value '%d')", g_data.language );

        server.send ( 200, "text/html", buffer);
        delay(200);
      }
    }   
    // Unknown action supplied :-(
    else
    {
      Serial.println("Unknown action");
      server.send ( 501, "text/html", "NOK (unknown action supplied)");      
    }
  }
}

// -----------------------------------------------------------------------------------------
// flash_internal_led()
// Flash internal LED of ESP8266 board
// -----------------------------------------------------------------------------------------
void flash_internal_led()
{
  // Set internal LED on
  digitalWrite(BUILTIN_LED1, LOW);
  delay(10);

  // Set internal LED off 
  digitalWrite(BUILTIN_LED1, HIGH);
  delay(10);
}

// -----------------------------------------------------------------------------------------
// handle_erase
// Erase the EEPROM contents of the ESP8266 board
// -----------------------------------------------------------------------------------------
void handle_erase()
{
  server.send ( 200, "text/html", "OK");
  Serial.println("Erase complete EEPROM");

  for (unsigned int i = 0 ; i < ALLOCATED_EEPROM_BLOCK; i++) 
  {
    EEPROM.write(i, 0);
  }  
  // Commit changes to EEPROM
  EEPROM.commit();

  // Restart board
  ESP.restart();
}

// -----------------------------------------------------------------------------------------
// is_authenticated(int set_cookie)
// Helper-method for authenticating for Web Access
// -----------------------------------------------------------------------------------------
bool is_authenticated(int set_cookie)
{
  MD5Builder md5;

  // Create unique MD5 value (hash) of Web Password
  md5.begin();
  md5.add(g_data.web_password);
  md5.calculate();

  String web_pass_md5 = md5.toString();
  String valid_cookie = "eTemperature=" + web_pass_md5;

  // Check if web-page has Cookie data
  if (server.hasHeader("Cookie"))
  {
    // Check if Cookie of web-session corresponds with our Cookie of eTemperature
    String cookie = server.header("Cookie");

    if (cookie.indexOf(valid_cookie) != -1)
    {
      // No, then set our Cookie for this Web session
      if (set_cookie == 1)
      {
        server.sendHeader("Set-Cookie","eTemperature=" + web_pass_md5 + "; max-age=86400");
      }
      return true;
    }
  }
  return false;
}

// -----------------------------------------------------------------------------------------
// handle_login_html()
// -----------------------------------------------------------------------------------------
void handle_login_html()
{
  String login_html_page;

  login_html_page  = "<title>" + l_login_screen[g_data.language] + "</title>";
  login_html_page += "<style> body { background-color: #fffff; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }</style>";

  login_html_page += "</head><body><h1>" + l_login_screen[g_data.language] + "</h1>";

  login_html_page += "<html><body>";
  login_html_page += "<form action='/login_ajax' method='POST'>" + l_provide_credentials[g_data.language];
  login_html_page += "<br>";
  login_html_page += "<input type='hidden' name='form' value='login'>";
  login_html_page += "<pre>";

  char length_web_user[10];
  sprintf( length_web_user, "%d", LENGTH_WEB_USER );
  String str_length_web_user = length_web_user;

  char length_web_password[10];
  sprintf( length_web_password, "%d", LENGTH_WEB_PASSWORD );
  String str_length_web_password = length_web_password;
 
  login_html_page += l_user_label[g_data.language] + "<input type='text' name='user' placeholder='" + l_username[g_data.language] + "' maxlength='" + str_length_web_user + "'>";
  login_html_page += "<br>";
  login_html_page += l_password_label[g_data.language] + "<input type='password' name='password' placeholder='" + l_password[g_data.language] + "' maxlength='" + str_length_web_password + "'>";
  login_html_page += "</pre>";
  login_html_page += "<input type='submit' name='Submit' value='Submit'></form>";
  login_html_page += "<br><br>";
  login_html_page += "</body></html>";
  
  server.send(200, "text/html", login_html_page);
}

// -----------------------------------------------------------------------------------------
// handle_login_ajax()
// -----------------------------------------------------------------------------------------
void handle_login_ajax()
{
  String form   = server.arg("form");
  String action = server.arg("action");
  
  if (form == "login")
  {
    String user_arg = server.arg("user");
    String pass_arg = server.arg("password");

    // Calculate MD5 (hash) of set web-password in EEPROM
    MD5Builder md5;   
    md5.begin();
    md5.add(g_data.web_password);
    md5.calculate();
    String web_pass_md5 = md5.toString();

    // Check if Credentials are OK and if yes set the Cookie for this session
    if (user_arg == g_data.web_user && pass_arg == g_data.web_password)
    {
      String header = "HTTP/1.1 301 OK\r\nSet-Cookie: eTemperature=" + web_pass_md5 + "; max-age=86400\r\nLocation: /\r\nCache-Control: no-cache\r\n\r\n";
      server.sendContent(header);
    }
    else
    {
      Serial.println("Wrong user-name and/or password supplied");

      // Reset Cookie and present login screen again
      String header = "HTTP/1.1 301 OK\r\nSet-Cookie: eTemperature=0\r\nLocation: /login\r\nCache-Control: no-cache\r\n\r\n";
      server.sendContent(header);
    }
  }
  else
  {
    server.send ( 200, "text/html", "Nothing");
  }
}

// -----------------------------------------------------------------------------------------
// handle_settings_html()
// -----------------------------------------------------------------------------------------
void handle_settings_html()
{ 
  // Check if you are authorized to see the Settings dialog
  if ( !is_authenticated(0) )
  {
    Serial.println("Unauthorized to change Settings. Reset cookie and present login screen");

    // Reset Cookie and present login screen
    String header = "HTTP/1.1 301 OK\r\nSet-Cookie: eTemperature=0\r\nLocation: /login\r\nCache-Control: no-cache\r\n\r\n";
    server.sendContent(header);
    
    server.send ( 200, "text/html", "Unauthorized");
  }
  else
  {   
    String settings_html_page;
  
    settings_html_page  = "<title>" + l_settings_screen[g_data.language] + "</title>";
    settings_html_page += "<style> body { background-color: #fffff; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }</style>";
  
    settings_html_page += "</head><body><h1>" + l_settings_screen[g_data.language] + "</h1>";
  
    settings_html_page += "<html><body>";
    settings_html_page += "<form action='/settings_ajax' method='POST'>";
    settings_html_page += l_provide_new_credentials[g_data.language];
    settings_html_page += "<br>";
    settings_html_page += "<input type='hidden' name='form' value='settings'>";
    settings_html_page += "<pre>";

    char length_web_user[10];
    sprintf( length_web_user, "%d", LENGTH_WEB_USER );
    String str_length_web_user = length_web_user;

    char length_web_password[10];
    sprintf( length_web_password, "%d", LENGTH_WEB_PASSWORD );
    String str_length_web_password = length_web_password;

    char length_api_key[10];
    sprintf( length_api_key, "%d", LENGTH_API_KEY );
    String str_length_api_key = length_api_key;
    
    settings_html_page += l_new_user_label[g_data.language] + "<input type='text' name='user' value='";
    settings_html_page += g_data.web_user;
    settings_html_page += "' maxlength='" + str_length_web_user + "'>";
    settings_html_page += "<br>";
    settings_html_page += l_new_password_label[g_data.language] + "<input type='text' name='password' value='";
    settings_html_page += g_data.web_password;
    settings_html_page += "' maxlength='" + str_length_web_password + "'>";
    settings_html_page += "<br>";
    settings_html_page += l_new_api_key[g_data.language] + "<input type='text' name='apikey' value='";
    settings_html_page += g_data.apikey;
    settings_html_page += "' maxlength='" + str_length_api_key + "'>";
    settings_html_page += "</pre>";
    settings_html_page += "<br>";

    settings_html_page += l_change_language_hostname[g_data.language];
    settings_html_page += "<br>";   
    settings_html_page += "<pre>";
    settings_html_page += l_language_label[g_data.language] + "<select name='language'>";

    // Default English
    if (g_data.language == 0)
    {
      settings_html_page += "<option selected>";
      settings_html_page += l_language_english[g_data.language];
      settings_html_page += "</option><option>";
      settings_html_page += l_language_dutch[g_data.language];
      settings_html_page += "</option></select>";
    }
    // Otherwise Dutch
    else
    {
      settings_html_page += "<option>";
      settings_html_page += l_language_english[g_data.language];
      settings_html_page += "</option><option selected>";
      settings_html_page += l_language_dutch[g_data.language];
      settings_html_page += "</option></select>";
    }

    char length_hostname[10];
    sprintf( length_hostname, "%d", LENGTH_HOSTNAME );
    String str_length_hostname = length_hostname;

    settings_html_page += "<br>";
    settings_html_page += l_hostname_label[g_data.language] + "<input type='text' name='hostname' value='";
    settings_html_page += g_data.hostname;
    settings_html_page += "' maxlength='" + str_length_hostname + "'>"; 
    settings_html_page += "</pre>";

    settings_html_page += "<br>";

    settings_html_page += l_change_domoticz_settings[g_data.language];
    settings_html_page += "<br>";   
    settings_html_page += "<pre>";

    char length_domoticz_server[10];
    sprintf( length_domoticz_server, "%d", LENGTH_DOMOTICZ_SERVER );
    String str_length_domoticz_server = length_domoticz_server;

    settings_html_page += l_domoticz_server_label[g_data.language] + "<input type='text' name='domoticz_server' value='";
    settings_html_page += g_data.domoticz_server;
    settings_html_page += "' maxlength='" + str_length_domoticz_server + "'>"; 
    settings_html_page += "<br>";
    settings_html_page += l_domoticz_port_label[g_data.language] + "<input type='text' name='domoticz_port' value='";
    settings_html_page += g_data.domoticz_port;
    settings_html_page += "'>"; 
    settings_html_page += "<br>";

//    char length_fingerprint[10];
//    sprintf( length_fingerprint, "%d", LENGTH_FINGERPRINT );
//    String str_length_fingerprint = length_fingerprint;
//
//    settings_html_page += l_fingerprint_label[g_data.language] + "<input type='text' name='fingerprint' style='width:500px;' value='";
//    settings_html_page += g_data.fingerprint;
//    settings_html_page += "' maxlength='" + str_length_fingerprint + "'>"; 
//    settings_html_page += "<br>";

    settings_html_page += l_idx_virtual_sensor_label[g_data.language] + "<input type='text' name='idx_virtual_sensor' value='";
    settings_html_page += g_data.idx_virtual_sensor;
    settings_html_page += "'>"; 
    settings_html_page += "<br>";
    settings_html_page += l_update_frequency[g_data.language] + "<input type='text' name='update_frequency' value='";
    settings_html_page += g_data.update_frequency;
    settings_html_page += "'>"; 
    settings_html_page += "</pre>";   

    settings_html_page += "<br>";

    settings_html_page += l_change_hardware_settings[g_data.language];
    settings_html_page += "<br>";   
    settings_html_page += "<pre>";

    settings_html_page += l_sensor_type_label[g_data.language] + "<select name='sensor_type'>";

    // DHT22 sensor
    if (g_data.sensor_type == 0)
    {
      settings_html_page += "<option selected>";
      settings_html_page += l_sensor_type_dht22[g_data.language];
      settings_html_page += "</option><option>";
      settings_html_page += l_sensor_type_bme280[g_data.language];
      settings_html_page += "</option></select>";
      
    }
    // BME280 sensor
    else
    {
      settings_html_page += "<option>";
      settings_html_page += l_sensor_type_dht22[g_data.language];
      settings_html_page += "</option><option selected>";
      settings_html_page += l_sensor_type_bme280[g_data.language];
      settings_html_page += "</option></select>";
    }
    settings_html_page += "<br>";
    settings_html_page += "<br>";

    // DHT22 sensor configured
    if (g_data.sensor_type == 0)
    {
      settings_html_page += l_pin_dht22_label[g_data.language] + "<input type='text' name='dht22_data' value='";
      settings_html_page += g_data.dht22_data;
      settings_html_page += "'>"; 
      settings_html_page += "<br>";
      settings_html_page += "<br>";
    }

    settings_html_page += l_display_type_label[g_data.language] + "<select name='display_type'>";

    // Max7219 Display?
    if (g_data.display_type == 0)
    {
      settings_html_page += "<option selected>";
      settings_html_page += l_display_type_max7219[g_data.language];
      settings_html_page += "</option>";
      settings_html_page += "<option>";
      settings_html_page += l_display_type_oled[g_data.language];
      settings_html_page += "</option>";
      settings_html_page += "<option>";
      settings_html_page += l_display_type_none[g_data.language];
      settings_html_page += "</option>";     
      settings_html_page += "</select>";
    }
    // Oled Display?
    else if (g_data.display_type == 1)
    {
      settings_html_page += "<option>";
      settings_html_page += l_display_type_max7219[g_data.language];
      settings_html_page += "</option>";
      settings_html_page += "<option selected>";
      settings_html_page += l_display_type_oled[g_data.language];
      settings_html_page += "</option>";
      settings_html_page += "<option>";
      settings_html_page += l_display_type_none[g_data.language];
      settings_html_page += "</option>";
      settings_html_page += "</select>";
    }
    // Unknown display
    else 
    {
      settings_html_page += "<option>";
      settings_html_page += l_display_type_max7219[g_data.language];
      settings_html_page += "</option>";
      settings_html_page += "<option>";
      settings_html_page += l_display_type_oled[g_data.language];
      settings_html_page += "</option>";
      settings_html_page += "<option selected>";
      settings_html_page += l_display_type_none[g_data.language];
      settings_html_page += "</option>";
      settings_html_page += "</select>";
    }
    settings_html_page += "<br>";
    settings_html_page += "<br>";

    // Max7219 Display configured
    if (g_data.display_type == 0)
    {
      settings_html_page += l_max7219_pin_data_label[g_data.language] + "<input type='text' name='max7219_data' value='";
      settings_html_page += g_data.max7219_data;
      settings_html_page += "'>"; 
      settings_html_page += "<br>";
      settings_html_page += l_max7219_pin_clk_label[g_data.language] + "<input type='text' name='max7219_clk' value='";
      settings_html_page += g_data.max7219_clk;
      settings_html_page += "'>"; 
      settings_html_page += "<br>";
      settings_html_page += l_max7219_pin_cs_label[g_data.language] + "<input type='text' name='max7219_cs' value='";
      settings_html_page += g_data.max7219_cs;
      settings_html_page += "'>"; 
      settings_html_page += "<br>";
      settings_html_page += l_max7219_brightness_label[g_data.language] + "<input type='text' name='max7219_brightness' value='";
      settings_html_page += g_data.max7219_brightness;
      settings_html_page += "'>"; 
      settings_html_page += "<br>";
      settings_html_page += "<br>";
    }
    // Oled Display configured
    else if (g_data.display_type == 1)
    {
      settings_html_page += l_oled_address_label[g_data.language] + "<input type='text' name='oled_address' value='";
      settings_html_page += g_data.oled_address;
      settings_html_page += "'>"; 
      settings_html_page += l_decimal_label[g_data.language];
      settings_html_page += "<br>";     
//      settings_html_page += l_oled_pin_sda_label[g_data.language] + "<input type='text' name='oled_sda' value='";
//      settings_html_page += g_data.oled_sda;
//      settings_html_page += "'>"; 
//      settings_html_page += "<br>";
//      settings_html_page += l_oled_pin_scl_label[g_data.language] + "<input type='text' name='oled_scl' value='";
//      settings_html_page += g_data.oled_scl;
//      settings_html_page += "'>"; 
//      settings_html_page += "<br>";
      settings_html_page += l_oled_brightness_label[g_data.language] + "<input type='text' name='oled_brightness' value='";
      settings_html_page += g_data.oled_brightness;
      settings_html_page += "'>"; 
      settings_html_page += "<br>";
    }
//    else
    {
      ;      
    }

    settings_html_page += "</pre>";   
    settings_html_page += "<br>";

    settings_html_page += l_change_calibration_settings[g_data.language];
    settings_html_page += "<br>";   
    settings_html_page += "<pre>";

    settings_html_page += l_calibration_temperature_label[g_data.language] + "<input type='text' name='calibration_temperature' value='";
    settings_html_page += g_data.calibration_temperature;
    settings_html_page += "'>"; 
    settings_html_page += "<br>";
    settings_html_page += l_calibration_humidity_label[g_data.language] + "<input type='text' name='calibration_humidity' value='";
    settings_html_page += g_data.calibration_humidity;
    settings_html_page += "'>"; 
    settings_html_page += "<br>";
    settings_html_page += l_calibration_pressure_label[g_data.language] + "<input type='text' name='calibration_pressure' value='";
    settings_html_page += g_data.calibration_pressure;
    settings_html_page += "'>"; 
    settings_html_page += "<br>";
    settings_html_page += "</pre>";

    settings_html_page += l_login_again[g_data.language];
    settings_html_page += "<br><br>";
     
    settings_html_page += "<input type='submit' name='Submit' value='Submit'></form>";
    settings_html_page += "<br><br>";
    settings_html_page += "</body></html>";
    
    server.send(200, "text/html", settings_html_page);
  }
}
  
// -----------------------------------------------------------------------------------------
// handle_settings_ajax()
// -----------------------------------------------------------------------------------------
void handle_settings_ajax()
{
  String form   = server.arg("form");
  String action = server.arg("action");
  
  if (form == "settings")
  {
    String user_arg           = server.arg("user");
    String pass_arg           = server.arg("password");
    String api_arg            = server.arg("apikey");
    
    String host_arg           = server.arg("hostname");
    String lang_arg           = server.arg("language");

    String serv_arg           = server.arg("domoticz_server");
    String port_arg           = server.arg("domoticz_port");
    String finger_arg         = server.arg("fingerprint");
    
    String idx_arg            = server.arg("idx_virtual_sensor");
    String upd_fr_arg         = server.arg("update_frequency");

    String c_temp_arg         = server.arg("calibration_temperature");
    String c_hum_arg          = server.arg("calibration_humidity");
    String c_pres_arg         = server.arg("calibration_pressure");

    String sensor_type_arg    = server.arg("sensor_type");

    String dht22_data_arg     = server.arg("dht22_data");

    String display_type_arg   = server.arg("display_type");

    String max7219_data_arg   = server.arg("max7219_data");
    String max7219_clk_arg    = server.arg("max7219_clk");
    String max7219_cs_arg     = server.arg("max7219_cs");
    String max7219_bright_arg = server.arg("max7219_brightness");
    
    String oled_address_arg   = server.arg("oled_address");
    String oled_sda_arg       = server.arg("oled_sda");
    String oled_scl_arg       = server.arg("oled_scl");
    String oled_bright_arg    = server.arg("oled_brightness");
    
    // Check if new web-user of web-password has been set
    int login_again = 0;
    if ( (strcmp (g_data.web_user, user_arg.c_str()) != 0) || (strcmp(g_data.web_password, pass_arg.c_str()) != 0) )
    {
      login_again = 1;
    }
    
    // Set new Web User and Web Password
    strcpy( g_data.web_user,     user_arg.c_str() );
    strcpy( g_data.web_password, pass_arg.c_str() );
    strcpy( g_data.hostname,     host_arg.c_str() );

    // Set new API key
    strcpy( g_data.apikey,       api_arg.c_str() );
    
    // Set new language
    if ( strcmp( lang_arg.c_str(), l_language_english[g_data.language].c_str()) == 0 )
    {
      g_data.language = 0;
    }
    else
    {
      g_data.language = 1;
    }

    // Set Hostname
    WiFi.hostname(g_data.hostname);         
    // Set Domoticz settings
    strcpy( g_data.domoticz_server,  serv_arg.c_str()   );
    g_data.domoticz_port           = port_arg.toInt();
//    strcpy( g_data.fingerprint,      finger_arg.c_str() );

    g_data.idx_virtual_sensor      = idx_arg.toInt();
    g_data.update_frequency        = upd_fr_arg.toInt();

    // Set new sensor type
    if ( strcmp( sensor_type_arg.c_str(), l_sensor_type_dht22[g_data.language].c_str()) == 0 )
    {
      g_data.sensor_type = 0;
    }
    else
    {
      g_data.sensor_type = 1;
    }

    g_data.dht22_data              = dht22_data_arg.toInt();

    // Set new display type
    if ( strcmp( display_type_arg.c_str(), l_display_type_max7219[g_data.language].c_str()) == 0 )
    {
      g_data.display_type = 0;
    }
    else if ( strcmp( display_type_arg.c_str(), l_display_type_oled[g_data.language].c_str()) == 0 )
    {
      g_data.display_type = 1;
    }
    else
    {
      g_data.display_type = 2;
    }

    g_data.max7219_data       = max7219_data_arg.toInt();
    g_data.max7219_clk        = max7219_clk_arg.toInt();
    g_data.max7219_cs         = max7219_cs_arg.toInt();
    g_data.max7219_brightness = max7219_bright_arg.toInt();

    g_data.oled_address       = oled_address_arg.toInt();
//    g_data.oled_sda           = oled_sda_arg.toInt();
//    g_data.oled_scl           = oled_scl_arg.toInt();
    g_data.oled_brightness    = oled_bright_arg.toInt();

    g_data.calibration_temperature = c_temp_arg.toFloat();
    g_data.calibration_humidity    = c_hum_arg.toFloat();
    g_data.calibration_pressure    = c_pres_arg.toFloat();

    // Initialize devices again

    // DHT22 configured
    if (g_data.sensor_type == 0)
    {
      // Initialize DHT libarary
      Serial.println("Initialize DHT22 sensor");
      
      g_dht.setup(g_data.dht22_data, DHTesp::DHT22);      // Connect DHT sensor configured GPIO (default GPIO4)
    }
    // BME280 configured
    else
    {
      Serial.println("Initialize BME280 sensor");

      // Check if you can find the BME280 sensor
      if (g_bme.begin())   
      {
        // BME280 found, now find exact type
        switch(g_bme.chipModel())
        {
          case BME280::ChipModel_BME280:
            Serial.println("Found BME280 sensor");
            break;
          case BME280::ChipModel_BMP280:
            Serial.println("Found BMP280 sensor, but no Humidity available");
            break;
          default:
            Serial.println("Found UNKNOWN sensor");
        }
      }
      else
      {
        Serial.println("Could not find BME280 sensor");
      }            
    }

    // Max7219 configured
    if (g_data.display_type == 0)
    {
      Serial.println("Initialize Max7219 Display");
      
      // Initialize the Max8217 Display: Ledcontrol (data, clk, cs, number_of_leds)
      g_lc = LedControl(g_data.max7219_data, g_data.max7219_clk, g_data.max7219_cs, 2);
   
      // Wake-up
      g_lc.shutdown(0, false);                          // Wake-up LED 0
      g_lc.shutdown(1, false);                          // Wake-up LED 1
  
      // Set brightness
      g_lc.setIntensity(0, g_data.max7219_brightness);  // LED Brightness to Medium
      g_lc.setIntensity(1, g_data.max7219_brightness);  // LED Brightness to Medium
  
      // Clear display
      g_lc.clearDisplay(0);                             // Clear LED 0 Display
      g_lc.clearDisplay(1);                             // Clear LED 1 Display
    
      // Display 'OK' on LED
      led_display_character('O', 1);
      led_display_character('K', 0);
    }
    // Oled Display configured
    else if (g_data.display_type == 1)
    {
      Serial.println("Initialize Oled Display");

      // Default: Address is 0x3c, SDA on GPIO4 (=D2) and SCL on GPIO5 (=D1)
      g_ssd1306 = SSD1306Wire(g_data.oled_address, g_data.oled_sda, g_data.oled_scl);
      g_ssd1306.init();

      // Set display on
      g_ssd1306.displayOn();
  
      // Set brightness
      g_ssd1306.setBrightness(g_data.oled_brightness);
  
      // Clear display
      g_ssd1306.clear();

      // Set Font
      g_ssd1306.setFont( ArialMT_Plain_24 );
    }
    else
    {
      ;
    }
   
    // Update values in EEPROM
    EEPROM.put( g_start_eeprom_address, g_data);
    EEPROM.commit();

    char buffer[256];
    sprintf(buffer,"New webuser-name='%s', new webuser-password='%s'", g_data.web_user, g_data.web_password);
    Serial.println(buffer);

    // Force login again in case web-user or web-password is changed
    if (login_again == 1)
    {
      // Reset Cookie and present login screen again
      String header = "HTTP/1.1 301 OK\r\nSet-Cookie: eTemperature=0\r\nLocation: /login\r\nCache-Control: no-cache\r\n\r\n";
      server.sendContent(header);
    }
    // Otherwise present main Web-Interface again
    else
    {
      String header = "HTTP/1.1 301 OK\r\nLocation: /\r\nCache-Control: no-cache\r\n\r\n";
      server.sendContent(header);      
    }
  }
}

// -----------------------------------------------------------------------------------------
// get_page
// Construct eTemperature Web-Interface
// -----------------------------------------------------------------------------------------
String get_page()
{ 
  String  page;
  page  = "<html lang=nl-NL><head><meta http-equiv='refresh' content='5'/>";  

  page += "<title>eTemperature</title>";
  page += "<style> body { background-color: #fffff; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }</style>";
  page += "</head>";
  
  page += "<body><h1>eTemperature Web-Interface</h1>";

  page += l_version[g_data.language] + " '";
  page += g_data.version;
  page += "', ";
  
  page += l_hostname[g_data.language] + "'";
  page += g_data.hostname;
  page += "'";
  page += "<br>";
  page += "<br>";

  page += "<form action='/' method='POST'>";
  page += "<input type='submit' name='logout' value='" + l_btn_logout[g_data.language] + "'>";
  page += "<input type='submit' name='settings' value='" + l_btn_settings[g_data.language] + "'>";

  page += "<br>";
  page += "<br>";

  page += l_update_frequency_message[g_data.language] + "'";
  page += g_data.update_frequency;
  page += "' " + l_seconds[g_data.language];
  page += "<br>";
  page += "<br>";
  
  page += "<hr>";
  page += "<h1><pre>";
  page += l_temperature[g_data.language];
  page += g_temperature;
  page += String("&#176;C");

  // If BMP280 then no humidity is measured
  if ((g_data.sensor_type == 1)&&(g_bme.chipModel() == BME280::ChipModel_BMP280))
  {
    ;
  }
  else
  {
    page += "<br>";
    page += l_humidity[g_data.language];
    page += g_humidity;
    page += "%";
  }
  // In case you have a BME280 sensor then also display pressure
  if (g_data.sensor_type == 1)
  {
    page += "<br>";
    page += l_pressure[g_data.language];
    page += g_pressure;
    page += " hPa";    
  }
  page += "</pre></h1>";
  page += "<hr>";
  
  page += "</body></html>";

  return (page);
}

// -----------------------------------------------------------------------------------------
// handle_root_ajax
// Handle user-interface actions executed on the Web-Interface
// -----------------------------------------------------------------------------------------
void handle_root_ajax()
{
  // Check if you are authorized to see the web-interface
  if ( !is_authenticated(0) )
  {
    Serial.println("Unauthorized to access Web-Interface. Reset Cookie andp present login screen.");

    // Reset Cookie and present login screen
    String header = "HTTP/1.1 301 OK\r\nSet-Cookie: eTemperature=0\r\nLocation: /login\r\nCache-Control: no-cache\r\n\r\n";
    server.sendContent(header);
    
    server.send ( 200, "text/html", "Unauthorized");
  }
  else
  {
    // Logout
    if (server.hasArg("logout") )
    {
      handle_logout();
    }
    // Change Credentials
    else if (server.hasArg("settings") )
    {
      handle_settings();
    }
    else
    {
      server.send ( 200, "text/html", get_page() ); 
    }
  }
}

// -----------------------------------------------------------------------------------------
// handle_upgradefw_html
// Handle upgrading of firmware of ESP8266 device
// -----------------------------------------------------------------------------------------
void handle_upgradefw_html()
{
  // Check if you are authorized to upgrade the firmware
  if ( !is_authenticated(0) )
  {
    Serial.println("Unauthorized to upgrade firmware");

    // Reset Cookie and present login screen
    String header = "HTTP/1.1 301 OK\r\nSet-Cookie: eTemperature=0\r\nLocation: /login\r\nCache-Control: no-cache\r\n\r\n";
    server.sendContent(header);
    
    server.send ( 200, "text/html", "Unauthorized");
  }
  else
  {
    String upgradefw_html_page;
  
    upgradefw_html_page  = "<style> body { background-color: #fffff; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }</style>";
    upgradefw_html_page += "</head><body><h1>Upgrade firmware</h1>";
    upgradefw_html_page += "<br>";
    upgradefw_html_page += "<form method='POST' action='/upgradefw2' enctype='multipart/form-data'>";
    upgradefw_html_page += "<input type='file' name='upgrade'>";
    upgradefw_html_page += "<input type='submit' value='Upgrade'>";
    upgradefw_html_page += "</form>";
    upgradefw_html_page += "<b>" + l_after_upgrade[g_data.language] + "</b>";
  
    server.send ( 200, "text/html", upgradefw_html_page);
  }
}

// -----------------------------------------------------------------------------------------
// handle_wifi_html
// Simple WiFi setup dialog
// -----------------------------------------------------------------------------------------
void handle_wifi_html()
{
  String wifi_page;

  wifi_page  = "<style> body { background-color: #fffff; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }</style>";
  wifi_page += "</head><body><h1>" + l_configure_wifi_network[g_data.language] + "</h1>";
  wifi_page += "<html><body>";
  wifi_page += "<form action='/wifi_ajax' method='POST'>" + l_enter_wifi_credentials[g_data.language];
  wifi_page += "<br>";  
  wifi_page += "<input type='hidden' name='form' value='wifi'>";  

  char length_ssid[10];
  sprintf( length_ssid, "%d", LENGTH_SSID );
  String str_length_ssid = length_ssid;

  char length_ssid_password[10];
  sprintf( length_ssid_password, "%d", LENGTH_PASSWORD );
  String str_length_ssid_password = length_ssid_password;
  
  wifi_page += "<pre>";
  wifi_page += l_network_ssid_label[g_data.language] + "<input type='text' name='ssid' placeholder='" + l_network_ssid_hint[g_data.language] + "' maxlength='" + str_length_ssid + "'>";
  wifi_page += "<br>";
  wifi_page += l_network_password_label[g_data.language] + "<input type='password' name='password' placeholder='" + l_network_password_hint[g_data.language] + "' maxlength='" + str_length_ssid_password + "'>";
  wifi_page += "</pre>";
  wifi_page += "<input type='submit' name='Submit' value='Submit'></form>";
  wifi_page += "<br><br>";
  wifi_page += "</body></html>";

  server.send ( 200, "text/html", wifi_page);
}

// -----------------------------------------------------------------------------------------
// handle_wifi_ajax
// Handle simple WiFi setup dialog configuration
// -----------------------------------------------------------------------------------------
void handle_wifi_ajax()
{
  String form = server.arg("form");

  if (form == "wifi")
  {
    String ssidArg = server.arg("ssid");
    String passArg = server.arg("password");

    strcpy( g_data.ssid, ssidArg.c_str() );
    strcpy( g_data.password, passArg.c_str() );

    // Update values in EEPROM
    EEPROM.put( g_start_eeprom_address, g_data);
    EEPROM.commit();
  
    server.send ( 200, "text/html", "OK");
    delay(500);
  
    ESP.restart();
  }
}

// -----------------------------------------------------------------------------------------
// handle_logout
// Logout from Web-Server
// -----------------------------------------------------------------------------------------
void handle_logout()
{
  Serial.println("[Logout] pressed");

  // Reset Cookie and present login screen
  String header = "HTTP/1.1 301 OK\r\nSet-Cookie: eTemperature=0\r\nLocation: /login\r\nCache-Control: no-cache\r\n\r\n";

  server.sendContent(header);
}

// -----------------------------------------------------------------------------------------
// handle_settings
// Change settings for Web-Server
// -----------------------------------------------------------------------------------------
void handle_settings()
{
  Serial.println("[Settings] pressed");
 
  // Present the 'Settings' screen
  String header = "HTTP/1.1 301 OK\r\nLocation: /settings\r\nCache-Control: no-cache\r\n\r\n";

  server.sendContent(header);
}

// -----------------------------------------------------------------------------------------
// led_display_character
// Display character on specified LED
// -----------------------------------------------------------------------------------------
void led_display_character(int character, int led)
{
  int offset = 0;
  int special_character = 0;
  
  // Digit character?
  if ( (character >= '0') && (character <= '9') )
  {
    // Determine offset in table
    offset = character - '0';

    // Display character on LED display
    for (int i = 0; i <= 7; i++)
    {
      g_lc.setRow(led, i, led_digits[offset][i]);
    }   
  }

  // Alphabet character?
  else if ( (character >= 'A') && (character <= 'Z') ) 
  {
    // Determine offset in table
    offset = character - 'A';

    // Display character on LED display
    for (int i = 0; i <= 7; i++)
    {
      g_lc.setRow(led, i, led_chars[offset][i]);
    }
    
  }
  else if ( (character >= 'a') && (character <= 'z') )
  {
    // Determine offset in table
    offset = character - 'a';

    // Display character on LED display
    for (int i = 0; i <= 7; i++)
    {
      g_lc.setRow(led, i, led_chars[offset][i]);
    }
  }

  // Special character ':'
  else if (character == ':')
  {
    offset = 0;    
    special_character = 1;
  }
  // Special character ':'
  else if (character == ':')
  {
    offset = 1;
    special_character = 1;
  }
  // Special character ':'
  else if (character == '-')
  {
    offset = 2;
    special_character = 1;
  }
  // Special character ':'
  else if (character == '.')
  {
    offset = 3;
    special_character = 1;
  }
  else if (character == '!')
  {
    offset = 4;
    special_character = 1;
  }
  // Unknown character
  else
  {
    offset = 0;
    special_character = 1;
  }

  if (special_character == 1)
  {
    // Display character on LED display
    for (int i = 0; i <= 7; i++)
    {
      g_lc.setRow(led, i, led_special_characters[offset][i]);
    }
  }
}

// -----------------------------------------------------------------------------------------
// History of 'eTemperature' software
//
// 21-Dec-2018  V1.00   Initial version
// 24-Dec-2018  V1.01   Added recommended delay before reading out DHT22
// 27-Dec-2018  V1.02   Make display type configurable (Max7219 or Oled 128x64)
// 28-Dec-2018  V1.03   Added reading of BME280 sensor
// 12-Jan-2019  V1.04   Improved responsiveness of the User-Interface
//                      Possibility to configure no display
// 26-Jan-2019  V1.05   Various fixes
// 21-Apr-2019  V1.06   Fixed bug in JSON output of temperature/humidity/pressure
// 15-Jun-2020  V1.07   Better handling of negative temperatures for Max7219 display
// -----------------------------------------------------------------------------------------
