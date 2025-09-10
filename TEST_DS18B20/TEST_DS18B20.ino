#include <OneWire.h>
#include <DallasTemperature.h>

// Pin data DS18B20 ke ESP32-S3
const int oneWireBus = 2; // Ganti sesuai pin yang digunakan

// Setup oneWire instance
OneWire oneWire(oneWireBus);

// Pass oneWire reference ke DallasTemperature library
DallasTemperature sensors(&oneWire);

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("DS18B20 ESP32-S3 siap...");

  sensors.begin(); // Inisialisasi sensor
}

void loop() {
  sensors.requestTemperatures(); // Minta data suhu dari sensor
  float temperatureC = sensors.getTempCByIndex(0); // Ambil sensor pertama

  // Cek apakah sensor terbaca
  if(temperatureC == DEVICE_DISCONNECTED_C){
    Serial.println("Sensor tidak terbaca!");
  } else {
    Serial.print("Suhu: ");
    Serial.print(temperatureC, 2); // tampilkan 2 angka dibelakang koma
    Serial.println(" °C");
  }

  delay(1000);
}
