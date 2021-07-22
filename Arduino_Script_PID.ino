#include <Arduino.h>
#include <SoftwareSerial.h>
#include <PID_v1.h>

// Debug modes
#define SNIFFER     -1
#define NONE        0
#define EVENT       1
#define ALL         2

// ==========================================================================
// ========================= SERIAL PROTOCOL ================================
// ==========================================================================
// Header: BYTE 1 & 2
#define XIAOMI_H1 0x55
#define XIAOMI_H2 0xAA
#define NINEBOT_H1 0x5A
#define NINEBOT_H2 0xA5
// Destination: BYTE 3 (XIAOMI) / Source: BYTE 3 & Destination: BYTE 4 (NINEBOT)
#define BROADCAST_TO_ALL  0x00
#define ESC               0x20 // Xiaomi: From BLE / Ninebot: From any
#define BLE               0x21 // Xiaomi: From ESC / Ninebot: From any
#define BMS               0x22 // Xiaomi: From BLE / Ninebot: From any
#define BLE_FROM_BMS      0x23 // Xiaomi only
#define BMS_FROM_ESC      0x24 // Xiaomi only
#define ESC_FROM_BMS      0x25 // Xiaomi only
#define WIRED_SERIAL      0x3D // Ninebot only
#define BLUETOOTH_SERIAL  0x3E // Ninebot only
#define UNKNOWN_SERIAL    0x3F // Ninebot only
// Command: BYTE 3 (XIAOMI) / BYTE 4 (NINEBOT)
#define BRAKE   0x65// ONLY AT DESTINATION 0x20 BLE>ESC
#define SPEED   0x64 // ONLY AT DESTINATION 0x21 ESC>BLE

// ==========================================================================
// =============================== CONFIG ===================================
// ==========================================================================
//
// Debug mode
const int   DEBUG_MODE         = NONE;  // Debug mode (NONE for no logging, EVENT for event logging, ALL for serial data logging)
// Kick detection
const float KICK_DETECT_LOW    = 2;   // What speed increase (speed above current speed) to detect as kick at or below 18 kmh
const float KICK_DETECT_HIGH   = 1.5; // What speed increase (speed above current speed) to detect as kick above 18 kmh
const int   KICK_DURATION[4]   = {2000,3000,4000,5000};  // Duration of boost {1st,2nd,3rd,4th,5th kick during boost and more} (boost is reset by braking or >5s no boost)
// Speed controller (PID)
const int   SPEED_READINGS     = 6;   // Amount of speed readings (increases steadiness but decreases reaction speed)
const float SPEED_PROPORTION   = 20;   // Power of increase
const float SPEED_INTEGRAL     = 5;   // Time to overcome steady state errors (difference between speed and target target)
const float SPEED_DISTURBANCE  = 1.2; // Time to remove overshoot / oscillations
const int   SPEED_INCREASE_LOW = 0;   // Speed increase beyond target at or below 18 kmh
const int   SPEED_INCREASE_HIGH = 0;  // Speed increase beyond target above 18 kmh
// Throttle and brake
const int   THROTTLE_OFF_PCT   = 0;     // Percentage of throttle when rolling out, 0 with KERS disabled, 10 with KERS enabled
const int   BRAKE_LIMIT        = 48;    // Limit for disabling throttle when pressing brake pedal (we recommend setting this as low as possible)
const int   THROTTLE_PIN       = 10;    // Pin of programming board (9=D9 or 10=D10)
const int   SERIAL_PIN         = 2;     // Pin of serial input (2=D2)
//
// ==========================================================================
// ============================= DISCLAIMER =================================
// ==========================================================================
//
// THIS SCRIPT, INSTRUCTIONS, INFORMATION AND OTHER SERVICES ARE PROVIDED BY THE DEVELOPER ON AN "AS IS" AND "AS AVAILLABLE" BASIS, UNLESS 
// OTHERWISE SPECIFIED IN WRITING. THE DEVELOPER DOES NOT MAKE ANY REPRESENTATIONS OR WARRANTIES OF ANY KIND, EXPRESS OR IMPLIED, AS TO THIS 
// SCRIPT, INSTRUCTIONS, INFORMATION AND OTHER SERVICES. YOU EXPRESSLY AGREE THAT YOUR USE OF THIS SCRIPT IS AT YOUR OWN RISK. 
//
// TO THE FULL EXTEND PERMISSABLE BY LAW, THE DEVELOPER DISCLAIMS ALL WARRANTIES, EXPRESS OR IMPLIED. TO THE FULL EXTEND PERMISSABLE BY LAW, 
// THE DEVELOPER WILL NOT BE LIABLE FOR ANY DAMAGES OF ANY KIND ARISING FROM THE USE OF THIS SCRIPT, INSTRUCTIONS, INFORMATION AND OTHER SERVICES,
// INCLUDING, BUT NOT LIMITED TO DIRECT, INDIRECT, INCIDENTAL, PUNITIVE AND CONSEQUENTIAL DAMAGES, UNLESS OTHERWISE SPECIFIED IN WRITING.
//
// ==========================================================================

// CODE BELOW THIS POINT


// Variables
int boostCount = 0;                       // count of boosts after reaching THROTTLE_MIN_KMH
unsigned long boostTime = -99999;          // start time of last boost
int speedRaw=0;                           // current raw speed
double speedCurrent=0;                    // current raw speed
double speedTarget=0;                     // current target speed
int speedReadings[SPEED_READINGS] = {0};  // the last 4 readings from the speedometer
int speedIndex = 0;                       // the index of the current reading
double throttleOut=45;                    // the current throttle position
bool isBraking = true;                    // brake activated

// Protocol variables
uint8_t h1 = 0x00;
uint8_t h2 = 0x00;
int destIndex = 1;
int cmdIndex = 2;
int lenOffset = 1;
SoftwareSerial SoftSerial(SERIAL_PIN, 3); // RX, TX
PID throttlePID(&speedCurrent, &throttleOut, &speedTarget,SPEED_PROPORTION,SPEED_INTEGRAL,SPEED_DISTURBANCE, DIRECT);

uint8_t readByte() {
    while (!SoftSerial.available())
    delay(1);
    return SoftSerial.read();
}

void setup() {
    Serial.begin(115200); SoftSerial.begin(115200); // Start reading SERIAL_PIN at 115200 baud
    TCCR1B = TCCR1B & 0b11111001; // TCCR1B = TIMER 1 (D9 and D10 on Nano) to 32 khz
    throttlePID.SetOutputLimits(45, 233);
    throttlePID.SetMode(AUTOMATIC);
    throttlePID.SetSampleTime(50);
}

uint8_t buff[256];

void loop() {    
    if(DEBUG_MODE==SNIFFER){
        uint8_t curr = readByte();
        if(curr==XIAOMI_H1 || curr==NINEBOT_H1)Serial.println("");
        Serial.print(curr,HEX);
        Serial.print(" ");
    } else {
        if(h1==0x00){
            switch(readByte()) {
                case XIAOMI_H1:
                    if(readByte()==XIAOMI_H2){
                        h1=XIAOMI_H1;
                        h2=XIAOMI_H2;
                        if(DEBUG_MODE>=EVENT)Serial.println("DETECTED XIAOMI");
                    }
                    break;
                case NINEBOT_H1:
                    if(readByte()==NINEBOT_H2){
                        h1=NINEBOT_H1;
                        h2=NINEBOT_H2;
                        lenOffset=4;
                        destIndex = 1;
                        cmdIndex = 1;
                        if(DEBUG_MODE>=EVENT)Serial.println("DETECTED NINEBOT");
                    }
                    break;
                // we can support more protocols on the future
            }
        } else {
            while (readByte() != h1); // WAIT FOR BYTE 1
            if (readByte() != h2)return; // STOP WHEN INVALID BYTE 2
            uint8_t len = readByte(); // BYTE 3 = LENGTH
            if (len<3 || len>8)return; // STOP ON INVALID OR TOO LONG LENGTHS
            buff[0] = len;
            uint8_t curr = 0x00;
            uint16_t sum = len;
            for (int i = 0; i < len+lenOffset; i++) { // BYTE 5+
                curr = readByte();
                buff[i + 1] = curr;
                sum += curr;
            }
            if(DEBUG_MODE==ALL)logBuffer(buff,len);
            uint16_t checksum = (uint16_t) readByte() | ((uint16_t) readByte() << 8); // LAST 2 BYTES IS CHECKSUM
            if (checksum != (sum ^ 0xFFFF)){ // CHECK XOR OF SUM AGAINST CHECKSUM
                if(DEBUG_MODE==ALL)Serial.println((String)"CHECKSUM: "+checksum+" == "+(sum ^ 0xFFFF)+" >> FAILED!");
                return; // STOP ON INVALID CHECKSUM
            }
            // When braking
            if(buff[destIndex]==ESC && buff[cmdIndex]==BRAKE){
                // Xiaomi
                // h1 h2 0 1  2  3 4 5 6  7 8  c1 c2
                // 55 AA 7 20 65 0 4 0 22 0 80 4C FF
                // Ninebot
                // h1 h2 0 1  2  3  4 5 6  7  8 9 c1 c2
                // 5A A5 5 21 20 65 0 4 29 29 0 0 FE FE
                isBraking = (buff[len+lenOffset-2]>=BRAKE_LIMIT);if(DEBUG_MODE>=EVENT && isBraking)Serial.println((String)"BRAKE: "+buff[len+lenOffset-2]+" ("+(isBraking?"yes":"no")+")");
                if(isBraking){              
                    boostTime=-99999;
                    boostCount = 0;
                }
            // Each speed reading
            } else if (buff[destIndex]==BLE && buff[2]==SPEED){
                // Xiaomi
                // h1 h2 l d  2  3 4 5  6 7 8 9 c1 c2
                // 55 AA 8 21 64 0 4 64 0 0 0 E FC FE 
                // Ninebot
                // h1 h2 l s  d  c  4 5 6  7 8 9 10 c1 c2
                // 5A A5 6 20 21 64 0 4 4E 0 0 0 7  F4 FE
                speedRaw = buff[len+lenOffset-1];
                speedReadings[speedIndex]=speedRaw;
                speedCurrent = avgReading();
                speedIndex = (speedIndex>=SPEED_READINGS?0:speedIndex+1);
                // Detect boost
                float diff = maxDifference(speedCurrent);
                if(speedRaw>7 && diff>=(speedRaw<18?KICK_DETECT_LOW:KICK_DETECT_HIGH) && (diff+speedCurrent)>speedTarget){ // Detect kick
                    Serial.println((String)"BOOST");
                    boostTime = millis();
                    boostCount +=1;
                }
                if(millis()-boostTime<150){ // After kick, detect max kick speed
                    float m = maxReading();
                    m = m + (m<18?SPEED_INCREASE_LOW:SPEED_INCREASE_HIGH);
                    speedTarget = (speedTarget<m?m:speedTarget);
                }
                // Calculate duration
                int arrLen = sizeof(KICK_DURATION)/sizeof(int)-1;
                int duration = KICK_DURATION[((boostCount-1)>arrLen?arrLen:(boostCount-1))];
                if((millis()-boostTime)>duration || speedRaw<8)speedTarget = 0; // Stop throttling if boost ended or speed < 8 kmh
                if((millis()-boostTime)>(duration+5000))boostCount=0; // Reset boost count if last boost ended over 5s ago

                throttlePID.Compute(); // Calculate throttle with PID Controller
                if(DEBUG_MODE>=EVENT && speedCurrent>0)Serial.println((String)"PID: "+speedCurrent+"("+speedRaw+") "+throttleOut+" "+speedTarget);
            }
            analogWrite(THROTTLE_PIN, (int)throttleOut);
        }
    }
}

void logBuffer(uint8_t buff[256],int len){
    uint16_t sum = 0x00;
    Serial.print("DATA: ");
    for (int i = 0; i <= len; i++) {
      Serial.print(buff[i],HEX);
      Serial.print(" ");
    }
    Serial.print("  (HEX) / ");
    for (int i = 0; i <= len; i++) {
      Serial.print(buff[i]);
      sum += buff[i];
      Serial.print(" ");
    }
    Serial.println("  (DEC)");
}
float avgReading() {
  int s = 0;
  for(int i = 0; i<SPEED_READINGS;i++){
      s += speedReadings[i];
  }
  return s/(float)SPEED_READINGS;
}
int maxReading() {
  int m = 0;
  for(int i = 0; i<SPEED_READINGS;i++){
      m = (speedReadings[i]>m?speedReadings[i]:m);
  }
  return m;
}
float maxDifference(float compare) {
  int d = 0;
  for(int i = 0; i<SPEED_READINGS;i++){
      d = (speedReadings[i]-compare>d?speedReadings[i]-compare:d);
  }
  return d;
}
