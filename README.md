🔐 Smart SOC — Smart Security Operations Center
A real-time intelligent security system for Server Rooms, built on ESP32 with RFID access control, motion detection, smoke/fire detection, and instant LINE notifications.

📌 Overview
Smart SOC monitors and protects server rooms automatically — no human monitoring required. It detects unauthorized access, smoke, fire, and abnormal motion, then instantly alerts administrators via LINE Messaging API while displaying live status on an OLED screen.

✨ Features
🪪 RFID Access Control — Whitelist-based card authentication with 10-minute session management
🚶 Motion Detection — PIR sensor detects unauthorized presence
🔥 Fire & Smoke Detection — MQ-2 gas sensor + DHT11 temperature monitoring
📲 LINE Instant Alerts — Real-time push notifications for every security event
🌡️ Auto Fan Control — Fan activates automatically when temperature exceeds 30°C
📟 OLED Display — Live status: temperature, humidity, door state, smoke level
📱 Blynk Dashboard — Remote monitoring and control from anywhere
🔴 Visual & Audio Alarm — Red LED + buzzer with mute option via Blynk
🛠️ Hardware
Component	Model	GPIO Pin
Microcontroller	ESP32	—
Temperature & Humidity	DHT11	4
Motion Sensor	PIR	13
Gas / Smoke Sensor	MQ-2	35 (ADC)
RFID Reader	MFRC522	SS=5, RST=16
OLED Display	SSD1306 128x64	I2C (SDA/SCL)
Door Lock Relay	5V Relay (Active LOW)	26
Fan Relay	5V Relay (Active LOW)	25
Red LED	—	15
Green LED	—	33
Buzzer	Active Buzzer	32
📐 System Architecture
[RFID Card] ──► [ESP32] ──► [Relay - Door Lock]
[PIR Sensor] ──►    │    ──► [Relay - Fan]
[MQ-2 Sensor] ──►   │    ──► [LED Red/Green]
[DHT11] ──►          │    ──► [Buzzer]
                     │    ──► [OLED Display]
                     │    ──► [Blynk App] (via WiFi)
                     └────► [LINE Notify] (via WiFi)
🔄 System Logic
1. RFID Session Management
Card Scanned
    ├── Authorized UID → Unlock door (3 sec) → Start 10-min session
    │       └── During session: No alarm even if motion detected
    │       └── Session expires → Wait for no motion → Re-lock
    └── Unknown UID → Deny access → Send LINE alert ❌
2. Alarm Conditions
Event	Trigger	Alert
SMOKE DETECT	MQ-2 > 2000 (and < 4090)	🚨 LINE + Buzzer + Red LED
FIRE ALERT	Temperature > 50°C	🚨 LINE + Buzzer + Red LED
UNAUTH MOTION	PIR = HIGH + No active session	🚨 LINE + Buzzer + Red LED
EMERGENCY LOCK	Blynk V5 = 1	🚨 LINE + Buzzer + Red LED
⚠️ LINE notification is sent once per event to avoid spam.

3. Fan Control Priority
Temperature ≥ 30°C → Fan ON (Auto override)
Temperature < 30°C → Follow Blynk manual switch (V4)
📱 Blynk Virtual Pins
Pin	Direction	Function
V0	OUT	Temperature (°C)
V1	OUT	Humidity (%)
V3	OUT	PIR Motion Status
V4	IN/OUT	Fan Manual Switch
V5	IN	Emergency Lock
V6	OUT	Smoke Level (Analog)
V7	OUT	RFID Access Result
V8	IN	Mute Alarm Buzzer
📦 Dependencies
Install via Arduino Library Manager:

- Blynk (BlynkSimpleEsp32)
- Adafruit GFX Library
- Adafruit SSD1306
- DHT sensor library (Adafruit)
- MFRC522
- WiFi (built-in ESP32)
- HTTPClient (built-in ESP32)
⚙️ Configuration
Open the .ino file and update the following:

// Blynk Credentials
#define BLYNK_TEMPLATE_ID   "your_template_id"
#define BLYNK_TEMPLATE_NAME "your_template_name"
#define BLYNK_AUTH_TOKEN    "your_auth_token"

// WiFi
char ssid[] = "your_wifi_ssid";
char pass[] = "your_wifi_password";

// LINE Messaging API Bearer Token
#define LINE_TOKEN "your_line_bearer_token"
Adding Authorized RFID Cards
In checkRFID(), add your card UID to the condition:

if (uid == "XX XX XX XX" || uid == "YY YY YY YY") {
    // Authorized
}
💡 Scan an unknown card first — the UID will print in Serial Monitor.

🚀 Getting Started
Clone this repository
git clone https://github.com/yourusername/smart-soc.git
Open in Arduino IDE

Set board to ESP32 Dev Module
Install all dependencies above
Update credentials in the config section

Upload to ESP32

Open Serial Monitor at 115200 baud to verify sensor readings

Scan your RFID card — note the UID and add it to the whitelist

🔐 Safety Features
Brownout Protection disabled to prevent reboot during power fluctuation
Sensor Error Guard — MQ-2 values at 4095 (ADC max) are filtered out as sensor errors
Grace Period on Boot — 10-minute authorized window on startup prevents false alarms during setup
Single LINE Alert per Event — lineSent flag prevents notification spam
📁 Project Structure
smart-soc/
├── smart_soc.ino        # Main source code
└── README.md            # This file
🖥️ OLED Display Layout
┌────────────────────────┐
│ Smart SOC Data Center  │
│ Temp: 28.5C Hum: 60%  │
│ Door: lock             │
│ Stat: NORMAL           │
│ Smoke lvl: 312         │
└────────────────────────┘
📜 License
This project is open source and available under the MIT License.

👨‍💻 Author
Developed as an IoT Security project using ESP32.
Feel free to fork, modify, and improve! 🚀
