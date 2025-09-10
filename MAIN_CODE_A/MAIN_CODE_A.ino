#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <EEPROM.h>
#include "GravityTDS.h"

// Deklarasi untuk sensor pH
#define PH_SENSOR_PIN 34
LiquidCrystal_I2C lcd(0x27, 16, 2);
float calibration_value = 21.34 - 2.87;
unsigned long int avgval_pH;
int buffer_arr_pH[10], temp_pH;
float ph_act;

// Deklarasi untuk sensor suhu DS18B20
const int oneWireBus = 2;
OneWire oneWire(oneWireBus);
DallasTemperature sensors(&oneWire);

// Deklarasi untuk sensor TDS
#define TdsSensorPin 35
#define EEPROM_SIZE 512
GravityTDS gravityTds;
float temperature, tdsValue;

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Inisialisasi LCD
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Sensor Ready!");
  delay(2000);
  lcd.clear();

  // Inisialisasi sensor DS18B20
  sensors.begin();

  // Inisialisasi EEPROM dan sensor TDS
  EEPROM.begin(EEPROM_SIZE);
  gravityTds.setPin(TdsSensorPin);
  gravityTds.setAref(3.3);
  gravityTds.setAdcRange(4096);
  gravityTds.begin();

  Serial.println("DS18B20, pH, dan TDS Sensor siap.");
}

void loop() {
  // === Bagian untuk Sensor Suhu DS18B20 ===
  sensors.requestTemperatures();
  temperature = sensors.getTempCByIndex(0);

  if (temperature == DEVICE_DISCONNECTED_C) {
    Serial.println("Sensor suhu tidak terbaca!");
    temperature = 25; // Gunakan nilai default jika sensor tidak terbaca
  } else {
    Serial.print("Suhu: ");
    Serial.print(temperature, 2);
    Serial.println(" °C");

    // Tampilkan suhu di LCD baris pertama
    lcd.setCursor(0, 0);
    lcd.print("Suhu: ");
    lcd.print(temperature, 2);
    lcd.print(" C");
  }

  // === Bagian untuk Sensor pH ===
  for (int i = 0; i < 10; i++) {
    buffer_arr_pH[i] = analogRead(PH_SENSOR_PIN);
    delay(30);
  }

  // Urutkan data
  for (int i = 0; i < 9; i++) {
    for (int j = i + 1; j < 10; j++) {
      if (buffer_arr_pH[i] > buffer_arr_pH[j]) {
        temp_pH = buffer_arr_pH[i];
        buffer_arr_pH[i] = buffer_arr_pH[j];
        buffer_arr_pH[j] = temp_pH;
      }
    }
  }

  // Ambil rata-rata dari data tengah
  avgval_pH = 0;
  for (int i = 2; i < 8; i++) {
    avgval_pH += buffer_arr_pH[i];
  }

  // Konversi ke voltase dan pH
  float volt = (float)avgval_pH * 3.3 / 4095.0 / 6;
  ph_act = -5.70 * volt + calibration_value;

  // Tampilkan pH di Serial Monitor
  Serial.print("pH Value: ");
  Serial.println(ph_act);

  // Tampilkan pH di LCD baris kedua
  lcd.setCursor(0, 1);
  lcd.print("pH: ");
  lcd.print(ph_act, 2);

  // === Bagian untuk Sensor TDS ===
  gravityTds.setTemperature(temperature); // set the temperature and execute temperature compensation
  gravityTds.update(); //sample and calculate
  tdsValue = gravityTds.getTdsValue();  // then get the value
  Serial.print("TDS Value: ");
  Serial.print(tdsValue, 0);
  Serial.println("ppm");

  delay(2000); // Penundaan untuk stabilitas pembacaan
  lcd.clear(); // Bersihkan LCD untuk tampilan berikutnya
}