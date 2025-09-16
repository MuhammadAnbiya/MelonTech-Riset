#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <EEPROM.h>

// ---------- CONFIG WIFI ----------
const char* ssid     = "anbi";
const char* password = "88888888";
const char* serverIP = "10.104.81.97"; // GANTI dengan IP ESP32 A

// ---------- LCD ----------
LiquidCrystal_I2C lcd(0x27, 16, 2); // LCD 16x2 I2C

// ---------- PIN SENSOR ----------
#define SENSOR_TDS_PIN   35
#define SENSOR_PH_PIN    34
#define SENSOR_SUHU_PIN   4   // ganti ke pin GPIO yang support OneWire

// ---------- EEPROM ----------
#define EEPROM_SIZE 512

// ---------- SENSOR SUHU ----------
OneWire oneWire(SENSOR_SUHU_PIN);
DallasTemperature sensors(&oneWire);

// ======================================================================
// --- DATA KALIBRASI TDS (3 TITIK) ---
// ======================================================================
// Titik 1: Air dengan TDS Rendah
const float voltage_clean = 0.24;  // Hasil ukur Anda untuk 228 ppm
const float ppm_clean     = 228.0;

// Titik 2: Larutan Standar Sedang
const float voltage_mid   = 1.41;  // Hasil ukur Anda untuk 869 ppm
const float ppm_mid       = 869.0;

// Titik 3: Larutan Standar Tinggi
const float voltage_high  = 2.03;  // Hasil ukur Anda untuk 1369 ppm
const float ppm_high      = 1369.0;
// ======================================================================

// ---------- VARIABEL ----------
float phB = 0, tdsB = 0, tempB = 0;

// ---------- PH calibration (atur sesuai hasil kalibrasi nyata) ----------
#define PH_OFFSET 7.0
#define PH_SLOPE  0.18

// ---------- Fungsi Baca Sensor ----------
void bacaSensorB(){
  // --- Suhu ---
  sensors.requestTemperatures();
  tempB = sensors.getTempCByIndex(0);
  if(tempB == DEVICE_DISCONNECTED_C || tempB < -10) tempB = 25.0;

  // --- TDS ---
  int rawValue = analogRead(SENSOR_TDS_PIN);
  float measuredVoltage = rawValue / 4095.0 * 3.3;
  float tdsValue = 0;

  // Mapping 3 titik
  if (measuredVoltage <= voltage_mid) {
    tdsValue = map(measuredVoltage * 1000, voltage_clean * 1000, voltage_mid * 1000, ppm_clean, ppm_mid);
  } else {
    tdsValue = map(measuredVoltage * 1000, voltage_mid * 1000, voltage_high * 1000, ppm_mid, ppm_high);
  }

  // Batasi nilai
  tdsValue = constrain(tdsValue, 0, 5000);

  // Kompensasi suhu
  tdsB = tdsValue / (1.0 + 0.02 * (tempB - 25.0));

  // --- pH ---
  float v_ph = (analogRead(SENSOR_PH_PIN) / 4095.0) * 3.3;
  phB = PH_OFFSET + ((2.366 - v_ph) / PH_SLOPE);
}

// ---------- Fungsi Kirim ke Server ----------
void kirimKeServer(){
  if(WiFi.status() != WL_CONNECTED) return;
  HTTPClient http;
  String url = String("http://") + serverIP + "/updateB?ph=" + String(phB,2) +
               "&tds=" + String(tdsB,1) + "&temp=" + String(tempB,2);
  http.begin(url);
  int code = http.GET();
  if(code>0){
    Serial.println("Sent -> " + url + " Resp:" + String(code));
  } else {
    Serial.println("Send failed");
  }
  http.end();
}

// ---------- SETUP ----------
void setup(){
  Serial.begin(115200);
  Wire.begin(21,22);

  EEPROM.begin(EEPROM_SIZE);

  lcd.init(); 
  lcd.backlight();
  sensors.begin();

  WiFi.begin(ssid, password);
  Serial.print("Connecting WiFi");
  int w=0;
  while(WiFi.status()!=WL_CONNECTED && w<40){ 
    delay(500); 
    Serial.print("."); 
    w++; 
  }
  if(WiFi.status()==WL_CONNECTED) 
    Serial.println("\nIP: " + WiFi.localIP().toString());
  else 
    Serial.println("\nWiFi failed");
}

// ---------- LOOP ----------
void loop(){
  bacaSensorB();

  // tampil di LCD 16x2
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.printf("T:%.1fC TDS:%d", tempB, (int)tdsB);
  lcd.setCursor(0,1);
  lcd.printf("pH:%.2f", phB);

  // log ke serial
  Serial.printf("Temp: %.1f C | TDS: %.1f ppm | pH: %.2f\n", tempB, tdsB, phB);

  // kirim ke server
  kirimKeServer();

  delay(5000); // kirim tiap 5 detik
}
