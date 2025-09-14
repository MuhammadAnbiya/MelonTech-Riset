/******************************************************************************
 * Proyek: Smart Watering Melon Tech Nusa Putra Riset BIMA
 * Versi: 3.0 - Dengan Web UI Grafik Futuristik & LCD 16x2
 * Deskripsi: Versi final dengan dasbor web modern yang menampilkan
 * grafik data sensor real-time menggunakan Chart.js.
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

// ---------- KONFIGURASI ----------
const char* ssid = "anbi";          // Ganti dengan SSID WiFi Anda
const char* password = "88888888";  // Ganti dengan password WiFi Anda

LiquidCrystal_I2C lcd(0x27, 16, 2); // Alamat 0x27, ukuran 16x2

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
float phB=0, tdsB=0, tempB=0; // Data dari ESP kedua (jika ada)
bool modeOtomatis = true;

AsyncWebServer server(80);
OneWire oneWire(SENSOR_SUHU_PIN);
DallasTemperature sensors(&oneWire);
DFRobot_EC tds;

unsigned long prevMillis = 0;
const long interval = 5000; // Interval pembacaan sensor

// --- KODE HTML, CSS, & JAVASCRIPT UNTUK WEB UI BARU (DENGAN FOOTER) ---
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>🍈 Smart Melon Greenhouse</title>
    <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
    <style>
        @import url('https://fonts.googleapis.com/css2?family=Orbitron:wght@400;700;900&display=swap');
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            font-family: 'Orbitron', monospace;
            background: linear-gradient(135deg, #0f2027, #203a43, #2c5364);
            color: #e0e0e0;
            overflow-x: hidden;
        }
        .container { max-width: 1200px; margin: 0 auto; padding: 20px; }
        .header {
            text-align: center;
            margin-bottom: 30px;
        }
        .title {
            font-size: 2.5rem;
            font-weight: 900;
            background: linear-gradient(45deg, #4ecf87, #fff, #a8f5c6);
            -webkit-background-clip: text;
            -webkit-text-fill-color: transparent;
            text-shadow: 0 0 25px rgba(78, 207, 135, 0.5);
            animation: glow 2s ease-in-out infinite alternate;
        }
        @keyframes glow {
            from { text-shadow: 0 0 20px rgba(78, 207, 135, 0.4); }
            to { text-shadow: 0 0 35px rgba(78, 207, 135, 0.7), 0 0 50px rgba(78, 207, 135, 0.2); }
        }
        .grid-layout {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(340px, 1fr));
            gap: 20px;
        }
        .card {
            background: rgba(42, 63, 80, 0.5);
            backdrop-filter: blur(10px);
            border: 1px solid rgba(78, 207, 135, 0.2);
            border-radius: 15px;
            padding: 20px;
            box-shadow: 0 8px 32px rgba(0, 0, 0, 0.3);
            transition: all 0.3s ease;
        }
        .card:hover {
            transform: translateY(-5px);
            box-shadow: 0 12px 40px rgba(0, 0, 0, 0.4), 0 0 40px rgba(78, 207, 135, 0.2);
        }
        .card-title {
            font-size: 1.3rem;
            color: #4ecf87;
            margin-bottom: 20px;
            text-align: center;
        }
        .glance-grid {
            display: grid;
            grid-template-columns: repeat(3, 1fr);
            gap: 15px;
            margin-bottom: 20px;
        }
        .glance-item {
            text-align: center;
        }
        .glance-value {
            font-size: 2rem;
            font-weight: 700;
            color: #fff;
        }
        .glance-label {
            font-size: 0.8rem;
            color: #a8f5c6;
        }
        .controls { text-align: center; }
        .mode-indicator {
            padding: 10px 20px; border-radius: 50px; font-weight: 700;
            font-size: 1rem; margin-bottom: 20px; border: 2px solid;
            cursor: pointer; transition: all 0.3s ease;
        }
        .mode-auto { background: #27ae60; border-color: #4ecf87; color: #fff; }
        .mode-manual { background: #f39c12; border-color: #f1c40f; color: #fff; }
        .relay-controls {
            display: grid; grid-template-columns: repeat(3, 1fr); gap: 10px;
        }
        .relay-btn {
            padding: 12px; border: none; border-radius: 10px; font-family: 'Orbitron';
            font-weight: 700; font-size: 0.9rem; cursor: pointer; transition: all 0.3s ease;
        }
        .relay-on { background: #27ae60; color: #fff; }
        .relay-off { background: #c0392b; color: #fff; }
        .chart-container { position: relative; height: 250px; width: 100%; }
        
        /* --- STYLING UNTUK FOOTER BARU --- */
        footer {
            text-align: center;
            padding: 30px 20px;
            margin-top: 40px;
            border-top: 1px solid rgba(78, 207, 135, 0.2);
        }
        .footer-copyright {
            color: rgba(224, 224, 224, 0.7);
            font-size: 0.9rem;
            margin-bottom: 15px;
            letter-spacing: 1px;
        }
        .footer-team {
            color: #a8f5c6;
            font-size: 0.85rem;
            line-height: 1.6;
            max-width: 900px;
            margin: 0 auto;
        }
        .footer-team span {
            display: inline-block;
            margin: 0 10px;
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1 class="title">SMARTWATERING MELON GREENHOUSE DASHBOARD</h1>
        </div>

        <div class="grid-layout">
            <!-- Card Panel A -->
            <div class="card">
                <h3 class="card-title">CONTROL PANEL A - OVERVIEW</h3>
                <div class="glance-grid">
                    <div class="glance-item">
                        <div class="glance-value" id="sa_val">--</div>
                        <div class="glance-label">Suhu (°C)</div>
                    </div>
                    <div class="glance-item">
                        <div class="glance-value" id="ta_val">--</div>
                        <div class="glance-label">TDS (ppm)</div>
                    </div>
                    <div class="glance-item">
                        <div class="glance-value" id="pa_val">--</div>
                        <div class="glance-label">pH Level</div>
                    </div>
                </div>
            </div>

            <!-- Card Panel B -->
            <div class="card">
                <h3 class="card-title">CONTROL PANEL B - OVERVIEW</h3>
                <div class="glance-grid">
                    <div class="glance-item">
                        <div class="glance-value" id="sb_val">--</div>
                        <div class="glance-label">Suhu (°C)</div>
                    </div>
                    <div class="glance-item">
                        <div class="glance-value" id="tb_val">--</div>
                        <div class="glance-label">TDS (ppm)</div>
                    </div>
                    <div class="glance-item">
                        <div class="glance-value" id="pb_val">--</div>
                        <div class="glance-label">pH Level</div>
                    </div>
                </div>
            </div>
            
            <!-- Card Output Control -->
            <div class="card controls">
                <h3 class="card-title">OUTPUT CONTROL</h3>
                <div id="modeBtn" onclick="toggleMode()" class="mode-indicator">Loading...</div>
                <div class="relay-controls">
                    <button class="relay-btn" id="r1" onclick="toggleRelay(1)">PUMP A</button>
                    <button class="relay-btn" id="r2" onclick="toggleRelay(2)">PUMP B</button>
                    <button class="relay-btn" id="r3" onclick="toggleRelay(3)">AERATOR</button>
                </div>
            </div>

            <!-- Card Grafik -->
            <div class="card">
                <h3 class="card-title">Temperature Trend (°C)</h3>
                <div class="chart-container">
                    <canvas id="tempChart"></canvas>
                </div>
            </div>
            <div class="card">
                <h3 class="card-title">TDS Trend (ppm)</h3>
                <div class="chart-container">
                    <canvas id="tdsChart"></canvas>
                </div>
            </div>
            <div class="card">
                <h3 class="card-title">pH Level Trend</h3>
                <div class="chart-container">
                    <canvas id="phChart"></canvas>
                </div>
            </div>
        </div>
    </div>

    <!-- --- FOOTER BARU DITAMBAHKAN DI SINI --- -->
    <footer>
        <p class="footer-copyright">&copy; 2025 TEAM RISET BIMA</p>
        <p class="footer-team">
            <span>Gina Purnama Insany</span> &bull;
            <span>Ivana Lucia Kharisma</span> &bull;
            <span>Kamdan</span> &bull;
            <span>Imam Sanjaya</span> &bull;
            <span>Muhammad Anbiya Fatah</span> &bull;
            <span>Panji Angkasa Putra</span>
        </p>
    </footer>

<script>
    // --- Chart.js Initialization ---
    const MAX_DATA_POINTS = 20;
    const chartData = {
        labels: [],
        tempA: [], tempB: [],
        tdsA: [], tdsB: [],
        phA: [], phB: []
    };

    function createChart(ctx, label, datasets) {
        return new Chart(ctx, {
            type: 'line',
            data: {
                labels: chartData.labels,
                datasets: datasets
            },
            options: {
                responsive: true,
                maintainAspectRatio: false,
                scales: {
                    x: { ticks: { color: '#a8f5c6' }, grid: { color: 'rgba(78, 207, 135, 0.1)' } },
                    y: { ticks: { color: '#a8f5c6' }, grid: { color: 'rgba(78, 207, 135, 0.1)' } }
                },
                plugins: {
                    legend: { labels: { color: '#e0e0e0' } }
                },
                animation: { duration: 500 },
                elements: { line: { tension: 0.3 } }
            }
        });
    }

    const tempChart = createChart(document.getElementById('tempChart'), 'Temperature', [
        { label: 'Sensor A', data: chartData.tempA, borderColor: '#4ecf87', backgroundColor: 'rgba(78, 207, 135, 0.2)', fill: true },
        { label: 'Sensor B', data: chartData.tempB, borderColor: '#f39c12', backgroundColor: 'rgba(243, 156, 18, 0.2)', fill: true }
    ]);
    const tdsChart = createChart(document.getElementById('tdsChart'), 'TDS', [
        { label: 'Sensor A', data: chartData.tdsA, borderColor: '#3498db', backgroundColor: 'rgba(52, 152, 219, 0.2)', fill: true },
        { label: 'Sensor B', data: chartData.tdsB, borderColor: '#9b59b6', backgroundColor: 'rgba(155, 89, 182, 0.2)', fill: true }
    ]);
    const phChart = createChart(document.getElementById('phChart'), 'pH', [
        { label: 'Sensor A', data: chartData.phA, borderColor: '#e74c3c', backgroundColor: 'rgba(231, 76, 60, 0.2)', fill: true },
        { label: 'Sensor B', data: chartData.phB, borderColor: '#1abc9c', backgroundColor: 'rgba(26, 188, 156, 0.2)', fill: true }
    ]);

    function updateChartData(data) {
        const now = new Date();
        const timeLabel = `${now.getHours().toString().padStart(2, '0')}:${now.getMinutes().toString().padStart(2, '0')}:${now.getSeconds().toString().padStart(2, '0')}`;
        
        if (chartData.labels.length >= MAX_DATA_POINTS) {
            chartData.labels.shift();
            chartData.tempA.shift(); chartData.tempB.shift();
            chartData.tdsA.shift(); chartData.tdsB.shift();
            chartData.phA.shift(); chartData.phB.shift();
        }

        chartData.labels.push(timeLabel);
        chartData.tempA.push(data.suhuA); chartData.tempB.push(data.suhuB);
        chartData.tdsA.push(data.tdsA); chartData.tdsB.push(data.tdsB);
        chartData.phA.push(data.phA); chartData.phB.push(data.phB);

        tempChart.update();
        tdsChart.update();
        phChart.update();
    }

    // --- Data Fetching and UI Update ---
    function refresh(){
        fetch('/data')
        .then(response => response.json())
        .then(data => {
            document.getElementById('sa_val').innerText = data.suhuA.toFixed(1);
            document.getElementById('ta_val').innerText = data.tdsA.toFixed(0);
            document.getElementById('pa_val').innerText = data.phA.toFixed(1);
            document.getElementById('sb_val').innerText = data.suhuB.toFixed(1);
            document.getElementById('tb_val').innerText = data.tdsB.toFixed(0);
            document.getElementById('pb_val').innerText = data.phB.toFixed(1);

            const modeBtn = document.getElementById('modeBtn');
            if(data.mode === "auto"){
                modeBtn.className = "mode-indicator mode-auto";
                modeBtn.innerText = 'MODE: AUTOMATIC';
            } else {
                modeBtn.className = "mode-indicator mode-manual";
                modeBtn.innerText = 'MODE: MANUAL';
            }

            document.getElementById("r1").className = data.relay1 ? "relay-btn relay-on" : "relay-btn relay-off";
            document.getElementById("r2").className = data.relay2 ? "relay-btn relay-on" : "relay-btn relay-off";
            document.getElementById("r3").className = data.relay3 ? "relay-btn relay-on" : "relay-btn relay-off";

            updateChartData(data);
        })
        .catch(error => console.error('Error fetching data:', error));
    }

    function toggleRelay(id){ fetch('/relay?id='+id).then(() => setTimeout(refresh, 200)); }
    function toggleMode(){ fetch('/mode?toggle=1').then(() => setTimeout(refresh, 200)); }
    
    setInterval(refresh, 2500);
    window.onload = refresh;
</script>
</body>
</html>
)rawliteral";

// --- FUNGSI SENSOR ---
void bacaSensorA(){
  sensors.requestTemperatures();
  tempA = sensors.getTempCByIndex(0);
  if(tempA==DEVICE_DISCONNECTED_C || tempA < -10) tempA = 25.0;

  float v_tds = analogRead(SENSOR_TDS_PIN) / 4095.0 * voltage;
  tdsA = tds.readEC(v_tds, tempA);
  // Pastikan nilai TDS tidak negatif
  if(tdsA < 0) tdsA = 0;

  float v_ph = analogRead(SENSOR_PH_PIN) / 4095.0 * 5.0;
  phA = PH_OFFSET + (v_ph - 2.5) * PH_SLOPE;
}

void updateSensorB(float ph, float tds, float temp){ phB=ph; tdsB=tds; tempB=temp; }

// --- FUNGSI LCD UNTUK 16x2 ---
void updateLCD16x2() {
  lcd.clear();

  // Baris 1: Suhu & TDS
  lcd.setCursor(0, 0);
  lcd.print("S:");
  lcd.print(tempA, 0);
  lcd.write(byte(0)); // Simbol derajat kustom
  lcd.print(" TDS:");
  lcd.print(tdsA, 0);

  // Baris 2: pH & Mode
  lcd.setCursor(0, 1);
  lcd.print("pH:");
  lcd.print(phA, 1);
  lcd.setCursor(8, 1);
  lcd.print(" M:");
  lcd.print(modeOtomatis ? "Auto" : "Manual");
}

// --- FUNGSI LOGIKA KONTROL OTOMATIS ---
void logicController() {
  bool outA = false, outB = false, outAer = false;

  // Logika Aerator selalu aktif jika suhu panas
  if (tempA > 28) outAer = true;

  // Logika Pompa dengan Prioritas
  if (tdsA > 1000) { // Prioritas 1: TDS terlalu tinggi, matikan semua pompa nutrisi
    outA = false;
    outB = false;
  } else if (tdsA < 400) { // Prioritas 2: TDS terlalu rendah, nyalakan keduanya untuk menambah nutrisi
    outA = true;
    outB = true;
  } else { // Prioritas 3: Jika TDS ideal, baru lakukan koreksi pH
    if (phA > 7.5) outA = true;  // pH terlalu basa, nyalakan pompa pH down
    else if (phA < 6) outB = true; // pH terlalu asam, nyalakan pompa pH up
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
  lcd.createChar(0, degree_char); // Daftarkan karakter derajat kustom

  pinMode(RELAY_POMPA_A_PIN, OUTPUT);
  pinMode(RELAY_POMPA_B_PIN, OUTPUT);
  pinMode(RELAY_AERATOR_PIN, OUTPUT);
  digitalWrite(RELAY_POMPA_A_PIN,HIGH);
  digitalWrite(RELAY_POMPA_B_PIN,HIGH);
  digitalWrite(RELAY_AERATOR_PIN,HIGH);

  sensors.begin(); 
  tds.begin();

  WiFi.begin(ssid,password);
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
    Serial.println("\nIP: " + WiFi.localIP().toString());
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
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print("AP IP Address:");
    lcd.setCursor(0, 1); lcd.print(WiFi.softAPIP().toString());
    delay(2000);
  }

  // Web Server Handlers
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
        modeOtomatis=false; // Setiap aksi manual akan menonaktifkan mode otomatis
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