/******************************************************************************
* Proyek: Smart Watering Melon - Modul Sensor B (Client)
* Versi: FINALIZED - Perbaikan Komunikasi
* Deskripsi: Kode ini membaca sensor lokal dan mengirimkan data ke server utama
* (Control Panel A) setiap 10 detik.
* PERBAIKAN:
* - Menghapus '/' di akhir serverIP untuk memperbaiki URL request.
* - Merapikan kode dan logika pengiriman.
******************************************************************************/

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <LiquidCrystal_I2C.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <EEPROM.h>

// ---------- KONFIGURASI PENTING ----------
const char* ssid     = "ADVAN V1 PRO-8F7379";
const char* password = "7C27964D";
const char* serverIP = "192.168.0.128"; // GANTI dengan IP ESP32 A (TANPA '/')

// --- Kalibrasi Sensor ---
const float CAL_PH7_VOLTAGE   = 2.8;  
const float CAL_PH4_VOLTAGE   = 3.05; 
const bool  FORCE_RECALIBRATE = false;
const float voltage_low       = 1.2780;   
const float ppm_low           = 731.0;
const float voltage_mid       = 2.2155; 
const float ppm_mid           = 1240.0;
const float voltage_high      = 2.2735; 
const float ppm_high          = 2610.0;
const float VOLTAGE_THRESHOLD_AIR = 0.1;

// ---------- PENGATURAN PERANGKAT KERAS ----------
LiquidCrystal_I2C lcd(0x27, 16, 2); 
#define SENSOR_TDS_PIN  34
#define SENSOR_PH_PIN   35
#define SENSOR_SUHU_PIN 4

// ---------- OBJEK & VARIABEL GLOBAL ----------
#define EEPROM_SIZE 64
OneWire oneWire(SENSOR_SUHU_PIN);
DallasTemperature sensors(&oneWire);

const int EEPROM_ADDR_SLOPE     = 0;
const int EEPROM_ADDR_NEUTRAL_V = 16;
float ph_slope                  = 0.0;
float ph_neutral_v              = 0.0;

float phB = 0, tdsB = 0, tempB = 0, ecB = 0.0;
const float TDS_TO_EC_FACTOR = 700.0;
unsigned long prevMillisKirim = 0;
const long intervalKirim = 10000; // Kirim data setiap 10 detik

// ---------- FUNGSI BANTU (HELPER FUNCTIONS) ----------
float linInterp(float x, float x0, float x1, float y0, float y1){
  if (x1 - x0 == 0) return y0;
  return y0 + (y1 - y0) * ((x - x0) / (x1 - x0));
}

float readVoltageADC(int pin) {
  const int NUM_SAMPLES = 50;
  const float VREF = 3.3; 
  long sum = 0;
  for (int i = 0; i < NUM_SAMPLES; i++) {
    sum += analogRead(pin);
    delay(2);
  }
  float raw = (float)sum / NUM_SAMPLES;
  return raw / 4095.0 * VREF;
}

float readTemperatureSensor() {
  sensors.requestTemperatures();
  float t = sensors.getTempCByIndex(0);
  if (t == DEVICE_DISCONNECTED_C || t < -10) return 25.0; // fallback
  return t;
}

float hitungEC(float tdsValue) {
  return tdsValue / TDS_TO_EC_FACTOR;
}

// ---------- FUNGSI UTAMA ----------
void bacaSensorB(){
  tempB = readTemperatureSensor();

  float measuredVoltage_TDS = readVoltageADC(SENSOR_TDS_PIN);
  if (measuredVoltage_TDS < VOLTAGE_THRESHOLD_AIR) {
    tdsB = 0.0;
  } else {
    float tdsValue = 0;
    if (measuredVoltage_TDS <= voltage_mid) {
      tdsValue = linInterp(measuredVoltage_TDS, voltage_low, voltage_mid, ppm_low, ppm_mid);
    } else {
      tdsValue = linInterp(measuredVoltage_TDS, voltage_mid, voltage_high, ppm_mid, ppm_high);
    }
    tdsValue = constrain(tdsValue, 0, 5000);
    tdsB = tdsValue / (1.0 + 0.02 * (tempB - 25.0));
  }
  
  ecB = hitungEC(tdsB);

  if (ph_slope != 0) {
    float v_ph = readVoltageADC(SENSOR_PH_PIN);
    float compensated_slope = ph_slope * (tempB + 273.15) / (25.0 + 273.15);
    phB = 7.0 + (ph_neutral_v - v_ph) / compensated_slope;
  } else {
    phB = 7.0;
  }
}

void kirimKeServer(){
  if(WiFi.status() != WL_CONNECTED) return;
  
  HTTPClient http;
  // Perbaikan URL: Pastikan format angka dan parameter benar
  char urlBuffer[256];
  snprintf(urlBuffer, sizeof(urlBuffer), "http://%s/updateB?temp=%.1f&tds=%.0f&ph=%.2f",
           serverIP, tempB, tdsB, phB);

  http.begin(urlBuffer);
  int code = http.GET();
  if(code > 0){
    Serial.printf("-> Data dikirim ke %s, Response: %d\n", serverIP, code);
  } else {
    Serial.printf("-> Gagal mengirim data. Error: %s\n", http.errorToString(code).c_str());
  }
  http.end();
}


void setup(){
  Serial.begin(115200);
  delay(100);

  EEPROM.begin(EEPROM_SIZE);
  EEPROM.get(EEPROM_ADDR_SLOPE, ph_slope);
  EEPROM.get(EEPROM_ADDR_NEUTRAL_V, ph_neutral_v);
  
  bool needsRecalculate = (isnan(ph_slope) || isnan(ph_neutral_v) || ph_slope == 0.0 || FORCE_RECALIBRATE);

  if (needsRecalculate) {
    Serial.println("Menghitung & menyimpan kalibrasi pH baru...");
    ph_slope = (CAL_PH4_VOLTAGE - CAL_PH7_VOLTAGE) / (4.0 - 7.0);
    ph_neutral_v = CAL_PH7_VOLTAGE;

    EEPROM.put(EEPROM_ADDR_SLOPE, ph_slope);
    EEPROM.put(EEPROM_ADDR_NEUTRAL_V, ph_neutral_v);
    EEPROM.commit();
  } else {
    Serial.println("Kalibrasi pH dimuat dari EEPROM.");
  }
  Serial.printf("Slope: %.4f V/pH, Neutral V: %.2f V\n", ph_slope, ph_neutral_v);
  
  lcd.init();
  lcd.backlight();
  sensors.begin();
  
  analogReadResolution(12);
  analogSetPinAttenuation(SENSOR_PH_PIN, ADC_11db);
  analogSetPinAttenuation(SENSOR_TDS_PIN, ADC_11db);

  WiFi.begin(ssid, password);
  Serial.print("Menghubungkan ke WiFi");
  while(WiFi.status() != WL_CONNECTED) {
    delay(500); 
    Serial.print("."); 
  }

  Serial.println("\nWiFi Terhubung! IP: " + WiFi.localIP().toString());
}

void loop(){
  unsigned long now = millis();

  if (WiFi.status() == WL_CONNECTED) {
    if (now - prevMillisKirim >= intervalKirim) {
      prevMillisKirim = now;
      
      bacaSensorB();

      lcd.clear();
      lcd.setCursor(0,0);
      lcd.printf("T:%.1fC EC:%.2f", tempB, ecB);
      lcd.setCursor(0,1);
      lcd.printf("pH:%.2f TDS:%d", phB, (int)tdsB);
      
      Serial.printf("Temp: %.1f C | TDS: %.0f ppm | pH: %.2f | EC: %.2f\n", tempB, tdsB, phB, ecB);
      
      kirimKeServer();
    }
  }
  delay(10);
}