#include <Arduino.h>
#include <SoftwareSerial.h>

// Protocols
#define XIAOMI 1
#define XIAOMI_HEADER_BYTE_1 0x55
#define XIAOMI_HEADER_BYTE_2 0xAA
#define NINEBOT 2
#define NINEBOT_HEADER_BYTE_1 0x5A
#define NINEBOT_HEADER_BYTE_2 0xA5

// States
#define READY 0
#define BOOST 1

// Debug modes
#define SNIFFER     -1
#define NONE        0
#define EVENT       1
#define ALL         2

// Destination // Source in comments
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

// Command
// 0x20 BLE>ESC
#define BRAKE   0x65
// 0x21 ESC>BLE
#define SPEED   0x64

// ==========================================================================
// =============================== CONFIG ===================================
// ==========================================================================
//
const float LIMIT              = 1.5;   // What speed increase limit in km/h to activate boost
const int   DURATION[3]        = {1000,3000,5000};  // Duration of Nth boost {1st,2nd,3rd}
const int   THROTTLE_MIN_KMH   = 5;     // What speed to start throttling
const int   THROTTLE_MAX_KMH   = 18;    // What speed to give max throttle (in km/h, we recommend vMax-5)
const int   THROTTLE_IDLE_PCT  = 0;     // Maximum is 10 to disable KERS when not braking above THROTTLE_MIN_KMH on stock firmware
const int   THROTTLE_MIN_PCT   = 30;    // Throttle minimum to set power at or below THROTTLE_MIN_KMH (71 for 350W motor, but we recommend adapting the firmware instead)
const int   THROTTLE_MAX_PCT   = 100;   // Throttle maximum to set power at or below THROTTLE_MIN_KMH (71 for 350W motor, but we recommend adapting the firmware instead)
const int   BRAKE_LIMIT        = 48;    // Limit for disabling throttle when pressing brake pedal (we recommend setting this as low as possible)
const int   THROTTLE_PIN       = 10;    // Pin of programming board (9=D9 or 10=D10)
const int   SERIAL_PIN         = 2;     // Pin of serial input (2=D2)
const int   DEBUG_MODE         = NONE;  // Debug mode (NONE for no logging, EVENT for event logging, ALL for serial data logging)
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
int boostCount = 0;                       // count of boosts after reaching THROTTLE_MIN_KMH
int brakeCount = 0;                       // count of brake moments after reaching THROTTLE_MIN_KMH
float speedIncrCount = 0;                 // count of consequent speed increases
int speedRaw = 0;                         // current raw speed
float speedValid = 0;                     // the validated speed (median+average over last 5 readings)
float speedPrevValid = 0;                 // the previous the validated speed 
int speedReadings[4] = {0};               // the last 4 readings from the speedometer
int index = 0;                            // the index of the current reading
int protocol = NONE;                      // the protocol of last data
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
    uint8_t len = 0x00;
    uint8_t addrSrc = 0x00;
    uint8_t addrDst = 0x00;
    uint16_t sum = 0x0000;
    uint16_t checksum = 0x0000;
    uint8_t curr = 0x00;
    if(DEBUG_MODE==SNIFFER){
        curr = readBlocking();
        if(curr==XIAOMI_HEADER_BYTE_1 || curr==NINEBOT_HEADER_BYTE_1)Serial.println("");
        Serial.print(curr,HEX);
        Serial.print(" ");
    } else {
        switch(protocol) {
            case NONE: // Auto detect protocol
                switch(readBlocking()) {
                    case XIAOMI_HEADER_BYTE_1:
                        if(readBlocking()==XIAOMI_HEADER_BYTE_2){
                            protocol=XIAOMI;
                            if(DEBUG_MODE>=EVENT)Serial.println((String)"DETECTED XIAOMI");
                        }
                        break;
                    case NINEBOT_HEADER_BYTE_1:
                        if(readBlocking()==NINEBOT_HEADER_BYTE_2){
                            protocol=NINEBOT;
                            if(DEBUG_MODE>=EVENT)Serial.println((String)"DETECTED NINEBOT");
                        }
                        break;
                }
                break;
            case XIAOMI:
                while (readBlocking() != XIAOMI_HEADER_BYTE_1); // WAIT FOR BYTE 1
                if (readBlocking() != XIAOMI_HEADER_BYTE_2)return; // IGNORE INVALID BYTE 2
                len = readBlocking(); // BYTE 3 = LENGTH
                if (len<3 || len>8)return; // IGNORE INVALID OR TOO LONG LENGTHS
                buff[0] = len;
                sum = len;
                for (int i = 0; i < len+1; i++) { // BYTE 5+
                    curr = readBlocking();
                    buff[i + 1] = curr; // CHECKSUM: BYTE 3 + 4 + 5+
                    sum += curr;
                }
                if(DEBUG_MODE==ALL)logBuffer(buff,len);
                checksum = (uint16_t) readBlocking() | ((uint16_t) readBlocking() << 8);
                if (checksum != (sum ^ 0xFFFF)){
                    if(DEBUG_MODE==ALL){
                        Serial.println((String)"CHECKSUM FAILED!");
                        Serial.println((String)"CHECKSUM: "+checksum);
                        logBuffer(buff,len);
                    }
                    return;
                }
                if(buff[1]==ESC && buff[2]==BRAKE){
                    if(isBraking&&(buff[6]<BRAKE_LIMIT))brakeCount+=1;
                    isBraking = (buff[6]>=BRAKE_LIMIT);
                    if(DEBUG_MODE>=EVENT)Serial.println((String)"BRAKE: "+buff[6]+" ("+(isBraking?"yes":"no")+")");
                } else if (buff[1]==BLE && buff[2]==SPEED){
                    speedRaw = buff[len];
                    speedValid=averagemidmedian(speedReadings[0],speedReadings[1],speedReadings[2],speedReadings[3],speedRaw);
                    speedReadings[index%4] = speedRaw;
                    index = (index == 3?0:index+1);
                    if(DEBUG_MODE>=EVENT)Serial.println((String)"SPEED: "+buff[len+4]+"kmh");
                    if(state==READY)speedIncrCount = (speedPrevValid>speedValid?0:speedIncrCount+(speedValid-speedPrevValid));
                }
                break;
            case NINEBOT:
                while (readBlocking() != NINEBOT_HEADER_BYTE_1); // WAIT FOR BYTE 1
                if (readBlocking() != NINEBOT_HEADER_BYTE_2)return; // IGNORE INVALID BYTE 2
                len = readBlocking(); // BYTE 3 = LENGTH
                if (len<3 || len>8)return; // IGNORE INVALID OR TOO LONG LENGTHS
                buff[0] = len;
                sum = len;
                for (int i = 0; i < len+4; i++) { // BYTE 5+
                    curr = readBlocking();
                    buff[i + 1] = curr; // CHECKSUM: BYTE 3 + 4 + 5+
                    sum += curr;
                }
                if(DEBUG_MODE==ALL)logBuffer(buff,len);
                checksum = (uint16_t) readBlocking() | ((uint16_t) readBlocking() << 8);
                if (checksum != (sum ^ 0xFFFF)){
                    if(DEBUG_MODE==ALL){
                        Serial.println((String)"CHECKSUM FAILED!");
                        Serial.println((String)"CHECKSUM: "+checksum);
                        logBuffer(buff,len);
                    }
                    return;
                }
                if(buff[2]==ESC && buff[3]==BRAKE){
                      if(isBraking&&(buff[6]<BRAKE_LIMIT))brakeCount+=1;
                      isBraking = (buff[7]>=BRAKE_LIMIT);
                      if(DEBUG_MODE>=EVENT)Serial.println((String)"BRAKE: "+buff[6]+" ("+(isBraking?"yes":"no")+")");
                } else if (buff[2]==BLE && buff[3]==SPEED){
                    speedRaw = buff[len+3];
                    speedValid=averagemidmedian(speedReadings[0],speedReadings[1],speedReadings[2],speedReadings[3],speedRaw);
                    speedReadings[index%4] = speedRaw;
                    index = (index == 3?0:index+1);
                    if(DEBUG_MODE>=EVENT)Serial.println((String)"SPEED: "+buff[len+4]+" ("+speedRaw+"kmh)");
                    if(state==READY)speedIncrCount = (speedPrevValid>speedValid?0:speedIncrCount+(speedValid-speedPrevValid));
                }
        }
        motion_control();
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
    /*Serial.print("DATA CHECKSUM: ");
    Serial.print(sum);
    Serial.print(" (DEC) / ");
    Serial.print(sum^0xFFFF);
    Serial.println(" (XOR)");*/
}

// Calculate median of 5 values by swapping them in a Bose-Nelson Algorithm (very light function!)
float averagemidmedian( int a0, int a1, int a2, int a3, int a4 ) {
    swap(&a0, &a1); 
    swap(&a3, &a4);
    swap(&a2, &a4);
    swap(&a2, &a3); 
    swap(&a0, &a3);
    swap(&a0, &a2);
    swap(&a1, &a4);
    swap(&a1, &a3);
    swap(&a1, &a2);
    return a1*0.25+a2*0.5+a3*0.25;
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

int getDuration(int count){
    int maxsize = sizeof(DURATION)/sizeof(int)-1;
    if(count>=maxsize){
        return DURATION[maxsize];
    }
    return DURATION[(count-1)];
}

float getPower(int spd){ // in full pct
    if(spd<=THROTTLE_MIN_KMH) return THROTTLE_MIN_PCT;
    //if(spd>=THROTTLE_MAX_KMH) return THROTTLE_MAX_PCT;
    if(spd>=THROTTLE_MAX_KMH) return (brakeCount>9?100:THROTTLE_MAX_PCT);
    return THROTTLE_MIN_PCT+(spd-THROTTLE_MIN_KMH)/(float)THROTTLE_DIFF_KMH*THROTTLE_DIFF_PCT;
}

int stopThrottle(bool braking){
    setThrottleIdle(braking?0:THROTTLE_IDLE_PCT);
    state = READY;
    speedIncrCount=0;
    speedReadings[0]=speedRaw; speedReadings[1]=speedRaw; speedReadings[2]=speedRaw; speedReadings[3]=speedRaw;
    if(speedValid>=THROTTLE_MIN_KMH && DEBUG_MODE>=EVENT)Serial.println((String)"READY");
}

void motion_control() {
    if (speedRaw < THROTTLE_MIN_KMH && brakeCount<9)brakeCount=0;
    if (speedRaw < THROTTLE_MIN_KMH || isBraking) {
        boostCount = 0;
        stopThrottle(true); // Stop throttling, regenerative braking
    } else {
        // Check if new boost needed
        if(DEBUG_MODE>=EVENT)Serial.println(state==BOOST?(String)"BOOSTING...":(String)"CHECKING: "+(speedIncrCount)+" > "+LIMIT+": "+(speedIncrCount>LIMIT?"BOOST ACTIVATED!":"NO BOOST"));
        if (speedIncrCount > LIMIT) { // If kick detected 
            speedIncrCount=0;
            boostCount += 1;
            state = BOOST; // Activate boost
            startBoost = millis();
        }
        // Check for existing boost
        if (state == BOOST){
            // Calculate duration and throttle
            float power = getPower(speedRaw);
            int timeElapsed = (millis()-startBoost);
            int timeDuration = getDuration(boostCount);
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
    speedPrevValid = speedValid;
}

int setThrottleIdle(int offset){
    analogWrite(THROTTLE_PIN, 45+offset*1.88); // Percentage in whole numbers: 0-100, results in a value of 45-233
}

int setThrottlePercentage(float percentageThrottle) {
    analogWrite(THROTTLE_PIN, 45+THROTTLE_MIN_PCT*1.88+percentageThrottle*THROTTLE_DIFF_PCT/THROTTLE_MAX_PCT*1.88); // Percentage in whole numbers: 0-100, results in a value of 45-233
}
