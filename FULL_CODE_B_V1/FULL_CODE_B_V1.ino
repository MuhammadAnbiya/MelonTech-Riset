// ESP32 B (Client) - baca sensor B, tampil di LCD16x2, kirim ke ESP32 A
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <EEPROM.h>
#include "GravityTDS.h"

// ---------- CONFIG ----------
const char* ssid = "anbi";
const char* password = "88888888";
const char* serverIP = "10.104.81.97"; // GANTI dengan IP ESP32 A

LiquidCrystal_I2C lcd(0x27, 16, 2); // LCD 16x2 I2C

// Pins sensor B
#define SENSOR_TDS_PIN 35
#define SENSOR_PH_PIN  34
#define SENSOR_SUHU_PIN 4   // ganti ke pin GPIO yang support OneWire (jangan 0)

// EEPROM
#define EEPROM_SIZE 512

OneWire oneWire(SENSOR_SUHU_PIN);
DallasTemperature sensors(&oneWire);

GravityTDS gravityTds;

float phB=0, tdsB=0, tempB=0;

// PH calibration (atur sesuai hasil kalibrasi nyata)
#define PH_OFFSET 7.0
#define PH_SLOPE  0.18

// Faktor kalibrasi TDS
const float slope = 1.66;   // bisa disesuaikan
const float offset = 0.0;

void bacaSensorB(){
  // --- Suhu ---
  sensors.requestTemperatures();
  tempB = sensors.getTempCByIndex(0);
  if(tempB==DEVICE_DISCONNECTED_C || tempB < -10) tempB = 25.0;

  // --- TDS ---
  gravityTds.setTemperature(tempB); // kompensasi suhu
  gravityTds.update();              // refresh data TDS
  float tdsRaw = gravityTds.getTdsValue();
  tdsB = (tdsRaw * slope) + offset;

  // --- pH ---
  float v_ph = (analogRead(SENSOR_PH_PIN) / 4095.0) * 3.3;
  phB = PH_OFFSET + ((2.366 - v_ph) / PH_SLOPE);
}

void kirimKeServer(){
  if(WiFi.status() != WL_CONNECTED) return;
  HTTPClient http;
  String url = String("http://") + serverIP + "/updateB?ph=" + String(phB,2) +
               "&tds=" + String(tdsB,1) + "&temp=" + String(tempB,2);
  http.begin(url);
  int code = http.GET();
  if(code>0){
    Serial.println("Sent -> " + url + " Resp:" + String(code));
  } else {
    Serial.println("Send failed");
  }
  http.end();
}

void setup(){
  Serial.begin(115200);
  Wire.begin(21,22);

  EEPROM.begin(EEPROM_SIZE);

  gravityTds.setPin(SENSOR_TDS_PIN);
  gravityTds.setAref(3.3);      // reference voltage pada ESP32
  gravityTds.setAdcRange(4096); // ADC ESP32 12bit = 0-4095
  gravityTds.begin();           // inisialisasi TDS

  lcd.init(); 
  lcd.backlight();
  sensors.begin();

  WiFi.begin(ssid, password);
  Serial.print("Connecting WiFi");
  int w=0;
  while(WiFi.status()!=WL_CONNECTED && w<40){ 
    delay(500); 
    Serial.print("."); 
    w++; 
  }
  if(WiFi.status()==WL_CONNECTED) 
    Serial.println("\nIP: " + WiFi.localIP().toString());
  else 
    Serial.println("\nWiFi failed");
}

void loop(){
  bacaSensorB();

  // tampil di LCD 16x2
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.printf("T:%.1fC TDS:%dppm", tempB, (int)tdsB);
  lcd.setCursor(0,1);
  lcd.printf("pH:%.2f", phB);

  // log ke serial
  Serial.printf("Temp: %.1f C | TDS: %.1f ppm | pH: %.2f\n", tempB, tdsB, phB);

  // kirim ke server
  kirimKeServer();

  delay(5000); // kirim tiap 5 detik
}
