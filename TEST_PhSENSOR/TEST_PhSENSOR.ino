#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// --- Konfigurasi Hardware ---
const int LCD_ADDR = 0x27;
const int LCD_COLS = 16;
const int LCD_ROWS = 2;
const int PH_SENSOR_PIN = 5;
const int SAMPLE_COUNT = 10;

// --- Inisialisasi Objek ---
LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);

// --- Variabel & Kalibrasi ---
const float CALIBRATION_VALUE = 22.84; // Nilai offset kalibrasi (21.34 + 1.5)
int buffer_arr[SAMPLE_COUNT];
float ph_act;

void setup() {
  Serial.begin(115200);

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("pH Sensor Ready");
  delay(2000);
  lcd.clear();
}

void loop() {
  // 1. Ambil sampel data analog
  for (int i = 0; i < SAMPLE_COUNT; i++) {
    buffer_arr[i] = analogRead(PH_SENSOR_PIN);
    delay(30);
  }

  // 2. Urutkan data untuk membuang nilai ekstrim (outlier)
  for (int i = 0; i < SAMPLE_COUNT - 1; i++) {
    for (int j = i + 1; j < SAMPLE_COUNT; j++) {
      if (buffer_arr[i] > buffer_arr[j]) {
        int temp = buffer_arr[i];
        buffer_arr[i] = buffer_arr[j];
        buffer_arr[j] = temp;
      }
    }
  }

  // 3. Ambil rata-rata dari 6 data di tengah
  unsigned long avg_adc_sum = 0;
  for (int i = 2; i < 8; i++) {
    avg_adc_sum += buffer_arr[i];
  }

  // 4. Konversi ke tegangan dan hitung nilai pH
  float voltage = (float)avg_adc_sum * 3.3 / 4095.0 / 6.0;
  ph_act = -5.70 * voltage + CALIBRATION_VALUE;

  // 5. Tampilkan hasil
  Serial.print("pH Value: ");
  Serial.println(ph_act, 2);

  lcd.setCursor(0, 0);
  lcd.print("pH Value:");
  lcd.setCursor(0, 1);
  lcd.print(ph_act, 2);
  lcd.print("   "); // Membersihkan sisa karakter dari tampilan sebelumnya

  delay(1000);
}