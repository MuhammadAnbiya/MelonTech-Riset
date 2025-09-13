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

LiquidCrystal_I2C lcd(0x27, 20, 4);

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

// --- Web UI (Tidak diubah) ---
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

    // update tombol mode
    if(d.mode === "auto"){
      modeBtn.innerText = "Mode: Otomatis";
      modeBtn.className = "on";
    } else {
      modeBtn.innerText = "Mode: Manual";
      modeBtn.className = "off";
    }

    // update tombol relay
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

// --- FUNGSI SENSOR (Tidak diubah) ---
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

// --- FUNGSI-FUNGSI BARU UNTUK MERAPIKAN LCD ---
// Helper untuk mencetak angka integer rata kanan
void printPaddedNumber(long value, int width) {
  int numDigits = (value == 0) ? 1 : floor(log10(abs(value))) + 1;
  if (value < 0) numDigits++; 
  for (int i = 0; i < width - numDigits; i++) { lcd.print(" "); }
  lcd.print(value);
}

// Helper untuk mencetak angka float rata kanan
void printPaddedNumber(float value, int width, int precision) {
  String str = String(value, precision);
  for (int i = 0; i < width - str.length(); i++) { lcd.print(" "); }
  lcd.print(str);
}

// --- FUNGSI LCD BARU YANG SUDAH RAPI ---
// --- FUNGSI LCD BARU YANG SUDAH DIRAPIKAN ULANG ---
void updateLCDmain(){
  // Definisikan posisi kolom baru untuk layout yang lebih baik
  const int COL_LABEL = 0;     // Posisi awal label (Suhu, TDS, pH)
  const int COL_SEPARATOR = 4; // Posisi untuk "-A:" atau ":"
  const int COL_A_VALUE = 7;   // Posisi awal nilai A
  const int COL_B_LABEL = 13;    // Posisi untuk "B:"
  const int COL_B_VALUE = 15;    // Posisi awal nilai B (lebih banyak ruang)

  // Bersihkan layar sekali saja di awal
  lcd.clear();

  // --- BARIS 1: SUHU ---
  lcd.setCursor(COL_LABEL, 0); lcd.print("Suhu");
  lcd.setCursor(COL_SEPARATOR, 0); lcd.print("-A:");
  lcd.setCursor(COL_A_VALUE, 0); printPaddedNumber((long)tempA, 3);
  lcd.write(byte(0)); // Simbol derajat
  lcd.print("C");
  lcd.setCursor(COL_B_LABEL, 0); lcd.print("B:");
  lcd.setCursor(COL_B_VALUE, 0); printPaddedNumber((long)tempB, 3);
  
  // --- BARIS 2: TDS ---
  lcd.setCursor(COL_LABEL, 1); lcd.print("TDS");
  lcd.setCursor(COL_SEPARATOR, 1); lcd.print("-A:");
  lcd.setCursor(COL_A_VALUE, 1); printPaddedNumber((long)tdsA, 4);
  lcd.setCursor(COL_B_LABEL, 1); lcd.print("B:");
  lcd.setCursor(COL_B_VALUE, 1); printPaddedNumber((long)tdsB, 4);

  // --- BARIS 3: pH ---
  lcd.setCursor(COL_LABEL, 2); lcd.print("pH");
  lcd.setCursor(COL_SEPARATOR, 2); lcd.print("-A:");
  lcd.setCursor(COL_A_VALUE, 2); printPaddedNumber(phA, 4, 1);
  lcd.setCursor(COL_B_LABEL, 2); lcd.print("B:");
  lcd.setCursor(COL_B_VALUE, 2); printPaddedNumber(phB, 4, 1);

  // --- BARIS 4: MODE (dengan perataan ":") ---
  lcd.setCursor(COL_LABEL, 3); lcd.print("Mode");
  // Pindahkan kursor agar ":" sejajar dengan ":" di atasnya (posisi 7)
  lcd.setCursor(COL_SEPARATOR + 2, 3); lcd.print(":");
  lcd.setCursor(COL_A_VALUE, 3);
  lcd.print(modeOtomatis ? "Otomatis" : "Manual");
}

// --- FUNGSI LOGIKA KONTROL OTOMATIS (DIPERBAIKI) ---
void logicController() {
  bool outA = false, outB = false, outAer = false;

  // Logika Aerator
  if (tempA > 28) outAer = true;

  // Logika Pompa dengan Prioritas
  if (tdsA > 1000) { // Prioritas 1: Jika TDS terlalu tinggi, matikan semua
    outA = false;
    outB = false;
  } else if (tdsA < 400) { // Prioritas 2: Jika TDS terlalu rendah, nyalakan keduanya
    outA = true;
    outB = true;
  } else { // Prioritas 3: Jika TDS ideal, baru koreksi pH
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
  lcd.createChar(0, degree_char); // Daftarkan karakter derajat kustom ke LCD

  pinMode(RELAY_POMPA_A_PIN, OUTPUT);
  pinMode(RELAY_POMPA_B_PIN, OUTPUT);
  pinMode(RELAY_AERATOR_PIN, OUTPUT);
  digitalWrite(RELAY_POMPA_A_PIN,HIGH);
  digitalWrite(RELAY_POMPA_B_PIN,HIGH);
  digitalWrite(RELAY_AERATOR_PIN,HIGH);

  sensors.begin(); tds.begin();

  // Koneksi WiFi
  WiFi.begin(ssid,password);
  Serial.print("Connecting to WiFi");
  while(WiFi.status()!=WL_CONNECTED){ delay(500); Serial.print("."); }
  Serial.println("\nIP: " + WiFi.localIP().toString());

  // Web Server Handlers (Tidak diubah)
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
    updateLCDmain(); // Panggil fungsi LCD yang sudah rapi
    if(modeOtomatis){
      logicController(); // Panggil fungsi logika yang sudah diperbaiki
    }
  }
}