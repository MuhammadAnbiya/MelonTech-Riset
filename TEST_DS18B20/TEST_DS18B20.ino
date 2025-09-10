const int tdsPin = A0; // ADC ESP8266

void setup() {
  Serial.begin(115200);  // Serial Monitor harus sama: 115200
  delay(1000);
  Serial.println("TDS Sensor ESP8266 siap...");
}

void loop() {
  int sensorValue = analogRead(tdsPin);  
  float voltage = sensorValue * (3.3 / 1023.0); // NodeMCU internal divider 0-3.3V
  float tdsValue = voltage * 500; // faktor kalibrasi sensor, sesuaikan

  Serial.print("ADC: ");
  Serial.print(sensorValue);
  Serial.print("\tVoltage: ");
  Serial.print(voltage, 2);
  Serial.print(" V\tTDS: ");
  Serial.print(tdsValue, 2);
  Serial.println(" ppm");

  delay(1000);
}
