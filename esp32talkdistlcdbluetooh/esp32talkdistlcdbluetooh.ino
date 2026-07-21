#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "BluetoothA2DPSource.h"
#include "TalkiePCM.h"
#include "Vocab_US_Large.h"

// --- Required for critical sections ---
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// --- HARDWARE PINS ---
#define TRIG_PIN  12  
#define ECHO_PIN  13  

// --- LCD SETUP ---
LiquidCrystal_I2C lcd(0x27, 16, 2);

// --- AUDIO & SYNTH SETUP ---
BluetoothA2DPSource a2dp_source;
TalkiePCM voice(Serial);   // Print placeholder

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
enum SpeechState { IDLE, SAY_WARNING_DANGER, SAY_WARNING_CHECK };
SpeechState speechState = IDLE;

// --- Ring buffer for speech PCM samples (size power of two) ---
#define SPEECH_BUFFER_SIZE 2048
int16_t speechBuffer[SPEECH_BUFFER_SIZE];
volatile int speechWriteIndex = 0;
volatile int speechReadIndex = 0;
volatile bool speechActive = false;   // true while samples are waiting to be played

// Mutex for buffer access (critical section)
portMUX_TYPE bufferMux = portMUX_INITIALIZER_UNLOCKED;

// ----------------------------------------------------------------------------
// Called by TalkiePCM when a chunk of PCM data is ready (8 kHz, mono)
void talkiePCMCallback(int16_t* data, int len) {
    if (len <= 0) return;
    portENTER_CRITICAL(&bufferMux);
    for (int i = 0; i < len; i++) {
        int nextWrite = (speechWriteIndex + 1) & (SPEECH_BUFFER_SIZE - 1);
        if (nextWrite == speechReadIndex) break;   // buffer full – drop
        speechBuffer[speechWriteIndex] = data[i];
        speechWriteIndex = nextWrite;
    }
    speechActive = true;   // indicate we have new data
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
// A2DP callback – runs at 44.1 kHz (default)
int32_t get_sound_data(Frame *data, int32_t len) {
    // --- If we have speech samples, output them (with upsampling) ---
    if (speechActive) {
        static int repeatCounter = 0;
        static int16_t currentSample = 0;

        for (int i = 0; i < len; i++) {
            if (repeatCounter == 0) {
                // fetch next speech sample from ring buffer
                portENTER_CRITICAL(&bufferMux);
                if (speechReadIndex != speechWriteIndex) {
                    currentSample = speechBuffer[speechReadIndex];
                    speechReadIndex = (speechReadIndex + 1) & (SPEECH_BUFFER_SIZE - 1);
                } else {
                    // buffer empty – stop speech
                    speechActive = false;
                    currentSample = 0;
                }
                portEXIT_CRITICAL(&bufferMux);
            }
            data[i].channel1 = currentSample;
            data[i].channel2 = currentSample;
            repeatCounter++;
            // Upsample from 8 kHz to 44.1 kHz: repeat each sample ~5.5 times
            if (repeatCounter >= 6) repeatCounter = 0;
        }
        return len;
    }

    // --- No speech – generate synth waveform (44.1 kHz) ---
    float active_frequency = noise_matrix[current_matrix_index] * frequency_tension;
    float sample_rate = 44100.0;   // actual A2DP rate
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
        sample = (sample / 256) * 256;   // 8‑bit crunch
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

    // --- Configure TalkiePCM ---
    voice.setDataCallback(talkiePCMCallback);

    Serial.println("Initializing Audio Transmit Grid...");
    // Start A2DP with default 44.1 kHz sample rate
    a2dp_source.start("", get_sound_data);
}

// ----------------------------------------------------------------------------
void loop() {
    distance_cm = readUltrasonicDistance();

    int step_delay = map(constrain(distance_cm, 5, 140), 5, 140, 40, 200);
    if (millis() - last_arp_step_time > (unsigned long)step_delay) {
        last_arp_step_time = millis();
        current_matrix_index = (current_matrix_index + 1) % matrix_size;
    }

    float raw_multiplier = map(distance_cm, 2, 150, 400, 80);
    frequency_tension = raw_multiplier / 100.0;

    if (millis() - last_lcd_update_time > 200) {
        last_lcd_update_time = millis();
        lcd.setCursor(0, 0);
        if (distance_cm >= 90) {
            lcd.print("STATE: VOID     ");
        } else if (distance_cm >= 30) {
            lcd.print("STATE: TENSION  ");
        } else {
            lcd.print("STATE: COLLAPSE ");
        }
        lcd.setCursor(0, 1);
        lcd.print("DIST:");
        lcd.print(distance_cm);
        lcd.print("cm      ");
    }

    // --- Trigger speech (blocking call, but short) ---
    // We use speechActive to prevent re-trigger while still playing.
    if (distance_cm < 12 && !speechActive && speechState == IDLE) {
        voice.say(sp4_WARNING);
        voice.say(sp4_DANGER);
        voice.say(sp4_ERROR2);
        speechState = SAY_WARNING_DANGER;
        Serial.println("THRESHOLD BREACH: COLLAPSE EVENT DETECTED");
    }
    else if (distance_cm >= 12 && distance_cm < 30 && !speechActive && speechState == IDLE) {
        voice.say(sp4_WARNING);
        voice.say(sp4_CHECK);
        speechState = SAY_WARNING_CHECK;
        Serial.println("THRESHOLD APPROACH: TENSION FIELD");
    }

    // Reset state when speech has finished playing (buffer emptied)
    if (speechState != IDLE && !speechActive) {
        speechState = IDLE;
    }

    delay(10);
}