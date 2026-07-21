# The Threshold Sentinel / 门槛哨兵

**Balkan Talisman Reimagined as a Smart Worksite & Home Sentinel**

---

## Overview / 概述

This project combines a traditional Balkan talisman, traditionally hung in homes for protection, with modern electronics to create a sentinel for both worksites and homes. Mounted on a canvas, the device runs independently on an ESP32 microcontroller, using an ultrasonic sensor to detect when someone approaches. It generates spoken warnings and retro sci‑fi synth textures, streamed directly to a Bluetooth loudspeaker. As you step closer, the sound shifts from a cautious pulse to an urgent emergency alert. It turns any entrance into a threshold where folklore meets industrial safety.

---

## Features / 功能

- **Ultrasonic Distance Sensing** – Detects approach from 2–150 cm
- **Bluetooth Audio Streaming** – Sends speech and synth sounds to any Bluetooth speaker
- **Talkie Speech Synthesis** – Generates spoken warnings from a US English vocabulary
- **Generative Synth** – Retro sci‑fi soundscapes that shift with distance
- **LCD Display** – Shows state and distance in real time
- **Non‑blocking Operation** – Smooth, responsive interaction without delays

---

## Hardware / 硬件清单

| Component | Quantity | Notes |
|-----------|----------|-------|
| ESP32 DevKit C | 1 | Any ESP32 board with Bluetooth |
| HC‑SR04 Ultrasonic Sensor | 1 | Distance sensor |
| LCD 1602 I2C | 1 | 16x2 display with I2C backpack |
| Bluetooth Speaker | 1 | Any standard Bluetooth speaker |
| USB‑C Cable | 1 | For power and programming |
| Breadboard & Jumper Wires | | For prototyping |

---

## Hookup Diagram / 接线图

```
ESP32 DevKit C          HC-SR04              LCD 1602 I2C
───────────────────────────────────────────────────────────────────

3.3V ─────────────────── VCC (Pin 1)
GND  ─────────────────── GND (Pin 2)
GPIO 12 ──────────────── TRIG (Pin 3)
GPIO 13 ──────────────── ECHO (Pin 4)

                                     ┌─────────────────────────┐
                                     │        LCD 1602 I2C     │
                                     │                         │
5V   ────────────────────────────────│ VCC                     │
GND  ────────────────────────────────│ GND                     │
GPIO 21 (SDA) ──────────────────────│ SDA                     │
GPIO 22 (SCL) ──────────────────────│ SCL                     │
                                     └─────────────────────────┘

ESP32 DevKit C
───────────────────────────────────────────────────────────────────
```

**Important Notes / 重要说明:**

1. **HC‑SR04 runs on 3.3V** – Connect VCC to ESP32 3.3V pin (not 5V)
2. **LCD runs on 5V** – Connect VCC to ESP32 5V pin (allows I2C to operate correctly)
3. **Default I2C pins:** GPIO 21 (SDA), GPIO 22 (SCL)
4. **Default I2C address:** 0x27 (check with I2C scanner if yours differs)
5. **Bluetooth:** The ESP32 connects to your Bluetooth speaker automatically – no pairing code required

---

## Library Dependencies / 库依赖

Install these libraries via Arduino Library Manager:

| Library | Version | Purpose |
|---------|---------|---------|
| LiquidCrystal I2C | Latest | LCD display control |
| BluetoothA2DPSource | Latest | Bluetooth audio streaming |
| TalkiePCM | Latest | Speech synthesis |
| Vocab_US_Large | (included with TalkiePCM) | US English vocabulary |

---

## Installation / 安装

1. **Clone or download this repository**
2. **Open the .ino file** in Arduino IDE
3. **Install required libraries** (see above)
4. **Select board:** ESP32 Dev Module
5. **Set USB port** for your ESP32
6. **Upload the sketch**

---

## How It Works / 工作原理

1. The ultrasonic sensor continuously measures distance to the nearest object.
2. The ESP32 processes this data and maps it to:
   - **Synth parameters** – frequency, waveform type, and intensity
   - **Speech triggers** – specific warnings based on proximity
3. Audio is generated in real time and streamed via Bluetooth A2DP.
4. The LCD shows current state and distance.

### Distance Zones / 距离分区

| Distance | Zone | Audio Behavior |
|----------|------|----------------|
| > 90 cm | Void | Triangle wave drift, ambient |
| 30 – 90 cm | Tension | Sawtooth sweep, cautious pulse |
| 12 – 30 cm | Approach | Spoken warnings: "Caution... Target approaches..." |
| < 12 cm | Collapse | Emergency alert: "Warning! Danger! Emergency! Evacuation! Abort! Failure!" |

---

## Usage / 使用说明

1. **Power on** the ESP32 via USB.
2. **Enable Bluetooth** on your speaker and put it in pairing mode.
3. The ESP32 will automatically connect to the speaker (it appears as an audio source).
4. **Place the device** near a doorway, worksite entrance, or home threshold.
5. **Approach the sensor** – the audio will respond to your distance.
6. **Step back** – the audio returns to its ambient state.

---

## Troubleshooting / 故障排除

| Issue | Solution |
|-------|----------|
| No sound from Bluetooth | Check speaker is in pairing mode. Restart ESP32. |
| LCD shows nothing | Adjust I2C address (try 0x27 or 0x3F) |
| Wrong distance readings | Check HC‑SR04 connections. Ensure 3.3V power. |
| Speech plays too fast/slow | Adjust upsampling ratio in `get_sound_data()` (line ~75) |
| Speech sounds robotic | This is expected – Talkie generates classic 8‑bit speech |

---

## Credits / 致谢

- **Talkie Library** – Peter Knight / Armin Joachimsmeyer
- **ESP32‑A2DP Library** – Phil Schatzmann
- **Project Concept** – Balkan talisman tradition meets interactive electronics
- **Video Production** – [Your Name / Studio]

---

## License / 许可

This project is open source under the GPLv2 license.

---

Here's a concise self‑reference section for your Arduino IDE setup and project notes.

---

## Project Setup Notes / 项目设置备注

### 1. Partition Scheme

| Setting | Value |
|---------|-------|
| Board | ESP32 Dev Module |
| Partition Scheme | **No OTA (Large APP)** |
| Why | This project needs extra program space for audio/vocabulary data. Default (OTA) will cause upload failure due to insufficient space. |

**Steps:**
- Arduino IDE → Tools → Partition Scheme → **No OTA (Large APP)**
- If using other ESP32 boards, choose the largest APP partition available

---

### 2. Required Libraries (Exact Versions)

| Library | Source | Notes |
|---------|--------|-------|
| LiquidCrystal I2C | Library Manager | Latest version (ignore AVR warning) |
| BluetoothA2DPSource | Library Manager | Phil Schatzmann version |
| TalkiePCM | GitHub (pschatzmann/TalkiePCM) | NOT the original Talkie library! |
| Vocab_US_Large | Included with TalkiePCM | Check that vocabulary is present |

**Important:** Do NOT install the original Talkie library (Peter Knight) – it uses different methods and will cause compilation errors.

---

### 3. I2C LCD Address

- Default: `0x27`
- Alternative: `0x3F`
- Run I2C scanner sketch if your LCD doesn't work

---

### 4. Upload Settings

| Setting | Value |
|---------|-------|
| Upload Speed | 115200 (default) |
| USB Port | Select correct COM port |
| Flash Size | 4MB (default for DevKit C) |

---

### 5. Serial Monitor

- Baud rate: `115200`
- Opens automatically after upload
- Shows distance readings and speech triggers

---

### 6. Power Supply

| Component | Voltage | Current |
|-----------|---------|---------|
| ESP32 | USB 5V | ~100mA |
| HC‑SR04 | **3.3V** (not 5V!) | ~15mA |
| LCD I2C | **5V** | ~20mA |

**Do NOT power HC‑SR04 with 5V – it will damage the sensor!**

---

### 7. First Time Upload Checklist

- [ ] ESP32 plugged in via USB
- [ ] Correct COM port selected
- [ ] Partition Scheme set to **No OTA (Large APP)**
- [ ] All libraries installed
- [ ] No other Talkie library installed (conflict)
- [ ] Power cycled after upload (sometimes needed)

---

### 8. Common Errors & Fixes

| Error | Fix |
|-------|-----|
| `'class TalkiePCM' has no member named 'process'` | You have the wrong Talkie library – remove original Talkie, install TalkiePCM |
| `Sketch too big` | Change Partition Scheme to No OTA |
| `I2C LCD not displaying` | Check address (0x27 vs 0x3F) or run scanner |
| `No Bluetooth audio` | Put speaker in pairing mode before powering ESP32 |
| `Distance always 150cm` | Check HC‑SR04 connections: VCC to 3.3V, GND to GND, TRIG/ECHO to GPIO 12/13 |

---

### 9. Quick Compilation Check

- Build time: ~30–60 seconds
- Final binary size: ~800KB – 1MB (within No OTA partition)
- If size > 1.2MB, you may need to reduce vocabulary or optimize code

---

### 10. Project Files Structure

```
esp32talkdistlcdbluetooh/
├── esp32talkdistlcdbluetooh.ino    # Main sketch
├── README.md                        # Project documentation
├── Vocab_US_Large.h                 # Included via library
└── (No other files needed)
```

---

### Full Quick‑Start Sequence

1. Open Arduino IDE
2. Set Board: **ESP32 Dev Module**
3. Set Partition Scheme: **No OTA (Large APP)**
4. Install required libraries
5. Open the .ino file
6. Connect ESP32 via USB
7. Select COM port
8. **Upload**
9. Power cycle ESP32
10. Pair Bluetooth speaker (the device appears as an audio source)
11. Test with hand movement toward sensor

---

These notes will save you time during future uploads. Keep them handy!

## Connect / 联系

- **Project Video:** [Insert link]
- **GitHub:** [Insert link]
- **Contact:** [Insert email/social]

---

**The Threshold Sentinel / 门槛哨兵**  
*Tradition meets technology at every doorway.*
