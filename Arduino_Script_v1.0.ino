#include <Arduino.h>
#include <SoftwareSerial.h>

// States
#define READY 0
#define BOOST 1

// Debug modes
#define NONE  0
#define EVENT 1
#define ALL   2

// Protocol decoding
#define DASHB2MOTOR 0x20
  #define BRAKEVALUE   0x65
  #define MILEAGEVALUE 0x64
#define MOTOR2DASHB 0x21
  #define SPEEDVALUE   0x64
#define DASHB2BATTR 0x22
#define BATTR2DASHB 0x23
#define MOTOR2BATTR 0x24
#define BATTR2MOTOR 0x25

// ==========================================================================
// =============================== CONFIG ===================================
// ==========================================================================
//
const int   DURATION         = 5000;  // Throttle duration (in millisec)
const float SENSITIVITY      = 2;     // Sensitivity for detecting new kicks (in average km/h difference)
const int   READINGS_COUNT   = 10;    // Amount of speed readings
const int   THROTTLE_MAX_KMH = 20;    // What speed to give max throttle (in km/h)
const int   THROTTLE_MIN_PCT = 0;     // What percentage of throttle is no throttle (can be used to disable KERS for Essential)
const int   BRAKE_LIMIT      = 35;    // Limit for disabling throttle when pressing brake pedal
const int   THROTTLE_PIN     = 10;    // Pin of programming board (9=D9 or 10=D10)
const int   SERIAL_PIN       = 2;     // Pin of serial input (2=D2)
const int   DEBUG_MODE       = NONE;  // Debug mode
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

SoftwareSerial SoftSerial(SERIAL_PIN, 3); // RX, TX

// Variables
unsigned long boostTime = 0;              // time of last boost
unsigned long brakeTime = 0;              // time of last boost
bool isBraking = true;                    // brake activated
int speedCurrent = 0;                     // current speed
float speedAverage = 0;                   // the average speed over last X readings
float speedLastAverage = 0;               // the average speed over last X readings in the last loop
int speedReadings[READINGS_COUNT] = {0};  // the readings from the speedometer
int index = 0;                            // the index of the current reading
int speedReadingsSum = 0;                 // the sum of all speed readings
uint8_t state = READY;                    // current state

uint8_t readBlocking() {
    while (!SoftSerial.available())
    delay(1);
    return SoftSerial.read();
}

void setup() {
    Serial.begin(115200); SoftSerial.begin(115200); // Start reading SERIAL_PIN at 115200 baud
    TCCR1B = TCCR1B & 0b11111001; // TCCR1B = TIMER 1 (D9 and D10 on Nano) to 32 khz
    setThrottle(0); // Set throtte to 0%
}

uint8_t buff[256];

void loop() {
    while (readBlocking() != 0x55);
    if (readBlocking() != 0xAA)return;
    uint8_t len = readBlocking();
    buff[0] = len;
    if (len > 8)return; // s
    uint8_t addr = readBlocking();
    buff[1] = addr;
    uint16_t sum = len + addr;
    for (int i = 0; i < len; i++) {
        uint8_t curr = readBlocking();
        buff[i + 2] = curr;
        sum += curr;
    }
    if(DEBUG_MODE==ALL){ // For debugging the serial data
        Serial.print("DATA: ");
        for (int i = 0; i <= len; i++) {
          Serial.print(buff[i],HEX);
          Serial.print(" ");
        }
        Serial.print("  (HEX) / ");
        for (int i = 0; i <= len; i++) {
          Serial.print(buff[i]);
          Serial.print(" ");
        }
        Serial.println("  (DEC)");
    }
    uint16_t checksum = (uint16_t) readBlocking() | ((uint16_t) readBlocking() << 8);
    if (checksum != (sum ^ 0xFFFF))return;  
    if(buff[1]==DASHB2MOTOR){
        if(buff[2]==BRAKEVALUE){
            isBraking = (buff[6]>=BRAKE_LIMIT);
            if(isBraking)debug((String)"BRAKE: "+buff[6]+" ("+(isBraking?"yes":"no")+")",EVENT);
        }
    } else if(buff[1]==MOTOR2DASHB){
        if(buff[2]==SPEEDVALUE){
            speedCurrent = buff[len];
            speedReadingsSum = speedReadingsSum - speedReadings[index];
            speedReadings[index] = speedCurrent;
            speedReadingsSum = speedReadingsSum + speedCurrent;
            index++;
            if(index >= READINGS_COUNT)index = 0;
            speedAverage = speedReadingsSum / (float)READINGS_COUNT;
            if(speedCurrent>0||speedAverage>0)debug((String)"SPEED: "+speedCurrent+" (Average: "+speedAverage+")",EVENT);
        }
    }
    motion_control();
}

bool debug(String text,int mode){
    if(DEBUG_MODE>=mode){
        Serial.println(text);
    }
    return false;
}

void motion_control() {
    if (speedCurrent < 5){
        setThrottle(0);
        state = READY;
    } else if (isBraking) {
        setThrottle(0);
        state = READY;
        brakeTime = millis(); 
    } else {
        if (state == READY && speedAverage-speedLastAverage > (SENSITIVITY/(float)READINGS_COUNT) && (millis()-brakeTime)>50*READINGS_COUNT) { // If ready, kick detected and no recent braking, activate boost mode
            state = BOOST;
            boostTime = millis();
            debug((String)"BOOST: Kick detected (+"+(speedAverage-speedLastAverage)+" km/h > "+(SENSITIVITY/(float)READINGS_COUNT)+")",EVENT);
        }
        if (state == BOOST){ // In boost mode, activate throttle depending on average speed and duration
            int power = (speedCurrent>THROTTLE_MAX_KMH?THROTTLE_MAX_KMH:speedCurrent)*(100/THROTTLE_MAX_KMH);
            float elapsedTime = (millis()-boostTime) / (float)1000;
            float percentageThrottle = power-power*pow(power,elapsedTime-DURATION/(float)1000); // Quadratic formula
            if(percentageThrottle>10){ 
                setThrottle(percentageThrottle);
                debug((String)"THROTTLE: "+percentageThrottle+"% ("+power+" @ "+elapsedTime+"s)",EVENT);
            } else { // End of boost
                state = READY;
            }
        } else {
            setThrottle(0); // Default throttle
        }
    }
    speedLastAverage = speedAverage;
}

int setThrottle(int percentageThrottle) {
    analogWrite(THROTTLE_PIN, 45+THROTTLE_MIN_PCT*1.88+percentageThrottle*(100-THROTTLE_MIN_PCT)/100*1.88); // Percentage in whole numbers: 0-100, results in a value of 45-233
}
