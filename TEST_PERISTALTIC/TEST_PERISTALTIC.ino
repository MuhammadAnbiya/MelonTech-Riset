// Menentukan pin GPIO mana pada ESP32 yang terhubung ke pin IN relay
// Kita tetap menggunakan GPIO 5, yang ada di hampir semua board ESP32
const int relayPin = 5;

// Waktu jeda dalam milidetik (3 detik = 3000 ms)
const long interval = 3000;

void setup() {
  // Mulai komunikasi serial untuk debugging (opsional)
  Serial.begin(115200);
  Serial.println("Kontrol Pompa Peristaltik dengan Relay (ESP32)");

  // Atur relayPin sebagai OUTPUT
  pinMode(relayPin, OUTPUT);

  // PENTING: Atur kondisi awal relay ke NON-AKTIF.
  // Untuk modul relay active-LOW, kita set ke HIGH agar pompa mati saat program dimulai.
  digitalWrite(relayPin, HIGH);
}

void loop() {
  // 1. Menyalakan Pompa
  Serial.println("Pompa Menyala...");
  digitalWrite(relayPin, LOW); // Mengirim sinyal LOW untuk MENGAKTIFKAN relay
  delay(interval);             // Tunggu selama 3 detik

  // 2. Mematikan Pompa
  Serial.println("Pompa Mati.");
  digitalWrite(relayPin, HIGH); // Mengirim sinyal HIGH untuk MEMATIKAN relay
  delay(interval);              // Tunggu selama 3 detik
}