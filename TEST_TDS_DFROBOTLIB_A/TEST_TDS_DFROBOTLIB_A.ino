#include <OneWire.h>
#include <DallasTemperature.h>

// ---------- PENGATURAN PIN SENSOR ----------
#define TdsSensorPin 34
#define TempSensorPin 4

// ======================================================================
// --- DATA KALIBRASI ASLI ANDA TELAH DIMASUKKAN DI SINI ---
// ======================================================================
// Titik 1: Air dengan TDS Rendah
const float voltage_clean = 0.24; // Hasil ukur Anda untuk 228 ppm
const float ppm_clean     = 228.0;

// Titik 2: Larutan Standar Sedang
const float voltage_mid   = 1.41; // Hasil ukur Anda untuk 869 ppm
const float ppm_mid       = 869.0;

// Titik 3: Larutan Standar Tinggi
const float voltage_high  = 2.03; // Hasil ukur Anda untuk 1369 ppm
const float ppm_high      = 1369.0;
// ======================================================================

// Inisialisasi Sensor Suhu
OneWire oneWire(TempSensorPin);
DallasTemperature sensors(&oneWire);

void setup() {
  Serial.begin(115200);
  sensors.begin(); // Mulai sensor suhu
  Serial.println("Sistem Pengukuran TDS Terkalibrasi Siap.");
}

void loop() {
  // 1. Baca suhu air
  sensors.requestTemperatures(); 
  float temperature = sensors.getTempCByIndex(0);

  // Pencegahan jika sensor suhu error, gunakan nilai standar 25°C
  if(temperature == DEVICE_DISCONNECTED_C || temperature < -5) {
    Serial.println("Peringatan: Gagal membaca suhu, menggunakan nilai default 25 C");
    temperature = 25.0;
  }
  
  // 2. Baca nilai tegangan dari sensor TDS
  int rawValue = analogRead(TdsSensorPin);
  float measuredVoltage = rawValue / 4095.0 * 3.3;

  float tdsValue = 0;

  // 3. Logika Pemetaan Cerdas dengan 3 Titik Kalibrasi
  if (measuredVoltage <= voltage_mid) {
    // Jika tegangan berada di RENTANG RENDAH, gunakan kalibrasi antara titik 'clean' dan 'mid'
    tdsValue = map(measuredVoltage * 1000, voltage_clean * 1000, voltage_mid * 1000, ppm_clean, ppm_mid);
  } else {
    // Jika tegangan berada di RENTANG TINGGI, gunakan kalibrasi antara titik 'mid' dan 'high'
    tdsValue = map(measuredVoltage * 1000, voltage_mid * 1000, voltage_high * 1000, ppm_mid, ppm_high);
  }
  
  // 4. Pastikan nilai tidak menjadi negatif (jika tegangan sedikit di bawah titik terendah)
  tdsValue = constrain(tdsValue, 0, 5000); // Batasi nilai TDS antara 0 dan 5000 ppm

  // 5. Lakukan kompensasi suhu untuk akurasi tertinggi
  float compensatedTds = tdsValue / (1.0 + 0.02 * (temperature - 25.0));

  // 6. Tampilkan semua hasilnya di Serial Monitor
  Serial.print("Suhu: ");
  Serial.print(temperature, 1);
  Serial.print(" C | Tegangan TDS: ");
  Serial.print(measuredVoltage, 2);
  Serial.print(" V | TDS Final Terkompensasi: ");
  Serial.print(compensatedTds, 0); // Tampilkan hasil akhir tanpa desimal
  Serial.println(" ppm");

  delay(2000); // Jeda 2 detik antar pengukuran
}