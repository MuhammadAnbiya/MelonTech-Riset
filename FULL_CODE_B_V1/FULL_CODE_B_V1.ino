/******************************************************************************
* Proyek: Smart Watering Melon - Modul Sensor B (Client)
* Versi: Disesuaikan untuk ESP32-S3
* Deskripsi: Kode ini diadaptasi untuk berjalan di board ESP32-S3 dengan
* mengubah pin ADC dan menginisialisasi pin I2C secara eksplisit.
******************************************************************************/

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <LiquidCrystal_I2C.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <EEPROM.h>
#include <Wire.h> // [DITAMBAHKAN] Diperlukan untuk inisialisasi I2C

// ---------- KONFIGURASI PENTING ----------
const char* ssid     = "pouio"; // <-- Ganti dengan nama WiFi Anda
const char* password = "11111111"; // <-- Ganti dengan password WiFi Anda
const char* serverIP = "192.168.4.97";   // GANTI dengan IP ESP32 A (Server Utama)

// [BARU] Threshold tegangan untuk mendeteksi sensor di udara
const float VOLTAGE_THRESHOLD_AIR = 0.1; // Tegangan di bawah ini dianggap 0 PPM

// ======================================================================
// --- [DISAMAKAN] KALIBRASI MANUAL SENSOR pH (SAMA DENGAN SERVER) ---
// Ukur tegangan sensor B pada buffer pH 7.0 dan 4.0, lalu masukkan nilainya di sini.
// ======================================================================
const float CAL_PH7_VOLTAGE   = 2.8;  // CONTOH: Ganti dengan hasil pengukuran Anda
const float CAL_PH4_VOLTAGE   = 3.05; // CONTOH: Ganti dengan hasil pengukuran Anda
const bool  FORCE_RECALIBRATE = false; // Set `true` untuk menimpa kalibrasi lama di EEPROM

// ======================================================================
// --- DATA KALIBRASI TDS (3 TITIK) ---
// ======================================================================
const float voltage_low = 1.2780;   
const float ppm_low     = 731.0;
const float voltage_mid   = 2.2155; 
const float ppm_mid       = 1240.0;
const float voltage_high  = 2.2735; 
const float ppm_high      = 2610.0;
// ======================================================================

// ---------- PENGATURAN PERANGKAT KERAS ----------
// [DIUBAH UNTUK S3] Pin I2C akan diinisialisasi di setup()
LiquidCrystal_I2C lcd(0x27, 16, 2); 

// [DIUBAH UNTUK S3] Ganti pin ADC ke pin yang tersedia di ESP32-S3 (ADC1)
#define SENSOR_TDS_PIN  1  // Contoh menggunakan GPIO 1 (ADC1_CH0)
#define SENSOR_PH_PIN   2  // Contoh menggunakan GPIO 2 (ADC1_CH1)
#define SENSOR_SUHU_PIN 4  // GPIO 4 aman digunakan

// ---------- EEPROM & SENSOR SUHU ----------
#define EEPROM_SIZE 64
OneWire oneWire(SENSOR_SUHU_PIN);
DallasTemperature sensors(&oneWire);

// [DISAMAKAN] Alamat EEPROM dan variabel kalibrasi pH
const int EEPROM_ADDR_SLOPE     = 0;
const int EEPROM_ADDR_NEUTRAL_V = 16;
float ph_slope                  = 0.0;
float ph_neutral_v              = 0.0;

// ---------- VARIABEL GLOBAL ----------
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
  const float VREF = 3.3; // Pastikan VREF sesuai dengan board Anda
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
  // 1. Baca Suhu
  tempB = readTemperatureSensor();

  // 2. Baca TDS dengan Kalibrasi 3 Titik dan Zeroing
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
    tdsB = tdsValue / (1.0 + 0.02 * (tempB - 25.0)); // Kompensasi Suhu
  }
  
  ecB = hitungEC(tdsB);

  if (ph_slope != 0) {
    float v_ph = readVoltageADC(SENSOR_PH_PIN);
    float compensated_slope = ph_slope * (tempB + 273.15) / (25.0 + 273.15);
    phB = 7.0 + (ph_neutral_v - v_ph) / compensated_slope;
  } else {
    phB = 7.0; // Fallback
  }
}

void kirimKeServer(){
  if(WiFi.status() != WL_CONNECTED) return;
  
  HTTPClient http;
  String url = String("http://") + serverIP + "/updateB?ph=" + String(phB,2) +
               "&tds=" + String(tdsB,1) + "&temp=" + String(tempB,2);
  
  http.begin(url);
  int code = http.GET();
  if(code > 0){
    Serial.println(String("-> Data terkirim ke ") + serverIP + ", Response: " + String(code));
  } else {
    Serial.println("-> Gagal mengirim data. Cek koneksi/IP Server.");
  }
  http.end();
}

void handleSerialCommands() {
  if (Serial.available() > 0) {
    char command = Serial.read();
    if (command == 'v' || command == 'V') {
      float voltage_ph = readVoltageADC(SENSOR_PH_PIN);
      float voltage_tds = readVoltageADC(SENSOR_TDS_PIN);
      Serial.println("======================================");
      Serial.print("Tegangan Sensor pH  : ");
      Serial.print(voltage_ph, 4);
      Serial.println(" V");
      Serial.print("Tegangan Sensor TDS : ");
      Serial.print(voltage_tds, 4);
      Serial.println(" V");
      Serial.println("======================================");
    }
  }
}

void setup(){
  Serial.begin(115200);
  delay(100);

  EEPROM.begin(EEPROM_SIZE);
  EEPROM.get(EEPROM_ADDR_SLOPE, ph_slope);
  EEPROM.get(EEPROM_ADDR_NEUTRAL_V, ph_neutral_v);
  
  bool needsRecalculate = false;
  if (isnan(ph_slope) || isnan(ph_neutral_v) || ph_slope == 0.0 || FORCE_RECALIBRATE) {
    needsRecalculate = true;
  }

  if (needsRecalculate) {
    Serial.println("Menghitung & menyimpan kalibrasi pH baru ke EEPROM...");
    ph_slope = (CAL_PH4_VOLTAGE - CAL_PH7_VOLTAGE) / (4.0 - 7.0);
    ph_neutral_v = CAL_PH7_VOLTAGE;

    EEPROM.put(EEPROM_ADDR_SLOPE, ph_slope);
    EEPROM.put(EEPROM_ADDR_NEUTRAL_V, ph_neutral_v);
    EEPROM.commit();
    Serial.println("Kalibrasi berhasil disimpan.");
  } else {
    Serial.println("Kalibrasi pH berhasil dimuat dari EEPROM.");
  }
  Serial.printf("Slope: %.4f V/pH, Neutral V: %.2f V\n", ph_slope, ph_neutral_v);
  
  // [DIUBAH UNTUK S3] Inisialisasi pin I2C secara eksplisit sebelum lcd.begin()
  // Pin I2C default di banyak board S3 adalah GPIO8 (SDA) dan GPIO9 (SCL)
  // Periksa kembali pinout board Anda!
  Wire.begin(8, 9); // (SDA, SCL)
  
  lcd.begin(); 
  lcd.backlight();
  sensors.begin();
  
  // Kode ini sudah benar untuk ESP32-S3
  // analogReadResolution(12); // Tidak perlu karena default sudah 12-bit
  // analogSetPinAttenuation(SENSOR_PH_PIN, ADC_11db); // Tidak diperlukan di S3, atenuasi diatur otomatis
  // analogSetPinAttenuation(SENSOR_TDS_PIN, ADC_11db);

  WiFi.begin(ssid, password);
  Serial.print("Menghubungkan ke WiFi");

  int attempts = 0;
  while(WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500); 
    Serial.print("."); 
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi Terhubung! IP Address: " + WiFi.localIP().toString());
  } else {
    Serial.println("\nGAGAL TERHUBUNG.");
    Serial.print("Status WiFi terakhir: ");
    Serial.println(WiFi.status());
    Serial.println("Silakan periksa penyebab umum (Jaringan 2.4GHz, Catu Daya, Sinyal).");
  }
  
  Serial.println("Kirim 'v' melalui Serial Monitor untuk melihat tegangan pH dan TDS sensor.");
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
      
      Serial.printf("Temp: %.1f C | TDS: %.1f ppm | pH: %.2f | EC: %.2f\n", tempB, tdsB, phB, ecB);
      
      kirimKeServer();
    }
  }

  handleSerialCommands();
  delay(10);
}