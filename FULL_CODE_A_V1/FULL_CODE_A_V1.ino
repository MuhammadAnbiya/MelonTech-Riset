/******************************************************************************
 * Proyek: Smart Watering Melon Tech Nusa Putra Riset BIMA
 * Versi: UI & LOGIC FINAL (Simulasi pH & LCD Statis V2)
 ******************************************************************************/

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ESPAsyncWebServer.h>
#include <LiquidCrystal_I2C.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <EEPROM.h>
#include <Fuzzy.h>
#include <ArduinoJson.h>
#include <WiFiUdp.h>
#include <NTPClient.h> 

// --- KONFIGURASI & PINOUT ---
const char* ssid = "ADVAN V1 PRO-8F7379";
const char* password = "7C27964D";
const char* googleScriptURL = "https://script.google.com/macros/s/AKfycbykPgTShvrR1f4P7--ePX_PreK6hs72qzP2epQvB62gPjbhT8BuM47060T0tFlP_ettiw/exec"; 
#define SENSOR_TDS_PIN          34
#define SENSOR_PH_PIN           35 // Pin ini tidak akan dibaca, tapi tetap didefinisikan
#define SENSOR_SUHU_PIN         4
#define RELAY_PUMP_A_PIN        25
#define RELAY_PUMP_B_PIN        26
const float PPM_TO_EC_CONVERSION_FACTOR = 700.0; // Sesuai permintaan: EC(mS) * 700 = PPM

// =================================================================================
// --- ALGORITMA: KALIBRASI MULTI-TITIK DOSIS ---
const float PUMP_A_CAL_TIME_MS[] = {0.0, 2805.0, 5102.0, 9703.0}; // Waktu untuk (0, 500, 1000, 2000) mL
const float PUMP_A_CAL_ML[] =      {0.0, 320.0,  796.0,  1812.0}; // Volume aktual yang keluar
const float PUMP_B_CAL_TIME_MS[] = {0.0, 2753.0, 5009.0, 9518.0}; // Waktu untuk (0, 500, 1000, 2000) mL
const float PUMP_B_CAL_ML[] =      {0.0, 276.0,  700.0,  1708.0}; // Volume aktual yang keluar
const int CAL_POINTS = 4; 
// =================================================================================

// --- Kalibrasi Sensor pH (Hanya untuk referensi, tidak dipakai) ---
const bool FORCE_RESET_CAL_PH = false;
const float CAL_PH7_VOLTAGE   = 2.85;
const float CAL_PH4_VOLTAGE   = 3.05;
const float PH_AIR_OFFSET     = 2.43;
const float VOLTAGE_THRESHOLD_PH_DRY = 3.20;

// --- PERBAIKAN: Kalibrasi Sensor EC 3-Titik (Berdasarkan data user) ---
// Data poin dalam microSiemens (uS) untuk presisi interpolasi
const float CAL_V_LOW   = 0.18;  // Tegangan untuk poin 1
const float CAL_EC_LOW  = 260.0; // uS
const float CAL_V_MID   = 2.15;  // Tegangan untuk poin 2
const float CAL_EC_MID  = 1834.0; // uS
const float CAL_V_HIGH  = 2.31;  // Tegangan untuk poin 3
const float CAL_EC_HIGH = 3940.0; // uS
const float VOLTAGE_THRESHOLD_TDS_DRY = 0.02; // Tetap gunakan threshold lama untuk deteksi kering
// ----------------------------------------------------------------------

// --- OBJEK & VARIABEL GLOBAL ---
Fuzzy *fuzzy_EC_Control = new Fuzzy();
Fuzzy *fuzzy_Status_Check = new Fuzzy();

LiquidCrystal_I2C lcd(0x27, 16, 2);
AsyncWebServer server(80);
OneWire oneWire(SENSOR_SUHU_PIN);
DallasTemperature sensors(&oneWire);

WiFiUDP ntpUDP;
const long utcOffsetInSeconds = 7 * 3600;
NTPClient ntpClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);

float phA=0, tdsA=0, tempA=0, ecA=0.0;
float phB=0, tdsB=0, tempB=0, ecB=0.0;
bool modeOtomatis = true;
int umurTanamanHari = 1;

String statusPanelA = "Menunggu Data...";
String statusPanelB = "Menunggu Data...";
String fuzzyDecisionMessage = "Mematikan Pompa";
String dosingStatusMessage = "Idle";

unsigned long prevMillis = 0;
const long interval = 15000;

bool isDosing = false;
unsigned long dosingStopA = 0; 
unsigned long dosingStopB = 0; 
bool dosingPumpA = false;
bool dosingPumpB = false;

bool doseB_is_pending = false;
unsigned long durationB_pending_ms = 0;

float ph_slope = 0.0;
float ph_neutral_v = 0.0;
#define EEPROM_SIZE 64
const int EEPROM_ADDR_SLOPE = 0;
const int EEPROM_ADDR_NEUTRAL_V = 16;
const int EEPROM_ADDR_PLANT_AGE = 32;
byte degree_char[8] = { B00110, B01001, B01001, B00110, B00000, B00000, B00000, B00000 };

// --- KODE HTML, CSS, JS (DASHBOARD BARU) ---
const char index_html[] PROGMEM = R"=====(
<!DOCTYPE html><html lang="en"><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width, initial-scale=1.0"><title>🍈 Smart Melon Greenhouse</title><script src="https://cdn.jsdelivr.net/npm/chart.js"></script><style>@import url('https://fonts.googleapis.com/css2?family=Orbitron:wght@400;700;900&display=swap');*{margin:0;padding:0;box-sizing:border-box}body{font-family:'Orbitron',monospace;background:linear-gradient(135deg,#0f2027,#203a43,#2c5364);color:#e0e0e0;overflow-x:hidden;transition:background .3s ease, color .3s ease}.container{max-width:1400px;margin:0 auto;padding:20px}.header{text-align:center;margin-bottom:30px;position:relative}.title{font-size:2.5rem;font-weight:900;background:linear-gradient(45deg,#4ecf87,#fff,#a8f5c6);-webkit-background-clip:text;-webkit-text-fill-color:transparent;text-shadow:0 0 25px rgba(78,207,135,.5);animation:glow 2s ease-in-out infinite alternate}@keyframes glow{from{text-shadow:0 0 20px rgba(78,207,135,.4)}to{text-shadow:0 0 35px rgba(78,207,135,.7),0 0 50px rgba(78,207,135,.2)}}.grid-layout{display:grid;grid-template-columns:repeat(auto-fit,minmax(340px,1fr));gap:20px}.card{background:rgba(42,63,80,.5);backdrop-filter:blur(10px);border:1px solid rgba(78,207,135,.2);border-radius:15px;padding:20px;box-shadow:0 8px 32px rgba(0,0,0,.3);transition:all .3s ease}.card-title{font-size:1.3rem;color:#4ecf87;margin-bottom:20px;text-align:center}.glance-grid{display:grid;grid-template-columns:repeat(4,1fr);gap:15px;margin-bottom:20px}.glance-item{text-align:center}.glance-value{font-size:1.8rem;font-weight:700;color:#fff}.glance-label{font-size:.75rem;color:#a8f5c6}.controls{text-align:center}.mode-indicator{padding:10px 20px;border-radius:50px;font-weight:700;font-size:1rem;margin-bottom:20px;border:2px solid;cursor:pointer;transition:all .3s ease}.mode-auto{background:#27ae60;border-color:#4ecf87;color:#fff}.mode-manual{background:#f39c12;border-color:#f1c40f;color:#fff}.relay-btn{padding:12px;border:none;border-radius:10px;font-family:'Orbitron';font-weight:700;font-size:.9rem;cursor:pointer;transition:all .3s ease}.relay-on{background:#27ae60;color:#fff}.relay-off{background:#c0392b;color:#fff}.chart-container{position:relative;height:250px;width:100%}.logic-status-grid{display:grid;grid-template-columns:1fr 1fr;gap:15px;text-align:center}.logic-label{font-size:.75rem;color:#a8f5c6;margin-bottom:5px}.logic-value{font-size:1.5rem;font-weight:700;color:#fff}.logic-value.small{font-size:1.2rem;word-wrap:break-word;}.full-span{grid-column:1 / -1}footer{text-align:center;padding:30px 20px;margin-top:40px;border-top:1px solid rgba(78,207,135,.2)}.relay-row{display:grid;grid-template-columns:1fr 1fr;gap:10px;margin-bottom:10px;align-items:center}.relay-row-label{text-align:left;font-size:.9rem;color:#a8f5c6}.dosing-controls{margin-top:20px;border-top:1px solid rgba(78,207,135,.2);padding-top:20px}.dosing-input-group{display:flex;gap:10px;margin-bottom:10px}.dosing-input{flex-grow:1;padding:10px;background:rgba(0,0,0,.3);border:1px solid #4ecf87;border-radius:8px;color:#fff;font-family:'Orbitron';font-size:.9rem}.dosing-btn-group{display:grid;grid-template-columns:1fr 1fr;gap:10px;margin-bottom:10px}.dosing-btn{background:#3498db;color:#fff}.dosing-status{margin-top:10px;font-size:1rem;color:#f1c40f;min-height:24px;text-align:center}.status-box{background:rgba(0,0,0,.2);border-radius:8px;padding:15px;text-align:center}.status-label{font-size:.9rem;color:#a8f5c6;margin-bottom:8px}.status-value{font-size:1.3rem;font-weight:700;color:#fff;min-height:30px}#theme-toggle-btn{position:absolute;top:15px;right:20px;background:rgba(255,255,255,0.1);border:1px solid rgba(255,255,255,0.2);color:#e0e0e0;cursor:pointer;padding:8px;border-radius:50%;display:flex;align-items:center;justify:content:center;transition:all .3s ease}#theme-toggle-btn:hover{background:rgba(255,255,255,0.2)}.icon-moon{display:none}html.light body{background:linear-gradient(135deg,#e0eafc,#f3f4f6);color:#1f2937}html.light .title{background:linear-gradient(45deg,#067d6e,#059669,#047857);-webkit-background-clip:text;-webkit-text-fill-color:transparent;text-shadow:none}html.light .card{background:rgba(255,255,255,.6);backdrop-filter:blur(10px);border:1px solid rgba(0,0,0,.1);box-shadow:0 4px 12px rgba(0,0,0,.05)}html.light .card-title{color:#047857}html.light .glance-value,html.light .logic-value{color:#1f2937}html.light .glance-label,html.light .logic-label,html.light .relay-row-label{color:#374151}html.light .status-value{color:#1f2937}html.light .status-label{color:#374151}html.light .status-box{background:rgba(0,0,0,.05)}html.light footer{border-top:1px solid rgba(0,0,0,.1)}html.light .footer-copyright{color:rgba(0,0,0,.7)}html.light .footer-team{color:#059669}html.light #theme-toggle-btn{background:rgba(0,0,0,0.05);border:1px solid rgba(0,0,0,0.1);color:#1f2937}html.light #theme-toggle-btn:hover{background:rgba(0,0,0,0.1)}html.light .icon-sun{display:none}html.light .icon-moon{display:block}</style></head><body><div class="container"><div class="header"><h1 class="title">SMARTWATERING MELON GREENHOUSE DASHBOARD</h1><button id="theme-toggle-btn" onclick="toggleTheme()"><svg class="icon-sun" xmlns="http://www.w3.org/2000/svg" width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><circle cx="12" cy="12" r="4"/><path d="M12 2v2"/><path d="M12 20v2"/><path d="m4.93 4.93 1.41 1.41"/><path d="m17.66 17.66 1.41 1.41"/><path d="M2 12h2"/><path d="M20 12h2"/><path d="m4.93 19.07 1.41-1.41"/><path d="m17.66 6.34 1.41-1.41"/></svg><svg class="icon-moon" xmlns="http://www.w3.org/2000/svg" width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M12 3a6 6 0 0 0 9 9 9 9 0 1 1-9-9Z"/></svg></button></div><div class="grid-layout"><div class="card"><h3 class="card-title">CONTROL PANEL A - OVERVIEW</h3><div class="glance-grid"><div class="glance-item"><div class="glance-value" id="sa_val">--</div><div class="glance-label">Suhu (°C)</div></div><div class="glance-item"><div class="glance-value" id="ta_val">--</div><div class="glance-label">TDS (ppm)</div></div><div class="glance-item"><div class="glance-value" id="eca_val">--</div><div class="glance-label">EC (mS/cm)</div></div><div class="glance-item"><div class="glance-value" id="pa_val">--</div><div class="glance-label">pH Level</div></div></div></div><div class="card"><h3 class="card-title">CONTROL PANEL B - OVERVIEW</h3><div class="glance-grid"><div class="glance-item"><div class="glance-value" id="sb_val">--</div><div class="glance-label">Suhu (°C)</div></div><div class="glance-item"><div class="glance-value" id="tb_val">--</div><div class="glance-label">TDS (ppm)</div></div><div class="glance-item"><div class="glance-value" id="ecb_val">--</div><div classs="glance-label">EC (mS/cm)</div></div><div class="glance-item"><div class="glance-value" id="pb_val">--</div><div class="glance-label">pH Level</div></div></div></div><div class="card"><h3 class="card-title">STATUS KONDISI NUTRISI</h3><div class="logic-status-grid"><div class="status-box"><div class="status-label">PANEL A</div><div class="status-value" id="status_a_val">--</div></div><div class="status-box"><div class="status-label">PANEL B</div><div class="status-value" id="status_b_val">--</div></div></div></div><div class="card"><h3 class="card-title">PENGATURAN & EC KONTROL</h3><div class="logic-status-grid"><div class="logic-item"><div class="logic-label">Umur Saat Ini (Hari)</div><div class="logic-value" id="plant_age">--</div></div><div class="logic-item"><div class="dosing-input-group"><input type="number" id="plant_age_input" class="dosing-input" placeholder="Set Hari..."><button class="relay-btn dosing-btn" onclick="setPlantAge()">Set</button></div></div><div class="logic-item"><div class="logic-label">Target EC</div><div class="logic-value small" id="target_nutrisi">--</div></div><div class="logic-item"><div class="logic-label">Keputusan Dosis</div><div class="logic-value small" id="fuzzy_decision">--</div></div></div></div><div class="card controls"><h3 class="card-title">OUTPUT CONTROL</h3><div id="modeBtn" onclick="toggleMode()" class="mode-indicator">Loading...</div><div class="relay-row"><div class="relay-row-label">Pompa Nutrisi AB Mix A</div><button class="relay-btn" id="r_pump1" onclick="toggleRelay(1)">Toggle</button></div><div class="relay-row"><div class="relay-row-label">Pompa Nutrisi AB Mix B</div><button class="relay-btn" id="r_pump2" onclick="toggleRelay(2)">Toggle</button></div><div class="dosing-controls"><h4 class="card-title" style="font-size:1.1rem;margin-bottom:15px">Kontrol Dosis Presisi (mL)</h4><div class="dosing-input-group"><input type="number" id="dose_ml" class="dosing-input" placeholder="Masukkan mL..."><button class="relay-btn dosing-btn" onclick="startDose('both')">Dosis A+B</button></div><div class="dosing-btn-group"><button class="relay-btn dosing-btn" onclick="startDose('A')">Dosis A</button><button class="relay-btn dosing-btn" onclick="startDose('B')">Dosis B</button></div><div class="dosing-status" id="dose_status">Idle</div></div></div><div class="card"><h3 class="card-title">Temperature Trend (°C)</h3><div class="chart-container"><canvas id="tempChart"></canvas></div></div><div class="card"><h3 class="card-title">TDS Trend (ppm)</h3><div class="chart-container"><canvas id="tdsChart"></canvas></div></div><div class="card"><h3 class="card-title">pH Level Trend</h3><div class="chart-container"><canvas id="phChart"></canvas></div></div><div class="card"><h3 class="card-title">EC Trend (mS/cm)</h3><div class="chart-container"><canvas id="ecChart"></canvas></div></div></div><footer><p class="footer-copyright">&copy; 2025 TEAM RISET BIMA</p><p class="footer-team"><span>Gina Purnama Insany</span> &bull; <span>Ivana Lucia Kharisma</span> &bull; <span>Kamdan</span> &bull; <span>Imam Sanjaya</span> &bull; <span>Muhammad Anbiya Fatah</span> &bull; <span>Panji Angkasa Putra</span></p></footer><script>const MAX_DATA_POINTS=20,chartData={labels:[],tempA:[],tempB:[],tdsA:[],tdsB:[],phA:[],phB:[],ecA:[],ecB:[]};let isManualMode=false;(function(){if(localStorage.getItem('theme')==='light'){document.documentElement.classList.add('light')}})();function createChart(t,e,a){return new Chart(t,{type:"line",data:{labels:chartData.labels,datasets:a},options:{responsive:!0,maintainAspectRatio:!1,scales:{x:{ticks:{color:"#a8f5c6"},grid:{color:"rgba(78,207,135,0.1)"}},y:{ticks:{color:"#a8f5c6"},grid:{color:"rgba(78,207,135,0.1)"}}},plugins:{legend:{labels:{color:"#e0e0e0"}}},animation:{duration:500},elements:{line:{tension:.3}}}})}const tempChart=createChart(document.getElementById("tempChart"),"Temperature",[{label:"Sensor A",data:chartData.tempA,borderColor:"#4ecf87",backgroundColor:"rgba(78,207,135,0.2)",fill:!0},{label:"Sensor B",data:chartData.tempB,borderColor:"#f39c12",backgroundColor:"rgba(243,156,18,0.2)",fill:!0}]),tdsChart=createChart(document.getElementById("tdsChart"),"TDS",[{label:"Sensor A",data:chartData.tdsA,borderColor:"#3498db",backgroundColor:"rgba(52,152,219,0.2)",fill:!0},{label:"Sensor B",data:chartData.tdsB,borderColor:"#9b59b6",backgroundColor:"rgba(155,89,182,0.2)",fill:!0}]),phChart=createChart(document.getElementById("phChart"),"pH",[{label:"Sensor A",data:chartData.phA,borderColor:"#e74c3c",backgroundColor:"rgba(231,76,60,0.2)",fill:!0},{label:"Sensor B",data:chartData.phB,borderColor:"#1abc9c",backgroundColor:"rgba(26,188,156,0.2)",fill:!0}]),ecChart=createChart(document.getElementById("ecChart"),"EC",[{label:"Sensor A",data:chartData.ecA,borderColor:"#f1c40f",backgroundColor:"rgba(241,196,15,0.2)",fill:!0},{label:"Sensor B",data:chartData.ecB,borderColor:"#e67e22",backgroundColor:"rgba(230,126,34,0.2)",fill:!0}]);function updateChartData(t){const e=new Date,a=`${e.getHours().toString().padStart(2,"0")}:${e.getMinutes().toString().padStart(2,"0")}:${e.getSeconds().toString().padStart(2,"0")}`;chartData.labels.length>=MAX_DATA_POINTS&&(chartData.labels.shift(),chartData.tempA.shift(),chartData.tempB.shift(),chartData.tdsA.shift(),chartData.tdsB.shift(),chartData.phA.shift(),chartData.phB.shift(),chartData.ecA.shift(),chartData.ecB.shift()),chartData.labels.push(a),chartData.tempA.push(t.suhuA),chartData.tempB.push(t.suhuB),chartData.tdsA.push(t.tdsA),chartData.tdsB.push(t.tdsB),chartData.phA.push(t.phA),chartData.phB.push(t.phB),chartData.ecA.push(t.ecA),chartData.ecB.push(t.ecB),tempChart.update(),tdsChart.update(),phChart.update(),ecChart.update()}function refresh(){fetch("/data").then(e=>{if(!e.ok)throw new Error(`Network response was not ok: ${e.statusText}`);return e.text()}).then(e=>{try{const t=JSON.parse(e);document.getElementById("sa_val").innerText=t.suhuA.toFixed(1),document.getElementById("ta_val").innerText=t.tdsA.toFixed(0),document.getElementById("pa_val").innerText=t.phA.toFixed(1),document.getElementById("eca_val").innerText=t.ecA.toFixed(2),document.getElementById("sb_val").innerText=t.suhuB.toFixed(1),document.getElementById("tb_val").innerText=t.tdsB.toFixed(0),document.getElementById("pb_val").innerText=t.phB.toFixed(1),document.getElementById("ecb_val").innerText=t.ecB.toFixed(2);const o=document.getElementById("modeBtn");isManualMode="manual"===t.mode,isManualMode?(o.className="mode-indicator mode-manual",o.innerText="MODE: MANUAL"):(o.className="mode-indicator mode-auto",o.innerText="MODE: AUTOMATIC"),document.getElementById("r_pump1").className=t.pumpAState?"relay-btn relay-on":"relay-btn relay-off",document.getElementById("r_pump2").className=t.pumpBState?"relay-btn relay-on":"relay-btn relay-off",document.getElementById("plant_age").innerText=t.plantAge,document.getElementById("target_nutrisi").innerText=t.targetNutrisi,document.getElementById("fuzzy_decision").innerText=t.fuzzyDecision,document.getElementById("dose_status").innerText=t.dosingStatus;const n=document.getElementById("status_a_val");n.innerText=t.statusA,n.style.color="Kelebihan"===t.statusA?"#e74c3c":"Kekurangan"===t.statusA?"#f1c40f":"#4ecf87";const d=document.getElementById("status_b_val");d.innerText=t.statusB,d.style.color="Kelebihan"===t.statusB?"#e74c3c":"Kekurangan"===t.statusB?"#f1c40f":"#4ecf87",updateChartData(t)}catch(t){console.error("Failed to parse JSON:",t),console.error("Raw response from server was:",e)}}).catch(e=>{console.error("Error fetching data:",e)})}function toggleTheme(){const t=document.documentElement;t.classList.toggle('light'),localStorage.setItem('theme',t.classList.contains('light')?'light':'dark')}function toggleRelay(t){fetch(`/relay?id=${t}`).then(()=>setTimeout(refresh,200))}function toggleMode(){fetch("/mode?toggle=1").then(()=>setTimeout(refresh,200))}function startDose(t){if(!isManualMode)return void alert("Dosing can only be done in MANUAL mode.");const e=document.getElementById("dose_ml").value;if(!e||e<=0)return void alert("Please enter a valid amount in mL.");const a=document.getElementById("dose_status");a.innerText=`Sending command for ${e}mL...`,fetch(`/dose?ml=${e}&pump=${t}`).then(e=>{if(!e.ok)return e.text().then(t=>{throw new Error(t||"Failed to start dosing")});return e.text()}).then(e=>{a.innerText=e,setTimeout(refresh,200)}).catch(e=>{a.innerText=`Error: ${e.message}`,console.error("Dosing error:",e)})}function setPlantAge(){const t=document.getElementById("plant_age_input"),e=t.value;if(!e||e<=0)return void alert("Masukkan umur tanaman yang valid.");fetch(`/setAge?days=${e}`).then(e=>{if(!e.ok)throw new Error("Gagal mengupdate umur tanaman.");return e.text()}).then(a=>{alert(`Sukses: ${a}`),t.value="",setTimeout(refresh,200)}).catch(t=>{alert(`Error: ${t.message}`)})}setInterval(refresh,2500),window.onload=refresh;</script></body></html>
)=====";

// --- FUNGSI-FUNGSI BANTU ---
float linInterp(float x, float x0, float x1, float y0, float y1) { if (x1 - x0 == 0) return y0; return y0 + (y1 - y0) * ((x - x0) / (x1 - x0)); }
float readVoltageADC(int pin) {
  const int NUM_SAMPLES = 40;
  const float VREF = 3.3;
  long sum = 0;
  for (int i = 0; i < NUM_SAMPLES; i++) {
    sum += analogRead(pin);
    delay(2);
  }
  float raw = (float)sum / NUM_SAMPLES;
  return raw / 4095.0 * VREF;
}
float readTemperatureSensor() { sensors.requestTemperatures(); float t = sensors.getTempCByIndex(0); if (t == DEVICE_DISCONNECTED_C || t < -20 || t > 80) return 25.0; return t; }

// --- PERBAIKAN: Fungsi konversi EC dan PPM ---
float hitungPPM_from_EC_mS(float ec_mS) {
  // Konversi dari EC (mS/cm) ke PPM
  if (PPM_TO_EC_CONVERSION_FACTOR == 0) return 0;
  return ec_mS * PPM_TO_EC_CONVERSION_FACTOR;
}

float hitungEC_mS_from_PPM(float tdsValue) {
  // Konversi dari PPM ke EC (mS/cm)
  if (PPM_TO_EC_CONVERSION_FACTOR == 0) return 0;
  return tdsValue / PPM_TO_EC_CONVERSION_FACTOR;
}
// ---------------------------------------------

float readpH(float v_ph, float temp_celsius) { if (ph_slope == 0.0) return 0.0; float compensated_slope = ph_slope * (temp_celsius + 273.15) / (25.0 + 273.15); float ph_value = 7.0 + (ph_neutral_v - v_ph) / compensated_slope; ph_value += PH_AIR_OFFSET; return constrain(ph_value, 0.0, 14.0); }

// --- FUNGSI UTAMA PEMBACAAN SENSOR ---
// --- PERBAIKAN: Menggunakan logika kalibrasi 3-titik ---
void bacaSensorA() {
  tempA = readTemperatureSensor();
  float measuredVoltage_TDS = readVoltageADC(SENSOR_TDS_PIN);
  float raw_ec_uS = 0.0; // Nilai EC mentah (uS) sebelum kompensasi suhu

  if (measuredVoltage_TDS < VOLTAGE_THRESHOLD_TDS_DRY) { 
    raw_ec_uS = 0.0; 
  } else {
    // --- Logika interpolasi multi-titik (piecewise) ---
    if (measuredVoltage_TDS <= CAL_V_LOW) {
      // Jika di bawah titik kalibrasi terendah, interpolasi dari 0
      raw_ec_uS = linInterp(measuredVoltage_TDS, 0.0, CAL_V_LOW, 0.0, CAL_EC_LOW);
    } else if (measuredVoltage_TDS <= CAL_V_MID) {
      // Interpolasi antara Poin 1 (LOW) dan Poin 2 (MID)
      raw_ec_uS = linInterp(measuredVoltage_TDS, CAL_V_LOW, CAL_V_MID, CAL_EC_LOW, CAL_EC_MID);
    } else if (measuredVoltage_TDS <= CAL_V_HIGH) {
      // Interpolasi antara Poin 2 (MID) dan Poin 3 (HIGH)
      raw_ec_uS = linInterp(measuredVoltage_TDS, CAL_V_MID, CAL_V_HIGH, CAL_EC_MID, CAL_EC_HIGH);
    } else {
      // Ekstrapolasi di atas Poin 3 (HIGH)
      // Menggunakan slope dari segmen terakhir (MID ke HIGH)
      raw_ec_uS = linInterp(measuredVoltage_TDS, CAL_V_MID, CAL_V_HIGH, CAL_EC_MID, CAL_EC_HIGH);
    }
  }

  // Constrain nilai mentah
  raw_ec_uS = constrain(raw_ec_uS, 0, 10000); // Batas aman 10000 uS = 10 mS
  
  // --- PERBAIKAN: Kompensasi Suhu pada EC, BUKAN PADA PPM ---
  // Kompensasi EC ke 25°C
  float compensated_ec_uS = raw_ec_uS / (1.0 + 0.02 * (tempA - 25.0));

  // --- PERBAIKAN: Alur Logika -> Hitung EC dulu, baru PPM ---
  // Konversi EC (uS) yang sudah dikompensasi ke EC (mS) untuk display
  ecA = compensated_ec_uS / 1000.0; // ecA sekarang dalam mS/cm
  
  // Hitung PPM dari EC (mS)
  tdsA = hitungPPM_from_EC_mS(ecA); // tdsA sekarang dalam PPM (faktor 700)

  // --- PERBAIKAN: SIMULASI DATA PH ---
  float basePH = 6.0; // Nilai tengah pH nutrisi
  float variasi = random(-20, 21) / 100.0; // Variasi acak +/- 0.20
  phA = basePH + variasi;
  // -------------------------------------
}
// --------------------------------------------------------

// --- PERBAIKAN: Tampilan LCD STATIS ---
void updateLCD16x2() {
  lcd.clear();
  // Baris 1: Suhu dan pH
  lcd.setCursor(0, 0); 
  lcd.print("T:"); 
  lcd.print(tempA, 1); 
  lcd.write(byte(0)); 
  lcd.print("C");
  
  lcd.setCursor(9, 0); 
  lcd.print("pH:"); 
  lcd.print(phA, 1);
  
  // Baris 2: PPM, EC, dan Mode
  String modeStr = modeOtomatis ? "[A]" : "[M]";
  char buffer[17]; // 16 karakter + null terminator
  
  // Format: P:[ppm] E:[ec] [Mode]
  snprintf(buffer, 17, "P:%-3.0f E:%-3.1f %s", tdsA, ecA, modeStr.c_str());
  
  lcd.setCursor(0, 1);
  lcd.print(buffer);
}

// --- BAGIAN LOGIKA FUZZY ---
struct TargetNutrisiEC { 
  float ec_bawah; 
  float ec_atas; 
};

TargetNutrisiEC getTargetNutrisiEC(int hari) { 
  TargetNutrisiEC target;
  if (hari >= 0 && hari <= 1)   { target.ec_bawah = 1.0; target.ec_atas = 1.5; }
  else if (hari >= 2 && hari <= 6)   { target.ec_bawah = 2.0; target.ec_atas = 2.5; }
  else if (hari >= 7 && hari <= 11)  { target.ec_bawah = 3.2; target.ec_atas = 3.5; }
  else if (hari >= 12 && hari <= 16) { target.ec_bawah = 3.0; target.ec_atas = 3.2; }
  else if (hari >= 17 && hari <= 21) { target.ec_bawah = 2.8; target.ec_atas = 3.0; }
  else if (hari >= 22 && hari <= 26) { target.ec_bawah = 2.2; target.ec_atas = 2.5; }
  else if (hari >= 27 && hari <= 31) { target.ec_bawah = 3.0; target.ec_atas = 3.2; }
  else if (hari >= 32 && hari <= 36) { target.ec_bawah = 2.8; target.ec_atas = 3.0; }
  else if (hari >= 37 && hari <= 56) { target.ec_bawah = 2.5; target.ec_atas = 2.8; }
  else if (hari >= 57 && hari <= 66) { target.ec_bawah = 2.2; target.ec_atas = 2.5; }
  else if (hari >= 67 && hari <= 75) { target.ec_bawah = 3.0; target.ec_atas = 3.2; }
  else { target.ec_bawah = 2.2; target.ec_atas = 2.5; }
  return target; 
}

void setupFuzzy_EC_Control() {
  FuzzyInput *errorEC = new FuzzyInput(1);
  FuzzySet *tinggi = new FuzzySet(-2.0, -2.0, -1.0, -0.3);
  errorEC->addFuzzySet(tinggi);
  FuzzySet *ideal = new FuzzySet(-0.3, -0.1, 0.1, 0.3);
  errorEC->addFuzzySet(ideal);
  FuzzySet *rendah = new FuzzySet(0.1, 0.3, 0.7, 0.9);
  errorEC->addFuzzySet(rendah);
  FuzzySet *sangatRendah = new FuzzySet(0.7, 1.0, 2.0, 2.0);
  errorEC->addFuzzySet(sangatRendah);
  fuzzy_EC_Control->addFuzzyInput(errorEC);

  FuzzyOutput *durasiDosis = new FuzzyOutput(1);
  const int DURASI_DOSIS_SEDANG = 5000;
  const int DURASI_DOSIS_LAMA = 10000;
  FuzzySet *mati = new FuzzySet(0, 0, 0, 0);
  durasiDosis->addFuzzySet(mati);
  FuzzySet *sedang = new FuzzySet(0, DURASI_DOSIS_SEDANG, DURASI_DOSIS_SEDANG, DURASI_DOSIS_SEDANG + 2000);
  durasiDosis->addFuzzySet(sedang);
  FuzzySet *lama = new FuzzySet(DURASI_DOSIS_SEDANG, DURASI_DOSIS_LAMA, DURASI_DOSIS_LAMA, DURASI_DOSIS_LAMA);
  durasiDosis->addFuzzySet(lama);
  fuzzy_EC_Control->addFuzzyOutput(durasiDosis);

  FuzzyRuleAntecedent *if_ec_tinggi = new FuzzyRuleAntecedent();
  if_ec_tinggi->joinSingle(tinggi);
  FuzzyRuleConsequent *then_mati = new FuzzyRuleConsequent();
  then_mati->addOutput(mati);
  fuzzy_EC_Control->addFuzzyRule(new FuzzyRule(1, if_ec_tinggi, then_mati));

  FuzzyRuleAntecedent *if_ec_ideal = new FuzzyRuleAntecedent();
  if_ec_ideal->joinSingle(ideal);
  fuzzy_EC_Control->addFuzzyRule(new FuzzyRule(2, if_ec_ideal, then_mati));

  FuzzyRuleAntecedent *if_ec_rendah = new FuzzyRuleAntecedent();
  if_ec_rendah->joinSingle(rendah);
  FuzzyRuleConsequent *then_sedang = new FuzzyRuleConsequent();
  then_sedang->addOutput(sedang);
  fuzzy_EC_Control->addFuzzyRule(new FuzzyRule(3, if_ec_rendah, then_sedang));

  FuzzyRuleAntecedent *if_ec_sangat_rendah = new FuzzyRuleAntecedent();
  if_ec_sangat_rendah->joinSingle(sangatRendah);
  FuzzyRuleConsequent *then_lama = new FuzzyRuleConsequent();
  then_lama->addOutput(lama);
  fuzzy_EC_Control->addFuzzyRule(new FuzzyRule(4, if_ec_sangat_rendah, then_lama));
}

void runPumpController() { 
  if (isDosing) return;
  TargetNutrisiEC target = getTargetNutrisiEC(umurTanamanHari); 
  float target_mid_ec = (target.ec_bawah + target.ec_atas) / 2.0;
  float error_ec = target_mid_ec - ecA;

  fuzzy_EC_Control->setInput(1, error_ec);
  fuzzy_EC_Control->fuzzify();
  long durasiNyala = fuzzy_EC_Control->defuzzify(1);

  if (durasiNyala > 500) {
    isDosing = true;
    unsigned long now = millis();
    
    dosingStopA = now + durasiNyala; 
    dosingPumpA = true;
    digitalWrite(RELAY_PUMP_A_PIN, LOW);
    
    doseB_is_pending = true; 
    durationB_pending_ms = durasiNyala; 
    
    dosingStatusMessage = "Dosis Otomatis " + String(durasiNyala) + " ms...";
  }
}

void setupFuzzy_Status_Check() {
  FuzzyInput *suhu = new FuzzyInput(1);
  suhu->addFuzzySet(new FuzzySet(0, 0, 15, 18));
  suhu->addFuzzySet(new FuzzySet(18, 21.5, 21.5, 25));
  suhu->addFuzzySet(new FuzzySet(25, 28, 30, 30));
  fuzzy_Status_Check->addFuzzyInput(suhu);

  FuzzyInput *ph = new FuzzyInput(2);
  ph->addFuzzySet(new FuzzySet(1, 1, 5.0, 5.5));
  ph->addFuzzySet(new FuzzySet(5.5, 6.0, 6.5, 7.0));
  ph->addFuzzySet(new FuzzySet(7.0, 7.5, 14, 14));
  fuzzy_Status_Check->addFuzzyInput(ph);

  FuzzyInput *tds = new FuzzyInput(3);
  FuzzySet *tds_rendah_fs = new FuzzySet(0, 0, 350, 700);
  tds->addFuzzySet(tds_rendah_fs);
  FuzzySet *tds_sedang_fs = new FuzzySet(700, 1150, 1150, 1600);
  tds->addFuzzySet(tds_sedang_fs);
  FuzzySet *tds_tinggi_fs = new FuzzySet(1600, 1950, 2300, 2300);
  tds->addFuzzySet(tds_tinggi_fs);
  fuzzy_Status_Check->addFuzzyInput(tds);

  FuzzyOutput *kondisi = new FuzzyOutput(1);
  FuzzySet *kekurangan_fs = new FuzzySet(1, 1, 1, 1);
  kondisi->addFuzzySet(kekurangan_fs);
  FuzzySet *cukup_fs = new FuzzySet(2, 2, 2, 2);
  kondisi->addFuzzySet(cukup_fs);
  FuzzySet *kelebihan_fs = new FuzzySet(3, 3, 3, 3);
  kondisi->addFuzzySet(kelebihan_fs);
  fuzzy_Status_Check->addFuzzyOutput(kondisi);

  FuzzyRuleAntecedent *if_tds_rendah = new FuzzyRuleAntecedent();
  if_tds_rendah->joinSingle(tds_rendah_fs);
  FuzzyRuleConsequent *then_kekurangan = new FuzzyRuleConsequent();
  then_kekurangan->addOutput(kekurangan_fs);
  fuzzy_Status_Check->addFuzzyRule(new FuzzyRule(1, if_tds_rendah, then_kekurangan));
  
  FuzzyRuleAntecedent *if_tds_sedang = new FuzzyRuleAntecedent();
  if_tds_sedang->joinSingle(tds_sedang_fs);
  FuzzyRuleConsequent *then_cukup = new FuzzyRuleConsequent();
  then_cukup->addOutput(cukup_fs);
  fuzzy_Status_Check->addFuzzyRule(new FuzzyRule(2, if_tds_sedang, then_cukup));

  FuzzyRuleAntecedent *if_tds_tinggi = new FuzzyRuleAntecedent();
  if_tds_tinggi->joinSingle(tds_tinggi_fs);
  FuzzyRuleConsequent *then_kelebihan = new FuzzyRuleConsequent();
  then_kelebihan->addOutput(kelebihan_fs);
  fuzzy_Status_Check->addFuzzyRule(new FuzzyRule(3, if_tds_tinggi, then_kelebihan));
}

String runStatusFuzzyLogic(float suhu, float ph, float tds) {
  fuzzy_Status_Check->setInput(1, suhu);
  fuzzy_Status_Check->setInput(2, ph);
  fuzzy_Status_Check->setInput(3, tds);
  fuzzy_Status_Check->fuzzify();
  float result = fuzzy_Status_Check->defuzzify(1);

  if (result < 1.5) return "Kekurangan";
  if (result > 2.5) return "Kelebihan";
  return "Cukup";
}

// --- FUNGSI BARU: INTERPOLASI MULTI-TITIK ---
unsigned long getAccurateDosingTime(float targetML, const float cal_ml[], const float cal_ms[], int points) {
  if (targetML <= 0) return 0;

  for (int i = 1; i < points; i++) {
    if (targetML <= cal_ml[i]) {
      return (unsigned long)linInterp(targetML, cal_ml[i-1], cal_ml[i], cal_ms[i-1], cal_ms[i]);
    }
  }
  
  return (unsigned long)linInterp(targetML, cal_ml[points-2], cal_ml[points-1], cal_ms[points-2], cal_ms[points-1]);
}
// ----------------------------------------------------------------


// --- KOMUNIKASI & WEB SERVER ---
void kirimDataKeGoogleSheet() {
  if (WiFi.status() == WL_CONNECTED) {
    float s_tempA = isnan(tempA) || isinf(tempA) ? 0.0 : tempA;
    float s_tdsA  = isnan(tdsA)  || isinf(tdsA)  ? 0.0 : tdsA;
    float s_phA   = isnan(phA)   || isinf(phA)   ? 0.0 : phA;
    float s_ecA   = isnan(ecA)   || isinf(ecA)   ? 0.0 : ecA;
    float s_tempB = isnan(tempB) || isinf(tempB) ? 0.0 : tempB;
    float s_tdsB  = isnan(tdsB)  || isinf(tdsB)  ? 0.0 : tdsB;
    float s_phB   = isnan(phB)   || isinf(phB)   ? 0.0 : phB;
    float s_ecB   = isnan(ecB)   || isinf(ecB)   ? 0.0 : ecB;
    
    // --- PERBAIKAN: Menggunakan epochTime untuk format yang stabil ---
    time_t epochTime = ntpClient.getEpochTime();
    struct tm *ptm = gmtime((time_t *)&epochTime);
    char tanggal[11]; // YYYY-MM-DD
    strftime(tanggal, sizeof(tanggal), "%Y-%m-%d", ptm);
    
    char waktu[9]; // HH:MM:SS
    strftime(waktu, sizeof(waktu), "%H:%M:%S", ptm);

    const char* hariArr[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
    String hari = hariArr[ptm->tm_wday];
    
    char urlBuffer[512];
    snprintf(urlBuffer, sizeof(urlBuffer),
             "%s?tanggal=%s&hari=%s&waktu=%s&suhuA=%.1f&suhuB=%.1f&tdsA=%.0f&tdsB=%.0f&phA=%.2f&phB=%.2f&ecA=%.2f&ecB=%.2f&statusA=%s&statusB=%s",
             googleScriptURL,
             tanggal, hari.c_str(), waktu,
             s_tempA, s_tempB, s_tdsA, s_tdsB, s_phA, s_phB, s_ecA, s_ecB, statusPanelA.c_str(), statusPanelB.c_str());

    HTTPClient http;
    http.begin(urlBuffer);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    
    Serial.println("[Google Sheet] Mengirim data...");
    int httpCode = http.GET();
    if (httpCode > 0) {
      Serial.printf("[Google Sheet] Kode Respon: %d\n", httpCode);
    } else {
      Serial.printf("[Google Sheet] Gagal terhubung. Error: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end();
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
      Serial.print(" -> Tegangan pH       : "); Serial.print(phVoltage, 4); Serial.println(" V");
      Serial.print(" -> Tegangan TDS/PPM  : "); Serial.print(tdsVoltage, 4); Serial.println(" V");
      Serial.println("========================================");
    }
  }
}
void setupWebServer() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *r){ r->send_P(200, "text/html", index_html); });
  
  server.on("/data", HTTP_GET, [&](AsyncWebServerRequest *r){
    TargetNutrisiEC target = getTargetNutrisiEC(umurTanamanHari); 
    String fuzzyTargetMessage = "EC " + String(target.ec_bawah, 1) + "-" + String(target.ec_atas, 1);
    
    if(isDosing) {
        if(doseB_is_pending) {
            fuzzyDecisionMessage = "Dosis A... (B Menunggu)";
        } else if (dosingPumpB) {
            fuzzyDecisionMessage = "Dosis B (Sekuensial)...";
        } else {
            fuzzyDecisionMessage = "Dosis Otomatis Aktif";
        }
    } else {
        fuzzyDecisionMessage = "Pompa Idle";
    }

    String json = "{";
    json += "\"suhuA\":" + String(tempA, 1) + ",\"tdsA\":" + String(tdsA, 0) + ",\"phA\":" + String(phA, 1) + ",\"ecA\":" + String(ecA, 2) + ",";
    json += "\"suhuB\":" + String(tempB, 1) + ",\"tdsB\":" + String(tdsB, 0) + ",\"phB\":" + String(phB, 1) + ",\"ecB\":" + String(ecB, 2) + ",";
    json += "\"mode\":\"" + String(modeOtomatis ? "auto" : "manual") + "\",";
    json += "\"pumpAState\":" + String(digitalRead(RELAY_PUMP_A_PIN) == LOW ? "true" : "false") + ",";
    json += "\"pumpBState\":" + String(digitalRead(RELAY_PUMP_B_PIN) == LOW ? "true" : "false") + ",";
    json += "\"statusA\":\"" + statusPanelA + "\",";
    json += "\"statusB\":\"" + statusPanelB + "\",";
    json += "\"fuzzyDecision\":\"" + fuzzyDecisionMessage + "\",";
    json += "\"targetNutrisi\":\"" + fuzzyTargetMessage + "\",";
    json += "\"dosingStatus\":\"" + dosingStatusMessage + "\",";
    json += "\"plantAge\":" + String(umurTanamanHari);
    json += "}";
    r->send(200, "application/json", json);
  });

  server.on("/updateB", HTTP_GET, [&](AsyncWebServerRequest *r){
    if(r->hasParam("ph") && r->hasParam("tds") && r->hasParam("temp")){
      phB = r->getParam("ph")->value().toFloat();
      tdsB = r->getParam("tds")->value().toFloat();
      tempB = r->getParam("temp")->value().toFloat();
      
      // --- PERBAIKAN: Menggunakan nama fungsi yang sudah diperbarui ---
      ecB = hitungEC_mS_from_PPM(tdsB);
      // ---------------------------------------------------------
      
      statusPanelB = runStatusFuzzyLogic(tempB, phB, tdsB);
      r->send(200, "text/plain", "OK");
    } else {
      r->send(400, "text/plain", "Missing params");
    }
  });


  server.on("/relay", HTTP_GET, [&](AsyncWebServerRequest *r){
    if (modeOtomatis || isDosing) return r->send(403, "text/plain", "Set to MANUAL mode and wait for dosing to finish.");
    if (r->hasParam("id")) {
      int pumpId = r->getParam("id")->value().toInt();
      if (pumpId == 1) {
        digitalWrite(RELAY_PUMP_A_PIN, !digitalRead(RELAY_PUMP_A_PIN));
      } else if (pumpId == 2) {
        digitalWrite(RELAY_PUMP_B_PIN, !digitalRead(RELAY_PUMP_B_PIN));
      }
      r->send(200, "text/plain", "OK");
    }
  });

  server.on("/mode", HTTP_GET, [&](AsyncWebServerRequest *r){
    if (r->hasParam("toggle")) {
      if (isDosing) return r->send(403, "text/plain", "Cannot change mode while dosing.");
      modeOtomatis = !modeOtomatis;
      if (!modeOtomatis) {
        digitalWrite(RELAY_PUMP_A_PIN, HIGH);
        digitalWrite(RELAY_PUMP_B_PIN, HIGH);
      }
    }
    r->send(200, "text/plain", "OK");
  });

  server.on("/setAge", HTTP_GET, [&](AsyncWebServerRequest *r){
    if (r->hasParam("days")) {
      int days = r->getParam("days")->value().toInt();
      if (days > 0 && days < 120) {
        umurTanamanHari = days;
        EEPROM.put(EEPROM_ADDR_PLANT_AGE, umurTanamanHari);
        EEPROM.commit();
        r->send(200, "text/plain", "Umur tanaman diupdate ke " + String(days) + " hari.");
      } else {
        r->send(400, "text/plain", "Umur tidak valid.");
      }
    }
  });

  // --- PERBAIKAN: LOGIKA DOSIS SEKUENSIAL & KALIBRASI BARU ---
  // (Tidak ada perubahan di blok ini, logikanya sudah benar)
  server.on("/dose", HTTP_GET, [&](AsyncWebServerRequest *r){
    if (modeOtomatis) return r->send(403, "text/plain", "Set to MANUAL mode first.");
    if (isDosing) return r->send(409, "text/plain", "System is busy dosing.");
    if (r->hasParam("ml") && r->hasParam("pump")) {
      float ml = r->getParam("ml")->value().toFloat();
      String pump = r->getParam("pump")->value();
      if (ml <= 0) return r->send(400, "text/plain", "Invalid mL value.");

      unsigned long durationA_ms = 0;
      unsigned long durationB_ms = 0;
      String responseMessage = "Dosing ";

      if (pump == "A") {
        durationA_ms = getAccurateDosingTime(ml, PUMP_A_CAL_ML, PUMP_A_CAL_TIME_MS, CAL_POINTS);
        responseMessage += String(ml, 0) + "mL Pump A...";
      } else if (pump == "B") {
        durationB_ms = getAccurateDosingTime(ml, PUMP_B_CAL_ML, PUMP_B_CAL_TIME_MS, CAL_POINTS);
        responseMessage += String(ml, 0) + "mL Pump B...";
      } else if (pump == "both") {
        float ml_each = ml / 2.0;
        durationA_ms = getAccurateDosingTime(ml_each, PUMP_A_CAL_ML, PUMP_A_CAL_TIME_MS, CAL_POINTS);
        durationB_ms = getAccurateDosingTime(ml_each, PUMP_B_CAL_ML, PUMP_B_CAL_TIME_MS, CAL_POINTS);
        responseMessage += String(ml, 0) + "mL Total (Sekuensial)...";
      }

      isDosing = true;
      unsigned long now = millis();
      dosingPumpA = (durationA_ms > 0);
      dosingPumpB = false; // Pompa B belum jalan
      dosingStopA = 0; 
      dosingStopB = 0;
      doseB_is_pending = false;
      durationB_pending_ms = 0;

      if (dosingPumpA) {
        dosingStopA = now + durationA_ms;
        digitalWrite(RELAY_PUMP_A_PIN, LOW);
        
        if (pump == "both" && durationB_ms > 0) {
          doseB_is_pending = true;
          durationB_pending_ms = durationB_ms;
        }
      } else if (durationB_ms > 0) {
        dosingPumpB = true;
        dosingStopB = now + durationB_ms;
        digitalWrite(RELAY_PUMP_B_PIN, LOW);
      }
      
      dosingStatusMessage = responseMessage;
      r->send(200, "text/plain", responseMessage);
    } else {
      r->send(400, "text/plain", "Missing parameters.");
    }
  });

  server.begin();
}

// --- FUNGSI SETUP ---
void setup() {
  Serial.begin(115200);
  delay(100);
  EEPROM.begin(EEPROM_SIZE);
  lcd.init();
  lcd.backlight();
  lcd.createChar(0, degree_char);

  setupFuzzy_EC_Control();
  setupFuzzy_Status_Check();

  EEPROM.get(EEPROM_ADDR_PLANT_AGE, umurTanamanHari);
  if (umurTanamanHari <= 0 || umurTanamanHari > 120) {
    umurTanamanHari = 1;
    EEPROM.put(EEPROM_ADDR_PLANT_AGE, umurTanamanHari);
    EEPROM.commit();
  }
  
  EEPROM.get(EEPROM_ADDR_NEUTRAL_V, ph_neutral_v);
  EEPROM.get(EEPROM_ADDR_SLOPE, ph_slope);
  if (ph_slope == 0.0 || isnan(ph_slope) || ph_neutral_v == 0.0 || isnan(ph_neutral_v)) {
    ph_neutral_v = CAL_PH7_VOLTAGE;
    ph_slope = (CAL_PH4_VOLTAGE - CAL_PH7_VOLTAGE) / (4.0 - 7.0);
  }

  pinMode(RELAY_PUMP_A_PIN, OUTPUT); digitalWrite(RELAY_PUMP_A_PIN, HIGH);
  pinMode(RELAY_PUMP_B_PIN, OUTPUT); digitalWrite(RELAY_PUMP_B_PIN, HIGH);

  sensors.begin(); 
  analogReadResolution(12);
  analogSetPinAttenuation(SENSOR_PH_PIN, ADC_11db);
  analogSetPinAttenuation(SENSOR_TDS_PIN, ADC_11db);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  
  Serial.println("\nWiFi connected!");
  Serial.print("IP Address: "); Serial.println(WiFi.localIP());

  ntpClient.begin();
  ntpClient.update();
  Serial.println("NTP Client Dimulai. Waktu saat ini: " + ntpClient.getFormattedTime());

  // Inisialisasi randomSeed untuk simulasi pH
  randomSeed(analogRead(A0)); 

  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("Terhubung!");
  lcd.setCursor(0, 1); lcd.print(WiFi.localIP());
  delay(3000); 

  setupWebServer();
}

// --- FUNGSI LOOP UTAMA ---
void loop() {
  unsigned long now = millis();
  
  if (now - prevMillis >= interval && !isDosing) {
    prevMillis = now;
    ntpClient.update(); 
    bacaSensorA();
    statusPanelA = runStatusFuzzyLogic(tempA, phA, tdsA);
    Serial.printf("\nSuhu=%.1f C, TDS=%.0f ppm, pH=%.2f, EC=%.2f mS\n", tempA, tdsA, phA, ecA);
    updateLCD16x2();
    kirimDataKeGoogleSheet();

    if (modeOtomatis) {
      runPumpController();
    } else {
      dosingStatusMessage = "Mode Manual Aktif";
    }
  }

  if (isDosing) {
    bool pumpA_was_running = dosingPumpA;

    // Cek Pompa A
    if (dosingPumpA && (now >= dosingStopA)) {
      digitalWrite(RELAY_PUMP_A_PIN, HIGH);
      dosingPumpA = false;
      dosingStopA = 0;
      Serial.println("Dosis Pompa A Selesai.");
    }
    
    // --- LOGIKA SEKUENSIAL ---
    if (pumpA_was_running && !dosingPumpA && doseB_is_pending) {
      Serial.println("Memulai Dosis Pompa B (Sekuensial)...");
      dosingStopB = now + durationB_pending_ms;
      dosingPumpB = true;
      digitalWrite(RELAY_PUMP_B_PIN, LOW);
      
      doseB_is_pending = false;
      durationB_pending_ms = 0;
    }
    // ------------------------

    // Cek Pompa B
    if (dosingPumpB && (now >= dosingStopB)) {
      digitalWrite(RELAY_PUMP_B_PIN, HIGH);
      dosingPumpB = false;
      dosingStopB = 0;
      Serial.println("Dosis Pompa B Selesai.");
    }

    if (!dosingPumpA && !dosingPumpB && !doseB_is_pending) {
      isDosing = false;
      dosingStatusMessage = "Dosing complete. Idle.";
    }
  }

  handleSerialCommands();
  delay(10);
}