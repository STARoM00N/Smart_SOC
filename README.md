# 🔐 Smart SOC — Smart Security Operations Center
A real-time intelligent security system for Server Rooms built on ESP32
Detects intrusion, smoke, and fire — alerts via LINE instantly, monitored via Blynk

# 📋 Table of Contents
Overview
Features
Hardware
System Architecture
How It Works
Quick Start
Blynk Setup Guide
Safety & Design Notes
OLED Display Layout
Project Structure
# 📌 Overview
Smart SOC monitors and protects server rooms automatically — no human monitoring required.
It handles three core questions:

🔑 Who is entering the room? — RFID access control
⚠️ Is anything wrong? — Motion, smoke, and fire detection
🌡️ Is the temperature safe? — Auto fan control
When something goes wrong, the system locks down, triggers an alarm, and sends a LINE notification instantly.

# ✨ Features
Feature	Description
🪪 RFID Access Control	Whitelist-based authentication with configurable session timer
🚶 Motion Detection	PIR sensor flags unauthorized presence
🔥 Fire & Smoke Detection	MQ-2 (gas/smoke) + DHT11 (temperature) work together
📲 LINE Instant Alerts	Push notification sent once per event — no spam
🌡️ Auto Fan Control	Fan turns on automatically above 30°C
📟 OLED Display	Shows live temp, humidity, door status, smoke level
📱 Blynk Dashboard	Remote monitoring and control via app or web
🔴 Visual & Audio Alarm	Red LED flashing + buzzer, mutable via Blynk
# 🛠️ Hardware
Component	Model	GPIO Pin
Microcontroller	ESP32	—
Temperature & Humidity	DHT11	4
Motion Sensor	PIR	13
Gas / Smoke Sensor	MQ-2	35 (ADC)
RFID Reader	MFRC522	SS=5, RST=16
OLED Display	SSD1306 128×64	I2C (SDA/SCL)
Door Lock Relay	5V Relay — Active LOW	26
Fan Relay	5V Relay — Active LOW	25
Red LED	—	15
Green LED	—	33
Buzzer	Active Buzzer — Active HIGH	32
# 📐 System Architecture
  INPUT SENSORS              ESP32 CORE            OUTPUT ACTUATORS
  ─────────────              ──────────            ────────────────
  [RFID MFRC522] ──────────►│        │──────────► [Relay - Door Lock GPIO26]
  [PIR Sensor]   ──────────►│        │──────────► [Relay - Fan       GPIO25]
  [MQ-2 Sensor]  ──────────►│  ESP32 │──────────► [LED Red           GPIO15]
  [DHT11]        ──────────►│        │──────────► [LED Green         GPIO33]
                            │        │──────────► [Buzzer            GPIO32]
                            │        │──────────► [OLED Display      I2C  ]
                            │        │
                  WiFi ────►│        │──────────► [Blynk Cloud]
                            └────────┘──────────► [LINE Messaging API]
# 🔄 How It Works
# 1 — RFID Access Control
Card Scanned
├── Known UID  → Unlock door for 3 sec → Start session timer
│                  During session : PIR motion will NOT trigger alarm
│                  Session expires: Wait until no motion → Auto re-lock
└── Unknown UID → Deny access → Send LINE alert ❌
🔧 Session duration is controlled by AUTH_TIMEOUT in the code.
Default is set to 10 seconds for demo. Change to 600000 for 10 minutes in production.

const unsigned long AUTH_TIMEOUT = 10000;  // 10 seconds (demo)
// const unsigned long AUTH_TIMEOUT = 600000; // 10 minutes (production)
# 2 — Alarm Conditions
# Status	           Trigger Condition	                    System Response
SMOKE DETECT	     MQ-2 analog > 2000 (and < 4090)	🚨 Buzzer + Red LED flash + LINE alert
FIRE ALERT	       DHT11 temperature > 50°C	        🚨 Buzzer + Red LED flash + LINE alert
UNAUTH MOTION	     PIR HIGH + no active session	    🚨 Buzzer + Red LED flash + LINE alert
EMERGENCY LOCK	   Blynk V5 toggled ON	            🚨 Buzzer + Red LED flash + LINE alert
NORMAL	           None of the above	              ✅ Green LED on, Buzzer off
⚠️ MQ-2 values at 4090–4095 are treated as sensor errors and ignored.
⚠️ LINE alert fires only once per event — resets automatically when situation clears.

# 3 — Fan Control Logic
Temperature ≥ 30°C  →  Fan ON  (automatic — overrides manual setting)
Temperature < 30°C  →  Fan follows Blynk switch V4 (manual control)
# 🚀 Quick Start
# Step 1 — Install Libraries
In Arduino IDE → Library Manager, install:

Blynk                   (by Volodymyr Shymanskyy)
Adafruit GFX Library    (by Adafruit)
Adafruit SSD1306        (by Adafruit)
DHT sensor library      (by Adafruit)
MFRC522                 (by GithubCommunity)
WiFi and HTTPClient are built into the ESP32 board package — no extra install needed.

# Step 2 — Set Board in Arduino IDE
Go to Tools → Board → ESP32 Arduino → ESP32 Dev Module

# Step 3 — Update Credentials in Code
Open smart_soc.ino and fill in your own values:

// ── Blynk ──────────────────────────────────────────
#define BLYNK_TEMPLATE_ID   "TMPLxxxxxxxx"   // From Blynk dashboard
#define BLYNK_TEMPLATE_NAME "Smart SOC V2"   // Must match exactly
#define BLYNK_AUTH_TOKEN    "xxxxxxxxxxxx"    // From your device page

// ── WiFi ───────────────────────────────────────────
char ssid[] = "your_wifi_ssid";
char pass[] = "your_wifi_password";

// ── LINE Messaging API ─────────────────────────────
#define LINE_TOKEN "your_line_channel_access_token"
# Step 4 — Add Your RFID Card UIDs
Inside checkRFID(), add your card UID:

if (uid == "XX XX XX XX" || uid == "YY YY YY YY") {
    // Authorized — access granted
}
💡 How to find your UID: Scan any card — the UID prints in Serial Monitor at 115200 baud.
Format example: 79 5B B5 01

# Step 5 — Upload & Verify
Connect ESP32 via USB → Click Upload
Open Serial Monitor at 115200 baud
You should see:
[SYSTEM] Booting up... System Armed in grace period.
[Blynk] Connected to server
Blynk dashboard should show device Online ✅
# 📱 Blynk Setup Guide
This project uses Blynk IoT (new cloud platform) — not legacy Blynk 1.0.

# 1 — Create Account
Go to https://blynk.cloud → Sign up → Verify email

# 2 — Create Template
# Developer Zone → My Templates → + New Template

# Field	     Value
Name	    Smart SOC V2
Hardware	   ESP32
Connection	  WiFi
Copy the TEMPLATE_ID shown after creation.

# 3 — Create Datastreams
# Template → Datastreams → + New Datastream → Virtual Pin

# Pin	Name	           Data Type	Min	Max	Direction
  V0	Temperature	    Double	    0	  100	Device → App
  V1	Humidity	      Double	    0	  100	Device → App
  V3	Motion	        Integer	    0	    1	Device → App
  V4	Fan Switch	    Integer	    0	    1	App → Device
  V5	Emergency Lock	Integer	    0	    1	App → Device
  V6	Smoke Level	    Integer	    0	 4095	Device → App
  V7	RFID Status	    String	    —	    —	Device → App
  V8	Mute Alarm	    Integer	    0   	1	App → Device
⚠️ Pin numbers must match exactly — the code is hardcoded to these pins.

# 4 — Build Web Dashboard
# Template → Web Dashboard → Edit → drag widgets

# Widget	   Pin	    Purpose
Gauge	        V0	Temperature display
Gauge	        V1	Humidity display
LED Indicator	V3	Motion status
Switch	      V4	Fan manual on/off
Button (Switch mode)	V5	Emergency lock
Gauge	V6	Smoke level (0–4095)
Label	V7	Last RFID scan result
Button (Switch mode)	V8	Mute alarm buzzer
# 5 — Build Mobile Dashboard
Download Blynk IoT app (iOS / Android)
Log in → Your template → Mobile Dashboard
Add the same widgets as above
Tap ▶️ to go live
# 6 — Create Device & Get Auth Token
# Devices → + New Device → From Template → Smart SOC V2

Name it (e.g., Server Room 1) → Create
Copy the Auth Token shown on the device info page → paste into code (Step 3 above)

# 🔐 Safety & Design Notes
# Feature	                               Detail
Brownout Protection	    Disabled — prevents ESP32 reboot on slight voltage drops
Sensor Error Guard	    MQ-2 values ≥ 4090 are treated as sensor fault and ignored
Boot Grace Period	      System starts authorized using AUTH_TIMEOUT — prevents false alarm during power-on
Single Alert per Event	lineSent flag ensures LINE notifies only once until the situation clears
Alarm Mute	            Buzzer silenced via Blynk V8 — visual red LED alarm remains active
# 🖥️ OLED Display Layout
┌──────────────────────────┐
│  Smart SOC Data Center   │  ← Title
│  Temp:28.5C  Hum:60%    │  ← DHT11 readings
│  Door: unlock (8s)       │  ← Door status + session countdown
│  Stat: ██ UNAUTH MOTION  │  ← Alarm status (inverted text when alarming)
│  Smoke lvl: 312          │  ← MQ-2 raw analog value
└──────────────────────────┘
Door states: lock / unlock (Xs) / securing...
Smoke field shows SENSOR ERROR if MQ-2 value ≥ 4090

# 📁 Project Structure
smart-soc/
├── smart_soc.ino     # Full source code
└── README.md         # This file
Built as an IoT Security project on ESP32 — feel free to fork and improve 🚀
