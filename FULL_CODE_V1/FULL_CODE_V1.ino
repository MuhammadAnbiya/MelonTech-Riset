/******************************************************************************
 * Proyek: Smart Watering Melon Tech Nusa Putra Riset BIMA
 * Hardware:
 * - ESP32 DevKit V1 (30-pin)
 * - Sensor pH Generik (Analog)
 * - DFRobot Analog TDS Sensor
 * - DFRobot Waterproof DS18B20 Temperature Sensor
 * - LCD I2C 20x4
 * - 3-Channel Relay Module
 * - 2x Pompa Peristaltik, 1x Aerator
 *
 * Dibuat oleh: Anbiya & Panji
 * Tanggal Update: 13 September 2025
 ******************************************************************************/

// --- LIBRARIES ---
#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <LiquidCrystal_I2C.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <DFRobot_EC.h>

// --- PENGATURAN JARINGAN ---
const char* ssid = "anbi";
const char* password = "88888888";

// --- PENGATURAN LCD ---
LiquidCrystal_I2C lcd(0x27, 20, 4); 

// --- PENGATURAN PIN ---
#define SENSOR_TDS_PIN      34
#define SENSOR_PH_PIN       35
#define SENSOR_SUHU_PIN     4

#define RELAY_POMPA_A_PIN   25
#define RELAY_POMPA_B_PIN   26
#define RELAY_AERATOR_PIN   14

// --- PENTING: KONSTANTA KALIBRASI SENSOR PH GENERIK ---
#define PH_OFFSET 14.30 
#define PH_SLOPE -5.00

// --- VARIABEL GLOBAL ---
float voltage = 3.3;
float phValue, tdsValue, tempValue;
bool modeOtomatis = true;

// Inisialisasi Objek
AsyncWebServer server(80);
OneWire oneWire(SENSOR_SUHU_PIN);
DallasTemperature sensors(&oneWire);
DFRobot_EC tds;

// Variabel untuk timer non-blocking
unsigned long previousMillis = 0;
const long interval = 5000; 

// --- KODE HTML WEB SERVER ---
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>ESP32 Smart Watering Control</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    html { font-family: Arial, Helvetica, sans-serif; display: inline-block; text-align: center; }
    h2 { font-size: 2.3rem; }
    p { font-size: 1.9rem; }
    body { max-width: 600px; margin:0px auto; padding-bottom: 25px; }
    .sensor-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 20px; margin-bottom: 20px;}
    .sensor { padding: 20px; background: #f2f2f2; border-radius: 10px; }
    .switch-grid { display: grid; grid-template-columns: 1fr 1fr 1fr; gap: 10px; margin-bottom: 20px;}
    .button { background-color: #4CAF50; border: none; color: white; padding: 16px 20px;
              text-decoration: none; font-size: 20px; margin: 2px; cursor: pointer; border-radius: 10px;}
    .button-off { background-color: #555555; }
    #mode-auto { background-color: #008CBA; width: 100%; }
  </style>
</head>
<body>
  <h2>ESP32 Smart Watering</h2>
  
  <div class="sensor-grid">
    <div class="sensor"><strong>Suhu:</strong> <span id="suhu">--</span> &deg;C</div>
    <div class="sensor"><strong>TDS:</strong> <span id="tds">--</span> ppm</div>
    <div class="sensor"><strong>pH:</strong> <span id="ph">--</span></div>
    <div class="sensor"><strong>Mode:</strong> <span id="mode">--</span></div>
  </div>

  <h3>Kontrol Manual</h3>
  <div class="switch-grid">
    <button onclick="toggleRelay(1)" id="btn1" class="button button-off">Pompa A</button>
    <button onclick="toggleRelay(2)" id="btn2" class="button button-off">Pompa B</button>
    <button onclick="toggleRelay(3)" id="btn3" class="button button-off">Aerator</button>
  </div>
  <button onclick="setAutoMode()" id="mode-auto" class="button">Kembali ke Mode Otomatis</button>

<script>
function toggleRelay(id) {
  var xhr = new XMLHttpRequest();
  xhr.open("GET", "/relay?id=" + id, true);
  xhr.send();
}

function setAutoMode() {
  var xhr = new XMLHttpRequest();
  xhr.open("GET", "/mode?auto=1", true);
  xhr.send();
}

setInterval(function () {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      var data = JSON.parse(this.responseText);
      document.getElementById("suhu").innerHTML = data.suhu.toFixed(2);
      document.getElementById("tds").innerHTML = data.tds.toFixed(2);
      document.getElementById("ph").innerHTML = data.ph.toFixed(2);
      document.getElementById("mode").innerHTML = data.mode;
      
      document.getElementById("btn1").className = data.relay1 == 1 ? "button" : "button button-off";
      document.getElementById("btn2").className = data.relay2 == 1 ? "button" : "button button-off";
      document.getElementById("btn3").className = data.relay3 == 1 ? "button" : "button button-off";
    }
  };
  xhttp.open("GET", "/data", true);
  xhttp.send();
}, 2000);
</script>
</body>
</html>
)rawliteral";

// ----------------- SENSOR -----------------
void bacaSemuaSensor() {
  sensors.requestTemperatures(); 
  tempValue = sensors.getTempCByIndex(0);
  if(tempValue == DEVICE_DISCONNECTED_C || tempValue < -10) {
    tempValue = 25.0;
  }

  float v_tds = analogRead(SENSOR_TDS_PIN) / 4095.0 * voltage;
  tdsValue = tds.readEC(v_tds, tempValue);

  float v_ph = analogRead(SENSOR_PH_PIN) / 4095.0 * 5.0; 
  phValue = PH_OFFSET + (v_ph - 2.5) * PH_SLOPE; 
}

void updateLCD() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Suhu: " + String(tempValue, 2) + " C");
  lcd.setCursor(0, 1);
  lcd.print("TDS : " + String(tdsValue, 1) + " ppm");
  lcd.setCursor(0, 2);
  lcd.print("pH  : " + String(phValue, 2));
  lcd.setCursor(0, 3);
  lcd.print("Mode: " + String(modeOtomatis ? "Otomatis" : "Manual"));
}

// ----------------- FUZZY LOGIC MANUAL -----------------
void fuzzyLogicManual() {
  bool outPompaA = false;
  bool outPompaB = false;
  bool outAerator = false;

  if (phValue > 7.5) outPompaA = true;      // Basa
  if (phValue < 6) outPompaB = true;        // Asam
  if (tdsValue < 400) { outPompaA = true; outPompaB = true; }
  if (tempValue > 28) outAerator = true;    // Suhu tinggi
  if (tdsValue > 1000) { outPompaA = false; outPompaB = false; }

  digitalWrite(RELAY_POMPA_A_PIN, outPompaA ? LOW : HIGH);
  digitalWrite(RELAY_POMPA_B_PIN, outPompaB ? LOW : HIGH);
  digitalWrite(RELAY_AERATOR_PIN, outAerator ? LOW : HIGH);
}

// ----------------- SETUP -----------------
void setup() {
  Serial.begin(115200);

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0,0);
  lcd.print("System Starting...");

  pinMode(RELAY_POMPA_A_PIN, OUTPUT);
  pinMode(RELAY_POMPA_B_PIN, OUTPUT);
  pinMode(RELAY_AERATOR_PIN, OUTPUT);
  digitalWrite(RELAY_POMPA_A_PIN, HIGH);
  digitalWrite(RELAY_POMPA_B_PIN, HIGH);
  digitalWrite(RELAY_AERATOR_PIN, HIGH);

  sensors.begin();
  tds.begin();

  // ---- MODE STATION (STA) ----
  WiFi.begin(ssid, password);
  Serial.print("Menghubungkan ke WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nTerhubung ke WiFi!");
  Serial.print("IP ESP32: ");
  Serial.println(WiFi.localIP());

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html);
  });
  server.on("/data", HTTP_GET, [](AsyncWebServerRequest *request){ 
    String json = "{"; 
    json += "\"suhu\":" + String(tempValue) + ",";
    json += "\"tds\":" + String(tdsValue) + ",";
    json += "\"ph\":" + String(phValue) + ",";
    json += "\"mode\":\"" + String(modeOtomatis ? "Otomatis" : "Manual") + "\",";
    json += "\"relay1\":" + String(digitalRead(RELAY_POMPA_A_PIN) == LOW ? 1 : 0) + ",";
    json += "\"relay2\":" + String(digitalRead(RELAY_POMPA_B_PIN) == LOW ? 1 : 0) + ",";
    json += "\"relay3\":" + String(digitalRead(RELAY_AERATOR_PIN) == LOW ? 1 : 0);
    json += "}"; 
    request->send(200, "application/json", json); 
  });
  server.on("/relay", HTTP_GET, [](AsyncWebServerRequest *request){ 
    if (request->hasParam("id")) { 
      int id = request->getParam("id")->value().toInt(); 
      modeOtomatis = false; 
      int pinToToggle = -1; 
      if (id == 1) pinToToggle = RELAY_POMPA_A_PIN; 
      else if (id == 2) pinToToggle = RELAY_POMPA_B_PIN; 
      else if (id == 3) pinToToggle = RELAY_AERATOR_PIN; 
      if (pinToToggle != -1) { digitalWrite(pinToToggle, !digitalRead(pinToToggle)); } 
    } 
    request->send(200, "text/plain", "OK"); 
  });
  server.on("/mode", HTTP_GET, [](AsyncWebServerRequest *request){ 
    if (request->hasParam("auto")) { modeOtomatis = true; } 
    request->send(200, "text/plain", "OK"); 
  });

  server.begin();
  Serial.println("Web server started.");
}

// ----------------- LOOP -----------------
void loop() {
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    bacaSemuaSensor();
    Serial.println("\n--- Sensor Readings ---");
    Serial.printf("Suhu: %.2f *C\n", tempValue);
    Serial.printf("TDS: %.2f ppm\n", tdsValue);
    Serial.printf("pH: %.2f\n", phValue);

    updateLCD();

    if (modeOtomatis) {
      Serial.println("Mode: Otomatis. Menjalankan fuzzy manual...");
      fuzzyLogicManual();
    } else {
      Serial.println("Mode: Manual. Kontrol dari Web.");
    }
  }
}
