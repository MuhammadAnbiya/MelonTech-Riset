#define PH_PIN 34
void setup() {
  Serial.begin(115200);
  analogReadResolution(12);
  analogSetPinAttenuation(PH_PIN, ADC_11db);
  Serial.println("Debug ADC start");
}
void loop() {
  int raw = analogRead(PH_PIN);
  float volt = raw * (3.3 / 4095.0);
  Serial.print("Raw: "); Serial.print(raw);
  Serial.print("  Volt: "); Serial.println(volt, 4);
  delay(500);
}
