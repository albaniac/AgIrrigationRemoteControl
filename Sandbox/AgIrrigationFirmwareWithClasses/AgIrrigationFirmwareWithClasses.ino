/******************************************************************************************************************//**
 * @brief  Arduino Sketch firmware to be uploaded onto the AgIrrigationRemoteControl Hand-Remote Device.
 * @see https://github.com/tgit23/AgIrrigationRemoteControl
 * @remarks Version 2017.06.17
 * @todo
 *  - Implement Firmata for Base/Desktop operation
 * @authors 
 *    tgit23        01/2017       Original
 *    tgit23        07/2017       Implemented keypad button interrupts and Non-Blocking functionality
**********************************************************************************************************************/
#include "PeerRemoteMenu.h"

#define HAND_REMOTE 0
#define PUMP_CONTROLLER 1
//=====================================================================================================================
//------------------------------ SIMPLE USER CONFIGURATION SETTINGS ---------------------------------------------------
//=====================================================================================================================
#define FIRMWARE_IS_FOR         PUMP_CONTROLLER   // Firmware for a 'HAND_REMOTE' or 'PUMP_CONTROLLER'
#define BUILD_VERSION                  20170706   // Release Version used to Build the Unit ( without the dots )
#define TRANSCEIVER_ID                        1   // Unique numeric (ID)entity for this Unit(1-15)
#define XBEECONFIG                            0   // Configure the XBEE using XCTU Digi Software by setting this to 1
#define CONTROLLER_HAS_LCD                    1   // Set to '1' if Pump-Controller has the LCD Screen expansion

//=====================================================================================================================
//------------------------------ ADVANCED CONFIGURATION SETTINGS ------------------------------------------------------
//=====================================================================================================================
#if BUILD_VERSION>20170524
  #define SS_TX_PIN 2                     // TX -> XBEE-DIN ( Closest to UNO )
  #define SS_RX_PIN 3                     // RX -> XBEE-DOUT ( Farthest from UNO )
  #define SBUZZ 12                        // Buzzer Signal Pin (S)
  #define PBUZZ 13                        // Buzzer Power Pin (+)
#else                                   // vvvvvvvv [ Build Release 2017.05.24 Pins ] vvvvvvvvvvvvvv
  #define SS_TX_PIN 11                    // TX -> XBEE-DIN ( Closest to UNO )
  #define SS_RX_PIN 12                    // RX -> XBEE-DOUT ( Farthest from UNO )
  #define SBUZZ 2                         // Buzzer Signal Pin (S)
#endif                                  // ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
#define BATT_R1_VINTOA1   1500          // Integer Value of BATTVOLT Resistor VIN -> A1 in KOhms
#define BATT_R2_A1TOGND   510           // Integer Value of BATTVOLT Resistor A1 -> GND in KOhms

#if FIRMWARE_IS_FOR==HAND_REMOTE
LiquidCrystal LCD(8, 9, 4, 5, 6, 7);        // Pins used by the LCD Keypad Shield on the Hand-Remote
#elif CONTROLLER_HAS_LCD==1
LiquidCrystal LCD(12, 13, 8, 9, 10, 11);    // Pins used by the LCD Keypad Shield on the Pump-Controller
#endif
SSoftwareSerial IOSerial(SS_RX_PIN,SS_TX_PIN);              // SSoftSerial for XBEE ( rxPin, txPin ) - allows interrupts
PeerIOSerialControl XBee(TRANSCEIVER_ID,IOSerial,Serial);   // XBee(ArduinoID, IOSerial, DebugSerial)
#if XBEECONFIG==0             
PeerRemoteMenu Menu(&XBee, &LCD, SBUZZ);    // Menu initizlization starts interrupts which disturb XBee Config.
#endif

/******************************************************************************************************************//**
 * @brief  Arduino Sketch Setup routine - Initialize the environment.
 * @remarks
 * - Setup() is called once; automatically when Arduino UNO is first powered on or reset.
 * - pin#10 INPUT Backlit shorting see http://forum.arduino.cc/index.php?topic=96747.0
**********************************************************************************************************************/
void setup(){
//--- FORWARD SERIAL TO XBEE for XBEECONFIG --------------------
#if XBEECONFIG!=0
  LCD.clear();LCD.setCursor(0,0);
  LCD.print( "XBEE Config Mode" );
#else
//^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
  pinMode(10, INPUT);           // Fix for Q1-LCD Backlit shorting issue
  pinMode(A1, INPUT);           // A0 Controlled by LCD-Display library.
  pinMode(A2, INPUT_PULLUP);    // A1 is used by the Battery Level Indicator.
  pinMode(A3, INPUT_PULLUP);    // Keep all other Analog pins from floating
  pinMode(A4, INPUT_PULLUP);    // so 'PCINT1_vect' interrupt only triggers
  pinMode(A5, INPUT_PULLUP);    // when analog pin A0 changes

  // Determin if the Buzzer needs I/O pins to drive (+) or (-) supplies
  #ifdef PBUZZ
    pinMode(PBUZZ,OUTPUT);digitalWrite(PBUZZ, HIGH);  // Supply Power to Buzzer (+) if needed
  #endif
  #ifdef GBUZZ
    pinMode(GBUZZ,OUTPUT);digitalWrite(GBUZZ, LOW);   // Supply Ground to Buzzer (-) if needed
  #endif
  pinMode(SBUZZ,OUTPUT);        // Buzzer Signal Pin (S)
  
  pinMode(SS_RX_PIN, INPUT);    // XBee DOUT Pin
  pinMode(SS_TX_PIN, OUTPUT);   // XBee DIN Pin
    
  XBee.Timeout(3000);           // Set the Timeout for XBEE communications
  IOSerial.begin(9600);         // Start UART Communications with the XBee->Module
  Serial.begin(9600);           // Start Serial Monitor for debug
  LCD.begin(16, 2);             // Start the LCD library

//=====================================================================================================================
//------------------------------ SYSTEM / MENU CONFIGURATION SETTINGS -------------------------------------------------
//=====================================================================================================================
  // Name the Devices in the System
  Menu.AddDeviceName( 1, "Hand-Remote");
  Menu.AddDeviceName( 10,"Ditch-Pump");
  Menu.AddDeviceName( 11,"Gate" );

  // StorePin allows Storing a user-set value on a virtual pin so the value can be changed remotely
  MenuItem *battItem, *powerItem, *pressItem, *waterItem;
  //               AddMenuItem(     Text,      Device,  Pin,    Modifier,   Settable, [SetStorePin] );
  battItem  = Menu.AddMenuItem( "Battery(B)",      1,   A1,    BATTVOLTS,   READONLY );
  powerItem = Menu.AddMenuItem( "Power(P)",       10,    7,        ONOFF,   SETTABLE );
  waterItem = Menu.AddMenuItem( "Water(L)",       10,   64,          RAW,     PIDSET,           80 );
  pressItem = Menu.AddMenuItem( "Pressure(R)",    10,   A3,     PRESSURE,   READONLY );
  
  
  //Add(Lo/Hi)Alarm(    *Item,  ID,  Compare, DriveDevice, DrivePin, DriveValue, [ValueStorePin] );
  Menu.AddLoAlarm(   battItem, 'b',     LESS,      BUZZER,   SBUZZ,      1000 );
  Menu.AddLoAlarm(  powerItem, 'p',    EQUAL,      BUZZER,   SBUZZ,      1000 );
  Menu.AddHiAlarm(  powerItem, 'P', NOTEQUAL,      BUZZER,   SBUZZ,      1000 );
  Menu.AddLoAlarm(  waterItem, 'w',     LESS,      BUZZER,   SBUZZ,      1000 );
  Menu.AddHiAlarm(  waterItem, 'W',  GREATER,      BUZZER,   SBUZZ,      1000 );
  Menu.AddLoAlarm(  pressItem, 'r',     LESS,      BUZZER,   SBUZZ,      1000 );
  Menu.AddHiAlarm(  pressItem, 'R',  GREATER,      BUZZER,   SBUZZ,      1000 );

  //AddPIDController(          *Item, DriveDevice, DrivePin,   Kp,   Ki,  Kd, POn)
  Menu.AddPIDController(   waterItem,          11,       A4,  100,  100, 100, P_ON_E);
  
  Menu.SetStartingItem(powerItem);
#endif
}

/******************************************************************************************************************//**
 * @brief  Arduino Sketch Loop() routine
 * @remarks
 * - This function is called automatically over-and-over again by the Arduino
 * - Handles incoming XBee communications
 * - Handles button presses and LCD response updates
 * - Handles Menu iteratation during idle.
**********************************************************************************************************************/
void loop(){

//--- FORWARD SERIAL TO XBEE for XBEECONFIG --------------------
#if XBEECONFIG!=0
  if ( IOSerial.available()>0 ) Serial.write(IOSerial.read());
  if ( Serial.available()>0 ) IOSerial.write(Serial.read());
#else
//^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ 
  Menu.loop();
#endif
}


