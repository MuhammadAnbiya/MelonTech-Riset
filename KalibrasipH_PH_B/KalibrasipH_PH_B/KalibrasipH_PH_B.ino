#include <Arduino.h>
#include <EEPROM.h>
#include <OneWire.h>
#include <DallasTemperature.h>



// ph4 1.1706
// ph7 0.7101
// ph10 0.3109


// Konfigurasi pin
const int PIN_PH = 34;     // pin ADC ESP32
const int ONE_WIRE_BUS = 0; // pin data DS18B20
const int NUM_SAMPLES = 50;
const float VREF = 3.3;
#define EEPROM_SIZE 64

// DS18B20
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// Variabel kalibrasi
float V7 = 0.0, Vx = 0.0;
float a = 0.0, b = 0.0;
bool calibrated = false;

void setup() {
  Serial.begin(115200);
  delay(1000);

  // EEPROM
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.get(0, a);
  EEPROM.get(16, b);

  if (!isnan(a) && !isnan(b) && a != 0.0) {
    calibrated = true;
    Serial.printf("Kalibrasi ditemukan: a=%.6f, b=%.6f\n", a, b);
  } else {
    Serial.println("Belum ada kalibrasi, lakukan dulu (2 lalu 3).");
  }

  analogReadResolution(12);
  analogSetPinAttenuation(PIN_PH, ADC_11db);

  sensors.begin(); // start DS18B20
}

float readVoltage(int pin) {
  long sum = 0;
  for (int i = 0; i < NUM_SAMPLES; i++) {
    sum += analogRead(pin);
    delay(10);
  }
  float raw = (float)sum / NUM_SAMPLES;
  return raw / 4095.0 * VREF;
}

float readTemperature() {
  sensors.requestTemperatures();
  return sensors.getTempCByIndex(0); // °C
}

void loop() {
  Serial.println("\nMenu:");
  Serial.println("1 = Baca tegangan, suhu, & pH");
  Serial.println("2 = Kalibrasi pH7 (celup ke buffer pH7, tekan 2)");
  Serial.println("3 = Kalibrasi pH4/10 (celup ke buffer pH4/10, tekan 3)");
  Serial.println("4 = Simpan kalibrasi ke EEPROM");
  Serial.println("5 = Reset kalibrasi");

  while (!Serial.available()) delay(100);
  int cmd = Serial.parseInt();

  if (cmd == 1) {
    float V = readVoltage(PIN_PH);
    float tempC = readTemperature();
    Serial.printf("Tegangan = %.4f V, Suhu = %.2f °C\n", V, tempC);

    if (calibrated) {
      // Hitung pH tanpa kompensasi
      float ph = a * V + b;

      // Koreksi slope berdasarkan suhu
      float slope25 = a; // slope dari kalibrasi (25°C)
      float slopeT = slope25 * ((tempC + 273.15) / 298.15); // Kelvin ratio
      float phTempComp = (slopeT / slope25) * (a * V) + b;

      Serial.printf("pH (tanpa kompensasi) = %.2f\n", ph);
      Serial.printf("pH (kompensasi suhu)  = %.2f\n", phTempComp);
    } else {
      Serial.println("Belum ada kalibrasi.");
    }
  }

  else if (cmd == 2) {
    V7 = readVoltage(PIN_PH);
    Serial.printf("V7 (pH7) = %.4f V\n", V7);
  }

  else if (cmd == 3) {
    Vx = readVoltage(PIN_PH);
    Serial.printf("Vx (pH4/10) = %.4f V\n", Vx);

    if (V7 > 0 && Vx > 0) {
      float pH7 = 7.0;
      float pHx = 4.0; // ganti jadi 10.0 kalau pakai buffer pH10
      a = (pHx - pH7) / (Vx - V7);
      b = pH7 - a * V7;
      calibrated = true;
      Serial.printf("Kalibrasi selesai: a=%.6f, b=%.6f\n", a, b);
    } else {
      Serial.println("Kalibrasi gagal, pastikan sudah ambil pH7 (command 2).");
    }
  }

  else if (cmd == 4) {
    EEPROM.put(0, a);
    EEPROM.put(16, b);
    EEPROM.commit();
    Serial.println("Kalibrasi disimpan ke EEPROM.");
  }

  else if (cmd == 5) {
    a = 0.0; b = 0.0; calibrated = false;
    EEPROM.put(0, a);
    EEPROM.put(16, b);
    EEPROM.commit();
    Serial.println("Kalibrasi direset.");
  }

  else {
    Serial.println("Perintah tidak dikenal.");
  }

  delay(500);
}
