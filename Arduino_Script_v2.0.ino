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
//#define DASHB2BATTR 0x22
//#define BATTR2DASHB 0x23
//#define MOTOR2BATTR 0x24
//#define BATTR2MOTOR 0x25

// ==========================================================================
// =============================== CONFIG ===================================
// ==========================================================================
//
const int   DURATION_MIN       = 1000;  // Throttle duration at min throttle (in millisec)
const int   DURATION_MAX       = 5000;  // Throttle duration at max throttle (in millisec)
const int   THROTTLE_MIN_KMH   = 5;     // What speed to start throttling
const int   THROTTLE_MAX_KMH   = 18;    // What speed to give max throttle (in km/h, we recommend vMax-5)
const int   THROTTLE_IDLE_PCT  = 0;     // Maximum is 10 to disable KERS when not braking above THROTTLE_MIN_KMH on stock firmware
const int   THROTTLE_MIN_PCT   = 30;    // Throttle minimum to set power at or below THROTTLE_MIN_KMH (71 for 350W motor, but we recommend adapting the firmware instead)
const int   THROTTLE_MAX_PCT   = 100;   // Throttle maximum to set power at or below THROTTLE_MIN_KMH (71 for 350W motor, but we recommend adapting the firmware instead)
const int   BRAKE_LIMIT        = 48;    // Limit for disabling throttle when pressing brake pedal (we recommend setting this as low as possible)
const int   THROTTLE_PIN       = 10;    // Pin of programming board (9=D9 or 10=D10)
const int   SERIAL_PIN         = 2;     // Pin of serial input (2=D2)
const int   DEBUG_MODE         = NONE;  // Debug mode (NONE for no logging, EVENT for event logging, ALL for serial data logging)
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
bool isBraking = true;                    // brake activated
unsigned long startBoost = 0;             // start time of last boost
int speedIncreased = 0;                   // count of consequent speed increases, is reset by boost or decrease
int speedCurrent = 0;                     // current speed
int speedMedian = 0;                      // the median speed over last 5 readings
int speedLastMedian = 0;                  // the previous median speed over last 5 readings
int speedReadings[4] = {0};               // the last 4 readings from the speedometer
int index = 0;                            // the index of the current reading
uint8_t state = READY;                    // current state

uint8_t readBlocking() {
    while (!SoftSerial.available())
    delay(1);
    return SoftSerial.read();
}

void setup() {
    Serial.begin(115200); SoftSerial.begin(115200); // Start reading SERIAL_PIN at 115200 baud
    TCCR1B = TCCR1B & 0b11111001; // TCCR1B = TIMER 1 (D9 and D10 on Nano) to 32 khz
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
            if(isBraking && DEBUG_MODE>=EVENT)Serial.println((String)"BRAKE: "+buff[6]+" ("+(isBraking?"yes":"no")+")");
        }
    } else if(buff[1]==MOTOR2DASHB){
        if(buff[2]==SPEEDVALUE){
            speedCurrent = buff[len];
            speedMedian=median5(speedReadings[0],speedReadings[1],speedReadings[2],speedReadings[3],speedCurrent);
            speedReadings[index%4] = speedCurrent;
            index = (index == 3?0:index+1);
            if(state==READY)speedIncreased = (speedLastMedian>speedMedian?0:speedIncreased+(speedMedian-speedLastMedian));
        }
    }
    motion_control();
}

// Calculate median of 5 values by swapping them in a Bose-Nelson Algorithm (very light function!)
int median5( int a0, int a1, int a2, int a3, int a4 ) {
    swap(&a0, &a1); 
    swap(&a3, &a4);
    swap(&a2, &a4);
    swap(&a2, &a3); 
    swap(&a0, &a3);
    swap(&a0, &a2);
    swap(&a1, &a4);
    swap(&a1, &a3);
    swap(&a1, &a2);
  
    return a2;
}

// Swap values if first is bigger than second
void swap(int *j,  int *k) {
    double x = *j;
    double y = *k;
    if (*j > *k) {
        *k = x;
        *j = y;
    }
}

float getThrottle(float pwr,float tme){
    return pwr-pwr*pow(pwr,0.5*tme);
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

int stopThrottle(bool braking){
    setThrottleIdle(braking?0:THROTTLE_IDLE_PCT);
    state = READY;
    // Reset measurements to prevent false kick detection
    speedIncreased=0;
    speedReadings[0]=speedCurrent+2;speedReadings[1]=speedCurrent+2;speedReadings[2]=speedCurrent+2;speedReadings[3]=speedCurrent+2;
    if(speedMedian>=THROTTLE_MIN_KMH && DEBUG_MODE>=EVENT)Serial.println((String)"READY");
}

void motion_control() {
    if (speedMedian < THROTTLE_MIN_KMH || isBraking) {
        stopThrottle(true); // Stop throttling, regenerative braking
    } else {
        // Check if new boost needed
        if(DEBUG_MODE>=EVENT)Serial.println(state==BOOST?(String)"BOOSTING...":(String)"CHECKING: "+(speedIncreased)+" > 1: "+(speedIncreased>1?"BOOST ACTIVATED!":"NO BOOST"));
        if (speedIncreased > 1) { // If kick detected 
            speedIncreased=0;
            state = BOOST; // Activate boost
            startBoost = millis();
        }
        // Check for existing boost
        if (state == BOOST){
            // Calculate duration and throttle
            float power = getPower(speedCurrent);
            int timeElapsed = (millis()-startBoost);
            int timeDuration = getDuration(speedMedian);
            float timeRemaining = (timeElapsed-timeDuration)/(float)1000;
            float throttlePercentage = getThrottle(power,timeRemaining);
            if(throttlePercentage>10){ 
                setThrottlePercentage(throttlePercentage);
                if(DEBUG_MODE>=EVENT)Serial.println((String)"THROTTLE: "+throttlePercentage+"% ("+power+" @ "+-timeRemaining+"s)");
            } else { // End of boost
                stopThrottle(false); // Idle throttle, no regenerative braking
            }
        // No current boost, above 5kmh and not braking
        } else {
            stopThrottle(false); // Idle throttle, no regenerative braking
        }
    }
    speedLastMedian = speedMedian;
}

int setThrottleIdle(int offset){
    analogWrite(THROTTLE_PIN, 45+offset*1.88); // Percentage in whole numbers: 0-100, results in a value of 45-233
}

int setThrottlePercentage(float percentageThrottle) {
    analogWrite(THROTTLE_PIN, 45+THROTTLE_MIN_PCT*1.88+percentageThrottle*THROTTLE_DIFF_PCT/THROTTLE_MAX_PCT*1.88); // Percentage in whole numbers: 0-100, results in a value of 45-233
}
