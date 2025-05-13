#include <SPI.h>
#include <MFRC522.h>

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// --- Librerie OTA ---
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

// --- Librerie BLE ---
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>

// --- Librerie Display ---
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SdFat_Adafruit_Fork.h>

#include <Adafruit_ImageReader.h>  
#include <BufferedPrint.h>


#include <Adafruit_SPIFlash.h>  


// --- Libreria Tastierino ---
#include <Keypad.h>

// --- Libreria EEPROM ---
#include <EEPROM.h>
#define EEPROM_SIZE 1024 

// --- Configurazione WiFi ---
const char* ssid = "iPhone di Mattia";
const char* password = "Mattia2024";

// --- Configurazione OTA ---
const char* ota_hostname = "ESP32_Porta_Opere";
const char* ota_password = "Administrator01"; 

// --- Configurazione BLE (Invariata) ---
#define SERVICE_UUID        "0000abcd-0000-1000-8000-00805f9b34fb"
#define TOKEN_CHARACTERISTIC_UUID "00001234-0000-1000-8000-00805f9b34fb"

// --- Configurazione Server Validazione ---
const char* bleValidationUrl = ""; // POST
const char* rfidValidationUrl = "https://accessi.mattiamorselli.it/core/arduino/controllo_badge.php";   // GET
const char* pinValidationUrl = "https://accessi.mattiamorselli.it/core/arduino/controllo_pin.php";    // GET
const char* doorEventUrl = "https://accessi.mattiamorselli.it/core/arduino/stato_porta.php";       // GET (Notifica stato porta)
const char* credentialsUrl = "https://accessi.mattiamorselli.it/core/arduino/recupero_credenziali_offline.php"; // GET (Cache offline)
const char* readerId = "2";

#define TFT_CS    D10     // Chip Select per il display TFT
#define TFT_DC    D9      // Data/Command per il display TFT
#define TFT_RST   D8      // Reset per il display TFT

// SPI Bus (condiviso per il display TFT):
#define TFT_SCK   D13     // SCK (SPI Clock)
#define TFT_MOSI  D11     // MOSI (SPI Data)

#define RFID_SS   D3
#define RFID_RST  D2   

MFRC522 mfrc522(RFID_SS, RFID_RST);  

#define KEYPAD_R1   A1
#define KEYPAD_R2   A6
#define KEYPAD_R3   A5
#define KEYPAD_R4   A3
#define KEYPAD_C1   A2
#define KEYPAD_C2   A0
#define KEYPAD_C3   A4

#define BUZZER   A7


#define LOCK_PIN    D6
#define PIN_BUTTON  D5

// LED integrato
#define LED_PIN     LED_BUILTIN 


#define REED_PIN    D4


const byte ROWS = 4;
const byte COLS = 3;
char keys[ROWS][COLS] = {
  {'1','2','3'}, 
  {'4','5','6'},
  {'7','8','9'},
  {'*','0','#'}
};

byte rowPins[ROWS] = {KEYPAD_R1, KEYPAD_R2, KEYPAD_R3};
byte colPins[COLS] = {KEYPAD_C1, KEYPAD_C2, KEYPAD_C3};
Keypad keypad = Keypad( makeKeymap(keys), rowPins, colPins, ROWS, COLS );

const int PIN_MAX_LENGTH = 6;
char pinBuffer[PIN_MAX_LENGTH + 1];
byte pinBufferIndex = 0;
const char ENTER_KEY = '#';
const char CLEAR_KEY = '*';


Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);


BLEServer* pServer = NULL;
BLECharacteristic* pTokenCharacteristic = NULL;
bool deviceConnected = false;
String receivedToken = "";
uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 }; 
uint8_t uidLength;
int lastReedState = -1; 

const int SOGLIA_APERTA = 200;
const int SOGLIA_CHIUSA = 4095;

bool isWifiConnected = false; 
bool mfrc522Active = false; 
unsigned long lastKeypadTime = 0;
unsigned long lastRfidReadTime = 0;
unsigned long lastMfrc522CheckTime = 0;
unsigned long lastCacheUpdateTime = 0;
unsigned long lastDoorNotifyTime = 0;

unsigned long lastInputTime = 0;  

// --- Timing Constants ---
const long KEYPAD_TIMEOUT = 5000;
const long RFID_READ_DELAY = 500; 
const long MFRC522_CHECK_INTERVAL = 5000; 
const long CACHE_UPDATE_INTERVAL = 20 * 60 * 1000; 
const long DOOR_NOTIFY_DEBOUNCE = 1000; 

// 'logo', 160x128px
const unsigned char logo [] PROGMEM = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf9, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xe7, 0x7f, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xdb, 0xdf, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xf8, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0xe0, 0x3b, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfd, 0xe5, 0x9d, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfb, 
	0x81, 0x87, 0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0x19, 0x59, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfc, 0x7b, 0xac, 0xef, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf8, 0xf3, 0xef, 0x3b, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf1, 
	0xf3, 0xd7, 0x9d, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xe5, 0x67, 0xeb, 0xe7, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xc7, 0xe7, 0xfb, 0xf3, 0xdf, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf7, 0x8f, 0xe7, 0xf5, 0xf8, 0xf7, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x07, 
	0xcf, 0xfc, 0xf2, 0x1b, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xfc, 0x63, 0xcf, 0xff, 0x63, 0x9e, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf1, 0xf9, 0xdf, 0xfd, 0x67, 0xdf, 0x3f, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfd, 0xe1, 0xf4, 0x9f, 0xfd, 0x4f, 0x9f, 
	0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xcf, 0xfe, 
	0x9f, 0xfe, 0x9f, 0xd8, 0x37, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xcf, 0xfd, 0x3f, 0xff, 0x18, 0x18, 0x1f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xef, 0xc1, 0xfc, 0x00, 0x00, 0x00, 0x1b, 0xc7, 0x7f, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xe7, 0x80, 0x3f, 0xff, 0xff, 0xfe, 0x1b, 
	0xc1, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfd, 0xef, 0xff, 
	0xff, 0xff, 0xff, 0xc3, 0xf0, 0x6f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xfd, 0xcf, 0xff, 0xf8, 0x7f, 0xff, 0xe0, 0x1e, 0x1f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xfd, 0xcf, 0xff, 0xf0, 0x3f, 0xff, 0xf2, 0x5f, 0x06, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfd, 0x8f, 0xff, 0xe7, 0x9f, 0xff, 0xfb, 
	0x01, 0xc3, 0xbf, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0x0f, 0xff, 
	0xef, 0xdf, 0xff, 0xfb, 0xc0, 0xf0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0x7f, 0xff, 0xef, 0xdf, 0xff, 0xfb, 0x60, 0x9d, 0x37, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f, 0xff, 0xee, 0xdf, 0xff, 0xfb, 0xee, 0x1f, 0x0f, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f, 0xff, 0xec, 0xdf, 0xff, 0xff, 
	0xce, 0x90, 0xc2, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f, 0xff, 
	0xe8, 0x1f, 0xff, 0xfb, 0x8e, 0xc0, 0xf0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xec, 0xdf, 0xff, 0xfb, 0xce, 0xe1, 0x78, 0x6f, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f, 0xff, 0xee, 0xdf, 0xff, 0xfb, 0xce, 0xa4, 0x3e, 0x0b, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f, 0xff, 0xee, 0xdf, 0xff, 0xfb, 
	0xce, 0xe6, 0x15, 0x86, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f, 0xff, 
	0xec, 0xdf, 0xff, 0xff, 0x8e, 0xe7, 0x80, 0xe3, 0xbf, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0x7f, 0xff, 0xec, 0xdf, 0xff, 0xff, 0xce, 0xe7, 0xa8, 0x18, 0x7f, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f, 0xff, 0xee, 0xff, 0xff, 0xf3, 0xce, 0xe7, 0xf8, 0x0e, 
	0x97, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f, 0xff, 0xec, 0xfe, 0x3f, 0xe3, 
	0x0e, 0xe3, 0xfa, 0x81, 0x87, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f, 0xff, 
	0xec, 0xff, 0x1f, 0xe3, 0x0e, 0xe3, 0xfa, 0xc3, 0xe3, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0x5f, 0xf2, 0xec, 0xdf, 0x1f, 0xeb, 0x4e, 0xc3, 0x7e, 0xe0, 0x98, 0x5f, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x07, 0xf0, 0xec, 0xde, 0x9f, 0xeb, 0x46, 0xf3, 0x76, 0xee, 
	0x3e, 0x0f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x07, 0xf0, 0xef, 0xde, 0x9f, 0xe7, 
	0x46, 0xf3, 0x76, 0xae, 0x81, 0x8f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x47, 0xf8, 
	0xe0, 0x9e, 0xff, 0xfb, 0x06, 0xf3, 0xd6, 0xea, 0xa1, 0xbf, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xcf, 0xf9, 0xf7, 0xff, 0xff, 0xfb, 0x86, 0xe3, 0x76, 0xea, 0xe1, 0xbf, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfb, 0xf2, 0xb3, 0x76, 0xea, 
	0x84, 0x3f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfb, 
	0xfc, 0xf3, 0x72, 0x7a, 0xa6, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0x80, 0xff, 0x33, 0xf0, 0x79, 0xa8, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0x9f, 0xfa, 0x67, 0xff, 0x7f, 0xc7, 0x72, 0x7b, 0xac, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0x1f, 0xff, 0xff, 0xf0, 0xff, 0xb7, 0xf9, 0x72, 0x7f, 
	0xee, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x3f, 0xf6, 0xff, 0xee, 0x93, 0x3f, 
	0xe3, 0xff, 0x76, 0x37, 0x4e, 0xbf, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x3b, 0xed, 
	0xfd, 0xdd, 0xf6, 0x91, 0xf1, 0x7f, 0xc7, 0x36, 0x6c, 0xbf, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0x7e, 0x87, 0xee, 0xfd, 0xbf, 0x55, 0xfc, 0xdf, 0xf8, 0x36, 0x66, 0xbf, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xdf, 0x1f, 0xdf, 0xff, 0x06, 0xbf, 0xfe, 0x6f, 0xfc, 0x37, 
	0x76, 0xbf, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xdb, 0x40, 0x77, 0xf7, 0xfd, 0xd2, 0x3e, 
	0xaf, 0x9f, 0x7f, 0x87, 0x76, 0xbf, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfc, 0x0f, 0xff, 
	0xff, 0xf3, 0xef, 0x7f, 0xcf, 0xcd, 0xff, 0xe1, 0x76, 0xbf, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xfd, 0xff, 0xdb, 0xb7, 0xf7, 0xdc, 0x0e, 0x1f, 0xff, 0xe6, 0xff, 0xfc, 0x76, 0x3f, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xf0, 0x7f, 0xff, 0xff, 0xff, 0xff, 0xc7, 0xff, 0xff, 0x7f, 0xbf, 
	0x86, 0xbf, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x6e, 0xff, 0xff, 0xff, 0xff, 0xfc, 0xff, 
	0xff, 0xf3, 0xc7, 0xdf, 0xe2, 0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x77, 0xcf, 0xff, 0xf8, 
	0xbf, 0xff, 0xff, 0xf8, 0x00, 0x00, 0x79, 0x65, 0xf8, 0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xdf, 0xff, 0xfd, 0xff, 0xff, 0xff, 0xfc, 0x05, 0x00, 0x00, 0x0e, 0x7f, 0xfe, 0x7f, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xe0, 0x00, 0x00, 0x00, 0x01, 0xbf, 0xa8, 0x01, 0xe5, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xef, 0xff, 0xed, 0x80, 0x00, 0xff, 0xfd, 0xff, 0xf0, 
	0x03, 0xec, 0xf0, 0x1e, 0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xe2, 0x6d, 0x81, 0xe0, 0x7e, 
	0xff, 0xf8, 0x7f, 0xe0, 0x01, 0xe8, 0x7c, 0x27, 0x5a, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfc, 
	0x20, 0x82, 0x0f, 0xf0, 0x0f, 0x80, 0x3f, 0xc0, 0x3f, 0xe0, 0x21, 0xe0, 0xec, 0xdf, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xfd, 0xe0, 0xfc, 0x03, 0xe0, 0x27, 0x80, 0x77, 0xff, 0xfc, 0x67, 0x21, 0x3c, 
	0x1d, 0x1f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xc0, 0x38, 0x01, 0x9f, 0xff, 0xff, 0xf3, 0x7f, 
	0xbf, 0x6f, 0xa0, 0x13, 0xf3, 0xed, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x80, 0x3f, 0xff, 0xff, 
	0x3a, 0x3f, 0xfb, 0x31, 0x77, 0x6d, 0xcf, 0x00, 0x7e, 0xe1, 0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xfe, 0xde, 0xc0, 0x1a, 0x10, 0x0b, 0x02, 0x83, 0x6d, 0x68, 0x60, 0x07, 0x8c, 0x9f, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0x77, 0xfb, 0xfe, 0x81, 0x5a, 0x2b, 0xdb, 0x5b, 0x77, 0x6c, 0x39, 0xea, 
	0x00, 0xe0, 0x0f, 0xff, 0xff, 0xff, 0xff, 0xff, 0x78, 0x70, 0x07, 0x82, 0xba, 0x29, 0xdb, 0xb6, 
	0x6b, 0x6c, 0x21, 0xec, 0x80, 0x44, 0x07, 0xff, 0xff, 0xff, 0xff, 0xff, 0x53, 0xdc, 0xef, 0x80, 
	0x3e, 0x63, 0x7b, 0xae, 0x79, 0x6c, 0x25, 0xec, 0xde, 0x00, 0xc0, 0x3f, 0xff, 0xff, 0xff, 0xff, 
	0x51, 0xd9, 0x7e, 0xc0, 0x5e, 0x7e, 0xfb, 0x2c, 0x2f, 0x6e, 0x27, 0xed, 0xc8, 0xe0, 0x40, 0x3f, 
	0xff, 0xff, 0xff, 0xff, 0x4b, 0xde, 0x0e, 0xc2, 0xda, 0x0f, 0xfb, 0xfc, 0x5d, 0x6c, 0x2f, 0xed, 
	0xc3, 0xbe, 0x04, 0x3f, 0xff, 0xff, 0xff, 0xff, 0x43, 0xfe, 0xfe, 0x82, 0xfa, 0xd0, 0xfb, 0xe2, 
	0xd7, 0x6c, 0x27, 0xed, 0xc3, 0xb9, 0xe0, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x56, 0x4e, 0xc5, 
	0x7a, 0xab, 0x5b, 0xe6, 0xf7, 0xec, 0x2b, 0xed, 0xc3, 0xff, 0xcc, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0x61, 0xfc, 0x8f, 0xa5, 0xfb, 0xf8, 0x1f, 0xcc, 0xe6, 0x6c, 0xa4, 0xec, 0xc3, 0xbb, 0xfa, 0xff, 
	0xff, 0xff, 0xfb, 0xf4, 0x71, 0xfd, 0xdf, 0x8a, 0x1b, 0x7d, 0x2f, 0xd0, 0x00, 0xec, 0x0f, 0xec, 
	0xe3, 0x9b, 0xf9, 0xff, 0xff, 0xff, 0xa2, 0x24, 0x3f, 0xfd, 0xbf, 0xad, 0x4b, 0xdb, 0x0f, 0x55, 
	0x4a, 0x96, 0x86, 0xec, 0xe2, 0xab, 0xe8, 0xff, 0xff, 0xff, 0xab, 0xab, 0x90, 0xfc, 0x27, 0xbf, 
	0xfb, 0xe0, 0x2f, 0x2d, 0xda, 0xd4, 0x1e, 0xee, 0xea, 0xbb, 0xf9, 0xff, 0xff, 0xff, 0x78, 0x3b, 
	0x8f, 0xf4, 0x67, 0xba, 0x8b, 0x96, 0xcf, 0x21, 0xff, 0xe6, 0x5a, 0xee, 0xe3, 0xbb, 0xb9, 0xff, 
	0xff, 0xff, 0x2f, 0x88, 0x7f, 0xf4, 0x4e, 0xb3, 0x79, 0x00, 0xeb, 0x0d, 0xec, 0x22, 0x1e, 0xee, 
	0xe3, 0xbb, 0xb9, 0xff, 0xff, 0xff, 0xc6, 0x9b, 0x41, 0xd4, 0x00, 0x00, 0x01, 0xf0, 0x00, 0x0f, 
	0x69, 0xf0, 0x1e, 0xee, 0xeb, 0xbb, 0xb9, 0xff, 0xff, 0xff, 0xe4, 0x07, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xef, 0x09, 0x87, 0x1e, 0xee, 0xeb, 0xbb, 0xb0, 0xff, 0xff, 0xff, 0xe7, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x01, 0xa6, 0xee, 0xe3, 0xbb, 0xb8, 0xff, 
	0xff, 0xfe, 0x22, 0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xdd, 0xce, 0xee, 
	0xe3, 0xbb, 0xb8, 0xff, 0xff, 0xfe, 0x00, 0x00, 0x18, 0x01, 0xfd, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xf2, 0xfd, 0xde, 0xee, 0xb3, 0xbb, 0xb9, 0xff, 0xff, 0xf8, 0x3f, 0xff, 0xff, 0xe0, 0x0f, 0xff, 
	0x80, 0x00, 0x60, 0x7f, 0xf9, 0x81, 0x80, 0x06, 0xf9, 0xbb, 0xb8, 0xff, 0xff, 0xf3, 0xfc, 0xff, 
	0xff, 0xff, 0xff, 0x3d, 0x3f, 0xff, 0xff, 0xc4, 0x60, 0x01, 0xb8, 0x00, 0x00, 0x09, 0xb8, 0xff, 
	0xff, 0xe0, 0x1f, 0xff, 0xff, 0xf0, 0x00, 0x0f, 0x83, 0xff, 0xff, 0xa9, 0xd2, 0x00, 0xa7, 0xff, 
	0xf8, 0x00, 0x23, 0xff, 0xff, 0xff, 0xff, 0xff, 0xc0, 0x03, 0xff, 0xfc, 0xc8, 0x86, 0x00, 0x03, 
	0xfb, 0xb6, 0xbf, 0xbf, 0xff, 0xfc, 0xef, 0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0x00, 0x41, 
	0xdf, 0x4e, 0xef, 0xdf, 0xe0, 0x14, 0xbd, 0xfe, 0xff, 0xfc, 0xf7, 0x7f, 0xff, 0xff, 0xff, 0xf8, 
	0x3d, 0xde, 0xe0, 0xcf, 0xf8, 0x1f, 0xbf, 0xb1, 0x3f, 0xfb, 0xe7, 0x5d, 0xff, 0x41, 0x00, 0x07, 
	0xff, 0xff, 0xff, 0xff, 0xf0, 0x70, 0x03, 0xbf, 0xfe, 0x1f, 0xef, 0xfe, 0xc7, 0xf6, 0xc0, 0x67, 
	0x81, 0xc2, 0x81, 0x81, 0xff, 0xff, 0xc0, 0x47, 0x8f, 0xf3, 0x09, 0xfc, 0x7f, 0xfd, 0xbd, 0xff, 
	0xbf, 0xeb, 0xfb, 0xe0, 0x21, 0x80, 0x00, 0x03, 0xff, 0xff, 0xbf, 0xff, 0xfb, 0xd0, 0x1b, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0x9f, 0xe0, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xf7, 0xff, 0xff, 
	0x7f, 0xf9, 0xf0, 0x06, 0xff, 0xff, 0xfe, 0xf7, 0xfa, 0x80, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0x1c, 0xf0, 0x00, 0x00, 0x07, 0xfc, 0x00, 0x00, 0x00, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x80, 0xdf, 0xff, 0x00, 0x00, 0x00, 0x00, 
	0x07, 0xff, 0xff, 0xff, 0xff, 0xf7, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7e, 0xfc, 0x00, 0x00, 0x0f, 
	0xff, 0xfc, 0x03, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xf1, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf7, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xcf, 0xfb, 0x3f, 0xf0, 0x07, 0xff, 0xff, 0x9f, 0xfc, 0x00, 0x7f, 
	0xff, 0xf7, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x8f, 0xff, 0xff, 0xff, 0xff, 
	0xef, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfd, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xf6, 0x1f, 0xfd, 0xff, 0xff, 0xff, 0xe8, 0xff, 0xff, 0xff, 0xe3, 0xf7, 0xff, 0xff, 0xff, 0xff, 
	0xe7, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf7, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xfb, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfc, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfc, 0xff, 0xfe, 0xdf, 0xff, 0xff, 
	0xff, 0xfc, 0x3f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfc, 0xff, 0xff, 0xfb, 0xff, 
	0xff, 0xec, 0x7f, 0xff, 0xff, 0xff, 0xff, 0x9f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xe0, 
	0xff, 0xff, 0xff, 0xff, 0x87, 0xfe, 0x1f, 0xf9, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xf8, 0x10, 0x02, 0x3f, 0xff, 0xdf, 0xff, 0xcf, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf0, 0x10, 0x03, 0x1f, 0xff, 0xff, 0xf9, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfc, 0x07, 0xff, 0xff, 0xfc, 0x07, 
	0xff, 0xff, 0xff, 0xfe, 0x77, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
};

void updateDisplay(String line1, String line2 = "", uint16_t textColor = ST77XX_WHITE, uint16_t bgColor = ST77XX_BLACK);
void openDoor();
bool updateLocalCacheFromServer();
bool isUidAllowedLocally(String uid);
bool isPinAllowedLocally(String pin);
void setupWifi();
void checkMfrc522Status();
void notifyDoorStateChange(bool isOpen);
void validateTokenOnServer(String token); 

// --- Callback BLE (Aggiornati con Display) ---
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true; Serial.println("Dispositivo connesso"); digitalWrite(LED_PIN, HIGH);
      updateDisplay("BLE Connesso", WiFi.localIP().toString(), ST77XX_GREEN);
    }
    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false; Serial.println("Dispositivo disconnesso"); digitalWrite(LED_PIN, LOW);
      updateDisplay("Pronto...", isWifiConnected ? "Online" : "OFFLINE", isWifiConnected ? ST77XX_CYAN:ST77XX_ORANGE);
      delay(500); // Piccolo delay per evitare riavvio advertising troppo veloce
      BLEDevice::startAdvertising(); Serial.println("Advertising riavviato");
    }
};
class MyCharacteristicCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string value = pCharacteristic->getValue();
      if (value.length() > 0) {
        receivedToken = ""; for (int i = 0; i < value.length(); i++) { receivedToken += (char)value[i]; }
        Serial.println("BLE Ricevuto: " + receivedToken);
        updateDisplay("BLE Ricevuto", "Verifica token...", ST77XX_YELLOW);
        validateTokenOnServer(receivedToken);
      }
    }
};


void setupWifi() {
  updateDisplay("WIFI", "Connetto...", ST77XX_YELLOW);
  WiFi.mode(WIFI_STA); // Assicura modalità Station
  WiFi.begin(ssid, password);
  Serial.print("Connessione a "); Serial.println(ssid);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500); Serial.print("."); attempts++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    isWifiConnected = true; // Imposta stato globale
    Serial.println("\nWiFi connesso! IP: " + WiFi.localIP().toString());
    updateDisplay("WiFi", "Connesso!", ST77XX_GREEN);
    delay(1500);
  } else {
    isWifiConnected = false; // Imposta stato globale
    Serial.println("\nImpossibile connettersi al WiFi.");
    updateDisplay("ERRORE RETE!", "", ST77XX_RED);
    delay(3000);
  }
}

// --- Funzione Setup OTA ---
void setupOTA() {
  ArduinoOTA.setHostname(ota_hostname);
  if (strlen(ota_password) > 0) {
    ArduinoOTA.setPassword(ota_password);
  }

  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH) type = "sketch";
      else type = "filesystem"; 
      Serial.println("Start updating " + type);
      updateDisplay("Aggiorno...", "", ST77XX_YELLOW);
    })
    .onEnd([]() {
      Serial.println("\nEnd");
      updateDisplay("Aggiornamento", "Completato", ST77XX_GREEN);
      delay(1000);
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      int percentage = (progress / (total / 100));
      Serial.printf("Progress: %u%%\r", percentage);
      updateDisplay("Aggiorno...", "Stato: " + String(percentage) + "%", ST77XX_YELLOW);
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      String errorMsg = "";
      if (error == OTA_AUTH_ERROR) errorMsg = "#01 - Login";
      else if (error == OTA_BEGIN_ERROR) errorMsg = "#02 - Inizio";
      else if (error == OTA_CONNECT_ERROR) errorMsg = "#03 - Network";
      else if (error == OTA_RECEIVE_ERROR) errorMsg = "#04 - Ricezione";
      else if (error == OTA_END_ERROR) errorMsg = "#05 - Fine";
      else errorMsg = "#00 - Generico";
      Serial.println(errorMsg);
      updateDisplay("ERRORE", errorMsg, ST77XX_BLACK, ST77XX_RED);
      delay(2000);
    });

  ArduinoOTA.begin();
  Serial.println("OTA Ready");
  Serial.print("IP address: "); Serial.println(WiFi.localIP());
}

bool isPermissionValid(JsonObject doc) {
    struct tm now;
    time_t t = time(NULL);
    localtime_r(&t, &now);

    // Formatta la data (gg-mm-aaaa)
    char data[11];
    sprintf(data, "%02d-%02d-%04d", now.tm_mday, now.tm_mon + 1, now.tm_year + 1900);

    // Formatta l'orario (hh:mm:ss)
    char orario[9];
    sprintf(orario, "%02d:%02d:%02d", now.tm_hour, now.tm_min, now.tm_sec);

    // Verifica la validità del permesso
    const char* giorno = doc["giorno"]; // Ad esempio "lunedì"
    const char* ora_inizio = doc["ora_inizio"];
    const char* ora_fine = doc["ora_fine"];

    if (strcmp(giorno, data) == 0 && strcmp(ora_inizio, orario) <= 0 && strcmp(ora_fine, orario) >= 0) {
        return true;
    }

    return false;
}


bool isUidAllowedLocally(String uid) {
    StaticJsonDocument<200> doc;


    JsonObject jsonObject = doc.as<JsonObject>(); 
    return isPermissionValid(jsonObject);
}


bool isPinAllowedLocally(String pin) {
    StaticJsonDocument<200> doc;


    JsonObject jsonObject = doc.as<JsonObject>(); 
    return isPermissionValid(jsonObject);
}


bool updateLocalCacheFromServer() {
    if (!isWifiConnected) {
        Serial.println("Cache Update: WiFi non connesso.");
        return false;
    }

    Serial.println("Aggiornamento cache locale dal server...");
    updateDisplay("BACKUP", "Scarico...", ST77XX_YELLOW);

    HTTPClient http;
    String url = String(credentialsUrl) + "?stanza=" + String(readerId);
    http.begin(url);
    int httpCode = http.GET();
    bool success = false;

    if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        Serial.println("Risposta credenziali: " + payload);
        StaticJsonDocument<EEPROM_SIZE - 50> doc;
        DeserializationError error = deserializeJson(doc, payload);

        if (!error) {
            JsonArray uids = doc["uids"]; 
            JsonArray pins = doc["pins"]; 

            if (!uids.isNull() || !pins.isNull()) {
                tft.fillScreen(ST7735_WHITE);
                tft.drawBitmap(0, 0, logo, 160, 128, ST77XX_BLACK);

                EEPROM.write(0, 0);
                int address = 0;

                // Scrittura UIDs
                for (JsonVariant uid : uids) {
                    String uidStr;
                    serializeJson(uid, uidStr); // serializza tutto il contenuto
                    if (address + uidStr.length() + 1 < EEPROM_SIZE) {
                        EEPROM.writeString(address, uidStr);
                        address += uidStr.length() + 1;
                    } else { Serial.println("EEPROM Piena (UIDs)"); break; }
                }

                EEPROM.write(address++, '\n'); 

                // Scrittura PINs
                for (JsonVariant pin : pins) {
                    String pinStr;
                    serializeJson(pin, pinStr);
                    if (address + pinStr.length() + 1 < EEPROM_SIZE) {
                        EEPROM.writeString(address, pinStr);
                        address += pinStr.length() + 1;
                    } else { Serial.println("EEPROM Piena (PINs)"); break; }
                }

                EEPROM.write(address, '\0'); 

                if (EEPROM.commit()) {
                    Serial.println("Cache locale aggiornata in EEPROM.");
                    lastCacheUpdateTime = millis();
                    success = true;
                } else {
                    Serial.println("Errore commit EEPROM!");
                }
            } else {
                Serial.println("JSON malformattato.");
            }
        } else {
            Serial.print("Errore parsing JSON: "); Serial.println(error.c_str());
        }
    } else {
        Serial.printf("Errore GET credenziali: %d\n", httpCode);
    }

    http.end();
    delay(1500);
    return success;
}


void setupTime() {
  configTime(3600, 3600, "pool.ntp.org", "time.nist.gov");
  Serial.println("Sincronizzazione NTP in corso...");
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    Serial.print("Ora sincronizzata: ");
    Serial.println(&timeinfo, "%F %T");
  } else {
    Serial.println("Errore sincronizzazione NTP");
  }
}

void checkMfrc522Status() {
    //Serial.println("DEBUG: Controllo stato MFRC522...");
    
    mfrc522.PCD_Init();  

    // Verifica se il modulo risponde: check antenna gain
    byte gain = mfrc522.PCD_GetAntennaGain();
    //Serial.print("DEBUG: Antenna gain MFRC522: ");
    //Serial.println(gain);

    if (gain == 0 && mfrc522Active) {
        Serial.println("DEBUG: MFRC522 non risponde più! Provo reinizializzazione...");
        updateDisplay("RFID", "Non risponde!", ST77XX_RED);

        // Reset hardware via pin RST
        Serial.println("DEBUG: Tentativo reset hardware via pin RST...");
        pinMode(RFID_RST, OUTPUT);
        digitalWrite(RFID_RST, LOW);
        delay(5);
        digitalWrite(RFID_RST, HIGH);
        delay(150);  // Attendi riavvio MFRC522

        // Ritenta inizializzazione
        mfrc522.PCD_Init();

        gain = mfrc522.PCD_GetAntennaGain();
        Serial.print("DEBUG: Antenna gain DOPO reset: ");
        Serial.println(gain);

        if (gain > 0) {
            tft.fillScreen(ST7735_WHITE);
            tft.drawBitmap(0, 0, logo, 160, 128, ST77XX_BLACK);
            mfrc522Active = true;
        } else {
            updateDisplay("RFID", "Non risponde!", ST77XX_RED);
        }

    } else if (gain > 0 && !mfrc522Active) {
        Serial.println("DEBUG: MFRC522 ora funzionante!");
        mfrc522Active = true;
        updateDisplay("RFID", "Pronto", ST77XX_GREEN);

    } else if (gain > 0) {
        Serial.println("DEBUG: MFRC522 operativo.");
    } else {
        updateDisplay("RFID", "Non risponde!", ST77XX_RED);
    }
}

// --- Funzione Notifica Stato Porta ---
void notifyDoorStateChange(bool isOpen) {
    if (!isWifiConnected) {
        Serial.println("Notifica stato porta: WiFi non connesso.");
        return; // Non inviare notifiche se WiFi non connesso
    }

    unsigned long now = millis();
    if (now - lastDoorNotifyTime < DOOR_NOTIFY_DEBOUNCE) {
        return; // Ignora se la notifica è troppo ravvicinata (debounce)
    }
    lastDoorNotifyTime = now;

    // Se la porta si apre entro 20 secondi da un input, consideriamo "porta aperta dall'interno"
    int state = 0;  // Default: porta chiusa
    if (isOpen) {
        if (now - lastInputTime <= 10000) { 
            state = 2;  // Porta aperta dall'interno
            Serial.println("Ora: " + String(now));
            Serial.println("lastInputtime: " + String(lastInputTime));
        } else {
            state = 1;  // Porta aperta
        }
    }

    // Invia la notifica del nuovo stato della porta
    Serial.println("Invio notifica stato porta: " + String(state));

    HTTPClient http;
    String url = String(doorEventUrl) + "?stanza=" + String(readerId) + "&stato=" + String(state);
    http.begin(url);
    int httpCode = http.GET();

    if (httpCode > 0) {
        Serial.printf("[HTTP-Notify] Codice risposta GET: %d\n", httpCode);
        // La notifica è stata inviata correttamente
    } else {
        Serial.printf("[HTTP-Notify] Errore GET fallito: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end();
}

// --- Gestione dell'input (pulsante o altro) ---
void handleInput() {
    // Verifica se il pulsante è stato premuto (o un altro ingresso, a tua scelta)
    if (digitalRead(PIN_BUTTON) == HIGH) {
        lastInputTime = millis();  // Salva il tempo dell'ultimo input
    }
}


// --- Funzioni Validazione (Modificate per Offline Mode) ---

void validateTokenOnServer(String token) { // BLE
  if (!isWifiConnected) {
    Serial.println("Validazione BLE: WiFi non connesso. Impossibile validare.");
    updateDisplay("Accesso BLE", "Offline non supp.", ST77XX_ORANGE);
    delay(2000); 
    updateDisplay("Pronto...", isWifiConnected ? "Online" : "OFFLINE", isWifiConnected ? ST77XX_CYAN:ST77XX_ORANGE);
    return;
  }

  Serial.println("Invio token BLE al server per validazione...");
  updateDisplay("BLE Validazione...", "Invio dati server", ST77XX_YELLOW);

  HTTPClient http; 
  http.begin(bleValidationUrl); 
  http.addHeader("Content-Type", "application/json");

  StaticJsonDocument<256> jsonDoc;
  jsonDoc["token"] = token;
  jsonDoc["reader_id"] = readerId;
  String jsonPayload; 
  serializeJson(jsonDoc, jsonPayload);

  int httpCode = http.POST(jsonPayload);
  bool accessGranted = false;

  if (httpCode > 0) {
    if (httpCode == HTTP_CODE_OK) {
      StaticJsonDocument<256> responseDoc;
      DeserializationError error = deserializeJson(responseDoc, http.getString());
      bool isValid = false;
      if (!error && responseDoc.containsKey("valid")) {
        isValid = responseDoc["valid"];
      }

      if (isValid) {
        updateDisplay("Accesso BLE", "Autorizzato!", ST77XX_GREEN);
        accessGranted = true;
        openDoor();
      } else {
        updateDisplay("Accesso BLE", "Negato!", ST77XX_RED);
      }
    } else {
      updateDisplay("Errore Server", "Codice: " + String(httpCode), ST77XX_RED);
    }
  } else {
    updateDisplay("Errore Invio", "Controlla URL/Rete", ST77XX_RED);
  }

  http.end();

  if (accessGranted) updateLocalCacheFromServer();

  delay(2000); 
  tft.fillScreen(ST7735_WHITE);
  tft.drawBitmap(0, 0, logo, 160, 128, ST77XX_BLACK); // Modifica le dimensioni se necessario

}


void validateRfidOnServer(String uidString) {
  bool accessGranted = false;
  if (isWifiConnected) {
    Serial.println("Invio UID RFID al server per validazione (Online)...");
    updateDisplay("Verifica...", "", ST77XX_YELLOW);
    String url = String(rfidValidationUrl) + "?uid=" + uidString + "&stanza=" + String(readerId);
    HTTPClient http;
    http.begin(url);
    int httpCode = http.GET();
    
    if (httpCode > 0) {
      String response = http.getString(); // Otteniamo la risposta del server

      if (httpCode == HTTP_CODE_OK) {
        Serial.println("Risposta del server: " + response);
        
        // Gestiamo le risposte del server
        if (response == "ACCESSO_CONSENTITO") {
          Serial.println("Accesso RFID Autorizzato (Online).");
          updateDisplay("Accesso", "Consentito", ST77XX_GREEN);
          accessGranted = true;
          openDoor();
        } else if (response == "ACCESSO_NEGATO") {
          Serial.println("Accesso RFID Negato (Online).");
          updateDisplay("Accesso", "Negato", ST77XX_RED);
          digitalWrite(BUZZER, HIGH);
          delay(300);
          digitalWrite(BUZZER, LOW);
          delay(300);
          digitalWrite(BUZZER, HIGH);
          delay(300);
          digitalWrite(BUZZER, LOW);    
          
        } else if (response == "VARCO_DISABILITATO") {
          Serial.println("Accesso Negato: Varco Disabilitato.");
          updateDisplay("Varco", "Non attivo", ST77XX_RED);
          digitalWrite(BUZZER, HIGH);
          delay(300);
          digitalWrite(BUZZER, LOW);
          delay(300);
          digitalWrite(BUZZER, HIGH);
          delay(300);
          digitalWrite(BUZZER, LOW);    
        } else if (response == "MANUTENZIONE") {
          Serial.println("Accesso Negato: Manutenzione.");
          updateDisplay("Accesso", "Negato", ST77XX_RED);
          digitalWrite(BUZZER, HIGH);
          delay(300);
          digitalWrite(BUZZER, LOW);
          delay(300);
          digitalWrite(BUZZER, HIGH);
          delay(300);
          digitalWrite(BUZZER, LOW);    
        } else {
          Serial.println("Risposta del server non riconosciuta.");
          updateDisplay("Errore Server", "Risposta Non Valida", ST77XX_RED);
          digitalWrite(BUZZER, HIGH);
          delay(300);
          digitalWrite(BUZZER, LOW);
          delay(300);
          digitalWrite(BUZZER, HIGH);
          delay(300);
          digitalWrite(BUZZER, LOW);    
        }
      } else {
        Serial.printf("Errore GET RFID: %s\n", http.errorToString(httpCode).c_str());
        updateDisplay("Errore Invio RFID", "Controlla URL/Rete", ST77XX_RED);
        digitalWrite(BUZZER, HIGH);
        delay(300);
        digitalWrite(BUZZER, LOW);
        delay(300);
        digitalWrite(BUZZER, HIGH);
        delay(300);
        digitalWrite(BUZZER, LOW);    
      }
    } else {
      Serial.printf("Errore GET RFID: %s\n", http.errorToString(httpCode).c_str());
      updateDisplay("Errore GET RFID", "Controlla URL/Rete", ST77XX_RED);
      digitalWrite(BUZZER, HIGH);
      delay(300);
      digitalWrite(BUZZER, LOW);
      delay(300);
      digitalWrite(BUZZER, HIGH);
      delay(300);
      digitalWrite(BUZZER, LOW);    
    }

    http.end();
    if (accessGranted) updateLocalCacheFromServer(); // Aggiorna cache dopo accesso online OK
  } else { // --- Modalità Offline ---
    Serial.println("Validazione RFID (Offline)...");
    updateDisplay("RFID Validazione...", "OFFLINE", ST77XX_ORANGE);
    if (isUidAllowedLocally(uidString)) {
      Serial.println("Accesso RFID Autorizzato (Offline).");
      updateDisplay("Accesso RFID", "Offline OK!", ST77XX_GREEN);
      openDoor();
      accessGranted = true; // Anche se offline, accesso è avvenuto
    } else {
      Serial.println("Accesso RFID Negato (Offline).");
      updateDisplay("Accesso RFID", "Offline NEGATO", ST77XX_RED);
    }
  }

  delay(2000); 
  tft.fillScreen(ST7735_WHITE);
  tft.drawBitmap(0, 0, logo, 160, 128, ST77XX_BLACK); // Modifica le dimensioni se necessario

}

void validatePinOnServer(String pin) { // PIN
  bool accessGranted = false;
  
  if (isWifiConnected) {
    Serial.println("Invio PIN al server per validazione (Online)...");
    updateDisplay("Verifica...", "", ST77XX_YELLOW);

    // URL di validazione del PIN
    String url = String(pinValidationUrl) + "?pin=" + pin + "&stanza=" + String(readerId);
    HTTPClient http;
    http.begin(url);
    int httpCode = http.GET();
    
    if (httpCode > 0) {
      String response = http.getString(); // Otteniamo la risposta del server

      if (httpCode == HTTP_CODE_OK) {
        Serial.println("Risposta del server: " + response);
        
        // Gestiamo le risposte del server
        if (response == "ACCESSO_CONSENTITO") {
          Serial.println("Accesso PIN Autorizzato (Online).");
          updateDisplay("Accesso", "Consentito", ST77XX_GREEN);
          accessGranted = true;
          openDoor();
        } else if (response == "ACCESSO_NEGATO") {
          Serial.println("Accesso PIN Negato (Online).");
          updateDisplay("Accesso", "Negato", ST77XX_RED);
          digitalWrite(BUZZER, HIGH);
          delay(300);
          digitalWrite(BUZZER, LOW);
          delay(300);
          digitalWrite(BUZZER, HIGH);
          delay(300);
          digitalWrite(BUZZER, LOW);                   
        } else if (response == "VARCO_DISABILITATO") {
          Serial.println("Accesso Negato: Varco Disabilitato.");
          updateDisplay("Varco", "Non attivo", ST77XX_RED);          
          digitalWrite(BUZZER, HIGH);
          delay(300);
          digitalWrite(BUZZER, LOW);
          delay(300);
          digitalWrite(BUZZER, HIGH);
          delay(300);
          digitalWrite(BUZZER, LOW);         
        } else if (response == "MANUTENZIONE") {
          Serial.println("Accesso Negato: Manutenzione.");
          updateDisplay("Accesso", "Negato", ST77XX_RED);
          digitalWrite(BUZZER, HIGH);
          delay(300);
          digitalWrite(BUZZER, LOW);
          delay(300);
          digitalWrite(BUZZER, HIGH);
          delay(300);
          digitalWrite(BUZZER, LOW);         
        } else {
          Serial.println("Risposta del server non riconosciuta.");
          updateDisplay("PIN", "Errore Rete", ST77XX_RED);
          digitalWrite(BUZZER, HIGH);
          delay(300);
          digitalWrite(BUZZER, LOW);
          delay(300);
          digitalWrite(BUZZER, HIGH);
          delay(300);
          digitalWrite(BUZZER, LOW);         
        }
      } else {
        Serial.printf("Errore GET PIN: %s\n", http.errorToString(httpCode).c_str());
        updateDisplay("PIN", "Errore Rete", ST77XX_RED);
        digitalWrite(BUZZER, HIGH);
        delay(300);
        digitalWrite(BUZZER, LOW);
        delay(300);
        digitalWrite(BUZZER, HIGH);
        delay(300);
        digitalWrite(BUZZER, LOW);         
      }
    } else {
      Serial.printf("Errore GET PIN: %s\n", http.errorToString(httpCode).c_str());
      updateDisplay("PIN", "Errore Rete", ST77XX_RED);
      digitalWrite(BUZZER, HIGH);
      delay(300);
      digitalWrite(BUZZER, LOW);
      delay(300);
      digitalWrite(BUZZER, HIGH);
      delay(300);
      digitalWrite(BUZZER, LOW);         
    }

    http.end();
    if (accessGranted) updateLocalCacheFromServer(); // Aggiorna cache dopo accesso online OK
  } else { // --- Modalità Offline ---
    Serial.println("Validazione PIN (Offline)...");
    updateDisplay("Verifica...", "", ST77XX_WHITE);

    // Verifica il PIN in modalità offline
    if (isPinAllowedLocally(pin)) {
      Serial.println("Accesso PIN Autorizzato (Offline).");
      updateDisplay("Accesso", "Consentito", ST77XX_GREEN);
      openDoor();
      accessGranted = true; // Anche se offline, accesso è avvenuto
    } else {
      Serial.println("Accesso PIN Negato (Offline).");
      updateDisplay("Accesso", "Negato", ST77XX_RED);
    }
  }

  delay(2000); 
  tft.fillScreen(ST7735_WHITE);
  tft.drawBitmap(0, 0, logo, 160, 128, ST77XX_BLACK); // Modifica le dimensioni se necessario
}


void openDoor() { 
    Serial.println("APERTURA PORTA...");
    updateDisplay("Accesso", "Consentito", ST77XX_WHITE);
    digitalWrite(LOCK_PIN, HIGH);
    digitalWrite(BUZZER, HIGH);
    delay(500);
    digitalWrite(BUZZER, LOW);     
    delay(5000);
    digitalWrite(LOCK_PIN, LOW);
    Serial.println("Porta richiusa.");
    digitalWrite(LED_PIN, deviceConnected ? HIGH : LOW);
}

void updateDisplay(String line1, String line2, uint16_t textColor, uint16_t bgColor) {
  digitalWrite(TFT_CS, LOW);
  tft.fillScreen(bgColor);

  int textSize = 2;
  int charWidth = 6 * textSize;
  int charHeight = 8 * textSize;

  int screenWidth = tft.width();
  int screenHeight = tft.height();

  int numLines = line2.length() > 0 ? 2 : 1;
  int totalTextHeight = numLines * charHeight + (numLines - 1) * 4; // Spazio tra le righe
  int startY = (screenHeight - totalTextHeight) / 2;

  tft.setTextColor(textColor);
  tft.setTextSize(textSize);

  // Linea 1 centrata
  int textWidth1 = line1.length() * charWidth;
  int startX1 = (screenWidth - textWidth1) / 2;
  tft.setCursor(startX1, startY);
  tft.print(line1);

  // Linea 2 centrata, se presente
  if (line2.length() > 0) {
    int textWidth2 = line2.length() * charWidth;
    int startX2 = (screenWidth - textWidth2) / 2;
    tft.setCursor(startX2, startY + charHeight + 4);  // 4 pixel di spazio tra le righe
    tft.print(line2);
  }

  Serial.print("Display: ");
  Serial.print(line1);
  if (line2.length() > 0) {
    Serial.print(" / ");
    Serial.print(line2);
  }
  Serial.println();

}


void setup() {
  Serial.begin(115200);
  Serial.println("\n\nAvvio Multi-Access Controller v2...");

  // 1. Configurazione Pin Essenziali
  Serial.println("Configuro Pin...");
  pinMode(LOCK_PIN, OUTPUT); digitalWrite(LOCK_PIN, LOW);
  pinMode(LED_PIN, OUTPUT); digitalWrite(LED_PIN, LOW);
  pinMode(REED_PIN, INPUT_PULLUP);
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  pinMode(BUZZER, OUTPUT); digitalWrite(BUZZER, LOW);

  pinMode(TFT_CS, OUTPUT); digitalWrite(TFT_CS, HIGH); 

  pinMode(RFID_SS, OUTPUT); digitalWrite(RFID_SS, HIGH); 
  pinMode(RFID_RST, OUTPUT); digitalWrite(RFID_RST, HIGH); 

  Serial.println("Inizializzo SPI...");
  SPI.begin();

  Serial.println("Inizializzo MFRC522...");
  mfrc522.PCD_Init();
  delay(50); // Piccolo delay dopo l'init

  // Verifica MFRC522 (con logica corretta!)
  byte v = mfrc522.PCD_ReadRegister(mfrc522.VersionReg);
  if (v == 0x00 || v == 0xFF || v == 0x90) { 
    mfrc522Active = false; 
    Serial.println("❌ MFRC522 NON TROVATO o errore comunicazione!");
    Serial.println("👉 Controlla:");
    Serial.print(" - SS (SDA) definito su pin: "); Serial.println(RFID_SS); 
    Serial.print(" - RST definito su pin: "); Serial.println(RFID_RST);   
    Serial.println(" - Alimentazione corretta (3.3V!) e GND comune.");
    Serial.println(" - Nessun altro dispositivo SPI attivo (CS alto).");
  } else {
    mfrc522Active = true; // << CORRETTO: true se funziona
    Serial.print("✅ MFRC522 Trovato! Versione firmware: 0x");
    Serial.println(v, HEX);
  }

  // 4. Inizializza Display TFT
  Serial.println("Inizializzo Display...");
  digitalWrite(RFID_SS, HIGH);
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(1);
  Serial.println("Display Inizializzato.");
  updateDisplay("Avvio...", "RFID " + String(mfrc522Active ? "OK" : "FAIL"), mfrc522Active ? ST77XX_GREEN : ST77XX_RED);
  delay(1000); 


  Serial.println("Inizializzo EEPROM...");
  if (!EEPROM.begin(EEPROM_SIZE)) {
    Serial.println("Errore montaggio EEPROM!");
    updateDisplay("Errore EEPROM!", "Impossibile avviare", ST77XX_RED);
    while(1); 
  }
  Serial.println("EEPROM inizializzata (" + String(EEPROM_SIZE) + " bytes).");
  updateDisplay("Avvio...", "EEPROM OK", ST77XX_GREEN); delay(500);

  // 6. Init WiFi
  updateDisplay("Avvio...", "WiFi...", ST77XX_YELLOW);
  setupWifi(); 

  if (isWifiConnected) {
    updateDisplay("Avvio...", "OTA...", ST77XX_YELLOW);
    setupOTA();
  } else {
    Serial.println("WiFi non connesso, OTA non avviato.");
    updateDisplay("Avvio...", "ERRORE WiFi", ST77XX_YELLOW); delay(500);
  }


  updateDisplay("Avvio...", "BLE...", ST77XX_BLUE);
  BLEDevice::init(ota_hostname);
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  BLEService *pService = pServer->createService(SERVICE_UUID);
  pTokenCharacteristic = pService->createCharacteristic(
                            TOKEN_CHARACTERISTIC_UUID,
                            BLECharacteristic::PROPERTY_WRITE
                         );
  pTokenCharacteristic->setCallbacks(new MyCharacteristicCallbacks());
  pService->start();
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();
  Serial.println("Server BLE avviato e advertising in corso.");
  updateDisplay("Avvio...", "BLE OK", ST77XX_GREEN); delay(500);


  lastReedState = analogRead(REED_PIN); 
  Serial.println("Stato iniziale porta: " + String(digitalRead(REED_PIN) == HIGH ? "APERTA" : "CHIUSA")); 

  if (isWifiConnected) {
    updateLocalCacheFromServer(); 
  }

  setupTime();

  updateDisplay("Pronto.", mfrc522Active ? "Avvicina Tag" : "RFID Inattivo", ST77XX_CYAN);
  Serial.println("Setup completato. Sistema pronto.");
}





// --- LOOP ---
void loop() {
  unsigned long currentTime = millis();

  // 1. OTA (Over-The-Air updates)
  if (isWifiConnected) {
    ArduinoOTA.handle();
  }

  int analogValue = analogRead(REED_PIN);


  int currentReedState;

if (analogValue >= 4000 && analogValue <= 4095) {
    currentReedState = HIGH;  // Porta chiusa
} else if (analogValue >= 0 && analogValue <= 200) {
    currentReedState = LOW;   // Porta aperta
  } else {
    // Zona morta: non cambiare stato
    currentReedState = lastReedState;
  }

  if (currentReedState != lastReedState) {
    Serial.println("Cambio stato porta rilevato!");
    lastReedState = currentReedState;

    bool isOpen = (currentReedState == HIGH); // HIGH = Aperta

    if (isOpen) {
      updateDisplay("Porta", "Aperta", ST77XX_YELLOW);
    } else {      
      tft.fillScreen(ST7735_WHITE);
      tft.drawBitmap(0, 0, logo, 160, 128, ST77XX_BLACK); // Modifica le dimensioni se necessario
    }

    notifyDoorStateChange(isOpen);
  }

  // 3. Controllo pulsante apertura porta
  handleInput();

// 4. Tastierino e timeout
char key = keypad.getKey();
if (key) {
  lastKeypadTime = currentTime;

  if (key == CLEAR_KEY) {
    pinBufferIndex = 0;
    pinBuffer[pinBufferIndex] = '\0';
    updateDisplay("PIN", "Cancellato", ST77XX_WHITE);

  } else if (key == ENTER_KEY) {
    if (pinBufferIndex > 0) {
      pinBuffer[pinBufferIndex] = '\0';
      validatePinOnServer(String(pinBuffer));
      pinBufferIndex = 0;
      pinBuffer[pinBufferIndex] = '\0';
    } else {
      delay(1000);
      
      tft.fillScreen(ST7735_WHITE);
      tft.drawBitmap(0, 0, logo, 160, 128, ST77XX_BLACK); // Modifica le dimensioni se necessario

    }

  // Gestione dell'inserimento delle cifre del PIN
  } else if (pinBufferIndex < PIN_MAX_LENGTH) {
    pinBuffer[pinBufferIndex++] = key;
    pinBuffer[pinBufferIndex] = '\0';

    // Mostriamo il numero sulla seriale
    Serial.print("Digitato: ");
    Serial.println(key);

    // Quando il PIN è completo (4 cifre), lo inviamo automaticamente
    if (pinBufferIndex == 4) {
      validatePinOnServer(String(pinBuffer));
      pinBufferIndex = 0;  // Resettiamo il buffer dopo la validazione
      pinBuffer[pinBufferIndex] = '\0';
      
      return;  // Esci dalla funzione per evitare ulteriori modifiche al PIN
    }

    // Mascheriamo il PIN per il display con '*'
    String maskedPin = "";
    for (int i = 0; i < pinBufferIndex; i++) maskedPin += '*';  // Mostriamo '*' al posto dei numeri
    updateDisplay("Digita PIN:", maskedPin, ST77XX_WHITE);
    digitalWrite(BUZZER, HIGH);
    delay(200);
    digitalWrite(BUZZER, LOW);

  // Gestione dell'errore se il PIN è troppo lungo
  } else {
    updateDisplay("PIN Troppo Lungo!", "Max " + String(PIN_MAX_LENGTH), ST77XX_YELLOW);
    digitalWrite(BUZZER, HIGH);
    delay(300);
    digitalWrite(BUZZER, LOW);
    delay(300);
    digitalWrite(BUZZER, HIGH);
    delay(300);
    digitalWrite(BUZZER, LOW);    
    delay(1000);
    String maskedPin = "";
    for (int i = 0; i < pinBufferIndex; i++) maskedPin += '*';
    updateDisplay("Digita PIN:", maskedPin, ST77XX_WHITE);
  }
}

// Timeout inserimento PIN
if (pinBufferIndex > 0 && (currentTime - lastKeypadTime > KEYPAD_TIMEOUT)) {
  pinBufferIndex = 0;
  pinBuffer[pinBufferIndex] = '\0';
  updateDisplay("PIN", "Tempo Scaduto", ST77XX_YELLOW);
  digitalWrite(BUZZER, HIGH);
  delay(300);
  digitalWrite(BUZZER, LOW);
  delay(300);
  digitalWrite(BUZZER, HIGH);
  delay(300);
  digitalWrite(BUZZER, LOW);    
  Serial.println("Timeout PIN.");
  delay(2000);
  tft.fillScreen(ST7735_WHITE);
  tft.drawBitmap(0, 0, logo, 160, 128, ST77XX_BLACK); // Modifica le dimensioni se necessario

}

  // 5. Lettura RFID tramite MFRC522
  if (mfrc522Active && pinBufferIndex == 0 && (currentTime - lastRfidReadTime > RFID_READ_DELAY)) {
    if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
      Serial.println("Tag trovato! UID:");
      String uidString = "";
      for (uint8_t i = 0; i < mfrc522.uid.size; i++) {
        char hexBuffer[3];
        sprintf(hexBuffer, "%02X", mfrc522.uid.uidByte[i]);
        uidString += hexBuffer;
      }
      Serial.println(uidString);
      validateRfidOnServer(uidString);
      lastRfidReadTime = currentTime;

      // Termina comunicazione con il tag
      mfrc522.PICC_HaltA();
      mfrc522.PCD_StopCrypto1();
    }
  }

  // 6. Check periodico dello stato MFRC522 (es. per reset)
  if (currentTime - lastMfrc522CheckTime > MFRC522_CHECK_INTERVAL) {
    checkMfrc522Status();
    lastMfrc522CheckTime = currentTime;
  }

  // 7. Aggiornamento cache offline (se online)
  if (isWifiConnected && (currentTime - lastCacheUpdateTime > CACHE_UPDATE_INTERVAL)) {
    updateLocalCacheFromServer();
  }

  // 8. Rilevamento cambio stato WiFi
  bool currentWifiStatus = (WiFi.status() == WL_CONNECTED);
  if (currentWifiStatus != isWifiConnected) {
    Serial.println("Cambio stato WiFi rilevato!");
    isWifiConnected = currentWifiStatus;
    if (!isWifiConnected) {
      updateDisplay("ERRORE RETE", "Modalità Offline", ST77XX_ORANGE);
    } else {
     tft.fillScreen(ST7735_WHITE);
     tft.drawBitmap(0, 0, logo, 160, 128, ST77XX_BLACK); // Modifica le dimensioni se necessario

    }
  }
}
