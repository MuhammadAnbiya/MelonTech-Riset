/******************************************************************************
 * Proyek: Smart Watering Melon Tech Nusa Putra Riset BIMA
 * Versi: Final - Standar Kalibrasi pH Akurat
 * Deskripsi: Versi ini merupakan standar final untuk pembacaan sensor pH
 * yang akurat untuk tujuan monitoring. Logika matematis dan panduan
 * kalibrasi telah disempurnakan sesuai standar instrumen pH.
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
const char* ssid = "ADVAN V1 PRO-8F7379";
const char* password = "7C27964D";
const char* googleScriptURL = "https://script.google.com/macros/s/AKfycbyDTbuSFk0GOylphGYnyH77oPurvq0hG1Nu5ydcPEyouZ5aDKhGN8Sg8kFctRgTV7Gyfg/exec";

// ====================================================================================
// --- [PANDUAN FINAL] KALIBRASI SENSOR pH UNTUK AKURASI MAKSIMAL ---
// Ikuti langkah-langkah ini dengan TELITI. Akurasi 99% bergantung pada proses ini.
// ------------------------------------------------------------------------------------
// **PRINSIP DASAR:**
// Sensor pH menghasilkan tegangan yang berbanding terbalik secara linear dengan nilai pH.
// - pH Asam (4.0)  -> Tegangan TINGGI
// - pH Netral (7.0) -> Tegangan TENGAH
// - pH Basa (10.0) -> Tegangan RENDAH
// Oleh karena itu, nilai tegangan untuk pH 4.0 HARUS LEBIH TINGGI dari pH 7.0.
// ------------------------------------------------------------------------------------
// **LANGKAH-LANGKAH KALIBRASI:**
// 1.  SIAPKAN ALAT: Larutan buffer pH 7.0 dan pH 4.0 yang baru/berkualitas, dan air bersih
//     (lebih baik aquades/air suling) untuk membilas.
//
// 2.  UPLOAD KODE: Upload kode ini ke ESP32 Anda.
//
// 3.  BUKA SERIAL MONITOR: Atur baud rate ke 115200.
//
// 4.  KALIBRASI TITIK NETRAL (pH 7.0):
//     a. Bilas probe pH dengan air bersih, lalu keringkan dengan menempelkan tisu secara
//        perlahan (jangan digosok).
//     b. Celupkan probe ke dalam larutan buffer pH 7.0. Aduk perlahan.
//     c. Tunggu 1-2 menit hingga pembacaan di Serial Monitor stabil.
//     d. Saat sudah stabil, kirim karakter 'v' melalui Serial Monitor.
//     e. CATAT NILAI TEGANGAN yang muncul (misal: 1.3215 V). Ini adalah nilai untuk CAL_PH7_VOLTAGE.
//
// 5.  KALIBRASI TITIK ASAM (pH 4.0):
//     a. Ulangi langkah 4a (bilas dan keringkan probe).
//     b. Celupkan probe ke dalam larutan buffer pH 4.0. Aduk perlahan.
//     c. Tunggu 1-2 menit hingga pembacaan stabil.
//     d. Kirim karakter 'v' dan CATAT NILAI TEGANGAN yang muncul (misal: 2.3245 V). Ini
//        adalah nilai untuk CAL_PH4_VOLTAGE.
//
// 6.  MASUKKAN NILAI & SIMPAN KE MEMORI:
//     a. Masukkan kedua nilai yang sudah Anda catat ke variabel di bawah ini.
//     b. Ubah nilai `FORCE_RECALIBRATE` menjadi `true`.
//     c. Upload ulang kode. Perhatikan Serial Monitor, akan ada konfirmasi kalibrasi disimpan.
//
// 7.  SELESAI:
//     a. Kembalikan `FORCE_RECALIBRATE` menjadi `false`.
//     b. Upload ulang kode untuk terakhir kalinya.
//     c. Sekarang perangkat Anda siap digunakan dengan kalibrasi yang akurat.
// ====================================================================================
const float CAL_PH7_VOLTAGE   = 1.32;  // Ganti dengan hasil pengukuran Anda
const float CAL_PH4_VOLTAGE   = 2.7327;  // Ganti dengan hasil pengukuran Anda
const bool  FORCE_RECALIBRATE = true; // Set `true` HANYA saat ingin menyimpan kalibrasi baru

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

#define SENSOR_TDS_PIN          34
#define SENSOR_PH_PIN           35
#define SENSOR_SUHU_PIN         4

#define RELAY_PERISTALTIC_1_PIN   25  // Relay untuk Nutrisi A
#define RELAY_PERISTALTIC_2_PIN   26  // Relay untuk Nutrisi B

byte degree_char[8] = {
  B00110, B01001, B01001, B00110,
  B00000, B00000, B00000, B00000
};

// ---------- VARIABEL & OBJEK GLOBAL ----------
float phA=0, tdsA=0, tempA=0;
float phB=0, tdsB=0, tempB=0;
bool modeOtomatis = true;

// --- EC Integration ---
float ecA = 0.0, ecB = 0.0;
const float TDS_TO_EC_FACTOR = 700.0; // Faktor konversi diubah ke 700 (TDS 700 scale)

AsyncWebServer server(80);
OneWire oneWire(SENSOR_SUHU_PIN);
DallasTemperature sensors(&oneWire);

unsigned long prevMillis = 0;
const long interval = 15000; // 15s

// Koefisien kalibrasi pH yang akan disimpan/dibaca dari EEPROM
// Rumus: pH = 7.0 + (tegangan_netral - tegangan_terukur) / slope
float ph_slope = 0.0;     // Volt per unit pH, idealnya bernilai negatif (~ -0.059 V/pH @ 25°C)
float ph_neutral_v = 0.0; // Tegangan yang dihasilkan sensor pada larutan pH 7.0

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html lang="en"><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width, initial-scale=1.0"><title>🍈 Smart Melon Greenhouse</title><script src="https://cdn.jsdelivr.net/npm/chart.js"></script><style>@import url('https://fonts.googleapis.com/css2?family=Orbitron:wght@400;700;900&display=swap');*{margin:0;padding:0;box-sizing:border-box}body{font-family:'Orbitron',monospace;background:linear-gradient(135deg,#0f2027,#203a43,#2c5364);color:#e0e0e0;overflow-x:hidden}.container{max-width:1400px;margin:0 auto;padding:20px}.header{text-align:center;margin-bottom:30px}.title{font-size:2.5rem;font-weight:900;background:linear-gradient(45deg,#4ecf87,#fff,#a8f5c6);-webkit-background-clip:text;-webkit-text-fill-color:transparent;text-shadow:0 0 25px rgba(78,207,135,.5);animation:glow 2s ease-in-out infinite alternate}@keyframes glow{from{text-shadow:0 0 20px rgba(78,207,135,.4)}to{text-shadow:0 0 35px rgba(78,207,135,.7),0 0 50px rgba(78,207,135,.2)}}.grid-layout{display:grid;grid-template-columns:repeat(auto-fit,minmax(340px,1fr));gap:20px}.card{background:rgba(42,63,80,.5);backdrop-filter:blur(10px);border:1px solid rgba(78,207,135,.2);border-radius:15px;padding:20px;box-shadow:0 8px 32px rgba(0,0,0,.3);transition:all .3s ease}.card:hover{transform:translateY(-5px);box-shadow:0 12px 40px rgba(0,0,0,.4),0 0 40px rgba(78,207,135,.2)}.card-title{font-size:1.3rem;color:#4ecf87;margin-bottom:20px;text-align:center}.glance-grid{display:grid;grid-template-columns:repeat(4,1fr);gap:15px;margin-bottom:20px}.glance-item{text-align:center}.glance-value{font-size:1.8rem;font-weight:700;color:#fff}.glance-label{font-size:.75rem;color:#a8f5c6}.controls{text-align:center}.mode-indicator{padding:10px 20px;border-radius:50px;font-weight:700;font-size:1rem;margin-bottom:20px;border:2px solid;cursor:pointer;transition:all .3s ease}.mode-auto{background:#27ae60;border-color:#4ecf87;color:#fff}.mode-manual{background:#f39c12;border-color:#f1c40f;color:#fff}.relay-btn{padding:12px;border:none;border-radius:10px;font-family:'Orbitron';font-weight:700;font-size:.9rem;cursor:pointer;transition:all .3s ease}.relay-on{background:#27ae60;color:#fff}.relay-off{background:#c0392b;color:#fff}.chart-container{position:relative;height:250px;width:100%}footer{text-align:center;padding:30px 20px;margin-top:40px;border-top:1px solid rgba(78,207,135,.2)}.footer-copyright{color:rgba(224,224,224,.7);font-size:.9rem;margin-bottom:15px;letter-spacing:1px}.footer-team{color:#a8f5c6;font-size:.85rem;line-height:1.6;max-width:900px;margin:0 auto}.footer-team span{display:inline-block;margin:0 10px}.relay-row{display:grid;grid-template-columns:1fr 1fr;gap:10px;margin-bottom:10px;align-items:center}.relay-row-label{text-align:left;font-size:.9rem;color:#a8f5c6}.full-width-btn{grid-column:1 / -1;}</style></head><body><div class="container"><div class="header"><h1 class="title">SMARTWATERING MELON GREENHOUSE DASHBOARD</h1></div><div class="grid-layout"><div class="card"><h3 class="card-title">CONTROL PANEL A - OVERVIEW</h3><div class="glance-grid"><div class="glance-item"><div class="glance-value" id="sa_val">--</div><div class="glance-label">Suhu (°C)</div></div><div class="glance-item"><div class="glance-value" id="ta_val">--</div><div class="glance-label">TDS (ppm)</div></div><div class="glance-item"><div class="glance-value" id="eca_val">--</div><div class="glance-label">EC (mS/cm)</div></div><div class="glance-item"><div class="glance-value" id="pa_val">--</div><div class="glance-label">pH Level</div></div></div></div><div class="card"><h3 class="card-title">CONTROL PANEL B - OVERVIEW</h3><div class="glance-grid"><div class="glance-item"><div class="glance-value" id="sb_val">--</div><div class="glance-label">Suhu (°C)</div></div><div class="glance-item"><div class="glance-value" id="tb_val">--</div><div class="glance-label">TDS (ppm)</div></div><div class="glance-item"><div class="glance-value" id="ecb_val">--</div><div class="glance-label">EC (mS/cm)</div></div><div class="glance-item"><div class="glance-value" id="pb_val">--</div><div class="glance-label">pH Level</div></div></div></div><div class="card controls"><h3 class="card-title">OUTPUT CONTROL</h3><div id="modeBtn" onclick="toggleMode()" class="mode-indicator">Loading...</div><div class="relay-row"><div class="relay-row-label">Nutrisi A</div><button class="relay-btn" id="r1" onclick="toggleRelay(1)">Toggle</button></div><div class="relay-row"><div class="relay-row-label">Nutrisi B</div><button class="relay-btn" id="r2" onclick="toggleRelay(2)">Toggle</button></div><div class="relay-row full-width-btn"><button class="relay-btn relay-on" onclick="setAllRelays(true)">Semua ON</button></div><div class="relay-row full-width-btn"><button class="relay-btn relay-off" onclick="setAllRelays(false)">Semua OFF</button></div></div><div class="card"><h3 class="card-title">Temperature Trend (°C)</h3><div class="chart-container"><canvas id="tempChart"></canvas></div></div><div class="card"><h3 class="card-title">TDS Trend (ppm)</h3><div class="chart-container"><canvas id="tdsChart"></canvas></div></div><div class="card"><h3 class="card-title">pH Level Trend</h3><div class="chart-container"><canvas id="phChart"></canvas></div></div><div class="card"><h3 class="card-title">EC Trend (mS/cm)</h3><div class="chart-container"><canvas id="ecChart"></canvas></div></div></div></div><footer><p class="footer-copyright">&copy; 2025 TEAM RISET BIMA</p><p class="footer-team"><span>Gina Purnama Insany</span> &bull; <span>Ivana Lucia Kharisma</span> &bull; <span>Kamdan</span> &bull; <span>Imam Sanjaya</span> &bull; <span>Muhammad Anbiya Fatah</span> &bull; <span>Panji Angkasa Putra</span></p></footer><script>const MAX_DATA_POINTS=20,chartData={labels:[],tempA:[],tempB:[],tdsA:[],tdsB:[],phA:[],phB:[],ecA:[],ecB:[]};function createChart(t,e,a){return new Chart(t,{type:"line",data:{labels:chartData.labels,datasets:a},options:{responsive:!0,maintainAspectRatio:!1,scales:{x:{ticks:{color:"#a8f5c6"},grid:{color:"rgba(78, 207, 135, 0.1)"}},y:{ticks:{color:"#a8f5c6"},grid:{color:"rgba(78, 207, 135, 0.1)"}}},plugins:{legend:{labels:{color:"#e0e0e0"}}},animation:{duration:500},elements:{line:{tension:.3}}}})}const tempChart=createChart(document.getElementById("tempChart"),"Temperature",[{label:"Sensor A",data:chartData.tempA,borderColor:"#4ecf87",backgroundColor:"rgba(78, 207, 135, 0.2)",fill:!0},{label:"Sensor B",data:chartData.tempB,borderColor:"#f39c12",backgroundColor:"rgba(243, 156, 18, 0.2)",fill:!0}]),tdsChart=createChart(document.getElementById("tdsChart"),"TDS",[{label:"Sensor A",data:chartData.tdsA,borderColor:"#3498db",backgroundColor:"rgba(52, 152, 219, 0.2)",fill:!0},{label:"Sensor B",data:chartData.tdsB,borderColor:"#9b59b6",backgroundColor:"rgba(155, 89, 182, 0.2)",fill:!0}]),phChart=createChart(document.getElementById("phChart"),"pH",[{label:"Sensor A",data:chartData.phA,borderColor:"#e74c3c",backgroundColor:"rgba(231, 76, 60, 0.2)",fill:!0},{label:"Sensor B",data:chartData.phB,borderColor:"#1abc9c",backgroundColor:"rgba(26, 188, 156, 0.2)",fill:!0}]),ecChart=createChart(document.getElementById("ecChart"),"EC",[{label:"Sensor A",data:chartData.ecA,borderColor:"#f1c40f",backgroundColor:"rgba(241, 196, 15, 0.2)",fill:!0},{label:"Sensor B",data:chartData.ecB,borderColor:"#e67e22",backgroundColor:"rgba(230, 126, 34, 0.2)",fill:!0}]);function updateChartData(t){const e=new Date,a=`${e.getHours().toString().padStart(2,"0")}:${e.getMinutes().toString().padStart(2,"0")}:${e.getSeconds().toString().padStart(2,"0")}`;chartData.labels.length>=MAX_DATA_POINTS&&(chartData.labels.shift(),chartData.tempA.shift(),chartData.tempB.shift(),chartData.tdsA.shift(),chartData.tdsB.shift(),chartData.phA.shift(),chartData.phB.shift(),chartData.ecA.shift(),chartData.ecB.shift()),chartData.labels.push(a),chartData.tempA.push(t.suhuA),chartData.tempB.push(t.suhuB),chartData.tdsA.push(t.tdsA),chartData.tdsB.push(t.tdsB),chartData.phA.push(t.phA),chartData.phB.push(t.phB),chartData.ecA.push(t.ecA),chartData.ecB.push(t.ecB),tempChart.update(),tdsChart.update(),phChart.update(),ecChart.update()}function refresh(){fetch("/data").then(t=>t.json()).then(t=>{document.getElementById("sa_val").innerText=t.suhuA.toFixed(1),document.getElementById("ta_val").innerText=t.tdsA.toFixed(0),document.getElementById("pa_val").innerText=t.phA.toFixed(1),document.getElementById("eca_val").innerText=t.ecA.toFixed(2),document.getElementById("sb_val").innerText=t.suhuB.toFixed(1),document.getElementById("tb_val").innerText=t.tdsB.toFixed(0),document.getElementById("pb_val").innerText=t.phB.toFixed(1),document.getElementById("ecb_val").innerText=t.ecB.toFixed(2);const e=document.getElementById("modeBtn");"auto"===t.mode?(e.className="mode-indicator mode-auto",e.innerText="MODE: AUTOMATIC"):(e.className="mode-indicator mode-manual",e.innerText="MODE: MANUAL"),document.getElementById("r1").className=t.relay1?"relay-btn relay-on":"relay-btn relay-off",document.getElementById("r2").className=t.relay2?"relay-btn relay-on":"relay-btn relay-off",updateChartData(t)}).catch(t=>console.error("Error fetching data:",t))}function toggleRelay(t){fetch("/relay?id="+t).then(()=>setTimeout(refresh,200))}function toggleMode(){fetch("/mode?toggle=1").then(()=>setTimeout(refresh,200))}function setAllRelays(t){fetch("/allrelays?state="+(t?"on":"off")).then(()=>setTimeout(refresh,200))}setInterval(refresh,2500),window.onload=refresh;</script></body></html>
)rawliteral";

// ---------------- EEPROM ----------------
#define EEPROM_SIZE 64
const int EEPROM_ADDR_SLOPE = 0;
const int EEPROM_ADDR_NEUTRAL_V = 16;

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

// ---------------- Pembacaan Sensor Utama ----------------
void bacaSensorA(){
  tempA = readTemperatureSensor();

  // --- Baca TDS & Hitung EC ---
  float measuredVoltage_TDS = readVoltageADC(SENSOR_TDS_PIN); 
  float tdsValue = 0.0;
  if (measuredVoltage_TDS <= voltage_mid) {
    tdsValue = linInterp(measuredVoltage_TDS, voltage_clean, voltage_mid, ppm_clean, ppm_mid);
  } else {
    tdsValue = linInterp(measuredVoltage_TDS, voltage_mid, voltage_high, ppm_mid, ppm_high);
  }
  tdsValue = constrain(tdsValue, 0, 5000);
  float compensatedTds = tdsValue / (1.0 + 0.02 * (tempA - 25.0));
  tdsA = compensatedTds;
  ecA = hitungEC(tdsA);

  // --- Baca pH berdasarkan Kalibrasi ---
  if (ph_slope != 0) { 
    float v_ph = readVoltageADC(SENSOR_PH_PIN); 
    Serial.print("pH Voltage Reading: "); Serial.print(v_ph, 4); Serial.println(" V");

    // Rumus Standar Kompensasi Suhu (Persamaan Nernst)
    float compensated_slope = ph_slope * (tempA + 273.15) / (25.0 + 273.15);
    
    // Rumus Standar Konversi Tegangan ke pH
    phA = 7.0 + (ph_neutral_v - v_ph) / compensated_slope;
    phA = constrain(phA, 0.0, 14.0);
  } else {
    phA = 0.0; // Jika belum dikalibrasi, tampilkan 0
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
  lcd.setCursor(0, 0);
  lcd.print("T:"); lcd.print(tempA, 0); lcd.write(byte(0)); lcd.print("C");
  lcd.print(" EC:"); lcd.print(ecA, 1);
  
  lcd.setCursor(0, 1);
  lcd.print("pH:"); lcd.print(phA, 1);
  lcd.setCursor(8, 1);
  lcd.print("M:"); lcd.print(modeOtomatis ? "Auto" : "Manual");
}

// ---------------- Fuzzy Controller (Hanya untuk Nutrisi) ----------------
void fuzzyController(bool &outPeris1, bool &outPeris2) {
  auto mf_tds_low = [](float v)->float{ if (v <= 1000) return 1.0; if (v >= 1300) return 0.0; return (1300.0 - v) / 300.0; };
  auto mf_tds_high = [](float v)->float{ if (v <= 1400) return 0.0; if (v >= 1600) return 1.0; return (v - 1400.0) / 200.0; };
  auto mf_ec_low = [](float ec)->float{ if (ec <= 1.8) return 1.0; if (ec >= 2.2) return 0.0; return (2.2 - ec) / 0.4; };
  auto mf_ec_high = [](float ec)->float{ if (ec <= 2.4) return 0.0; if (ec >= 2.8) return 1.0; return (ec - 2.4) / 0.4; };

  float nutrient_needed_score = max(mf_tds_low(tdsA), mf_ec_low(ecA));
  bool need_nutrients = (nutrient_needed_score >= 0.7);
  
  outPeris1 = need_nutrients;
  outPeris2 = need_nutrients;

  float shutdown_score = max(mf_tds_high(tdsA), mf_ec_high(ecA));
  if (shutdown_score >= 0.8) {
    outPeris1 = false;
    outPeris2 = false;
  }
}

void logicController() {
  bool outPeris1 = false, outPeris2 = false;
  fuzzyController(outPeris1, outPeris2);
  digitalWrite(RELAY_PERISTALTIC_1_PIN, outPeris1 ? LOW : HIGH);
  digitalWrite(RELAY_PERISTALTIC_2_PIN, outPeris2 ? LOW : HIGH);
}

// ---------------- Komunikasi & Web Server ----------------
void kirimDataKeGoogleSheet() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = String(googleScriptURL) +
                 "?suhuA=" + String(tempA) +
                 "&tdsA=" + String(tdsA) +
                 "&phA=" + String(phA) +
                 "&ecA=" + String(ecA) + 
                 "&suhuB=" + String(tempB) +
                 "&tdsB=" + String(tdsB) +
                 "&phB=" + String(phB) +
                 "&ecB=" + String(ecB);
    http.begin(url);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.GET();
    http.end();
  }
}

void handleSerialCommands() {
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    if (command.equalsIgnoreCase("v")) {
      float currentVoltage = readVoltageADC(SENSOR_PH_PIN);
      Serial.println("========================================");
      Serial.print("Membaca Tegangan Sensor pH Saat Ini: ");
      Serial.print(currentVoltage, 4);
      Serial.println(" V");
      Serial.println("========================================");
    }
  }
}

void setupWebServer(){
  server.on("/",HTTP_GET,[](AsyncWebServerRequest *r){ r->send_P(200,"text/html",index_html); });
  
  server.on("/data",HTTP_GET,[](AsyncWebServerRequest *r){
    String json="{";
    json+="\"suhuA\":"+String(tempA,1)+",\"tdsA\":"+String(tdsA,0)+",\"phA\":"+String(phA,1)+",\"ecA\":"+String(ecA,2)+",";
    json+="\"suhuB\":"+String(tempB,1)+",\"tdsB\":"+String(tdsB,0)+",\"phB\":"+String(phB,1)+",\"ecB\":"+String(ecB,2)+",";
    json+="\"mode\":\""+String(modeOtomatis?"auto":"manual")+"\",";
    json += "\"relay1\":" + String(digitalRead(RELAY_PERISTALTIC_1_PIN)==LOW ? "true" : "false") + ",";
    json += "\"relay2\":" + String(digitalRead(RELAY_PERISTALTIC_2_PIN)==LOW ? "true" : "false");
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
      int pin = (id==1) ? RELAY_PERISTALTIC_1_PIN : (id==2) ? RELAY_PERISTALTIC_2_PIN : -1;
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

  server.on("/allrelays", HTTP_GET, [](AsyncWebServerRequest *r){
    if(r->hasParam("state")) {
      String state = r->getParam("state")->value();
      modeOtomatis = false; 
      uint8_t relayState = (state == "on") ? LOW : HIGH;
      digitalWrite(RELAY_PERISTALTIC_1_PIN, relayState);
      digitalWrite(RELAY_PERISTALTIC_2_PIN, relayState);
      r->send(200, "text/plain", "OK");
    } else {
      r->send(400, "text/plain", "Missing state parameter.");
    }
  });

  server.begin();
}

// ---------------- Setup ----------------
void setup(){
  Serial.begin(115200);
  delay(100);

  EEPROM.begin(EEPROM_SIZE);

  // Jika dipaksa kalibrasi ulang, atau jika EEPROM kosong/rusak
  if (FORCE_RECALIBRATE || isnan(EEPROM.get(EEPROM_ADDR_SLOPE, ph_slope)) || ph_slope == 0.0) {
    Serial.println("==================================================");
    Serial.println("MEMULAI PROSES KALIBRASI & PENYIMPANAN BARU...");
    
    // Safety Check: Validasi input pengguna sebelum menghitung
    if (CAL_PH4_VOLTAGE <= CAL_PH7_VOLTAGE) {
        Serial.println("\n!!!!!!!!!!!!!! KESALAHAN KRITIS !!!!!!!!!!!!!!");
        Serial.println("Tegangan untuk pH 4.0 HARUS LEBIH TINGGI dari pH 7.0.");
        Serial.println("Periksa kembali nilai Anda di kode. Program Dihentikan.");
        Serial.println("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
        while(true) { delay(1000); } // Hentikan program
    }

    ph_neutral_v = CAL_PH7_VOLTAGE;
    // Rumus slope standar dari dua titik referensi
    ph_slope = (CAL_PH4_VOLTAGE - CAL_PH7_VOLTAGE) / (4.0 - 7.0);

    EEPROM.put(EEPROM_ADDR_NEUTRAL_V, ph_neutral_v);
    EEPROM.put(EEPROM_ADDR_SLOPE, ph_slope);
    if (EEPROM.commit()) {
      Serial.println("-> SUKSES: Kalibrasi baru berhasil disimpan ke EEPROM.");
    } else {
      Serial.println("-> GAGAL: Tidak bisa menyimpan kalibrasi ke EEPROM!");
    }
  } else {
    // Muat kalibrasi yang sudah ada dari EEPROM
    EEPROM.get(EEPROM_ADDR_NEUTRAL_V, ph_neutral_v);
    EEPROM.get(EEPROM_ADDR_SLOPE, ph_slope);
    Serial.println("Kalibrasi pH berhasil dimuat dari EEPROM.");
  }

  Serial.println("--------------------------------------------------");
  Serial.printf("Nilai Kalibrasi yang Digunakan:\n");
  Serial.printf(" -> Tegangan Netral (pH 7.0): %.4f V\n", ph_neutral_v);
  Serial.printf(" -> Slope Sensor: %.4f V/pH\n", ph_slope);
  Serial.println("==================================================");

  lcd.init();
  lcd.backlight();
  lcd.createChar(0, degree_char);

  pinMode(RELAY_PERISTALTIC_1_PIN, OUTPUT); digitalWrite(RELAY_PERISTALTIC_1_PIN,HIGH);
  pinMode(RELAY_PERISTALTIC_2_PIN, OUTPUT); digitalWrite(RELAY_PERISTALTIC_2_PIN,HIGH);

  sensors.begin(); 

  analogReadResolution(12);
  analogSetPinAttenuation(SENSOR_PH_PIN, ADC_11db);
  analogSetPinAttenuation(SENSOR_TDS_PIN, ADC_11db);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid,password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");
  Serial.println(WiFi.localIP());

  delay(1000);
  setupWebServer();
  Serial.println("\n-- Smart Melon Ready --");
  Serial.println("Kirim 'v' melalui Serial Monitor untuk melihat tegangan pH sensor.");
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
    }
  }

  handleSerialCommands();
  delay(10);
}

