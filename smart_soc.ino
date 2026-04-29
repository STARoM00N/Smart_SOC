#define BLYNK_PRINT Serial 
#define BLYNK_TEMPLATE_ID "BLYNK_ID"
#define BLYNK_TEMPLATE_NAME "BLYNK_NAME"
#define BLYNK_AUTH_TOKEN "BLYNK_TOKEN"

char ssid[] = "WIFI_NAME"; 
char pass[] = "WIFI_PASSWORD";        
#define LINE_TOKEN "LINE_TOKEN" 

#include <WiFi.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <BlynkSimpleEsp32.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>
#include <SPI.h>
#include <MFRC522.h>

// --- กำหนดขา PIN ---
#define DHTPIN 4      
#define DHTTYPE DHT11
#define PIR_PIN 13
#define MQ2_PIN 35  
#define SS_PIN 5
#define RST_PIN 16  
#define LED_RED 15 
#define LED_GREEN 33
#define BUZZER_PIN 32
#define RELAY_LOCK 26
#define RELAY_FAN 25

// --- กำหนดขั้วฮาร์ดแวร์ให้ถูกต้อง ---
#define RELAY_ON LOW      // รีเลย์ทำงานเมื่อสั่ง LOW (Active LOW)
#define RELAY_OFF HIGH    

#define BUZZER_ON HIGH    // ลำโพงดังเมื่อสั่ง HIGH (Active HIGH)
#define BUZZER_OFF LOW    
// ----------------------------------

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
DHT dht(DHTPIN, DHTTYPE);
MFRC522 mfrc522(SS_PIN, RST_PIN);
BlynkTimer timer;

// ตัวแปรควบคุมระบบ
int manualFanState = 1; 
int emergencyLockState = 0;
bool lineSent = false;
bool alarmMuted = false;   

// --- ตัวแปรจัดการ Session การเข้าห้อง (อัปเกรดใหม่) ---
unsigned long authorizedTimer = 0;
unsigned long lastMotionTime = 0; // เก็บเวลาล่าสุดที่เจอความเคลื่อนไหว
unsigned long securingTimer = 0;  // เก็บเวลาเริ่มต้นของโหมด securing

const unsigned long AUTH_TIMEOUT = 10000;     // เวลาอนุญาตสูงสุด (คุณตั้งไว้ 10 วินาที)
const unsigned long IDLE_TIMEOUT = 5000;      // ⏳ ถ้าไม่มีการเคลื่อนไหวเกิน 5 วิ = ล็อกห้องทันที
const unsigned long SECURING_TIMEOUT = 3000;  // ⏳ บังคับออกจากโหมด securing ภายใน 3 วิ

bool isAuthorized = false;
bool waitingForClear = false; 
bool isAlarmTriggered = false; 

BLYNK_CONNECTED() { Blynk.virtualWrite(V4, manualFanState); }
BLYNK_WRITE(V4) { manualFanState = param.asInt(); }
BLYNK_WRITE(V5) { emergencyLockState = param.asInt(); }
BLYNK_WRITE(V8) { if (param.asInt() == 1) alarmMuted = true; }

void sendLineNotify(String message) {
  if (WiFi.status() == WL_CONNECTED && String(LINE_TOKEN) != "token") {
    HTTPClient http;
    http.begin("https://api.line.me/v2/bot/message/broadcast");
    http.addHeader("Authorization", "Bearer " + String(LINE_TOKEN));
    http.addHeader("Content-Type", "application/json"); 
    String payload = "{\"messages\":[{\"type\":\"text\",\"text\":\"" + message + "\"}]}";
    
    Serial.println("[LINE API] กำลังส่งข้อความ: " + message);
    int httpCode = http.POST(payload);
    Serial.printf("[LINE API] Response Code: %d\n", httpCode);
    http.end();
  }
}

void runSmartSOC() {
  Serial.println("\n========== [ SYSTEM LOG ] ==========");
  
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  int motionDetected = (digitalRead(PIR_PIN) == HIGH);
  int smokeAnalog = analogRead(MQ2_PIN); 
  
  // Safe Mode: ป้องกันเซนเซอร์ควันรวน (4095)
  bool isSmoke = (smokeAnalog > 2000 && smokeAnalog < 4090); 
  bool isFire = (t > 50.0);

  if (isnan(t) || isnan(h)) { t = 0; h = 0; }
  unsigned long currentTime = millis();

  // บันทึกเวลาล่าสุดที่ยังเห็นคนขยับตัวอยู่
  if (motionDetected) {
    lastMotionTime = currentTime;
  }

  Serial.println("--> SENSOR DATA:");
  Serial.printf("    Temp: %.1f C | Hum: %.1f %%\n", t, h);
  Serial.printf("    Smoke (Analog): %d | isSmoke: %s\n", smokeAnalog, isSmoke ? "YES" : "NO");

  // --- จัดการเวลาเข้าห้อง (Session) รูปแบบใหม่ ---
  if (isAuthorized) {
    // ยกเลิกสิทธิ์เมื่อ: 1. หมดโควต้าเวลา หรือ 2. ไม่มีคนอยู่เกิน 5 วินาที
    if ((currentTime - authorizedTimer >= AUTH_TIMEOUT) || 
        (currentTime - lastMotionTime >= IDLE_TIMEOUT)) {
      isAuthorized = false;
      waitingForClear = true; 
      securingTimer = currentTime; // เริ่มนับเวลาถอยหลังโหมด Securing
    }
  }
  
  if (waitingForClear) {
    // กลับไปสถานะ Lock ทันทีที่: เซนเซอร์บอกว่าไม่มีคนแล้ว หรือ หมดเวลา Securing โควต้า 3 วินาที
    if (!motionDetected || (currentTime - securingTimer >= SECURING_TIMEOUT)) {
      waitingForClear = false; // ต้องรอการสแกนเข้าห้องใหม่
    }
  }

  String statusMsg = "NORMAL";
  isAlarmTriggered = false;

  // --- Logic การแจ้งเตือนความปลอดภัย ---
  if (isFire || isSmoke) {
    statusMsg = isFire ? "FIRE ALERT!" : "SMOKE DETECT!";
    isAlarmTriggered = true; 
  } else if (motionDetected && !isAuthorized && !waitingForClear) {
    statusMsg = "UNAUTH MOTION!";
    isAlarmTriggered = true;
  } else if (emergencyLockState == 1) {
    statusMsg = "EMERGENCY LOCK";
    isAlarmTriggered = true;
  }

  Serial.println("--> SYSTEM STATUS: " + statusMsg);

  // --- สั่งงานฮาร์ดแวร์ ---
  if (isAlarmTriggered) {
    digitalWrite(RELAY_LOCK, RELAY_ON);
    if (!lineSent) {
      sendLineNotify("🚨 " + statusMsg + " @ Server Room");
      lineSent = true;
    }
    Serial.println("--> ACTUATORS: 🚨 ALARM TRIGGERED!");
  } else {
    digitalWrite(LED_RED, LOW);
    digitalWrite(LED_GREEN, HIGH);
    digitalWrite(BUZZER_PIN, BUZZER_OFF); 
    digitalWrite(RELAY_LOCK, RELAY_ON); 
    lineSent = false;
    alarmMuted = false; 
    Serial.println("--> ACTUATORS: ปกติ (Green LED ON, Siren OFF)");
  }

  // --- ควบคุมพัดลม ---
  Serial.print("--> FAN STATE: ");
  if (t >= 30.0) {
    digitalWrite(RELAY_FAN, RELAY_ON);
    Serial.println("ON (ระบบออโต้: อุณหภูมิเกิน 30)");
  } else if (manualFanState == 1) {
    digitalWrite(RELAY_FAN, RELAY_ON);
    Serial.println("ON (เปิดผ่านแอป Blynk)");
  } else {
    digitalWrite(RELAY_FAN, RELAY_OFF);
    Serial.println("OFF");
  }
  Serial.println("====================================");

  // --- อัปเดตหน้าจอ OLED ---
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,0);  display.print("Smart SOC Data Center");
  display.setCursor(0,12); display.print("Temp:"); display.print(t,1); display.print("C Hum:"); display.print(h,0); display.print("%");

  display.setCursor(0,25);
  if (isAuthorized) {
    long remain = (AUTH_TIMEOUT - (currentTime - authorizedTimer)) / 1000;
    if(remain < 0) remain = 0; 
    display.print("Door: unlock("); display.print(remain); display.print("s)");
  } else if (waitingForClear) {
    display.print("Door: securing...");
  } else {
    display.print("Door: lock");
  }

  display.setCursor(0,40); display.print("Stat: ");
  if (isAlarmTriggered) {
    display.setTextColor(BLACK, WHITE); 
    display.print(statusMsg); 
    display.setTextColor(WHITE);
  } else {
    display.print(statusMsg); 
  }

  display.setCursor(0,55);
  display.print("Smoke lvl: "); 
  if(smokeAnalog >= 4090) display.print("SENSOR ERROR");
  else display.print(smokeAnalog);
  display.display();

  // ส่งข้อมูลเข้า Blynk
  Blynk.virtualWrite(V0, t); Blynk.virtualWrite(V1, h);
  Blynk.virtualWrite(V3, motionDetected);
  Blynk.virtualWrite(V6, smokeAnalog);
}

void checkRFID() {
  if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) return;

  String uid = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    uid += (mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
    uid += String(mfrc522.uid.uidByte[i], HEX);
  }
  uid.toUpperCase(); uid.trim();
  
  Serial.println("\n[RFID] ตรวจพบบัตร UID: " + uid);

  // *** เปลี่ยนรหัส UID ให้ตรงกับบัตรของคุณ ***
  if (uid == "79 5B B5 01" || uid == "F5 9F C8 05" || uid == "67 8E 35 06" || uid == "4D 9C 19 06") { 
    isAuthorized = true;
    waitingForClear = false;
    authorizedTimer = millis(); 
    lastMotionTime = millis();  // รีเซ็ตการจับเวลาตรวจจับคน
    alarmMuted = false;         
    digitalWrite(RELAY_LOCK, RELAY_OFF); 
    Blynk.virtualWrite(V7, "Access: Admin");
    
    Serial.println("[RFID] ✅ บัตรถูกต้อง! ปลดล็อกประตู 3 วินาที...");
    delay(3000); 
    digitalWrite(RELAY_LOCK, RELAY_ON);
  } else {
    Blynk.virtualWrite(V7, "Access: Denied");
    Serial.println("[RFID] ❌ บัตรแปลกปลอม! ปฏิเสธการเข้าถึง");
    sendLineNotify("❌ บัตรแปลกปลอม!");
  }
  mfrc522.PICC_HaltA(); 
}

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); // ปิดระบบ Brownout ป้องกันบอร์ดรีบูตตอนไฟตกนิดหน่อย
  Serial.begin(115200);
  
  Wire.begin();
  Wire.setClock(100000); // กันจอ OLED เพี้ยนจากสัญญาณรบกวน
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.display();

  dht.begin();
  pinMode(PIR_PIN, INPUT);
  pinMode(MQ2_PIN, INPUT);
  pinMode(LED_RED, OUTPUT); pinMode(LED_GREEN, OUTPUT); pinMode(BUZZER_PIN, OUTPUT);
  pinMode(RELAY_LOCK, OUTPUT); pinMode(RELAY_FAN, OUTPUT);

  // ค่าเริ่มต้นตอนเปิดเครื่อง
  digitalWrite(RELAY_LOCK, RELAY_ON);
  digitalWrite(RELAY_FAN, RELAY_ON);
  digitalWrite(LED_GREEN, HIGH);
  digitalWrite(LED_RED, LOW);
  digitalWrite(BUZZER_PIN, BUZZER_OFF); 

  isAuthorized = true; // ให้สิทธิ์ตอนเสียบปลั๊ก ป้องกันเสียงดังตอน Setup
  authorizedTimer = millis();
  lastMotionTime = millis();
  
  Serial.println("\n[SYSTEM] Booting up... System Armed in 10 minutes grace period.");
  
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  SPI.begin();
  mfrc522.PCD_Init();
  timer.setInterval(1000L, runSmartSOC); 
}

void loop() {
  Blynk.run();
  timer.run();
  checkRFID(); 

  // กระพริบไฟและเสียงเตือน
  if (isAlarmTriggered && !alarmMuted) {
    if (millis() % 500 < 250) {
      digitalWrite(LED_RED, HIGH);
      digitalWrite(BUZZER_PIN, BUZZER_ON); 
    } else {
      digitalWrite(LED_RED, LOW);
      digitalWrite(BUZZER_PIN, BUZZER_OFF); 
    }
    digitalWrite(LED_GREEN, LOW);
  }
}