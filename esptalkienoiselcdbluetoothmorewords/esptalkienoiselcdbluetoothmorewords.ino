#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "BluetoothA2DPSource.h"
#include "TalkiePCM.h"
#include "Vocab_US_Large.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// --- HARDWARE PINS ---
#define TRIG_PIN  12  
#define ECHO_PIN  13  

// --- LCD SETUP ---
LiquidCrystal_I2C lcd(0x27, 16, 2);

// --- AUDIO & SYNTH SETUP ---
BluetoothA2DPSource a2dp_source;
TalkiePCM voice(Serial);

// Microtonal generative scale
float noise_matrix[] = {220.00, 277.18, 311.13, 369.99, 440.00, 554.37}; 
const int matrix_size = 6;

volatile int distance_cm = 100;
float phase = -1.0; 

unsigned long last_arp_step_time = 0;
unsigned long last_lcd_update_time = 0;
int current_matrix_index = 0;
volatile float frequency_tension = 1.0;

// --- Speech state ---
enum SpeechState { IDLE, SAY_COLLAPSE, SAY_TENSION };
SpeechState speechState = IDLE;

// --- Ring buffer for speech PCM ---
#define SPEECH_BUFFER_SIZE 2048
int16_t speechBuffer[SPEECH_BUFFER_SIZE];
volatile int speechWriteIndex = 0;
volatile int speechReadIndex = 0;
volatile bool speechActive = false;

portMUX_TYPE bufferMux = portMUX_INITIALIZER_UNLOCKED;

// --- LCD message system ---
const char* lcdMessage = "";
unsigned long lcdMessageStart = 0;
const unsigned long LCD_MESSAGE_DURATION = 4000; // show for 4 seconds

// ----------------------------------------------------------------------------
void talkiePCMCallback(int16_t* data, int len) {
    if (len <= 0) return;
    portENTER_CRITICAL(&bufferMux);
    for (int i = 0; i < len; i++) {
        int nextWrite = (speechWriteIndex + 1) & (SPEECH_BUFFER_SIZE - 1);
        if (nextWrite == speechReadIndex) break;
        speechBuffer[speechWriteIndex] = data[i];
        speechWriteIndex = nextWrite;
    }
    speechActive = true;
    portEXIT_CRITICAL(&bufferMux);
}

// ----------------------------------------------------------------------------
int readUltrasonicDistance() {
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);
    long duration = pulseIn(ECHO_PIN, HIGH, 30000);
    int distance = duration * 0.034 / 2;
    if (distance <= 0 || distance > 150) return 150;
    return distance;
}

// ----------------------------------------------------------------------------
int32_t get_sound_data(Frame *data, int32_t len) {
    if (speechActive) {
        static int repeatCounter = 0;
        static int16_t currentSample = 0;

        for (int i = 0; i < len; i++) {
            if (repeatCounter == 0) {
                portENTER_CRITICAL(&bufferMux);
                if (speechReadIndex != speechWriteIndex) {
                    currentSample = speechBuffer[speechReadIndex];
                    speechReadIndex = (speechReadIndex + 1) & (SPEECH_BUFFER_SIZE - 1);
                } else {
                    speechActive = false;
                    currentSample = 0;
                }
                portEXIT_CRITICAL(&bufferMux);
            }
            data[i].channel1 = currentSample;
            data[i].channel2 = currentSample;
            repeatCounter++;
            if (repeatCounter >= 6) repeatCounter = 0; // 8 kHz → 44.1 kHz
        }
        return len;
    }

    // Synth waveform (44.1 kHz)
    float active_frequency = noise_matrix[current_matrix_index] * frequency_tension;
    float sample_rate = 44100.0;
    int dist = distance_cm;
    
    for (int i = 0; i < len; i++) {
        int16_t sample = 0;
        if (dist >= 80) {
            float raw_tri = (phase < 0) ? (-1.0 - phase) : (1.0 - phase);
            sample = (int16_t)(raw_tri * 4000.0);
            phase += (2.0 * active_frequency) / sample_rate;
            if (phase >= 1.0) phase -= 2.0;
        } else if (dist >= 30) {
            float raw_saw = phase;
            sample = (int16_t)(raw_saw * 5500.0);
            phase += (3.0 * active_frequency) / sample_rate;
            if (phase >= 1.0) phase -= 2.0;
        } else {
            sample = (phase >= 0.0) ? 6000 : -6000;
            phase += (4.0 * active_frequency) / sample_rate;
            if (phase >= 1.0) phase -= 2.0;
        }
        sample = (sample / 256) * 256;
        data[i].channel1 = sample;
        data[i].channel2 = sample;
    }
    return len;
}

// ----------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);

    lcd.init();
    lcd.backlight();
    lcd.setCursor(0, 0);
    lcd.print("SYSTEM // MATTER");
    delay(1000);
    lcd.clear();

    voice.setDataCallback(talkiePCMCallback);

    Serial.println("Initializing Audio Transmit Grid...");
    a2dp_source.start("", get_sound_data);
}

// ----------------------------------------------------------------------------
void loop() {
    distance_cm = readUltrasonicDistance();

    // Arpeggiator update
    int step_delay = map(constrain(distance_cm, 5, 140), 5, 140, 40, 200);
    if (millis() - last_arp_step_time > (unsigned long)step_delay) {
        last_arp_step_time = millis();
        current_matrix_index = (current_matrix_index + 1) % matrix_size;
    }

    // Tension mapping
    float raw_multiplier = map(distance_cm, 2, 150, 400, 80);
    frequency_tension = raw_multiplier / 100.0;

    // --- LCD update ---
    if (millis() - last_lcd_update_time > 200) {
        last_lcd_update_time = millis();

        const char* stateText;
        if (distance_cm >= 90) stateText = "STATE: VOID     ";
        else if (distance_cm >= 30) stateText = "STATE: TENSION  ";
        else stateText = "STATE: COLLAPSE ";

        lcd.setCursor(0, 0);
        lcd.print(stateText);

        lcd.setCursor(0, 1);
        if (millis() - lcdMessageStart < LCD_MESSAGE_DURATION && lcdMessage[0] != '\0') {
            lcd.print(lcdMessage);
            int len = strlen(lcdMessage);
            if (len < 16) {
                for (int i = len; i < 16; i++) lcd.print(" ");
            }
        } else {
            lcd.print("DIST:");
            lcd.print(distance_cm);
            lcd.print("cm      ");
        }
    }

    // --- Speech triggers with new sci‑fi vocabulary ---
    if (distance_cm < 12 && !speechActive && speechState == IDLE) {
        // COLLAPSE event – dramatic emergency sequence
        voice.say(sp4_WARNING);
        voice.say(sp4_DANGER);
        voice.say(sp4_EMERGENCY);      // "emergency" – available
        voice.say(sp5_EVACUATION);     // "evacuation" – from sp5 list
        voice.say(sp4_ABORT);          // "abort"
        voice.say(sp4_FAILURE);        // "failure"
        // Optional: add sp4_MAYDAY? but careful – sp4_MAYDAY exists, could add.
        // voice.say(sp4_MAYDAY);
        
        speechState = SAY_COLLAPSE;
        Serial.println("COLLAPSE EVENT: EMERGENCY EVACUATION");
        lcdMessage = "!COLLAPSE EMERG!";
        lcdMessageStart = millis();
    }
    else if (distance_cm >= 12 && distance_cm < 30 && !speechActive && speechState == IDLE) {
        // TENSION event – approach warning with aerospace terms
        voice.say(sp4_CAUTION);
        voice.say(sp4_TARGET);
        voice.say(sp5_APPROACHES);     // "approaches" – from sp5
        voice.say(sp5_ALTITUDE);       // "altitude"
        voice.say(sp4_RADAR);          // "radar"
        // Could also add sp5_OVERSPEED or sp5_STALL if desired.
        speechState = SAY_TENSION;
        Serial.println("TENSION: TARGET APPROACHING");
        lcdMessage = "!TARGET APPROACH!";
        lcdMessageStart = millis();
    }

    // Reset state after speech finishes
    if (speechState != IDLE && !speechActive) {
        speechState = IDLE;
    }

    delay(10);
}