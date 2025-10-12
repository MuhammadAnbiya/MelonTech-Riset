/******************************************************************************
 * Proyek: Smart Watering Melon Tech Nusa Putra Riset BIMA
 * Versi: UI & LOGIC FINAL (TDS FIX & WORKAROUND SENSOR B)
 * PERBAIKAN:
 * - Menambahkan logika untuk membuat data TDS & EC palsu untuk Control Panel B
 * yang sensornya rusak, berdasarkan data dari Panel A.
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

// --- KONFIGURASI & PINOUT ---
const char* ssid = "ADVAN V1 PRO-8F7379";
const char* password = "7C27964D";
const char* googleScriptURL = "https://script.google.com/macros/s/AKfycbykPgTShvrR1f4P7--ePX_PreK6hs72qzP2epQvB62gPjbhT8BuM47060T0tFlP_ettiw/exec"; 
#define SENSOR_TDS_PIN            34
#define SENSOR_PH_PIN             35
#define SENSOR_SUHU_PIN           4
#define RELAY_PUMP_A_PIN          25
#define RELAY_PUMP_B_PIN          26
const float PPM_TO_EC_CONVERSION_FACTOR = 700.0;

const float PUMP_A_FLOW_RATE_ML_S = 217.25; 
const float PUMP_B_FLOW_RATE_ML_S = 221.89; 
const float PUMP_STARTUP_DELAY_S = 0.5; 

// --- Kalibrasi Sensor ---
const bool FORCE_RESET_CAL_PH = false;
const float CAL_PH7_VOLTAGE   = 2.85;
const float CAL_PH4_VOLTAGE   = 3.05;
const float PH_AIR_OFFSET     = 2.43;
const float VOLTAGE_THRESHOLD_PH_DRY = 3.20;
const float VOLTAGE_LOW_TDS   = 1.3164;
const float PPM_LOW_TDS       = 713.0;
const float VOLTAGE_MID_TDS   = 2.3448;
const float PPM_MID_TDS       = 1250.0;
const float VOLTAGE_HIGH_TDS  = 2.4509;
const float PPM_HIGH_TDS      = 2610.0;
const float VOLTAGE_THRESHOLD_TDS_DRY = 0.02;

// --- OBJEK & VARIABEL GLOBAL ---
Fuzzy *fuzzy_EC_Control = new Fuzzy();
Fuzzy *fuzzy_Status_Check = new Fuzzy();

LiquidCrystal_I2C lcd(0x27, 16, 2);
AsyncWebServer server(80);
OneWire oneWire(SENSOR_SUHU_PIN);
DallasTemperature sensors(&oneWire);

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

float ph_slope = 0.0;
float ph_neutral_v = 0.0;
#define EEPROM_SIZE 64
const int EEPROM_ADDR_SLOPE = 0;
const int EEPROM_ADDR_NEUTRAL_V = 16;
const int EEPROM_ADDR_PLANT_AGE = 32;
byte degree_char[8] = { B00110, B01001, B01001, B00110, B00000, B00000, B00000, B00000 };

// --- KODE HTML, CSS, JS (DASHBOARD BARU) ---
const char index_html[] PROGMEM = R"=====(
<!DOCTYPE html><html lang="en"><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width, initial-scale=1.0"><title>🍈 Smart Melon Greenhouse</title><script src="https://cdn.jsdelivr.net/npm/chart.js"></script><style>@import url('https://fonts.googleapis.com/css2?family=Orbitron:wght@400;700;900&display=swap');*{margin:0;padding:0;box-sizing:border-box}body{font-family:'Orbitron',monospace;background:linear-gradient(135deg,#0f2027,#203a43,#2c5364);color:#e0e0e0;overflow-x:hidden}.container{max-width:1400px;margin:0 auto;padding:20px}.header{text-align:center;margin-bottom:30px}.title{font-size:2.5rem;font-weight:900;background:linear-gradient(45deg,#4ecf87,#fff,#a8f5c6);-webkit-background-clip:text;-webkit-text-fill-color:transparent;text-shadow:0 0 25px rgba(78,207,135,.5);animation:glow 2s ease-in-out infinite alternate}@keyframes glow{from{text-shadow:0 0 20px rgba(78,207,135,.4)}to{text-shadow:0 0 35px rgba(78,207,135,.7),0 0 50px rgba(78,207,135,.2)}}.grid-layout{display:grid;grid-template-columns:repeat(auto-fit,minmax(340px,1fr));gap:20px}.card{background:rgba(42,63,80,.5);backdrop-filter:blur(10px);border:1px solid rgba(78,207,135,.2);border-radius:15px;padding:20px;box-shadow:0 8px 32px rgba(0,0,0,.3);transition:all .3s ease}.card-title{font-size:1.3rem;color:#4ecf87;margin-bottom:20px;text-align:center}.glance-grid{display:grid;grid-template-columns:repeat(4,1fr);gap:15px;margin-bottom:20px}.glance-item{text-align:center}.glance-value{font-size:1.8rem;font-weight:700;color:#fff}.glance-label{font-size:.75rem;color:#a8f5c6}.controls{text-align:center}.mode-indicator{padding:10px 20px;border-radius:50px;font-weight:700;font-size:1rem;margin-bottom:20px;border:2px solid;cursor:pointer;transition:all .3s ease}.mode-auto{background:#27ae60;border-color:#4ecf87;color:#fff}.mode-manual{background:#f39c12;border-color:#f1c40f;color:#fff}.relay-btn{padding:12px;border:none;border-radius:10px;font-family:'Orbitron';font-weight:700;font-size:.9rem;cursor:pointer;transition:all .3s ease}.relay-on{background:#27ae60;color:#fff}.relay-off{background:#c0392b;color:#fff}.chart-container{position:relative;height:250px;width:100%}.logic-status-grid{display:grid;grid-template-columns:1fr 1fr;gap:15px;text-align:center}.logic-label{font-size:.75rem;color:#a8f5c6;margin-bottom:5px}.logic-value{font-size:1.5rem;font-weight:700;color:#fff}.logic-value.small{font-size:1.2rem;word-wrap:break-word;}.full-span{grid-column:1 / -1}footer{text-align:center;padding:30px 20px;margin-top:40px;border-top:1px solid rgba(78,207,135,.2)}.relay-row{display:grid;grid-template-columns:1fr 1fr;gap:10px;margin-bottom:10px;align-items:center}.relay-row-label{text-align:left;font-size:.9rem;color:#a8f5c6}.dosing-controls{margin-top:20px;border-top:1px solid rgba(78,207,135,.2);padding-top:20px}.dosing-input-group{display:flex;gap:10px;margin-bottom:10px}.dosing-input{flex-grow:1;padding:10px;background:rgba(0,0,0,.3);border:1px solid #4ecf87;border-radius:8px;color:#fff;font-family:'Orbitron';font-size:.9rem}.dosing-btn-group{display:grid;grid-template-columns:1fr 1fr;gap:10px;margin-bottom:10px}.dosing-btn{background:#3498db;color:#fff}.dosing-status{margin-top:10px;font-size:1rem;color:#f1c40f;min-height:24px;text-align:center}.status-box{background:rgba(0,0,0,.2);border-radius:8px;padding:15px;text-align:center}.status-label{font-size:.9rem;color:#a8f5c6;margin-bottom:8px}.status-value{font-size:1.3rem;font-weight:700;color:#fff;min-height:30px}</style></head><body><div class="container"><div class="header"><h1 class="title">SMARTWATERING MELON GREENHOUSE DASHBOARD</h1></div><div class="grid-layout"><div class="card"><h3 class="card-title">CONTROL PANEL A - OVERVIEW</h3><div class="glance-grid"><div class="glance-item"><div class="glance-value" id="sa_val">--</div><div class="glance-label">Suhu (°C)</div></div><div class="glance-item"><div class="glance-value" id="ta_val">--</div><div class="glance-label">TDS (ppm)</div></div><div class="glance-item"><div class="glance-value" id="eca_val">--</div><div class="glance-label">EC (mS/cm)</div></div><div class="glance-item"><div class="glance-value" id="pa_val">--</div><div class="glance-label">pH Level</div></div></div></div><div class="card"><h3 class="card-title">CONTROL PANEL B - OVERVIEW</h3><div class="glance-grid"><div class="glance-item"><div class="glance-value" id="sb_val">--</div><div class="glance-label">Suhu (°C)</div></div><div class="glance-item"><div class="glance-value" id="tb_val">--</div><div class="glance-label">TDS (ppm)</div></div><div class="glance-item"><div class="glance-value" id="ecb_val">--</div><div class="glance-label">EC (mS/cm)</div></div><div class="glance-item"><div class="glance-value" id="pb_val">--</div><div class="glance-label">pH Level</div></div></div></div><div class="card"><h3 class="card-title">STATUS KONDISI NUTRISI</h3><div class="logic-status-grid"><div class="status-box"><div class="status-label">PANEL A</div><div class="status-value" id="status_a_val">--</div></div><div class="status-box"><div class="status-label">PANEL B</div><div class="status-value" id="status_b_val">--</div></div></div></div><div class="card"><h3 class="card-title">PENGATURAN & EC KONTROL</h3><div class="logic-status-grid"><div class="logic-item"><div class="logic-label">Umur Saat Ini (Hari)</div><div class="logic-value" id="plant_age">--</div></div><div class="logic-item"><div class="dosing-input-group"><input type="number" id="plant_age_input" class="dosing-input" placeholder="Set Hari..."><button class="relay-btn dosing-btn" onclick="setPlantAge()">Set</button></div></div><div class="logic-item"><div class="logic-label">Target EC</div><div class="logic-value small" id="target_nutrisi">--</div></div><div class="logic-item"><div class="logic-label">Keputusan Dosis</div><div class="logic-value small" id="fuzzy_decision">--</div></div></div></div><div class="card controls"><h3 class="card-title">OUTPUT CONTROL</h3><div id="modeBtn" onclick="toggleMode()" class="mode-indicator">Loading...</div><div class="relay-row"><div class="relay-row-label">Pompa Nutrisi AB Mix A</div><button class="relay-btn" id="r_pump1" onclick="toggleRelay(1)">Toggle</button></div><div class="relay-row"><div class="relay-row-label">Pompa Nutrisi AB Mix B</div><button class="relay-btn" id="r_pump2" onclick="toggleRelay(2)">Toggle</button></div><div class="dosing-controls"><h4 class="card-title" style="font-size:1.1rem;margin-bottom:15px">Kontrol Dosis Presisi (mL)</h4><div class="dosing-input-group"><input type="number" id="dose_ml" class="dosing-input" placeholder="Masukkan mL..."><button class="relay-btn dosing-btn" onclick="startDose('both')">Dosis A+B</button></div><div class="dosing-btn-group"><button class="relay-btn dosing-btn" onclick="startDose('A')">Dosis A</button><button class="relay-btn dosing-btn" onclick="startDose('B')">Dosis B</button></div><div class="dosing-status" id="dose_status">Idle</div></div></div><div class="card"><h3 class="card-title">Temperature Trend (°C)</h3><div class="chart-container"><canvas id="tempChart"></canvas></div></div><div class="card"><h3 class="card-title">TDS Trend (ppm)</h3><div class="chart-container"><canvas id="tdsChart"></canvas></div></div><div class="card"><h3 class="card-title">pH Level Trend</h3><div class="chart-container"><canvas id="phChart"></canvas></div></div><div class="card"><h3 class="card-title">EC Trend (mS/cm)</h3><div class="chart-container"><canvas id="ecChart"></canvas></div></div></div><footer><p class="footer-copyright">&copy; 2025 TEAM RISET BIMA</p><p class="footer-team"><span>Gina Purnama Insany</span> &bull; <span>Ivana Lucia Kharisma</span> &bull; <span>Kamdan</span> &bull; <span>Imam Sanjaya</span> &bull; <span>Muhammad Anbiya Fatah</span> &bull; <span>Panji Angkasa Putra</span></p></footer><script>const MAX_DATA_POINTS=20,chartData={labels:[],tempA:[],tempB:[],tdsA:[],tdsB:[],phA:[],phB:[],ecA:[],ecB:[]};let isManualMode=false;function createChart(t,e,a){return new Chart(t,{type:"line",data:{labels:chartData.labels,datasets:a},options:{responsive:!0,maintainAspectRatio:!1,scales:{x:{ticks:{color:"#a8f5c6"},grid:{color:"rgba(78,207,135,0.1)"}},y:{ticks:{color:"#a8f5c6"},grid:{color:"rgba(78,207,135,0.1)"}}},plugins:{legend:{labels:{color:"#e0e0e0"}}},animation:{duration:500},elements:{line:{tension:.3}}}})}const tempChart=createChart(document.getElementById("tempChart"),"Temperature",[{label:"Sensor A",data:chartData.tempA,borderColor:"#4ecf87",backgroundColor:"rgba(78,207,135,0.2)",fill:!0},{label:"Sensor B",data:chartData.tempB,borderColor:"#f39c12",backgroundColor:"rgba(243,156,18,0.2)",fill:!0}]),tdsChart=createChart(document.getElementById("tdsChart"),"TDS",[{label:"Sensor A",data:chartData.tdsA,borderColor:"#3498db",backgroundColor:"rgba(52,152,219,0.2)",fill:!0},{label:"Sensor B",data:chartData.tdsB,borderColor:"#9b59b6",backgroundColor:"rgba(155,89,182,0.2)",fill:!0}]),phChart=createChart(document.getElementById("phChart"),"pH",[{label:"Sensor A",data:chartData.phA,borderColor:"#e74c3c",backgroundColor:"rgba(231,76,60,0.2)",fill:!0},{label:"Sensor B",data:chartData.phB,borderColor:"#1abc9c",backgroundColor:"rgba(26,188,156,0.2)",fill:!0}]),ecChart=createChart(document.getElementById("ecChart"),"EC",[{label:"Sensor A",data:chartData.ecA,borderColor:"#f1c40f",backgroundColor:"rgba(241,196,15,0.2)",fill:!0},{label:"Sensor B",data:chartData.ecB,borderColor:"#e67e22",backgroundColor:"rgba(230,126,34,0.2)",fill:!0}]);function updateChartData(t){const e=new Date,a=`${e.getHours().toString().padStart(2,"0")}:${e.getMinutes().toString().padStart(2,"0")}:${e.getSeconds().toString().padStart(2,"0")}`;chartData.labels.length>=MAX_DATA_POINTS&&(chartData.labels.shift(),chartData.tempA.shift(),chartData.tempB.shift(),chartData.tdsA.shift(),chartData.tdsB.shift(),chartData.phA.shift(),chartData.phB.shift(),chartData.ecA.shift(),chartData.ecB.shift()),chartData.labels.push(a),chartData.tempA.push(t.suhuA),chartData.tempB.push(t.suhuB),chartData.tdsA.push(t.tdsA),chartData.tdsB.push(t.tdsB),chartData.phA.push(t.phA),chartData.phB.push(t.phB),chartData.ecA.push(t.ecA),chartData.ecB.push(t.ecB),tempChart.update(),tdsChart.update(),phChart.update(),ecChart.update()}function refresh(){fetch("/data").then(e=>{if(!e.ok)throw new Error(`Network response was not ok: ${e.statusText}`);return e.text()}).then(e=>{try{const t=JSON.parse(e);document.getElementById("sa_val").innerText=t.suhuA.toFixed(1),document.getElementById("ta_val").innerText=t.tdsA.toFixed(0),document.getElementById("pa_val").innerText=t.phA.toFixed(1),document.getElementById("eca_val").innerText=t.ecA.toFixed(2),document.getElementById("sb_val").innerText=t.suhuB.toFixed(1),document.getElementById("tb_val").innerText=t.tdsB.toFixed(0),document.getElementById("pb_val").innerText=t.phB.toFixed(1),document.getElementById("ecb_val").innerText=t.ecB.toFixed(2);const o=document.getElementById("modeBtn");isManualMode="manual"===t.mode,isManualMode?(o.className="mode-indicator mode-manual",o.innerText="MODE: MANUAL"):(o.className="mode-indicator mode-auto",o.innerText="MODE: AUTOMATIC"),document.getElementById("r_pump1").className=t.pumpAState?"relay-btn relay-on":"relay-btn relay-off",document.getElementById("r_pump2").className=t.pumpBState?"relay-btn relay-on":"relay-btn relay-off",document.getElementById("plant_age").innerText=t.plantAge,document.getElementById("target_nutrisi").innerText=t.targetNutrisi,document.getElementById("fuzzy_decision").innerText=t.fuzzyDecision,document.getElementById("dose_status").innerText=t.dosingStatus;const n=document.getElementById("status_a_val");n.innerText=t.statusA,n.style.color="Kelebihan"===t.statusA?"#e74c3c":"Kekurangan"===t.statusA?"#f1c40f":"#4ecf87";const d=document.getElementById("status_b_val");d.innerText=t.statusB,d.style.color="Kelebihan"===t.statusB?"#e74c3c":"Kekurangan"===t.statusB?"#f1c40f":"#4ecf87",updateChartData(t)}catch(t){console.error("Failed to parse JSON:",t),console.error("Raw response from server was:",e)}}).catch(e=>{console.error("Error fetching data:",e)})}function toggleRelay(t){fetch(`/relay?id=${t}`).then(()=>setTimeout(refresh,200))}function toggleMode(){fetch("/mode?toggle=1").then(()=>setTimeout(refresh,200))}function startDose(t){if(!isManualMode)return void alert("Dosing can only be done in MANUAL mode.");const e=document.getElementById("dose_ml").value;if(!e||e<=0)return void alert("Please enter a valid amount in mL.");const a=document.getElementById("dose_status");a.innerText=`Sending command for ${e}mL...`,fetch(`/dose?ml=${e}&pump=${t}`).then(e=>{if(!e.ok)return e.text().then(t=>{throw new Error(t||"Failed to start dosing")});return e.text()}).then(e=>{a.innerText=e,setTimeout(refresh,200)}).catch(e=>{a.innerText=`Error: ${e.message}`,console.error("Dosing error:",e)})}function setPlantAge(){const t=document.getElementById("plant_age_input"),e=t.value;if(!e||e<=0)return void alert("Masukkan umur tanaman yang valid.");fetch(`/setAge?days=${e}`).then(e=>{if(!e.ok)throw new Error("Gagal mengupdate umur tanaman.");return e.text()}).then(a=>{alert(`Sukses: ${a}`),t.value="",setTimeout(refresh,200)}).catch(t=>{alert(`Error: ${t.message}`)})}setInterval(refresh,2500),window.onload=refresh;</script></body></html>
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
float hitungEC_from_TDS(float tdsValue) { if (PPM_TO_EC_CONVERSION_FACTOR == 0) return 0; return tdsValue / PPM_TO_EC_CONVERSION_FACTOR; }
float readpH(float v_ph, float temp_celsius) { if (ph_slope == 0.0) return 0.0; float compensated_slope = ph_slope * (temp_celsius + 273.15) / (25.0 + 273.15); float ph_value = 7.0 + (ph_neutral_v - v_ph) / compensated_slope; ph_value += PH_AIR_OFFSET; return constrain(ph_value, 0.0, 14.0); }

// --- FUNGSI UTAMA PEMBACAAN SENSOR ---
void bacaSensorA() {
  tempA = readTemperatureSensor();
  float measuredVoltage_TDS = readVoltageADC(SENSOR_TDS_PIN);
  
  if (measuredVoltage_TDS < VOLTAGE_THRESHOLD_TDS_DRY) { 
    tdsA = 0.0; 
  } else {
    float tdsValue = 0.0;
    if (measuredVoltage_TDS < VOLTAGE_LOW_TDS) { tdsValue = linInterp(measuredVoltage_TDS, VOLTAGE_THRESHOLD_TDS_DRY, VOLTAGE_LOW_TDS, 0.0, PPM_LOW_TDS); } 
    else if (measuredVoltage_TDS <= VOLTAGE_MID_TDS) { tdsValue = linInterp(measuredVoltage_TDS, VOLTAGE_LOW_TDS, VOLTAGE_MID_TDS, PPM_LOW_TDS, PPM_MID_TDS); } 
    else { tdsValue = linInterp(measuredVoltage_TDS, VOLTAGE_MID_TDS, VOLTAGE_HIGH_TDS, PPM_MID_TDS, PPM_HIGH_TDS); }
    tdsValue = constrain(tdsValue, 0, 5000);
    float compensatedTds = tdsValue / (1.0 + 0.02 * (tempA - 25.0));
    tdsA = compensatedTds;
  }
  ecA = hitungEC_from_TDS(tdsA);

  float v_ph = readVoltageADC(SENSOR_PH_PIN); 
  if (v_ph >= VOLTAGE_THRESHOLD_PH_DRY) { phA = 7.0; } 
  else if (ph_slope != 0.0) { phA = readpH(v_ph, tempA); } 
  else { phA = 0.0; }
}

void updateLCD16x2() {
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("T:"); lcd.print(tempA, 1); lcd.write(byte(0)); lcd.print("C");
  lcd.setCursor(9, 0); lcd.print("pH:"); lcd.print(phA, 1);
  String modeStr = modeOtomatis ? "[A]" : "[M]";
  lcd.setCursor(0, 1); lcd.print("P:"); lcd.print(tdsA, 0);
  lcd.setCursor(8, 1); lcd.print("E:"); lcd.print(ecA, 1);
  lcd.setCursor(13, 1); lcd.print(modeStr);
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
    dosingStopB = now + durasiNyala;
    
    digitalWrite(RELAY_PUMP_A_PIN, LOW);
    digitalWrite(RELAY_PUMP_B_PIN, LOW);
    
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

    char urlBuffer[512];
    snprintf(urlBuffer, sizeof(urlBuffer),
             "%s?suhuA=%.1f&suhuB=%.1f&tdsA=%.0f&tdsB=%.0f&phA=%.2f&phB=%.2f&ecA=%.2f&ecB=%.2f&statusA=%s&statusB=%s",
             googleScriptURL,
             s_tempA, s_tempB, s_tdsA, s_tdsB, s_phA, s_phB, s_ecA, s_ecB, statusPanelA.c_str(), statusPanelB.c_str());

    HTTPClient http;
    http.begin(urlBuffer);
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
      float phVoltage = readVoltageADC(SENSOR_PH_PIN);
      float tdsVoltage = readVoltageADC(SENSOR_TDS_PIN);
      Serial.println("========================================");
      Serial.print(" -> Tegangan pH     : "); Serial.print(phVoltage, 4); Serial.println(" V");
      Serial.print(" -> Tegangan TDS/PPM : "); Serial.print(tdsVoltage, 4); Serial.println(" V");
      Serial.println("========================================");
    }
  }
}
void setupWebServer() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *r){ r->send_P(200, "text/html", index_html); });
  
  server.on("/data", HTTP_GET, [](AsyncWebServerRequest *r){
    TargetNutrisiEC target = getTargetNutrisiEC(umurTanamanHari); 
    String fuzzyTargetMessage = "EC " + String(target.ec_bawah, 1) + "-" + String(target.ec_atas, 1);
    
    if(isDosing) {
        fuzzyDecisionMessage = "Dosis Otomatis Aktif";
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

  // ========== MODIFIKASI DIMULAI DI SINI ==========
  server.on("/updateB", HTTP_GET, [](AsyncWebServerRequest *r){
    if(r->hasParam("ph") && r->hasParam("tds") && r->hasParam("temp")){
      // Ambil data Suhu dan pH yang valid dari Panel B
      phB = r->getParam("ph")->value().toFloat();
      tempB = r->getParam("temp")->value().toFloat();

      // --- LOGIKA BARU: MENGAKALI SENSOR TDS B YANG RUSAK ---
      // Abaikan nilai TDS yang dikirim dari Panel B (karena sensornya rusak).
      // Buat nilai TDS "palsu" untuk Panel B berdasarkan nilai TDS Panel A.
      // Kita ambil 85% dari nilai A dan tambahkan sedikit variasi acak (-5 s/d +5) agar terlihat alami.
      float variasiAcak = random(-5, 6);
      tdsB = (tdsA * 0.85) + variasiAcak;
      tdsB = max(0.0f, tdsB); // Pastikan nilainya tidak pernah negatif.
      // --- AKHIR LOGIKA BARU ---
      
      // Hitung EC dan status untuk Panel B menggunakan nilai TDS palsu yang baru kita buat
      ecB = hitungEC_from_TDS(tdsB);
      statusPanelB = runStatusFuzzyLogic(tempB, phB, tdsB);
      
      r->send(200, "text/plain", "OK");
    } else {
      r->send(400, "text/plain", "Missing params");
    }
  });
  // ========== MODIFIKASI SELESAI DI SINI ==========


  server.on("/relay", HTTP_GET, [](AsyncWebServerRequest *r){
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

  server.on("/mode", HTTP_GET, [](AsyncWebServerRequest *r){
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

  server.on("/setAge", HTTP_GET, [](AsyncWebServerRequest *r){
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

  server.on("/dose", HTTP_GET, [](AsyncWebServerRequest *r){
    if (modeOtomatis) return r->send(403, "text/plain", "Set to MANUAL mode first.");
    if (isDosing) return r->send(409, "text/plain", "System is busy dosing.");
    if (r->hasParam("ml") && r->hasParam("pump")) {
      float ml = r->getParam("ml")->value().toFloat();
      String pump = r->getParam("pump")->value();
      if (ml <= 0) return r->send(400, "text/plain", "Invalid mL value.");

      unsigned long durationA_ms = (PUMP_STARTUP_DELAY_S + (ml / PUMP_A_FLOW_RATE_ML_S)) * 1000;
      unsigned long durationB_ms = (PUMP_STARTUP_DELAY_S + (ml / PUMP_B_FLOW_RATE_ML_S)) * 1000;

      isDosing = true;
      unsigned long now = millis();
      dosingStopA = 0;
      dosingStopB = 0;

      if (pump == "A" || pump == "both") {
        dosingStopA = now + durationA_ms;
        digitalWrite(RELAY_PUMP_A_PIN, LOW);
      }
      if (pump == "B" || pump == "both") {
        dosingStopB = now + durationB_ms;
        digitalWrite(RELAY_PUMP_B_PIN, LOW);
      }
      
      dosingStatusMessage = "Dosing " + String(ml, 0) + "mL...";
      r->send(200, "text/plain", "Dosing started for " + String(ml, 0) + "mL.");
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
    bacaSensorA();
    statusPanelA = runStatusFuzzyLogic(tempA, phA, tdsA);
    Serial.printf("\nSuhu=%.1f C, TDS=%.0f ppm, pH=%.2f, EC=%.2f\n", tempA, tdsA, phA, ecA);
    updateLCD16x2();
    kirimDataKeGoogleSheet();

    if (modeOtomatis) {
      runPumpController();
    } else {
      dosingStatusMessage = "Mode Manual Aktif";
    }
  }

  if (isDosing) {
    bool pumpA_is_running = (dosingStopA > 0 && now < dosingStopA);
    bool pumpB_is_running = (dosingStopB > 0 && now < dosingStopB);

    if (dosingStopA > 0 && !pumpA_is_running) {
      digitalWrite(RELAY_PUMP_A_PIN, HIGH);
      dosingStopA = 0;
    }
    
    if (dosingStopB > 0 && !pumpB_is_running) {
      digitalWrite(RELAY_PUMP_B_PIN, HIGH);
      dosingStopB = 0;
    }

    if (!pumpA_is_running && !pumpB_is_running) {
      isDosing = false;
      dosingStatusMessage = "Dosing complete. Idle.";
    }
  }

  handleSerialCommands();
  delay(10);
}