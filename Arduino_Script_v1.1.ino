#include <Arduino.h>
#include <SoftwareSerial.h>

// States
#define READY 0
#define BOOST 1

// Debug modes
#define SERIAL_ONLY -1
#define NONE        0
#define EVENT       1
#define ALL         2

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
const int   DURATION_MIN       = 1000;  // Throttle duration at min throttle (in millisec)
const int   DURATION_MAX       = 4000;  // Throttle duration at max throttle (in millisec)
const float SENSITIVITY        = 0.2;   // Sensitivity for detecting new kicks (in average km/h difference)
const int   READINGS_COUNT     = 10;    // Amount of speed readings (more readings will make kick detection slower but more accurate)
const int   THROTTLE_BOOST     = 1;     // Increase throttle during boost (improves acceleration)
const int   THROTTLE_MIN_KMH   = 5;     // What speed to start throttling
const int   THROTTLE_MAX_KMH   = 20;    // What speed to give max throttle (in km/h, we recommend vMax-5)
const int   THROTTLE_MIN_PCT   = 0;     // Maximum is 10 to disable KERS above THROTTLE_MIN_KMH on stock firmware
const int   THROTTLE_MAX_PCT   = 100;   // Limit throttle maximum to reduce power (71 for 350W motor, but we recommend adapting the firmware instead)
const int   BRAKE_LIMIT        = 48;    // Limit for disabling throttle when pressing brake pedal (we recommend setting this as low as possible)
const int   THROTTLE_PIN       = 10;    // Pin of programming board (9=D9 or 10=D10)
const int   SERIAL_PIN         = 2;     // Pin of serial input (2=D2)
const int   DEBUG_MODE         = EVENT;  // Debug mode (NONE for no logging, EVENT for event logging, ALL for serial data logging)
const int   DURATION_DIFF      = DURATION_MAX-DURATION_MIN;
const int   THROTTLE_DIFF_KMH  = THROTTLE_MAX_KMH-THROTTLE_MIN_KMH;
const int   THROTTLE_DIFF_PCT  = THROTTLE_MAX_PCT-THROTTLE_MIN_PCT;
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
unsigned long startTime = 0;              // start time of last boost
unsigned long stopTime = 0;               // end time of last boost
bool isBraking = true;                    // brake activated
int speedCurrent = 0;                     // current speed
int speedBoost = 0;                       // speed at start of boost
float speedAverage = 0;                   // the average speed over last X readings
float speedLastAverage = 0;               // the average speed over last X readings in the last loop
int speedReadings[READINGS_COUNT] = {0};  // the readings from the speedometer
int index = 0;                            // the index of the current reading
int speedReadingsSum = 0;                 // the sum of all speed readings
int speedReadingsStart = 0;               // the start of last reading block
int speedReadingsInt = 1000;              // the duration of a readings block
uint8_t state = READY;                    // current state

uint8_t readBlocking() {
    while (!SoftSerial.available())
    delay(1);
    return SoftSerial.read();
}

void setup() {
    Serial.begin(115200); SoftSerial.begin(115200); // Start reading SERIAL_PIN at 115200 baud
    TCCR1B = TCCR1B & 0b11111001; // TCCR1B = TIMER 1 (D9 and D10 on Nano) to 32 khz
    speedReadingsStart = millis();
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
    if(DEBUG_MODE==SERIAL_ONLY || DEBUG_MODE ==ALL){ // For debugging the serial data
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
            if(index >= READINGS_COUNT){
              index = 0;
              speedReadingsInt = millis()-speedReadingsStart;
              speedReadingsStart = millis();
            }
            speedAverage = speedReadingsSum / (float)READINGS_COUNT;
            if(speedCurrent>0||speedAverage>0)debug((String)"SPEED: "+speedCurrent+" (Average: "+speedAverage+")",EVENT);
        }
    }
    motion_control();
}

float getThrottle(float pwr,float tme){
    return pwr-pwr*pow(pwr,tme);
}

int getDuration(int spd){ // in ms
    if(spd<=THROTTLE_MIN_KMH) return DURATION_MIN;
    if(spd>=THROTTLE_MAX_KMH) return DURATION_MAX;
    return DURATION_MIN+(spd-THROTTLE_MIN_KMH)/(float)THROTTLE_DIFF_KMH*DURATION_DIFF;
}

float getPower(int spd){ // in full pct
    if(spd<=THROTTLE_MIN_KMH) return THROTTLE_MIN_PCT;
    if(spd>=THROTTLE_MAX_KMH) return THROTTLE_MAX_PCT;
    return THROTTLE_MIN_PCT+(spd-THROTTLE_MIN_KMH)/(float)THROTTLE_DIFF_KMH*THROTTLE_DIFF_PCT;
}

bool debug(String text,int mode){
    if(DEBUG_MODE>=mode){
        Serial.println(text);
    }
    return false;
}

void motion_control() {
    if (speedCurrent < THROTTLE_MIN_KMH || isBraking) {
        stopThrottle();
        stopTime = millis();
        state = READY;
    } else {
        // Check if new boost needed
        if (state != BOOST && (speedAverage-speedLastAverage) > SENSITIVITY && (millis()-stopTime)>speedReadingsInt/0.5) { // If not boosting, kick detected and no false-positive reading
            state = BOOST;
            startTime = millis();
            speedBoost = speedCurrent;
            debug((String)"BOOST: "+startTime,EVENT);
            debug((String)"BOOST: Kick detected (+"+(speedAverage-speedLastAverage)+" km/h > "+SENSITIVITY+")",EVENT);
            debug((String)"BOOST: Last stop: "+(millis()-stopTime)+" ms ago > "+(speedReadingsInt/0.5)+"",EVENT);
        }
        // Check for existing boost
        if (state == BOOST){
            // Calculate duration and throttle
            float power = getPower(THROTTLE_BOOST==1?speedCurrent:speedBoost);
            int timeElapsed = (millis()-startTime);
            int timeDuration = getDuration(speedBoost);
            float timeToGo = (timeElapsed-timeDuration)/(float)1000;
            float throttlePercentage = getThrottle(power,timeToGo);
            debug((String)"THROTTLE: "+throttlePercentage+"% ("+power+" @ "+-timeToGo+"s)",EVENT);
            if(throttlePercentage>10){ 
                setThrottle(throttlePercentage);
            } else { // End of boost
                stopTime = millis()-200;
                debug((String)"READY: "+stopTime,EVENT);
                state = READY;
            }
        // No current boost, above 5kmh and not braking
        } else {
            setThrottle(0); // Default throttle
        }
    }
    speedLastAverage = speedAverage;
}

int stopThrottle(){
    analogWrite(THROTTLE_PIN, 45); // Percentage in whole numbers: 0-100, results in a value of 45-233
}

int setThrottle(int percentageThrottle) {
    analogWrite(THROTTLE_PIN, 45+THROTTLE_MIN_PCT*1.88+percentageThrottle*THROTTLE_DIFF_PCT/THROTTLE_MAX_PCT*1.88); // Percentage in whole numbers: 0-100, results in a value of 45-233
}
