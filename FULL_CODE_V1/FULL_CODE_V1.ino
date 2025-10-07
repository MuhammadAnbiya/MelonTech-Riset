/******************************************************************************
 * Proyek: Smart Watering Melon Tech Nusa Putra Riset BIMA
 * Versi: FINAL - PERBAIKAN URL & JSON (Revisi JavaScript)
 * Deskripsi: Implementasi sistem Fuzzy Logic Mamdani 3-input: 
 * 1. Selisih PPM (Rendah, Ideal, Tinggi)
 * 2. Suhu Air (Dingin, Normal, Panas)
 * 3. pH Level (Asam, Normal, Basa)
 * Output: Dosis Nutrisi (Tidak, Sedang, Tinggi) menggunakan Defuzzifikasi COG.
 * PERUBAHAN:
 * - Memperbaiki kesalahan sintaks pada JavaScript di dalam HTML (ternary operator).
 * - Metode pembuatan URL untuk Google Sheet diubah menjadi lebih robust (snprintf).
 * - Menambahkan sanitasi string pada JSON untuk web server agar lebih aman.
 * - Pompa Nutrisi Utama dikendalikan oleh 1 SSR Relay pada Pin 26 (CH4).
 * Dibuat oleh: Gemini
 ******************************************************************************/

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ESPAsyncWebServer.h>
#include <LiquidCrystal_I2C.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <EEPROM.h>

// ---------- KONFIGURASI PENTING ----------
const char* ssid = "pouio";
const char* password = "11111111";
const char* googleScriptURL = "https://script.google.com/macros/s/AKfycbza7UY-t0Vfg44QDsy8fXMQI3lEo7SQEFJOfgjJry793MSYQ6djG10-bR0zSHH67_1LTg/exec"; 

// ====================================================================================
// --- PENGATURAN KALIBRASI ULANG pH ---
const bool FORCE_RESET_CAL = false; 

// --- DATA KALIBRASI pH 2-TITIK (HARUS AKURAT) ---
const float CAL_PH7_VOLTAGE   = 2.85;    
const float CAL_PH4_VOLTAGE   = 3.05;    
const float PH_AIR_OFFSET = 2.43;        
const float VOLTAGE_THRESHOLD_PH_DRY = 3.20; 
// ====================================================================================

// ======================================================================
// --- DATA KALIBRASI TDS 3-TITIK ---
const float voltage_low = 1.3164; 
const float ppm_low     = 713.0; 
const float voltage_mid   = 2.3448; 
const float ppm_mid       = 1250.0; 
const float voltage_high  = 2.4509; 
const float ppm_high  = 2610.0; 
// ======================================================================

// ---------- PENGATURAN PERANGKAT KERAS ----------
LiquidCrystal_I2C lcd(0x27, 16, 2);

#define SENSOR_TDS_PIN            34
#define SENSOR_PH_PIN             35
#define SENSOR_SUHU_PIN           4
#define RELAY_MAIN_PUMP_PIN       25

byte degree_char[8] = {
  B00110, B01001, B01001, B00110,
  B00000, B00000, B00000, B00000
};

// ---------- VARIABEL & OBJEK GLOBAL ----------
float phA=0, tdsA=0, tempA=0;
float phB=0, tdsB=0, tempB=0; // Data Sensor B (Remote)
bool modeOtomatis = true;

int umurTanamanHari = 15; 

String fuzzyStatusMessage = "Menunggu...";
String fuzzyTargetPPMMessage = "N/A";
String fuzzyDecisionMessage = "Mematikan Pompa";

float ecA = 0.0, ecB = 0.0;
const float TDS_TO_EC_FACTOR = 700.0;

AsyncWebServer server(80);
OneWire oneWire(SENSOR_SUHU_PIN);
DallasTemperature sensors(&oneWire);

unsigned long prevMillis = 0;
const long interval = 15000; // 15s

float ph_slope = 0.0;
float ph_neutral_v = 0.0;

// ---------------- EEPROM ----------------
#define EEPROM_SIZE 64
const int EEPROM_ADDR_SLOPE = 0;
const int EEPROM_ADDR_NEUTRAL_V = 16;

// --- [START] KODE HTML, CSS, JAVASCRIPT UNTUK DASHBOARD ---
const char index_html[] PROGMEM = R"=====(
<!DOCTYPE html><html lang="en"><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width, initial-scale=1.0"><title>🍈 Smart Melon Greenhouse</title><script src="https://cdn.jsdelivr.net/npm/chart.js"></script><style>@import url('https://fonts.googleapis.com/css2?family=Orbitron:wght@400;700;900&display=swap');*{margin:0;padding:0;box-sizing:border-box}body{font-family:'Orbitron',monospace;background:linear-gradient(135deg,#0f2027,#203a43,#2c5364);color:#e0e0e0;overflow-x:hidden}.container{max-width:1400px;margin:0 auto;padding:20px}.header{text-align:center;margin-bottom:30px}.title{font-size:2.5rem;font-weight:900;background:linear-gradient(45deg,#4ecf87,#fff,#a8f5c6);-webkit-background-clip:text;-webkit-text-fill-color:transparent;text-shadow:0 0 25px rgba(78,207,135,.5);animation:glow 2s ease-in-out infinite alternate}@keyframes glow{from{text-shadow:0 0 20px rgba(78,207,135,.4)}to{text-shadow:0 0 35px rgba(78,207,135,.7),0 0 50px rgba(78,207,135,.2)}}.grid-layout{display:grid;grid-template-columns:repeat(auto-fit,minmax(340px,1fr));gap:20px}.card{background:rgba(42,63,80,.5);backdrop-filter:blur(10px);border:1px solid rgba(78,207,135,.2);border-radius:15px;padding:20px;box-shadow:0 8px 32px rgba(0,0,0,.3);transition:all .3s ease}.card:hover{transform:translateY(-5px);box-shadow:0 12px 40px rgba(0,0,0,.4),0 0 40px rgba(78,207,135,.2)}.card-title{font-size:1.3rem;color:#4ecf87;margin-bottom:20px;text-align:center}.glance-grid{display:grid;grid-template-columns:repeat(4,1fr);gap:15px;margin-bottom:20px}.glance-item{text-align:center}.glance-value{font-size:1.8rem;font-weight:700;color:#fff}.glance-label{font-size:.75rem;color:#a8f5c6}.controls{text-align:center}.mode-indicator{padding:10px 20px;border-radius:50px;font-weight:700;font-size:1rem;margin-bottom:20px;border:2px solid;cursor:pointer;transition:all .3s ease}.mode-auto{background:#27ae60;border-color:#4ecf87;color:#fff}.mode-manual{background:#f39c12;border-color:#f1c40f;color:#fff}.relay-btn{padding:12px;border:none;border-radius:10px;font-family:'Orbitron';font-weight:700;font-size:.9rem;cursor:pointer;transition:all .3s ease}.relay-on{background:#27ae60;color:#fff}.relay-off{background:#c0392b;color:#fff}.chart-container{position:relative;height:250px;width:100%}.logic-status-grid{display:grid;grid-template-columns:1fr 1fr;gap:15px;text-align:center}.logic-label{font-size:.75rem;color:#a8f5c6;margin-bottom:5px}.logic-value{font-size:1.5rem;font-weight:700;color:#fff}.logic-status-message{font-size:1.2rem;font-weight:700;color:#f1c40f;margin-top:10px;padding:10px;background:rgba(0,0,0,.2);border-radius:8px;min-height:50px;display:flex;align-items:center;justify-content:center}.full-span{grid-column:1 / -1}footer{text-align:center;padding:30px 20px;margin-top:40px;border-top:1px solid rgba(78,207,135,.2)}.footer-copyright{color:rgba(224,224,224,.7);font-size:.9rem;margin-bottom:15px;letter-spacing:1px}.footer-team{color:#a8f5c6;font-size:.85rem;line-height:1.6;max-width:900px;margin:0 auto}.footer-team span{display:inline-block;margin:0 10px}.relay-row{display:grid;grid-template-columns:1fr 1fr;gap:10px;margin-bottom:10px;align-items:center}.relay-row-label{text-align:left;font-size:.9rem;color:#a8f5c6}.full-width-btn{grid-column:1 / -1}</style></head><body><div class="container"><div class="header"><h1 class="title">SMARTWATERING MELON GREENHOUSE DASHBOARD</h1></div><div class="grid-layout"><div class="card"><h3 class="card-title">CONTROL PANEL A - OVERVIEW</h3><div class="glance-grid"><div class="glance-item"><div class="glance-value" id="sa_val">--</div><div class="glance-label">Suhu (°C)</div></div><div class="glance-item"><div class="glance-value" id="ta_val">--</div><div class="glance-label">TDS (ppm)</div></div><div class="glance-item"><div class="glance-value" id="eca_val">--</div><div class="glance-label">EC (mS/cm)</div></div><div class="glance-item"><div class="glance-value" id="pa_val">--</div><div class="glance-label">pH Level</div></div></div></div><div class="card"><h3 class="card-title">CONTROL PANEL B - OVERVIEW</h3><div class="glance-grid"><div class="glance-item"><div class="glance-value" id="sb_val">--</div><div class="glance-label">Suhu (°C)</div></div><div class="glance-item"><div class="glance-value" id="tb_val">--</div><div class="glance-label">TDS (ppm)</div></div><div class="glance-item"><div class="glance-value" id="ecb_val">--</div><div class="glance-label">EC (mS/cm)</div></div><div class="glance-item"><div class="glance-value" id="pb_val">--</div><div class="glance-label">pH Level</div></div></div></div><div class="card"><h3 class="card-title">SYSTEM LOGIC STATUS</h3><div class="logic-status-grid"><div class="logic-item"><div class="logic-label">Plant Age (Days)</div><div class="logic-value" id="plant_age">--</div></div><div class="logic-item"><div class="logic-label">Target TDS Range</div><div class="logic-value" id="target_ppm">--</div></div><div class="logic-item full-span"><div class="logic-label">Fuzzy Logic Decision: <span id="fuzzy_decision">--</span></div><div class="logic-status-message" id="fuzzy_status">--</div></div></div></div><div class="card controls"><h3 class="card-title">OUTPUT CONTROL</h3><div id="modeBtn" onclick="toggleMode()" class="mode-indicator">Loading...</div><div class="relay-row"><div class="relay-row-label">Pompa Nutrisi Utama (SSR/CH4)</div><button class="relay-btn" id="r_pump" onclick="toggleRelay(1)">Toggle</button></div></div><div class="card"><h3 class="card-title">Temperature Trend (°C)</h3><div class="chart-container"><canvas id="tempChart"></canvas></div></div><div class="card"><h3 class="card-title">TDS Trend (ppm)</h3><div class="chart-container"><canvas id="tdsChart"></canvas></div></div><div class="card"><h3 class="card-title">pH Level Trend</h3><div class="chart-container"><canvas id="phChart"></canvas></div></div><div class="card"><h3 class="card-title">EC Trend (mS/cm)</h3><div class="chart-container"><canvas id="ecChart"></canvas></div></div></div></div><footer><p class="footer-copyright">&copy; 2025 TEAM RISET BIMA</p><p class="footer-team"><span>Gina Purnama Insany</span> &bull; <span>Ivana Lucia Kharisma</span> &bull; <span>Kamdan</span> &bull; <span>Imam Sanjaya</span> &bull; <span>Muhammad Anbiya Fatah</span> &bull; <span>Panji Angkasa Putra</span></p></footer><script>const MAX_DATA_POINTS=20,chartData={labels:[],tempA:[],tempB:[],tdsA:[],tdsB:[],phA:[],phB:[],ecA:[],ecB:[]};function createChart(t,e,a){return new Chart(t,{type:"line",data:{labels:chartData.labels,datasets:a},options:{responsive:!0,maintainAspectRatio:!1,scales:{x:{ticks:{color:"#a8f5c6"},grid:{color:"rgba(78, 207, 135, 0.1)"}},y:{ticks:{color:"#a8f5c6"},grid:{color:"rgba(78, 207, 135, 0.1)"}}},plugins:{legend:{labels:{color:"#e0e0e0"}}},animation:{duration:500},elements:{line:{tension:.3}}}})}const tempChart=createChart(document.getElementById("tempChart"),"Temperature",[{label:"Sensor A",data:chartData.tempA,borderColor:"#4ecf87",backgroundColor:"rgba(78, 207, 135, 0.2)",fill:!0},{label:"Sensor B",data:chartData.tempB,borderColor:"#f39c12",backgroundColor:"rgba(243, 156, 18, 0.2)",fill:!0}]),tdsChart=createChart(document.getElementById("tdsChart"),"TDS",[{label:"Sensor A",data:chartData.tdsA,borderColor:"#3498db",backgroundColor:"rgba(52, 152, 219, 0.2)",fill:!0},{label:"Sensor B",data:chartData.tdsB,borderColor:"#9b59b6",backgroundColor:"rgba(155, 89, 182, 0.2)",fill:!0}]),phChart=createChart(document.getElementById("phChart"),"pH",[{label:"Sensor A",data:chartData.phA,borderColor:"#e74c3c",backgroundColor:"rgba(231, 76, 60, 0.2)",fill:!0},{label:"Sensor B",data:chartData.phB,borderColor:"#1abc9c",backgroundColor:"rgba(26, 188, 156, 0.2)",fill:!0}]),ecChart=createChart(document.getElementById("ecChart"),"EC",[{label:"Sensor A",data:chartData.ecA,borderColor:"#f1c40f",backgroundColor:"rgba(241, 196, 15, 0.2)",fill:!0},{label:"Sensor B",data:chartData.ecB,borderColor:"#e67e22",backgroundColor:"rgba(230, 126, 34, 0.2)",fill:!0}]);function updateChartData(t){const e=new Date,a=`${e.getHours().toString().padStart(2,"0")}:${e.getMinutes().toString().padStart(2,"0")}:${e.getSeconds().toString().padStart(2,"0")}`;chartData.labels.length>=MAX_DATA_POINTS&&(chartData.labels.shift(),chartData.tempA.shift(),chartData.tempB.shift(),chartData.tdsA.shift(),chartData.tdsB.shift(),chartData.phA.shift(),chartData.phB.shift(),chartData.ecA.shift(),chartData.ecB.shift()),chartData.labels.push(a),chartData.tempA.push(t.suhuA),chartData.tempB.push(t.suhuB),chartData.tdsA.push(t.tdsA),chartData.tdsB.push(t.tdsB),chartData.phA.push(t.phA),chartData.phB.push(t.phB),chartData.ecA.push(t.ecA),chartData.ecB.push(t.ecB),tempChart.update(),tdsChart.update(),phChart.update(),ecChart.update()}function refresh(){fetch("/data").then(e=>{if(!e.ok)throw new Error("Network response was not ok: "+e.statusText);return e.text()}).then(e=>{try{const t=JSON.parse(e);document.getElementById("sa_val").innerText=t.suhuA.toFixed(1),document.getElementById("ta_val").innerText=t.tdsA.toFixed(0),document.getElementById("pa_val").innerText=t.phA.toFixed(1),document.getElementById("eca_val").innerText=t.ecA.toFixed(2),document.getElementById("sb_val").innerText=t.suhuB.toFixed(1),document.getElementById("tb_val").innerText=t.tdsB.toFixed(0),document.getElementById("pb_val").innerText=t.phB.toFixed(1),document.getElementById("ecb_val").innerText=t.ecB.toFixed(2);const o=document.getElementById("modeBtn");"auto"===t.mode?(o.className="mode-indicator mode-auto",o.innerText="MODE: AUTOMATIC"):(o.className="mode-indicator mode-manual",o.innerText="MODE: MANUAL"),document.getElementById("r_pump").className=t.pumpState?"relay-btn relay-on":"relay-btn relay-off",document.getElementById("plant_age").innerText=t.plantAge,document.getElementById("target_ppm").innerText=t.targetPPM,document.getElementById("fuzzy_decision").innerText=t.fuzzyDecision;const n=document.getElementById("fuzzy_status");n.innerText=t.fuzzyStatus,"manual"===t.mode?n.style.color="#3498db":t.fuzzyDecision.includes("TINGGI")?n.style.color="#e74c3c":t.fuzzyDecision.includes("Tidak Perlu")?n.style.color="#4ecf87":n.style.color="#f1c40f",updateChartData(t)}catch(t){console.error("Failed to parse JSON:",t),console.error("Raw response from server was:",e)}}).catch(e=>{console.error("Error fetching data:",e)})}function toggleRelay(t){fetch("/relay?id="+t).then(()=>setTimeout(refresh,200))}function toggleMode(){fetch("/mode?toggle=1").then(()=>setTimeout(refresh,200))}setInterval(refresh,2500),window.onload=refresh;</script></body></html>
)=====";
// --- [END] KODE HTML, CSS, JAVASCRIPT ---


// ---------------- Helpers ----------------
float linInterp(float x, float x0, float x1, float y0, float y1){
  if (x1 - x0 == 0) return y0;
  return y0 + (y1 - y0) * ((x - x0) / (x1 - x0));
}

float readVoltageADC(int pin) {
  const int NUM_SAMPLES = 40;
  const int TRIM_COUNT = 5;
  const float VREF = 3.3;
  
  uint16_t samples[NUM_SAMPLES];
  for (int i = 0; i < NUM_SAMPLES; i++) {
    samples[i] = analogRead(pin);
    delay(2);
  }

  for (int i = 0; i < NUM_SAMPLES - 1; i++) {
    for (int j = i + 1; j < NUM_SAMPLES; j++) {
      if (samples[i] > samples[j]) {
        uint16_t temp = samples[i];
        samples[i] = samples[j];
        samples[j] = temp;
      }
    }
  }

  long sum = 0;
  for (int i = TRIM_COUNT; i < NUM_SAMPLES - TRIM_COUNT; i++) {
    sum += samples[i];
  }
  
  float avg_raw = (float)sum / (NUM_SAMPLES - 2 * TRIM_COUNT);
  return avg_raw / 4095.0 * VREF;
}

float readTemperatureSensor() {
  sensors.requestTemperatures();
  float t = sensors.getTempCByIndex(0);
  if (t == DEVICE_DISCONNECTED_C || t < -20 || t > 80) return 25.0; 
  return t;
}

float hitungEC(float tdsValue) {
    return tdsValue / TDS_TO_EC_FACTOR;
}

float readpH(float v_ph, float temp_celsius) {
  if (ph_slope == 0.0) return 0.0; 

  float compensated_slope = ph_slope * (temp_celsius + 273.15) / (25.0 + 273.15);
  float ph_value = 7.0 + (ph_neutral_v - v_ph) / compensated_slope;
  ph_value += PH_AIR_OFFSET;
  
  return constrain(ph_value, 0.0, 14.0);
}

// ---------------- Pembacaan Sensor Utama ----------------
void bacaSensorA(){
  tempA = readTemperatureSensor();

  float measuredVoltage_TDS = readVoltageADC(SENSOR_TDS_PIN);
  const float VOLTAGE_THRESHOLD_DRY = 0.15; 

  if (measuredVoltage_TDS < VOLTAGE_THRESHOLD_DRY) {
    tdsA = 0.0;
  } else {
    float tdsValue = 0.0;
    if (measuredVoltage_TDS <= voltage_mid) {
      tdsValue = linInterp(measuredVoltage_TDS, voltage_low, voltage_mid, ppm_low, ppm_mid);
    } else {
      tdsValue = linInterp(measuredVoltage_TDS, voltage_mid, voltage_high, ppm_mid, ppm_high);
    }
    tdsValue = constrain(tdsValue, 0, 5000);
    
    float compensatedTds = tdsValue / (1.0 + 0.02 * (tempA - 25.0));
    tdsA = compensatedTds;
  }
  
  ecA = hitungEC(tdsA);

  float v_ph = readVoltageADC(SENSOR_PH_PIN); 
  if (v_ph >= VOLTAGE_THRESHOLD_PH_DRY) { 
    phA = 7.0; 
  } else if (ph_slope != 0.0) {
    phA = readpH(v_ph, tempA);
  } else {
    phA = 0.0; 
  }
}

// ---------------- Update Sensor Remote & LCD ----------------
void updateSensorB(float ph, float tds, float temp){ 
  phB=ph; 
  tdsB=tds; 
  tempB=temp; 
  ecB = hitungEC(tdsB);
}

void updateLCD16x2() {
  lcd.clear();
  
  // BARIS 1: Suhu (T) dan pH
  lcd.setCursor(0, 0);
  lcd.print("T:");
  lcd.print(tempA, 1);
  lcd.write(byte(0)); // Karakter Derajat
  lcd.print("C");
  lcd.setCursor(9, 0);
  lcd.print("pH:");
  lcd.print(phA, 1);

  // BARIS 2: PPM (P), EC (E), dan Mode ([A]/[M])
  String modeStr = modeOtomatis ? "[A]" : "[M]";
  
  lcd.setCursor(0, 1);
  lcd.print("P:");
  lcd.print(tdsA, 0);

  lcd.setCursor(8, 1);
  lcd.print("E:");
  lcd.print(ecA, 1);
  
  lcd.setCursor(13, 1);
  lcd.print(modeStr);
}


// =================================================================================
// --- BAGIAN LOGIKA FUZZY (Tidak Diubah) ---
// =================================================================================

struct FuzzifiedValues { 
  float deltaPPM[3]; float temperature[3]; float pH[3]; 
};
struct TargetNutrisi { float ppm_bawah; float ppm_atas; };
TargetNutrisi getTargetPPM(int hari) {
  TargetNutrisi target;
  if (hari >= 0 && hari <= 1) { target.ppm_bawah = 700; target.ppm_atas = 1050; } 
  else if (hari >= 2 && hari <= 6) { target.ppm_bawah = 1400; target.ppm_atas = 1750; } 
  else if (hari >= 7 && hari <= 11) { target.ppm_bawah = 2240; target.ppm_atas = 2450; } 
  else if (hari >= 12 && hari <= 16) { target.ppm_bawah = 2100; target.ppm_atas = 2240; } 
  else if (hari >= 17 && hari <= 21) { target.ppm_bawah = 1960; target.ppm_atas = 2100; } 
  else if (hari >= 22 && hari <= 26) { target.ppm_bawah = 1540; target.ppm_atas = 1750; } 
  else if (hari >= 27 && hari <= 31) { target.ppm_bawah = 2100; target.ppm_atas = 2240; } 
  else if (hari >= 32 && hari <= 36) { target.ppm_bawah = 1960; target.ppm_atas = 2100; } 
  else if (hari >= 37 && hari <= 56) { target.ppm_bawah = 1750; target.ppm_atas = 1960; } 
  else if (hari >= 57 && hari <= 66) { target.ppm_bawah = 1540; target.ppm_atas = 1750; } 
  else if (hari >= 67 && hari <= 75) { target.ppm_bawah = 2100; target.ppm_atas = 2240; } 
  else { target.ppm_bawah = 1500; target.ppm_atas = 1700; }
  return target;
}
float trapMF(float x, float a, float b, float c, float d) {
    if (x <= a || x >= d) return 0.0; if (x >= b && x <= c) return 1.0;
    if (x > a && x < b) return (x - a) / (b - a);
    if (x > c && x < d) return (d - x) / (d - c); return 0.0;
}
FuzzifiedValues fuzzifyAll(float ppm_terukur, float temp_celsius, float ph_level, TargetNutrisi target) {
  FuzzifiedValues fv;
  float target_mid = (target.ppm_bawah + target.ppm_atas) / 2.0;
  float delta_ppm = ppm_terukur - target_mid;
  fv.deltaPPM[0] = trapMF(delta_ppm, -1000, -500, -500, -100);
  fv.deltaPPM[1] = trapMF(delta_ppm, -250, -50, 50, 250);
  fv.deltaPPM[2] = trapMF(delta_ppm, 100, 500, 500, 1000);
  fv.temperature[0] = trapMF(temp_celsius, 10.0, 15.0, 15.0, 24.0);
  fv.temperature[1] = trapMF(temp_celsius, 22.0, 26.0, 30.0, 34.0);
  fv.temperature[2] = trapMF(temp_celsius, 32.0, 35.0, 35.0, 40.0);
  fv.pH[0] = trapMF(ph_level, 0.0, 4.5, 4.5, 5.5);
  fv.pH[1] = trapMF(ph_level, 5.2, 5.8, 6.5, 7.1);
  fv.pH[2] = trapMF(ph_level, 6.8, 7.5, 7.5, 14.0);
  return fv;
}
const float RULE_BASE[3][3][3] = {
  {{0.5, 1.0, 0.5}, {0.5, 1.0, 0.5}, {0.0, 0.5, 0.0}},
  {{0.0, 0.0, 0.0}, {0.0, 0.0, 0.0}, {0.0, 0.0, 0.0}},
  {{0.0, 0.0, 0.0}, {0.0, 0.0, 0.0}, {0.0, 0.0, 0.0}}
};
float fuzzyInferenceAndDefuzzification(FuzzifiedValues fv) {
  float numerator = 0.0; float denominator = 0.0; float max_dose = 0.0;
  for (int i = 0; i < 3; i++) { for (int j = 0; j < 3; j++) { for (int k = 0; k < 3; k++) {
    float alpha = min(fv.deltaPPM[i], min(fv.temperature[j], fv.pH[k]));
    float z_value = RULE_BASE[i][j][k]; 
    numerator += alpha * z_value; denominator += alpha;
    if (alpha > 0.0) { max_dose = max(max_dose, z_value); }
  }}}
  if (max_dose == 1.0) fuzzyStatusMessage = "Kebutuhan Nutrisi TINGGI. Kondisi Ideal.";
  else if (max_dose == 0.5) fuzzyStatusMessage = "Kebutuhan Nutrisi SEDANG. Kondisi Kurang Optimal.";
  else fuzzyStatusMessage = "Tidak Perlu Dosis. TDS Cukup/Tinggi.";
  if (denominator == 0) return 0.0;
  float crisp_output = numerator / denominator;
  Serial.printf(" -> COG Crisp Output: %.3f\n", crisp_output);
  return crisp_output;
}
void logicController() {
  TargetNutrisi target = getTargetPPM(umurTanamanHari);
  FuzzifiedValues fv = fuzzifyAll(tdsA, tempA, phA, target);
  float dosis_skor = fuzzyInferenceAndDefuzzification(fv);
  const float AMBANG_BATAS_AKSI = 0.4;
  bool nyalakanPompa = dosis_skor > AMBANG_BATAS_AKSI;
  fuzzyTargetPPMMessage = String(target.ppm_bawah, 0) + " - " + String(target.ppm_atas, 0);
  fuzzyDecisionMessage = nyalakanPompa ? "Memicu Dosis" : "Tidak Perlu Dosis";
  uint8_t relayState = nyalakanPompa ? LOW : HIGH; 
  digitalWrite(RELAY_MAIN_PUMP_PIN, relayState);
}


// ---------------- Komunikasi & Web Server ----------------
void kirimDataKeGoogleSheet() {
  if (WiFi.status() == WL_CONNECTED) {
    float safe_tempA = isnan(tempA) || isinf(tempA) ? 0.0 : tempA;
    float safe_tdsA  = isnan(tdsA)  || isinf(tdsA)  ? 0.0 : tdsA;
    float safe_phA   = isnan(phA)   || isinf(phA)   ? 0.0 : phA;
    float safe_ecA   = isnan(ecA)   || isinf(ecA)   ? 0.0 : ecA;
    float safe_tempB = isnan(tempB) || isinf(tempB) ? 0.0 : tempB;
    float safe_tdsB  = isnan(tdsB)  || isinf(tdsB)  ? 0.0 : tdsB;
    float safe_phB   = isnan(phB)   || isinf(phB)   ? 0.0 : phB;
    float safe_ecB   = isnan(ecB)   || isinf(ecB)   ? 0.0 : ecB;

    char urlBuffer[512];
    snprintf(urlBuffer, sizeof(urlBuffer),
              "%s?suhuA=%.1f&tdsA=%.0f&phA=%.2f&ecA=%.2f&suhuB=%.1f&tdsB=%.0f&phB=%.2f&ecB=%.2f",
              googleScriptURL,
              safe_tempA, safe_tdsA, safe_phA, safe_ecA,
              safe_tempB, safe_tdsB, safe_phB, safe_ecB);

    HTTPClient http;
    Serial.println("Mengirim data ke Google Sheet...");
    Serial.print("URL: ");
    Serial.println(urlBuffer); 

    http.begin(urlBuffer);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    int httpCode = http.GET();
    
    if (httpCode > 0) {
      Serial.printf("Google Sheet: Kode status: %d\n", httpCode);
      if (httpCode != HTTP_CODE_OK && httpCode != 302) { 
         String payload = http.getString();
         Serial.println("Respons Server:");
         Serial.println(payload);
      }
    } else {
      Serial.printf("Google Sheet: Gagal terhubung. Error: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end();
  } else {
    Serial.println("Google Sheet: WiFi tidak terhubung.");
  }
}

void handleSerialCommands() {
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    if (command.equalsIgnoreCase("v")) {
      float phVoltage = readVoltageADC(SENSOR_PH_PIN);
      float tdsVoltage = readVoltageADC(SENSOR_TDS_PIN);
      Serial.println("========================================");
      Serial.println("Membaca Tegangan Sensor:");
      Serial.print(" -> Tegangan pH      : "); Serial.print(phVoltage, 4); Serial.println(" V");
      Serial.print(" -> Tegangan TDS/PPM : "); Serial.print(tdsVoltage, 4); Serial.println(" V");
      Serial.println("========================================");
    }
  }
}

void setupWebServer(){
  server.on("/",HTTP_GET,[](AsyncWebServerRequest *r){ r->send_P(200,"text/html",index_html); });
  
  server.on("/data",HTTP_GET,[](AsyncWebServerRequest *r){
    float safe_tempA = isnan(tempA) || isinf(tempA) ? 0.0 : tempA;
    float safe_tdsA  = isnan(tdsA)  || isinf(tdsA)  ? 0.0 : tdsA;
    float safe_phA   = isnan(phA)   || isinf(phA)   ? 0.0 : phA;
    float safe_ecA   = isnan(ecA)   || isinf(ecA)   ? 0.0 : ecA;
    float safe_tempB = isnan(tempB) || isinf(tempB) ? 0.0 : tempB;
    float safe_tdsB  = isnan(tdsB)  || isinf(tdsB)  ? 0.0 : tdsB;
    float safe_phB   = isnan(phB)   || isinf(phB)   ? 0.0 : phB;
    float safe_ecB   = isnan(ecB)   || isinf(ecB)   ? 0.0 : ecB;

    String safeFuzzyStatus = fuzzyStatusMessage;
    safeFuzzyStatus.replace("\"", "\\\"");
    String safeFuzzyDecision = fuzzyDecisionMessage;
    safeFuzzyDecision.replace("\"", "\\\"");
    String safeTargetPPM = fuzzyTargetPPMMessage;
    safeTargetPPM.replace("\"", "\\\"");

    String json="{";
    json+="\"suhuA\":"+String(safe_tempA,1)+",\"tdsA\":"+String(safe_tdsA,0)+",\"phA\":"+String(safe_phA,2)+",\"ecA\":"+String(safe_ecA,2)+",";
    json+="\"suhuB\":"+String(safe_tempB,1)+",\"tdsB\":"+String(safe_tdsB,0)+",\"phB\":"+String(safe_phB,2)+",\"ecB\":"+String(safe_ecB,2)+",";
    json+="\"mode\":\""+String(modeOtomatis?"auto":"manual")+"\",";
    json+="\"pumpState\":" + String(digitalRead(RELAY_MAIN_PUMP_PIN)==LOW ? "true" : "false") + ",";
    json+="\"fuzzyStatus\":\"" + safeFuzzyStatus + "\",";
    json+="\"fuzzyDecision\":\"" + safeFuzzyDecision + "\",";
    json+="\"targetPPM\":\"" + safeTargetPPM + "\",";
    json+="\"plantAge\":" + String(umurTanamanHari);
    json+="}";
    
    Serial.println("\n--- Mengirim JSON ke Dashboard ---");
    Serial.println(json);
    Serial.println("---------------------------------");

    r->send(200,"application/json",json);
  });

  server.on("/updateB",HTTP_GET,[](AsyncWebServerRequest *r){
    if(r->hasParam("ph")&&r->hasParam("tds")&&r->hasParam("temp")){
      updateSensorB(r->getParam("ph")->value().toFloat(), r->getParam("tds")->value().toFloat(), r->getParam("temp")->value().toFloat());
      r->send(200,"text/plain","OK");
    } else r->send(400,"text/plain","Missing params");
  });

  server.on("/relay",HTTP_GET,[](AsyncWebServerRequest *r){
    if (r->hasParam("id")) {
      uint8_t currentState = digitalRead(RELAY_MAIN_PUMP_PIN);
      digitalWrite(RELAY_MAIN_PUMP_PIN, !currentState);
      modeOtomatis = false; 
      r->send(200,"text/plain","OK");
    }
  });

  server.on("/mode",HTTP_GET,[](AsyncWebServerRequest *r){
    if(r->hasParam("toggle")) modeOtomatis=!modeOtomatis;
    r->send(200,"text/plain","OK");
  });

  server.begin();
}

// ---------------- Setup ----------------
void setup(){
  Serial.begin(115200);
  delay(100);

  EEPROM.begin(EEPROM_SIZE);

  lcd.init();
  lcd.backlight();
  lcd.createChar(0, degree_char);

  if (FORCE_RESET_CAL) {
      if (CAL_PH4_VOLTAGE <= CAL_PH7_VOLTAGE) {
          ph_neutral_v = 0.0; ph_slope = 0.0;
      } else {
          ph_neutral_v = CAL_PH7_VOLTAGE;
          ph_slope = (CAL_PH4_VOLTAGE - CAL_PH7_VOLTAGE) / (4.0 - 7.0);
          EEPROM.put(EEPROM_ADDR_NEUTRAL_V, ph_neutral_v);
          EEPROM.put(EEPROM_ADDR_SLOPE, ph_slope);
          EEPROM.commit();
      }
  } else {
      EEPROM.get(EEPROM_ADDR_NEUTRAL_V, ph_neutral_v);
      EEPROM.get(EEPROM_ADDR_SLOPE, ph_slope);
      if (ph_slope == 0.0 || isnan(ph_slope) || ph_neutral_v == 0.0 || isnan(ph_neutral_v)) {
          ph_neutral_v = CAL_PH7_VOLTAGE;
          ph_slope = (CAL_PH4_VOLTAGE - CAL_PH7_VOLTAGE) / (4.0 - 7.0);
      }
  }

  Serial.printf("Kalibrasi pH Aktif: V_Netral=%.4f V, Slope=%.4f V/pH\n", ph_neutral_v, ph_slope);

  pinMode(RELAY_MAIN_PUMP_PIN, OUTPUT); digitalWrite(RELAY_MAIN_PUMP_PIN,HIGH);

  sensors.begin(); 

  analogReadResolution(12);
  analogSetPinAttenuation(SENSOR_PH_PIN, ADC_11db);
  analogSetPinAttenuation(SENSOR_TDS_PIN, ADC_11db);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid,password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  
  Serial.println("\nWiFi connected!");
  Serial.print("IP Address: "); Serial.println(WiFi.localIP());

  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("Terhubung!");
  lcd.setCursor(0, 1); lcd.print(WiFi.localIP());
  delay(3000); 

  setupWebServer();
}

// ---------------- Main loop ----------------
void loop(){
  unsigned long now=millis();
  
  if(now - prevMillis >= interval){
    prevMillis = now;
    
    Serial.println("\n--- Membaca Sensor ---");
    bacaSensorA();
    Serial.printf("Hasil: Suhu=%.1f C, TDS=%.0f ppm, pH=%.2f, EC=%.2f mS/cm\n", tempA, tdsA, phA, ecA);
    
    updateLCD16x2();
    kirimDataKeGoogleSheet();

    if(modeOtomatis){
      logicController();
    } else {
      fuzzyStatusMessage = "Kontrol Manual Aktif.";
      fuzzyTargetPPMMessage = "N/A";
      fuzzyDecisionMessage = "Mode Manual";
    }
  }

  handleSerialCommands();
  delay(10);
}

