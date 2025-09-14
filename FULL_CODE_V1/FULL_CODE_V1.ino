/******************************************************************************
 * Proyek: Smart Watering Melon Tech Nusa Putra Riset BIMA
 * Versi: Final - Dengan Kalibrasi TDS Akurasi Tinggi
 ******************************************************************************/

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ESPAsyncWebServer.h>
#include <LiquidCrystal_I2C.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// ---------- KONFIGURASI PENTING ----------
const char* ssid = "anbi";
const char* password = "88888888";

// URL WEB APP DARI GOOGLE APPS SCRIPT
const char* googleScriptURL = "https://script.google.com/macros/s/AKfycbyDTbuSFk0GOylphGYnyH77oPurvq0hG1Nu5ydcPEyouZ5aDKhGN8Sg8kFctRgTV7Gyfg/exec";

// ======================================================================
// --- DATA KALIBRASI TDS AKURAT ANDA ---
// ======================================================================
const float voltage_clean = 0.24; // Tegangan untuk ~228 ppm
const float ppm_clean     = 228.0;
const float voltage_mid   = 1.41; // Tegangan untuk ~869 ppm
const float ppm_mid       = 869.0;
const float voltage_high  = 2.03; // Tegangan untuk ~1369 ppm
const float ppm_high      = 1369.0;
// ======================================================================

// ---------- PENGATURAN PERANGKAT KERAS ----------
LiquidCrystal_I2C lcd(0x27, 16, 2);

#define SENSOR_TDS_PIN      34
#define SENSOR_PH_PIN       35
#define SENSOR_SUHU_PIN     4

#define RELAY_POMPA_A_PIN   25
#define RELAY_POMPA_B_PIN   26
#define RELAY_AERATOR_PIN   14

#define PH_OFFSET 14.30
#define PH_SLOPE -5.00

byte degree_char[8] = {
  B00110, B01001, B01001, B00110,
  B00000, B00000, B00000, B00000
};

// ---------- VARIABEL & OBJEK GLOBAL ----------
float voltage = 3.3;
float phA=0, tdsA=0, tempA=0;
float phB=0, tdsB=0, tempB=0;
bool modeOtomatis = true;

AsyncWebServer server(80);
OneWire oneWire(SENSOR_SUHU_PIN);
DallasTemperature sensors(&oneWire);

unsigned long prevMillis = 0;
const long interval = 15000;

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html lang="en"><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width, initial-scale=1.0"><title>🍈 Smart Melon Greenhouse</title><script src="https://cdn.jsdelivr.net/npm/chart.js"></script><style>@import url('https://fonts.googleapis.com/css2?family=Orbitron:wght@400;700;900&display=swap');*{margin:0;padding:0;box-sizing:border-box}body{font-family:'Orbitron',monospace;background:linear-gradient(135deg,#0f2027,#203a43,#2c5364);color:#e0e0e0;overflow-x:hidden}.container{max-width:1200px;margin:0 auto;padding:20px}.header{text-align:center;margin-bottom:30px}.title{font-size:2.5rem;font-weight:900;background:linear-gradient(45deg,#4ecf87,#fff,#a8f5c6);-webkit-background-clip:text;-webkit-text-fill-color:transparent;text-shadow:0 0 25px rgba(78,207,135,.5);animation:glow 2s ease-in-out infinite alternate}@keyframes glow{from{text-shadow:0 0 20px rgba(78,207,135,.4)}to{text-shadow:0 0 35px rgba(78,207,135,.7),0 0 50px rgba(78,207,135,.2)}}.grid-layout{display:grid;grid-template-columns:repeat(auto-fit,minmax(340px,1fr));gap:20px}.card{background:rgba(42,63,80,.5);backdrop-filter:blur(10px);border:1px solid rgba(78,207,135,.2);border-radius:15px;padding:20px;box-shadow:0 8px 32px rgba(0,0,0,.3);transition:all .3s ease}.card:hover{transform:translateY(-5px);box-shadow:0 12px 40px rgba(0,0,0,.4),0 0 40px rgba(78,207,135,.2)}.card-title{font-size:1.3rem;color:#4ecf87;margin-bottom:20px;text-align:center}.glance-grid{display:grid;grid-template-columns:repeat(3,1fr);gap:15px;margin-bottom:20px}.glance-item{text-align:center}.glance-value{font-size:2rem;font-weight:700;color:#fff}.glance-label{font-size:.8rem;color:#a8f5c6}.controls{text-align:center}.mode-indicator{padding:10px 20px;border-radius:50px;font-weight:700;font-size:1rem;margin-bottom:20px;border:2px solid;cursor:pointer;transition:all .3s ease}.mode-auto{background:#27ae60;border-color:#4ecf87;color:#fff}.mode-manual{background:#f39c12;border-color:#f1c40f;color:#fff}.relay-controls{display:grid;grid-template-columns:repeat(3,1fr);gap:10px}.relay-btn{padding:12px;border:none;border-radius:10px;font-family:'Orbitron';font-weight:700;font-size:.9rem;cursor:pointer;transition:all .3s ease}.relay-on{background:#27ae60;color:#fff}.relay-off{background:#c0392b;color:#fff}.chart-container{position:relative;height:250px;width:100%}footer{text-align:center;padding:30px 20px;margin-top:40px;border-top:1px solid rgba(78,207,135,.2)}.footer-copyright{color:rgba(224,224,224,.7);font-size:.9rem;margin-bottom:15px;letter-spacing:1px}.footer-team{color:#a8f5c6;font-size:.85rem;line-height:1.6;max-width:900px;margin:0 auto}.footer-team span{display:inline-block;margin:0 10px}</style></head><body><div class="container"><div class="header"><h1 class="title">SMARTWATERING MELON GREENHOUSE DASHBOARD</h1></div><div class="grid-layout"><div class="card"><h3 class="card-title">CONTROL PANEL A - OVERVIEW</h3><div class="glance-grid"><div class="glance-item"><div class="glance-value" id="sa_val">--</div><div class="glance-label">Suhu (°C)</div></div><div class="glance-item"><div class="glance-value" id="ta_val">--</div><div class="glance-label">TDS (ppm)</div></div><div class="glance-item"><div class="glance-value" id="pa_val">--</div><div class="glance-label">pH Level</div></div></div></div><div class="card"><h3 class="card-title">CONTROL PANEL B - OVERVIEW</h3><div class="glance-grid"><div class="glance-item"><div class="glance-value" id="sb_val">--</div><div class="glance-label">Suhu (°C)</div></div><div class="glance-item"><div class="glance-value" id="tb_val">--</div><div class="glance-label">TDS (ppm)</div></div><div class="glance-item"><div class="glance-value" id="pb_val">--</div><div class="glance-label">pH Level</div></div></div></div><div class="card controls"><h3 class="card-title">OUTPUT CONTROL</h3><div id="modeBtn" onclick="toggleMode()" class="mode-indicator">Loading...</div><div class="relay-controls"><button class="relay-btn" id="r1" onclick="toggleRelay(1)">PUMP A</button><button class="relay-btn" id="r2" onclick="toggleRelay(2)">PUMP B</button><button class="relay-btn" id="r3" onclick="toggleRelay(3)">AERATOR</button></div></div><div class="card"><h3 class="card-title">Temperature Trend (°C)</h3><div class="chart-container"><canvas id="tempChart"></canvas></div></div><div class="card"><h3 class="card-title">TDS Trend (ppm)</h3><div class="chart-container"><canvas id="tdsChart"></canvas></div></div><div class="card"><h3 class="card-title">pH Level Trend</h3><div class="chart-container"><canvas id="phChart"></canvas></div></div></div></div><footer><p class="footer-copyright">&copy; 2025 TEAM RISET BIMA</p><p class="footer-team"><span>Gina Purnama Insany</span> &bull; <span>Ivana Lucia Kharisma</span> &bull; <span>Kamdan</span> &bull; <span>Imam Sanjaya</span> &bull; <span>Muhammad Anbiya Fatah</span> &bull; <span>Panji Angkasa Putra</span></p></footer><script>const MAX_DATA_POINTS=20,chartData={labels:[],tempA:[],tempB:[],tdsA:[],tdsB:[],phA:[],phB:[]};function createChart(t,e,a){return new Chart(t,{type:"line",data:{labels:chartData.labels,datasets:a},options:{responsive:!0,maintainAspectRatio:!1,scales:{x:{ticks:{color:"#a8f5c6"},grid:{color:"rgba(78, 207, 135, 0.1)"}},y:{ticks:{color:"#a8f5c6"},grid:{color:"rgba(78, 207, 135, 0.1)"}}},plugins:{legend:{labels:{color:"#e0e0e0"}}},animation:{duration:500},elements:{line:{tension:.3}}}})}const tempChart=createChart(document.getElementById("tempChart"),"Temperature",[{label:"Sensor A",data:chartData.tempA,borderColor:"#4ecf87",backgroundColor:"rgba(78, 207, 135, 0.2)",fill:!0},{label:"Sensor B",data:chartData.tempB,borderColor:"#f39c12",backgroundColor:"rgba(243, 156, 18, 0.2)",fill:!0}]),tdsChart=createChart(document.getElementById("tdsChart"),"TDS",[{label:"Sensor A",data:chartData.tdsA,borderColor:"#3498db",backgroundColor:"rgba(52, 152, 219, 0.2)",fill:!0},{label:"Sensor B",data:chartData.tdsB,borderColor:"#9b59b6",backgroundColor:"rgba(155, 89, 182, 0.2)",fill:!0}]),phChart=createChart(document.getElementById("phChart"),"pH",[{label:"Sensor A",data:chartData.phA,borderColor:"#e74c3c",backgroundColor:"rgba(231, 76, 60, 0.2)",fill:!0},{label:"Sensor B",data:chartData.phB,borderColor:"#1abc9c",backgroundColor:"rgba(26, 188, 156, 0.2)",fill:!0}]);function updateChartData(t){const e=new Date,a=`${e.getHours().toString().padStart(2,"0")}:${e.getMinutes().toString().padStart(2,"0")}:${e.getSeconds().toString().padStart(2,"0")}`;chartData.labels.length>=MAX_DATA_POINTS&&(chartData.labels.shift(),chartData.tempA.shift(),chartData.tempB.shift(),chartData.tdsA.shift(),chartData.tdsB.shift(),chartData.phA.shift(),chartData.phB.shift()),chartData.labels.push(a),chartData.tempA.push(t.suhuA),chartData.tempB.push(t.suhuB),chartData.tdsA.push(t.tdsA),chartData.tdsB.push(t.tdsB),chartData.phA.push(t.phA),chartData.phB.push(t.phB),tempChart.update(),tdsChart.update(),phChart.update()}function refresh(){fetch("/data").then(t=>t.json()).then(t=>{document.getElementById("sa_val").innerText=t.suhuA.toFixed(1),document.getElementById("ta_val").innerText=t.tdsA.toFixed(0),document.getElementById("pa_val").innerText=t.phA.toFixed(1),document.getElementById("sb_val").innerText=t.suhuB.toFixed(1),document.getElementById("tb_val").innerText=t.tdsB.toFixed(0),document.getElementById("pb_val").innerText=t.phB.toFixed(1);const e=document.getElementById("modeBtn");"auto"===t.mode?(e.className="mode-indicator mode-auto",e.innerText="MODE: AUTOMATIC"):(e.className="mode-indicator mode-manual",e.innerText="MODE: MANUAL"),document.getElementById("r1").className=t.relay1?"relay-btn relay-on":"relay-btn relay-off",document.getElementById("r2").className=t.relay2?"relay-btn relay-on":"relay-btn relay-off",document.getElementById("r3").className=t.relay3?"relay-btn relay-on":"relay-btn relay-off",updateChartData(t)}).catch(t=>console.error("Error fetching data:",t))}function toggleRelay(t){fetch("/relay?id="+t).then(()=>setTimeout(refresh,200))}function toggleMode(){fetch("/mode?toggle=1").then(()=>setTimeout(refresh,200))}setInterval(refresh,2500),window.onload=refresh;</script></body></html>
)rawliteral";

void kirimDataKeGoogleSheet() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = String(googleScriptURL) +
                 "?suhuA=" + String(tempA) +
                 "&tdsA=" + String(tdsA) +
                 "&phA=" + String(phA) +
                 "&suhuB=" + String(tempB) +
                 "&tdsB=" + String(tdsB) +
                 "&phB=" + String(phB);
    http.begin(url);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.GET();
    http.end();
  }
}

void bacaSensorA(){
  // 1. Baca Suhu
  sensors.requestTemperatures();
  tempA = sensors.getTempCByIndex(0);
  if(tempA == DEVICE_DISCONNECTED_C || tempA < -5) {
    tempA = 25.0; // Nilai default jika sensor error
  }

  // 2. Baca dan Hitung TDS dengan Kalibrasi 3 Titik
  int rawValue = analogRead(SENSOR_TDS_PIN);
  float measuredVoltage = rawValue / 4095.0 * 3.3;
  float tdsValue = 0;

  if (measuredVoltage <= voltage_mid) {
    tdsValue = map(measuredVoltage * 1000, voltage_clean * 1000, voltage_mid * 1000, ppm_clean, ppm_mid);
  } else {
    tdsValue = map(measuredVoltage * 1000, voltage_mid * 1000, voltage_high * 1000, ppm_mid, ppm_high);
  }
  
  tdsValue = constrain(tdsValue, 0, 5000);
  float compensatedTds = tdsValue / (1.0 + 0.02 * (tempA - 25.0));
  tdsA = compensatedTds; // Simpan hasil akhir ke variabel global tdsA

  // 3. Baca pH
  float v_ph = analogRead(SENSOR_PH_PIN) / 4095.0 * 5.0;
  phA = PH_OFFSET + (v_ph - 2.5) * PH_SLOPE;
}

void updateSensorB(float ph, float tds, float temp){ 
  phB=ph; 
  tdsB=tds; 
  tempB=temp; 
}

void updateLCD16x2() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("S:");
  lcd.print(tempA, 0);
  lcd.write(byte(0));
  lcd.print("C");
  lcd.print(" TDS:");
  lcd.print(tdsA, 0);
  
  lcd.setCursor(0, 1);
  lcd.print("pH:");
  lcd.print(phA, 1);
  lcd.setCursor(9, 1);
  lcd.print("M:");
  lcd.print(modeOtomatis ? "Auto" : "Manual");
}

void logicController() {
  bool outA = false, outB = false, outAer = false;
  if (tempA > 28) outAer = true;

  if (tdsA > 1400) { // Target atas untuk melon
    outA = false; outB = false;
  } else if (tdsA < 1100) { // Target bawah untuk melon
    outA = true; outB = true;
  } else {
    if (phA > 6.5) outA = true; // Target atas pH
    else if (phA < 5.5) outB = true; // Target bawah pH
  }
  digitalWrite(RELAY_POMPA_A_PIN, outA ? LOW : HIGH);
  digitalWrite(RELAY_POMPA_B_PIN, outB ? LOW : HIGH);
  digitalWrite(RELAY_AERATOR_PIN, outAer ? LOW : HIGH);
}

void setup(){
  Serial.begin(115200);
  
  lcd.init();
  lcd.backlight();
  lcd.createChar(0, degree_char);

  pinMode(RELAY_POMPA_A_PIN, OUTPUT); digitalWrite(RELAY_POMPA_A_PIN,HIGH);
  pinMode(RELAY_POMPA_B_PIN, OUTPUT); digitalWrite(RELAY_POMPA_B_PIN,HIGH);
  pinMode(RELAY_AERATOR_PIN, OUTPUT); digitalWrite(RELAY_AERATOR_PIN,HIGH);

  sensors.begin(); 

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  lcd.setCursor(0, 0); lcd.print("Connecting WiFi");
  int attempts = 0;
  while(WiFi.status() != WL_CONNECTED && attempts < 20){
    delay(500); 
    Serial.print(".");
    lcd.print(".");
    attempts++;
  }

  if(WiFi.status() == WL_CONNECTED){
    Serial.println("\nIP Address: " + WiFi.localIP().toString());
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print("IP Address:");
    lcd.setCursor(0, 1); lcd.print(WiFi.localIP().toString());
    delay(2000);
  } else {
    Serial.println("\nFailed to connect. Starting AP.");
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print("WiFi Failed!");
    lcd.setCursor(0, 1); lcd.print("Starting AP...");
    WiFi.softAP("SmartMelon-AP", "12345678");
    delay(2000);
  }

  server.on("/",HTTP_GET,[](AsyncWebServerRequest *r){ r->send_P(200,"text/html",index_html); });
  
  server.on("/data",HTTP_GET,[](AsyncWebServerRequest *r){
    String json="{";
    json+="\"suhuA\":"+String(tempA,1)+",\"tdsA\":"+String(tdsA,0)+",\"phA\":"+String(phA,1)+",";
    json+="\"suhuB\":"+String(tempB,1)+",\"tdsB\":"+String(tdsB,0)+",\"phB\":"+String(phB,1)+",";
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
    if (r->hasParam("id")) {
      int id=r->getParam("id")->value().toInt();
      int pin=(id==1?RELAY_POMPA_A_PIN:id==2?RELAY_POMPA_B_PIN:id==3?RELAY_AERATOR_PIN:-1);
      if(pin!=-1){ 
        digitalWrite(pin,!digitalRead(pin)); 
        modeOtomatis=false;
      }
      r->send(200,"text/plain","OK");
    }
  });

  server.on("/mode",HTTP_GET,[](AsyncWebServerRequest *r){
    if(r->hasParam("toggle")) modeOtomatis=!modeOtomatis;
    r->send(200,"text/plain","OK");
  });

  server.begin();
}

void loop(){
  unsigned long now=millis();
  if(now - prevMillis >= interval){
    prevMillis = now;
    
    bacaSensorA();
    updateLCD16x2();
    kirimDataKeGoogleSheet();

    if(modeOtomatis){
      logicController();
    }
  }
}