/******************************************************************************
 * Proyek: Smart Watering Melon Tech Nusa Putra Riset BIMA
 * Hardware:
 * - ESP32 DevKit V1 (30-pin)
 * - Sensor pH Generik (Analog)
 * - DFRobot Analog TDS Sensor
 * - DFRobot Waterproof DS18B20 Temperature Sensor
 * - LCD I2C 16x2
 * - 3-Channel Relay Module
 * - 2x Pompa Peristaltik, 1x Aerator
 ******************************************************************************/

#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <LiquidCrystal_I2C.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <DFRobot_EC.h>

// ---------- CONFIG ----------
const char* ssid = "anbi";
const char* password = "88888888";

LiquidCrystal_I2C lcd(0x27, 16, 2);

#define SENSOR_TDS_PIN      34
#define SENSOR_PH_PIN       35
#define SENSOR_SUHU_PIN     4

#define RELAY_POMPA_A_PIN   25
#define RELAY_POMPA_B_PIN   26
#define RELAY_AERATOR_PIN   14

#define PH_OFFSET 14.30
#define PH_SLOPE -5.00

// --- Karakter Kustom untuk Simbol Derajat (°) ---
byte degree_char[8] = {
  B00110, B01001, B01001, B00110,
  B00000, B00000, B00000, B00000
};

// --- VARIABEL GLOBAL ---
float voltage = 3.3;
float phA=0, tdsA=0, tempA=0;
float phB=0, tdsB=0, tempB=0;
bool modeOtomatis = true;

AsyncWebServer server(80);
OneWire oneWire(SENSOR_SUHU_PIN);
DallasTemperature sensors(&oneWire);
DFRobot_EC tds;

unsigned long prevMillis = 0;
const long interval = 5000;

// --- Web UI  ---
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>ESP32 Smart Watering</title>
<style>
body{margin:0;font-family:Arial;background:#0f2027;
background:linear-gradient(315deg,#0f2027,#203a43,#2c5364);color:#fff;text-align:center;}
h2{margin:20px 0;}
.card{background:rgba(255,255,255,0.08);padding:20px;margin:10px;border-radius:12px;box-shadow:0 0 10px #000;}
table{width:100%;border-collapse:collapse;color:#fff;font-size:14px;}
td,th{padding:10px;}
button{padding:10px 18px;margin:5px;border:none;border-radius:8px;cursor:pointer;
color:white;font-size:14px;transition:0.3s;}
.on{background:#27ae60;}    /* hijau */
.off{background:#c0392b;}   /* merah */
</style></head><body>
<h2>ESP32 Smart Watering</h2>

<div class="card">
<h3>Data Sensor</h3>
<table>
<tr><th>Parameter</th><th>Sensor A</th><th>Sensor B</th></tr>
<tr><td>Suhu (&deg;C)</td><td id="sa">--</td><td id="sb">--</td></tr>
<tr><td>TDS (ppm)</td><td id="ta">--</td><td id="tb">--</td></tr>
<tr><td>pH</td><td id="pa">--</td><td id="pb">--</td></tr>
</table>
</div>

<div class="card">
<h3>Kontrol</h3>
<button id="modeBtn" onclick="toggleMode()">Mode</button><br>
<button id="r1" onclick="toggleRelay(1)">Pompa A</button>
<button id="r2" onclick="toggleRelay(2)">Pompa B</button>
<button id="r3" onclick="toggleRelay(3)">Aerator</button>
</div>

<script>
function refresh(){
  fetch('/data').then(r=>r.json()).then(d=>{
    sa.innerText = d.suhuA.toFixed(0);
    sb.innerText = d.suhuB.toFixed(0);
    ta.innerText = d.tdsA.toFixed(0);
    tb.innerText = d.tdsB.toFixed(0);
    pa.innerText = d.phA.toFixed(1);
    pb.innerText = d.phB.toFixed(1);

    if(d.mode === "auto"){
      modeBtn.innerText = "Mode: Otomatis";
      modeBtn.className = "on";
    } else {
      modeBtn.innerText = "Mode: Manual";
      modeBtn.className = "off";
    }

    document.getElementById("r1").className = d.relay1 ? "on" : "off";
    document.getElementById("r2").className = d.relay2 ? "on" : "off";
    document.getElementById("r3").className = d.relay3 ? "on" : "off";
  });
}
function toggleRelay(id){ fetch('/relay?id='+id).then(refresh); }
function toggleMode(){ fetch('/mode?toggle=1').then(refresh); }
setInterval(refresh,2000); refresh();
</script>
</body></html>
)rawliteral";

// --- FUNGSI SENSOR  ---
void bacaSensorA(){
  sensors.requestTemperatures();
  tempA = sensors.getTempCByIndex(0);
  if(tempA==DEVICE_DISCONNECTED_C || tempA < -10) tempA = 25.0;

  float v_tds = analogRead(SENSOR_TDS_PIN) / 4095.0 * voltage;
  tdsA = tds.readEC(v_tds, tempA);

  float v_ph = analogRead(SENSOR_PH_PIN) / 4095.0 * 5.0;
  phA = PH_OFFSET + (v_ph - 2.5) * PH_SLOPE;
}

void updateSensorB(float ph, float tds, float temp){ phB=ph; tdsB=tds; tempB=temp; }

// --- FUNGSI LCD UNTUK 16x2 ---
void updateLCD16x2() {
  lcd.clear();


lcd.setCursor(0, 0);
lcd.print("S:");
lcd.print(tempA, 0);   // tampil 26
lcd.write(223);        // simbol derajat (°)
lcd.print("C ");

lcd.print("OD:");
lcd.print(tdsA, 0);    // tampil 100
lcd.print("ppm");

lcd.setCursor(0, 1);
lcd.print("pH:");
lcd.print(phA, 0);     // tampil 6.8

lcd.print("Mode:");
lcd.print(modeOtomatis ? "Auto " : "Manual");
}

// --- FUNGSI LOGIKA KONTROL OTOMATIS ---
void logicController() {
  bool outA = false, outB = false, outAer = false;

  if (tempA > 28) outAer = true;

  if (tdsA > 1000) {
    outA = false; outB = false;
  } else if (tdsA < 400) {
    outA = true; outB = true;
  } else {
    if (phA > 7.5) outA = true;
    else if (phA < 6) outB = true;
  }
  
  digitalWrite(RELAY_POMPA_A_PIN, outA ? LOW : HIGH);
  digitalWrite(RELAY_POMPA_B_PIN, outB ? LOW : HIGH);
  digitalWrite(RELAY_AERATOR_PIN, outAer ? LOW : HIGH);
}

// --- SETUP ---
void setup(){
  Serial.begin(115200);
  
  lcd.init();
  lcd.backlight();
  lcd.createChar(0, degree_char);

  pinMode(RELAY_POMPA_A_PIN, OUTPUT);
  pinMode(RELAY_POMPA_B_PIN, OUTPUT);
  pinMode(RELAY_AERATOR_PIN, OUTPUT);
  digitalWrite(RELAY_POMPA_A_PIN,HIGH);
  digitalWrite(RELAY_POMPA_B_PIN,HIGH);
  digitalWrite(RELAY_AERATOR_PIN,HIGH);

  sensors.begin(); tds.begin();

  WiFi.begin(ssid,password);
  Serial.print("Connecting to WiFi");
  while(WiFi.status()!=WL_CONNECTED){ delay(500); Serial.print("."); }
  Serial.println("\nIP: " + WiFi.localIP().toString());

  server.on("/",HTTP_GET,[](AsyncWebServerRequest *r){ r->send_P(200,"text/html",index_html); });
  server.on("/data",HTTP_GET,[](AsyncWebServerRequest *r){
    String json="{";
    json+="\"suhuA\":"+String(tempA,0)+",\"tdsA\":"+String(tdsA,0)+",\"phA\":"+String(phA,1)+",";
    json+="\"suhuB\":"+String(tempB,0)+",\"tdsB\":"+String(tdsB,0)+",\"phB\":"+String(phB,1)+",";
    json+="\"mode\":\""+String(modeOtomatis?"auto":"manual")+"\",";
    json += "\"relay1\":" + String(digitalRead(RELAY_POMPA_A_PIN)==LOW ? "true" : "false") + ",";
    json += "\"relay2\":" + String(digitalRead(RELAY_POMPA_B_PIN)==LOW ? "true" : "false") + ",";
    json += "\"relay3\":" + String(digitalRead(RELAY_AERATOR_PIN)==LOW ? "true" : "false");
    json+="}";
    r->send(200,"application/json",json);
  });
  server.on("/updateB",HTTP_GET,[](AsyncWebServerRequest *r){
    if(r->hasParam("ph")&&r->hasParam("tds")&&r->hasParam("temp")){
      updateSensorB(r->getParam("ph")->value().toFloat(),
                    r->getParam("tds")->value().toFloat(),
                    r->getParam("temp")->value().toFloat());
      r->send(200,"text/plain","OK");
    } else r->send(400,"text/plain","Missing params");
  });
  server.on("/relay",HTTP_GET,[](AsyncWebServerRequest *r){
    int id=r->getParam("id")->value().toInt();
    int pin=(id==1?RELAY_POMPA_A_PIN:id==2?RELAY_POMPA_B_PIN:id==3?RELAY_AERATOR_PIN:-1);
    if(pin!=-1){ digitalWrite(pin,!digitalRead(pin)); modeOtomatis=false; }
    r->send(200,"text/plain","OK");
  });
  server.on("/mode",HTTP_GET,[](AsyncWebServerRequest *r){
    if(r->hasParam("toggle")) modeOtomatis=!modeOtomatis;
    else modeOtomatis=true;
    r->send(200,"text/plain","OK");
  });

  server.begin();
}

// --- LOOP UTAMA ---
void loop(){
  unsigned long now=millis();
  if(now-prevMillis>=interval){
    prevMillis=now;
    bacaSensorA();
    updateLCD16x2();
    if(modeOtomatis){
      logicController();
    }
  }
}
