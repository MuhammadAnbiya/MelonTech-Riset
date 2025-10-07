// Menambahkan definisi manual untuk LED_BUILTIN di pin GPIO 2
#define LED_BUILTIN 2

/*
  Kode Blink Sederhana untuk ESP32
  Tujuan: Mengetes apakah board ESP32 berfungsi dengan baik.
*/

void setup() {
  // Fungsi setup() hanya berjalan sekali saat ESP32 pertama kali nyala atau di-reset.
  
  // Sekarang, pinMode akan tahu bahwa LED_BUILTIN adalah pin 2.
  pinMode(LED_BUILTIN, OUTPUT); 
}

void loop() {
  // Fungsi loop() berjalan terus-menerus setelah setup() selesai.
  
  // 1. Menyalakan LED
  digitalWrite(LED_BUILTIN, HIGH);
  
  // 2. Jeda selama 1 detik (1000 milidetik)
  delay(1000);                     
  
  // 3. Mematikan LED
  digitalWrite(LED_BUILTIN, LOW);
  
  // 4. Jeda lagi selama 1 detik
  delay(1000);                     
}