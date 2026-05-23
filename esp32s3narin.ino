#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_NeoPixel.h>

const char* ssid = "";
const char* password = "";

IPAddress local_IP(192, 168, 1, 15);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress dns(8, 8, 8, 8);

WebServer server(99);

#define LED_R 14
#define LED_Y 15
#define LED_G 16

#define BUZZER_PIN 5

#define LDR_PIN 1
#define POT_PIN 6

#define SW1_PIN 35
#define SW2_PIN 36
#define SW3_PIN 37

#define RGB_PIN 21
#define RGB_COUNT 4

#define ROTARY_SW_PIN 40   
#define ROTARY_A_PIN 38   
#define ROTARY_B_PIN 39    

Adafruit_NeoPixel rgbStrip(RGB_COUNT, RGB_PIN, NEO_GRB + NEO_KHZ800);
bool rgbReady = false;

#define I2C_SDA 8
#define I2C_SCL 9

unsigned long bootMillis = 0;

bool ledRState = false;
bool ledYState = false;
bool ledGState = false;

bool buzzerState = false;
int rgbMode = 0;  


const unsigned long BUZZER_HALF_PERIOD_US = 500;
unsigned long lastBuzzerToggleUs = 0;
bool buzzerPinLevel = false;

volatile long rotaryCount = 0;
int lastRotaryA = HIGH;
int lastRotaryB = HIGH;
int lastRotarySw = HIGH;
unsigned long lastRotaryMs = 0;
unsigned long lastRotarySwMs = 0;
String rotaryDirection = "STOP";

bool plcMode = false;       
String plcInput = "NONE";   
String plcOutput = "NONE";  
String plcAction = "ON";    
bool plcRuleMatched = false;
unsigned long lastPlcMs = 0;

bool rgbRotateMode = false;
int rgbRotateIndex = 0;
unsigned long lastRgbRotateMs = 0;
int rgbRotateDelay = 0; 


const int MORSE_FREQ_HZ = 700;
const int MORSE_DOT_MS = 120;
const int MORSE_DASH_MS = MORSE_DOT_MS * 3;
const int MORSE_SYMBOL_GAP_MS = MORSE_DOT_MS;
const int MORSE_LETTER_GAP_MS = MORSE_DOT_MS * 3;
bool morseBusy = false;
String lastMorseText = "";
String lastMorsePattern = "";

int lastMorseDotSw = HIGH;
int lastMorseDashSw = HIGH;
unsigned long lastMorseButtonMs = 0;

String liveMorseBuffer = "";
String liveMorseDecoded = "";
String liveMorseLastLetter = "";
unsigned long lastLiveMorseInputMs = 0;
const unsigned long LIVE_MORSE_LETTER_TIMEOUT_MS = 900;
const int LIVE_MORSE_MAX_BUFFER = 8;

bool colorTestMode = false;
String colorTestSeq = "";
String colorTestAnswerSeq = "";
String colorTestResult = "READY";
String colorTestCurrentName = "-";
char colorTestTargets[9];
int colorTestRound = 0;     
int colorTestCorrect = 0;
int colorTestWrong = 0;
bool colorTestWaiting = false;
unsigned long colorTestLastMs = 0;
int lastColorSw1 = HIGH;
int lastColorSw2 = HIGH;
int lastColorSw3 = HIGH;

String jsonEscape(String s) {
  s.replace("\\", "\\\\");
  s.replace("\"", "\\\"");
  s.replace("\n", "\\n");
  s.replace("\r", "");
  return s;
}

String i2cScan() {
  String result = "";
  byte count = 0;

  for (byte address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    byte error = Wire.endTransmission();

    if (error == 0) {
      result += "0x";
      if (address < 16) result += "0";
      result += String(address, HEX);
      result += " ";
      count++;
    }
    delay(1);
  }

  if (count == 0) result = "ไม่พบอุปกรณ์ I2C";
  return result;
}

bool initRgbIfNeeded() {
  if (rgbReady) return true;

  Serial.println("RGB lazy init start...");
  rgbStrip.begin();
  rgbStrip.setBrightness(35);   
  rgbStrip.clear();
  rgbStrip.show();
  delay(10);

  rgbReady = true;
  Serial.println("RGB lazy init OK");
  return true;
}

void setAllRgb(uint8_t r, uint8_t g, uint8_t b) {
  if (!initRgbIfNeeded()) return;

  for (int i = 0; i < RGB_COUNT; i++) {
    rgbStrip.setPixelColor(i, rgbStrip.Color(r, g, b));
  }
  rgbStrip.show();
}

void applyRgbMode() {
  if (!initRgbIfNeeded()) return;

  if (rgbMode == 0) {
    rgbStrip.clear();
    rgbStrip.show();
  } else if (rgbMode == 1) {
    rgbStrip.setPixelColor(0, rgbStrip.Color(255, 0, 0));
    rgbStrip.setPixelColor(1, rgbStrip.Color(255, 180, 0));
    rgbStrip.setPixelColor(2, rgbStrip.Color(0, 255, 0));
    rgbStrip.setPixelColor(3, rgbStrip.Color(0, 0, 255));
    rgbStrip.show();
  } else if (rgbMode == 2) {
    setAllRgb(255, 0, 0);
  } else if (rgbMode == 3) {
    setAllRgb(255, 180, 0);
  } else if (rgbMode == 4) {
    setAllRgb(0, 255, 0);
  } else if (rgbMode == 5) {
    setAllRgb(0, 0, 255);
  } else if (rgbMode == 6) {
    setAllRgb(255, 255, 255);
  }
}

String pageHtml() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="th">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>ESP32-S3 Dashboard V12D</title>
<style>
*{box-sizing:border-box}html{scroll-behavior:smooth}
body{margin:0;font-family:Arial,'Tahoma',sans-serif;color:#0f172a;background:radial-gradient(circle at 10% 5%,rgba(239,68,68,.22),transparent 24%),radial-gradient(circle at 85% 10%,rgba(234,179,8,.20),transparent 28%),radial-gradient(circle at 70% 90%,rgba(34,197,94,.20),transparent 30%),radial-gradient(circle at 20% 90%,rgba(59,130,246,.18),transparent 26%),linear-gradient(135deg,#0f172a 0%,#1e293b 38%,#e2e8f0 38%,#f8fafc 100%);min-height:100vh}
.app{display:grid;grid-template-columns:270px 1fr;min-height:100vh}
.sidebar{position:sticky;top:0;height:100vh;overflow-y:auto;background:rgba(15,23,42,.94);color:white;padding:22px;border-right:1px solid rgba(255,255,255,.12)}
.logo{width:56px;height:56px;border-radius:20px;display:flex;align-items:center;justify-content:center;background:linear-gradient(135deg,#ef4444,#eab308,#22c55e,#3b82f6);box-shadow:0 10px 30px rgba(234,179,8,.25);font-size:24px;font-weight:900}
.brand{font-size:22px;font-weight:900;margin-top:14px}.sub{color:#cbd5e1;font-size:13px;line-height:1.6;margin-top:8px}
.nav{margin-top:22px;display:flex;flex-direction:column;gap:8px}.nav a{color:#e2e8f0;text-decoration:none;padding:12px 14px;border-radius:14px;background:rgba(255,255,255,.06);border:1px solid rgba(255,255,255,.08)}.nav a:hover{background:rgba(59,130,246,.35)}
.content{height:100vh;overflow-y:auto;padding:28px}.topbar{display:flex;justify-content:space-between;align-items:center;gap:14px;background:rgba(255,255,255,.76);backdrop-filter:blur(12px);border:1px solid rgba(148,163,184,.35);border-radius:26px;padding:18px 20px;box-shadow:0 16px 40px rgba(15,23,42,.12)}
h1{margin:0;font-size:30px}.badge{display:inline-flex;align-items:center;gap:8px;border-radius:999px;padding:9px 14px;background:#dcfce7;color:#166534;font-weight:900}.dot{width:10px;height:10px;border-radius:999px;background:#22c55e;box-shadow:0 0 0 6px rgba(34,197,94,.18)}
.section{margin-top:22px}.section-title{font-size:22px;font-weight:900;margin:0 0 14px}.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(240px,1fr));gap:16px}.card{background:rgba(255,255,255,.88);border:1px solid rgba(148,163,184,.35);border-radius:26px;padding:18px;box-shadow:0 14px 34px rgba(15,23,42,.10)}.card:hover{transform:translateY(-2px);transition:.18s}.label{font-size:14px;color:#64748b;font-weight:900;text-transform:uppercase;letter-spacing:.04em}.value{font-size:42px;font-weight:900;margin-top:8px}.unit{font-size:16px;color:#64748b}.bar{margin-top:12px;width:100%;height:14px;border-radius:999px;background:#e2e8f0;overflow:hidden}.fill{height:100%;width:0%;border-radius:999px;background:linear-gradient(90deg,#06b6d4,#3b82f6,#8b5cf6);transition:width .35s ease}
.led-grid,.device-grid,.switch-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(260px,1fr));gap:16px}.led-card{position:relative;overflow:hidden;min-height:190px}.led-card:before{content:"";position:absolute;width:160px;height:160px;border-radius:999px;right:-50px;top:-50px;opacity:.16}.led-red:before{background:#ef4444}.led-yellow:before{background:#eab308}.led-green:before{background:#22c55e}.led-head{display:flex;justify-content:space-between;align-items:flex-start;gap:10px;position:relative}.led-icon{width:58px;height:58px;border-radius:20px;display:flex;align-items:center;justify-content:center;font-size:30px;font-weight:900;background:#e5e7eb;color:#475569;box-shadow:inset 0 0 0 1px rgba(15,23,42,.08)}.led-card.active .led-icon{color:white;animation:pulseGlow 1.2s infinite alternate}.led-red.active .led-icon{background:#ef4444;box-shadow:0 0 28px rgba(239,68,68,.75)}.led-yellow.active .led-icon{background:#eab308;box-shadow:0 0 28px rgba(234,179,8,.75)}.led-green.active .led-icon{background:#22c55e;box-shadow:0 0 28px rgba(34,197,94,.75)}@keyframes pulseGlow{from{transform:scale(1)}to{transform:scale(1.07)}}.led-name{font-size:22px;font-weight:900;margin-top:18px;position:relative}.led-pin{font-size:14px;color:#64748b;font-weight:800;margin-top:4px;position:relative}
.toggle{width:96px;height:46px;border-radius:999px;border:0;cursor:pointer;background:#cbd5e1;padding:5px;display:flex;align-items:center;transition:.22s}.knob{width:36px;height:36px;border-radius:999px;background:white;box-shadow:0 6px 16px rgba(15,23,42,.25);transition:.22s}.toggle.active .knob{transform:translateX(50px)}.toggle.red.active{background:#ef4444}.toggle.yellow.active{background:#eab308}.toggle.green.active{background:#22c55e}.toggle.dark.active{background:#0f172a}.state-text{margin-top:14px;display:inline-flex;padding:9px 14px;border-radius:999px;font-weight:900;position:relative}.state-on{background:#dcfce7;color:#166534}.state-off{background:#e5e7eb;color:#475569}
.switch-card{min-height:170px;display:flex;flex-direction:column;justify-content:space-between;position:relative;overflow:hidden}.switch-card.pressed{border-color:rgba(34,197,94,.55);background:linear-gradient(135deg,rgba(220,252,231,.95),rgba(255,255,255,.88))}.switch-top{display:flex;justify-content:space-between;align-items:center;position:relative}.switch-name{font-size:24px;font-weight:900}.switch-pin{color:#64748b;font-size:14px;font-weight:800;margin-top:4px}.switch-lamp{width:64px;height:64px;border-radius:22px;display:flex;align-items:center;justify-content:center;background:#e5e7eb;color:#64748b;font-size:28px;font-weight:900}.switch-card.pressed .switch-lamp{background:#22c55e;color:white;box-shadow:0 0 32px rgba(34,197,94,.75)}.switch-status{margin-top:20px;font-size:30px;font-weight:900;position:relative}.switch-status small{display:block;font-size:13px;font-weight:800;color:#64748b;margin-top:4px}
.rgb-dots{display:flex;gap:10px;margin-top:16px}.rgb-dot{width:34px;height:34px;border-radius:999px;background:#cbd5e1;border:3px solid white;box-shadow:0 6px 16px rgba(15,23,42,.18)}.rgb-dot.on:nth-child(1){background:#ef4444;box-shadow:0 0 22px rgba(239,68,68,.75)}.rgb-dot.on:nth-child(2){background:#eab308;box-shadow:0 0 22px rgba(234,179,8,.75)}.rgb-dot.on:nth-child(3){background:#22c55e;box-shadow:0 0 22px rgba(34,197,94,.75)}.rgb-dot.on:nth-child(4){background:#3b82f6;box-shadow:0 0 22px rgba(59,130,246,.75)}
.btn-row{display:flex;flex-wrap:wrap;gap:10px;margin-top:12px}.btn{border:0;border-radius:16px;padding:13px 18px;cursor:pointer;font-size:16px;font-weight:900;box-shadow:0 10px 18px rgba(15,23,42,.12)}.btn:active{transform:scale(.97)}.btn-blue{background:#dbeafe;color:#1d4ed8}.btn-red{background:#fee2e2;color:#991b1b}.btn-yellow{background:#fef3c7;color:#92400e}.btn-green{background:#dcfce7;color:#166534}.btn-dark{background:#0f172a;color:white}.btn-white{background:#fff;color:#334155;border:1px solid #cbd5e1}.logbox{min-height:74px;max-height:170px;overflow:auto;background:#0f172a;color:#e2e8f0;border-radius:18px;padding:14px;font-family:Consolas,monospace;line-height:1.6}.footer{padding:24px 0;color:#475569;text-align:center}
.rotary-card{position:relative;overflow:hidden;min-height:210px;background:linear-gradient(135deg,rgba(239,246,255,.94),rgba(255,255,255,.88))}
.rotary-card:before{content:"";position:absolute;right:-55px;top:-55px;width:170px;height:170px;border-radius:999px;background:#3b82f6;opacity:.12}
.rotary-dial{width:96px;height:96px;border-radius:999px;background:linear-gradient(135deg,#0f172a,#475569);box-shadow:0 18px 38px rgba(15,23,42,.28);display:flex;align-items:center;justify-content:center;color:white;font-size:36px;font-weight:900;margin-top:12px}
.rotary-count{font-size:52px;font-weight:900;line-height:1;margin-top:10px}
.rotary-dir{display:inline-flex;margin-top:10px;padding:9px 14px;border-radius:999px;background:#dbeafe;color:#1d4ed8;font-weight:900}
.rotary-pin-grid{display:grid;grid-template-columns:repeat(3,1fr);gap:8px;margin-top:14px}
.rotary-pin{background:#f1f5f9;border-radius:14px;padding:10px;text-align:center;font-weight:900}
.rotary-pin small{display:block;color:#64748b;font-size:12px;margin-top:4px}


.plc-board{display:grid;grid-template-columns:repeat(auto-fit,minmax(260px,1fr));gap:16px}
.plc-zone{min-height:170px;border:2px dashed #93c5fd;border-radius:24px;padding:16px;background:rgba(239,246,255,.78)}
.plc-zone.active{background:#dbeafe;border-color:#2563eb}
.block-list{display:flex;flex-wrap:wrap;gap:10px;margin-top:12px}
.plc-block{user-select:none;cursor:grab;border:0;border-radius:16px;padding:12px 14px;font-weight:900;box-shadow:0 10px 18px rgba(15,23,42,.12)}
.plc-input{background:#dbeafe;color:#1d4ed8}
.plc-output{background:#dcfce7;color:#166534}
.plc-action{background:#fef3c7;color:#92400e}
.plc-selected{margin-top:12px;padding:12px;border-radius:16px;background:#0f172a;color:white;font-weight:900;min-height:44px}
.plc-mode-card{background:linear-gradient(135deg,rgba(224,242,254,.94),rgba(255,255,255,.88))}
.plc-status-on{background:#dcfce7;color:#166534}
.plc-status-off{background:#fee2e2;color:#991b1b}


.morse-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(260px,1fr));gap:16px}
.morse-keypad{display:flex;flex-wrap:wrap;gap:8px;margin-top:12px}
.morse-btn{border:0;border-radius:14px;padding:11px 14px;font-weight:900;cursor:pointer;background:#ede9fe;color:#5b21b6;box-shadow:0 8px 14px rgba(15,23,42,.10)}
.morse-btn:active{transform:scale(.96)}
.morse-dot{background:#fee2e2;color:#991b1b}
.morse-dash{background:#dcfce7;color:#166534}
.morse-display{background:#0f172a;color:#e2e8f0;border-radius:18px;padding:14px;font-family:Consolas,monospace;min-height:60px;margin-top:12px;line-height:1.6}
.morse-note{font-size:13px;color:#64748b;line-height:1.6;margin-top:8px}


.morse-live{background:linear-gradient(135deg,rgba(15,23,42,.96),rgba(30,41,59,.92));color:white;border-radius:22px;padding:18px;margin-top:14px}
.morse-live-label{font-size:13px;color:#cbd5e1;font-weight:900;text-transform:uppercase;letter-spacing:.06em}
.morse-live-buffer{font-size:46px;font-weight:900;letter-spacing:8px;margin-top:8px;color:#facc15;min-height:58px}
.morse-live-letter{font-size:52px;font-weight:900;color:#22c55e;line-height:1.1}
.morse-live-decoded{font-size:32px;font-weight:900;color:#38bdf8;word-break:break-all}


.color-test-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(270px,1fr));gap:16px}
.color-big{font-size:34px;font-weight:900;margin-top:12px}
.color-seq{background:#0f172a;color:white;border-radius:18px;padding:14px;font-size:28px;font-weight:900;letter-spacing:6px;margin-top:12px;min-height:64px}
.color-pass{background:#dcfce7;color:#166534}
.color-fail{background:#fee2e2;color:#991b1b}
.color-ready{background:#e0f2fe;color:#075985}
.color-btn-red{background:#fee2e2;color:#991b1b}
.color-btn-yellow{background:#fef3c7;color:#92400e}
.color-btn-green{background:#dcfce7;color:#166534}
.color-display{border-radius:28px;min-height:260px;display:flex;align-items:center;justify-content:center;text-align:center;color:white;box-shadow:0 18px 44px rgba(15,23,42,.22);transition:.25s}
.color-display-red{background:linear-gradient(135deg,#7f1d1d,#ef4444)}
.color-display-yellow{background:linear-gradient(135deg,#78350f,#eab308)}
.color-display-green{background:linear-gradient(135deg,#14532d,#22c55e)}
.color-display-idle{background:linear-gradient(135deg,#0f172a,#475569)}
.color-current-name{font-size:64px;font-weight:900;line-height:1.05}
.color-current-sub{font-size:20px;font-weight:900;margin-top:12px;opacity:.92}
.color-score-grid{display:grid;grid-template-columns:repeat(3,1fr);gap:10px;margin-top:14px}
.color-score-box{background:#f1f5f9;border-radius:18px;padding:14px;text-align:center}
.color-score-num{font-size:38px;font-weight:900}
.color-score-label{font-size:13px;color:#64748b;font-weight:900}

@media(max-width:820px){.app{grid-template-columns:1fr}.sidebar{position:relative;height:auto}.content{height:auto;padding:16px}.topbar{align-items:flex-start;flex-direction:column}}
</style>
</head>
<body>
<div class="app">
  <aside class="sidebar"><div class="logo">S3</div><div class="brand">ESP32-S3 Lab</div><div class="sub">Dashboard V12<br>IP: 192.168.1.15:99<br>LED + RGB + Buzzer</div><div class="nav"><a href="#sensor">📊 Sensor</a><a href="#led">💡 LED Toggle</a><a href="#rgb">🌈 RGB + Buzzer</a><a href="#switch">🎛 Switch Monitor</a><a href="#rotary">🌀 Rotary Encoder</a><a href="#plc">🧩 Mini PLC</a><a href="#morse">📡 Morse Code</a><a href="#colorTest">🚦 สอบสี DLT</a><a href="#i2c">🔎 I2C Scan</a><a href="#status">✅ Status</a></div></aside>
  <main class="content">
    <div class="topbar"><div><h1>ESP32-S3 Test Dashboard</h1><div class="sub" style="color:#475569">V12: สอบสี 9 ครั้ง สีละ 3 ครั้ง ผ่าน 6/9</div></div><div class="badge"><span class="dot"></span><span id="wifiText">ONLINE</span></div></div>
    <section id="sensor" class="section"><h2 class="section-title">📊 Sensor / Analog</h2><div class="grid"><div class="card"><div class="label">LDR แสง / IO1</div><div class="value"><span id="ldr">0</span> <span class="unit">/4095</span></div><div class="bar"><div id="ldrBar" class="fill"></div></div></div><div class="card"><div class="label">Potentiometer / IO6</div><div class="value"><span id="pot">0</span> <span class="unit">/4095</span></div><div class="bar"><div id="potBar" class="fill"></div></div></div></div></section>
    <section id="led" class="section"><h2 class="section-title">💡 LED Toggle Control</h2><div class="led-grid"><div id="ledCardR" class="card led-card led-red"><div class="led-head"><div class="led-icon">●</div><button id="toggleR" class="toggle red" onclick="toggleLed('r')"><span class="knob"></span></button></div><div class="led-name">LED สีแดง</div><div class="led-pin">GPIO 14</div><div id="ledTextR" class="state-text state-off">OFF</div></div><div id="ledCardY" class="card led-card led-yellow"><div class="led-head"><div class="led-icon">●</div><button id="toggleY" class="toggle yellow" onclick="toggleLed('y')"><span class="knob"></span></button></div><div class="led-name">LED สีเหลือง</div><div class="led-pin">GPIO 15</div><div id="ledTextY" class="state-text state-off">OFF</div></div><div id="ledCardG" class="card led-card led-green"><div class="led-head"><div class="led-icon">●</div><button id="toggleG" class="toggle green" onclick="toggleLed('g')"><span class="knob"></span></button></div><div class="led-name">LED สีเขียว</div><div class="led-pin">GPIO 16</div><div id="ledTextG" class="state-text state-off">OFF</div></div></div></section>
    <section id="rgb" class="section"><h2 class="section-title">🌈 RGB WS2812 + 🔊 Buzzer</h2><div class="device-grid"><div class="card"><div class="label">RGB WS2812 / GPIO21 / 4 ดวง</div><div class="value" style="font-size:30px"><span id="rgbText">OFF</span></div><div class="rgb-dots"><div id="rgbDot1" class="rgb-dot"></div><div id="rgbDot2" class="rgb-dot"></div><div id="rgbDot3" class="rgb-dot"></div><div id="rgbDot4" class="rgb-dot"></div></div><div class="btn-row"><button class="btn btn-blue" onclick="setRgb(1)">Test 4 สี</button><button class="btn btn-red" onclick="setRgb(2)">แดง</button><button class="btn btn-yellow" onclick="setRgb(3)">เหลือง</button><button class="btn btn-green" onclick="setRgb(4)">เขียว</button><button class="btn btn-blue" onclick="setRgb(5)">น้ำเงิน</button><button class="btn btn-white" onclick="setRgb(6)">ขาว</button><button class="btn btn-dark" onclick="setRgb(0)">ปิด</button></div></div><div class="card"><div class="label">Buzzer / GPIO5 / ไม่ใช้ tone()</div><div class="value" style="font-size:30px"><span id="buzzerText">OFF</span></div><div class="btn-row"><button class="btn btn-dark" onclick="beepBuzzer()">Beep 1kHz</button><button id="buzzerToggle" class="toggle dark" onclick="toggleBuzzer()"><span class="knob"></span></button></div><div class="sub" style="color:#64748b">สร้างคลื่น 1kHz ด้วย micros() ไม่ใช้ tone() / LEDC</div></div></div></section>
    <section id="switch" class="section"><h2 class="section-title">🎛 Switch Monitor</h2><div class="switch-grid"><div id="swCard1" class="card switch-card"><div class="switch-top"><div><div class="switch-name">SW1</div><div class="switch-pin">GPIO 35</div></div><div class="switch-lamp">⏻</div></div><div id="swText1" class="switch-status">ไม่กด<small>สถานะปุ่ม</small></div></div><div id="swCard2" class="card switch-card"><div class="switch-top"><div><div class="switch-name">SW2</div><div class="switch-pin">GPIO 36</div></div><div class="switch-lamp">⏻</div></div><div id="swText2" class="switch-status">ไม่กด<small>สถานะปุ่ม</small></div></div><div id="swCard3" class="card switch-card"><div class="switch-top"><div><div class="switch-name">SW3</div><div class="switch-pin">GPIO 37</div></div><div class="switch-lamp">⏻</div></div><div id="swText3" class="switch-status">ไม่กด<small>สถานะปุ่ม</small></div></div></div></section>
    <section id="rotary" class="section"><h2 class="section-title">🌀 Rotary Encoder EC11 / EC12</h2><div class="device-grid"><div class="card rotary-card"><div class="label">Rotary Encoder / ECS40 / ECA38 / ECB39</div><div style="display:flex;gap:18px;align-items:center;flex-wrap:wrap"><div class="rotary-dial">↻</div><div><div class="rotary-count"><span id="rotaryCount">0</span></div><div id="rotaryDir" class="rotary-dir">STOP</div><div style="margin-top:10px;font-weight:900">RGB Speed: <span id="rgbRotateDelay">0</span> ms</div></div></div><div class="rotary-pin-grid"><div class="rotary-pin">ECA<small>GPIO38: <span id="rotaryA">-</span></small></div><div class="rotary-pin">ECB<small>GPIO39: <span id="rotaryB">-</span></small></div><div class="rotary-pin">ECS<small>GPIO40: <span id="rotarySw">-</span></small></div></div><div class="btn-row"><button class="btn btn-blue" onclick="resetRotary()">Reset Count</button></div></div></div></section>
<section id="plc" class="section">
  <h2 class="section-title">🧩 Drag & Drop Mini PLC</h2>

  <div class="card plc-mode-card">
    <div class="label">PLC Mode</div>
    <div class="btn-row">
      <button class="btn btn-blue" onclick="setPlcMode(1)">RUN PLC AUTO</button>
      <button class="btn btn-dark" onclick="setPlcMode(0)">MANUAL MODE</button>
    </div>
    <div class="value" style="font-size:28px;margin-top:14px">
      Mode: <span id="plcModeText">MANUAL</span>
    </div>
    <div id="plcMatchText" class="state-text state-off">Rule: Waiting</div>
    <div id="plcSavedText" class="plc-selected" style="margin-top:12px">Saved Rule: ยังไม่ได้บันทึก</div>
    <div id="plcMsgText" class="state-text state-off">Status: Ready</div>
  </div>

  <div class="plc-board" style="margin-top:16px">
    <div class="card">
      <div class="label">1) ลาก Input</div>
      <div class="block-list">
        <button class="plc-block plc-input" draggable="true" ondragstart="drag(event)" data-type="input" data-value="SW1">SW1 ON</button>
        <button class="plc-block plc-input" draggable="true" ondragstart="drag(event)" data-type="input" data-value="SW2">SW2 ON</button>
        <button class="plc-block plc-input" draggable="true" ondragstart="drag(event)" data-type="input" data-value="SW3">SW3 ON</button>
        <button class="plc-block plc-input" draggable="true" ondragstart="drag(event)" data-type="input" data-value="LDR_DARK">LDR มืด</button>
        <button class="plc-block plc-input" draggable="true" ondragstart="drag(event)" data-type="input" data-value="POT_HIGH">POT สูง</button>
        <button class="plc-block plc-input" draggable="true" ondragstart="drag(event)" data-type="input" data-value="ROTARY_POS">Rotary > 5</button>
      </div>
      <div id="dropInput" class="plc-zone" ondrop="drop(event,'input')" ondragover="allowDrop(event)">
        <div class="label">Drop Input Here</div>
        <div id="selectedInput" class="plc-selected">ยังไม่ได้เลือก Input</div>
      </div>
    </div>

    <div class="card">
      <div class="label">2) ลาก Output</div>
      <div class="block-list">
        <button class="plc-block plc-output" draggable="true" ondragstart="drag(event)" data-type="output" data-value="LED_R">LED แดง</button>
        <button class="plc-block plc-output" draggable="true" ondragstart="drag(event)" data-type="output" data-value="LED_Y">LED เหลือง</button>
        <button class="plc-block plc-output" draggable="true" ondragstart="drag(event)" data-type="output" data-value="LED_G">LED เขียว</button>
        <button class="plc-block plc-output" draggable="true" ondragstart="drag(event)" data-type="output" data-value="BUZZER">Buzzer</button>
      </div>
      <div id="dropOutput" class="plc-zone" ondrop="drop(event,'output')" ondragover="allowDrop(event)">
        <div class="label">Drop Output Here</div>
        <div id="selectedOutput" class="plc-selected">ยังไม่ได้เลือก Output</div>
      </div>
    </div>

    <div class="card">
      <div class="label">3) Action + Save</div>
      <div class="block-list">
        <button class="plc-block plc-action" onclick="selectAction('ON')">ACTION ON</button>
        <button class="plc-block plc-action" onclick="selectAction('OFF')">ACTION OFF</button>
      </div>
      <div class="plc-zone">
        <div class="label">Current Rule</div>
        <div id="selectedAction" class="plc-selected">Action: ON</div>
        <div id="rulePreview" class="plc-selected">IF - THEN -</div>
        <div class="btn-row">
          <button class="btn btn-green" onclick="savePlcRule()">SAVE RULE</button>
          <button class="btn btn-red" onclick="clearPlcRule()">CLEAR</button>
        </div>
      </div>
    </div>
  </div>
</section>

<section id="morse" class="section">
  <h2 class="section-title">📡 Morse Code Trainer</h2>
  <div class="morse-grid">
    <div class="card">
      <div class="label">Manual Morse</div>
      <div class="btn-row">
        <button class="btn morse-dot" onclick="playMorsePattern('.')">DOT . สีแดง</button>
        <button class="btn morse-dash" onclick="playMorsePattern('-')">DASH - สีเขียว</button>
      </div>
      <div class="morse-note">
        ความถี่เสียง 700Hz / จุด = 1 unit / ขีด = 3 units<br>
        POT GPIO6 ใช้ปรับความดังแบบ duty software
      </div>
      <div class="morse-display">
        Last Morse: <span id="morsePattern">-</span><br>
        Volume POT: <span id="morsePot">0</span> /4095
      </div>
      <div class="morse-live">
        <div class="morse-live-label">Live Buffer: รอหยุดกดประมาณ 0.9 วินาทีแล้วแปล</div>
        <div id="liveMorseBuffer" class="morse-live-buffer">-</div>
        <div class="morse-live-label">Last Letter</div>
        <div id="liveMorseLastLetter" class="morse-live-letter">-</div>
        <div class="morse-live-label">Decoded Text</div>
        <div id="liveMorseDecoded" class="morse-live-decoded">-</div>
        <div class="btn-row">
          <button class="btn btn-red" onclick="clearMorseLive()">CLEAR LIVE</button>
        </div>
      </div>
    </div>

    <div class="card">
      <div class="label">A-Z Standard Morse</div>
      <div class="morse-keypad">
        <button class="morse-btn" onclick="playMorseText('A')">A .-</button>
        <button class="morse-btn" onclick="playMorseText('B')">B -...</button>
        <button class="morse-btn" onclick="playMorseText('C')">C -.-.</button>
        <button class="morse-btn" onclick="playMorseText('D')">D -..</button>
        <button class="morse-btn" onclick="playMorseText('E')">E .</button>
        <button class="morse-btn" onclick="playMorseText('F')">F ..-.</button>
        <button class="morse-btn" onclick="playMorseText('G')">G --.</button>
        <button class="morse-btn" onclick="playMorseText('H')">H ....</button>
        <button class="morse-btn" onclick="playMorseText('I')">I ..</button>
        <button class="morse-btn" onclick="playMorseText('J')">J .---</button>
        <button class="morse-btn" onclick="playMorseText('K')">K -.-</button>
        <button class="morse-btn" onclick="playMorseText('L')">L .-..</button>
        <button class="morse-btn" onclick="playMorseText('M')">M --</button>
        <button class="morse-btn" onclick="playMorseText('N')">N -.</button>
        <button class="morse-btn" onclick="playMorseText('O')">O ---</button>
        <button class="morse-btn" onclick="playMorseText('P')">P .--.</button>
        <button class="morse-btn" onclick="playMorseText('Q')">Q --.-</button>
        <button class="morse-btn" onclick="playMorseText('R')">R .-.</button>
        <button class="morse-btn" onclick="playMorseText('S')">S ...</button>
        <button class="morse-btn" onclick="playMorseText('T')">T -</button>
        <button class="morse-btn" onclick="playMorseText('U')">U ..-</button>
        <button class="morse-btn" onclick="playMorseText('V')">V ...-</button>
        <button class="morse-btn" onclick="playMorseText('W')">W .--</button>
        <button class="morse-btn" onclick="playMorseText('X')">X -..-</button>
        <button class="morse-btn" onclick="playMorseText('Y')">Y -.--</button>
        <button class="morse-btn" onclick="playMorseText('Z')">Z --..</button>
      </div>
    </div>

    <div class="card">
      <div class="label">Send Text</div>
      <input id="morseTextInput" value="SOS" style="width:100%;padding:14px;border-radius:14px;border:1px solid #cbd5e1;font-size:20px;font-weight:900;margin-top:12px">
      <div class="btn-row">
        <button class="btn btn-blue" onclick="sendMorseText()">SEND TEXT</button>
        <button class="btn btn-dark" onclick="playMorseText('SOS')">SOS</button>
      </div>
      <div class="morse-note">
        RGB: DOT = แดง / DASH = เขียว / เสียงออก Buzzer GPIO5<br>ปุ่มจริง: SW1 GPIO35 = DOT . / SW2 GPIO36 = DASH -
      </div>
    </div>
  </div>
</section>

<section id="colorTest" class="section">
  <h2 class="section-title">🚦 แบบทดสอบสี 9 ครั้ง ตามหลักกรมการขนส่งทางบก</h2>
  <div class="color-test-grid">
    <div class="card">
      <div class="label">กติกา</div>
      <div class="color-big">เครื่องจะสุ่มลำดับสี 9 ครั้ง</div>
      <div class="morse-note">
        LED/RGB จะแสดงสีทีละครั้ง โดยมี แดง 3 / เหลือง 3 / เขียว 3 แล้วรอจนกว่าผู้สอบจะกดปุ่มตอบ<br>
        SW35 = แดง / SW36 = เหลือง / SW37 = เขียว<br>
        ผ่านเมื่อถูกอย่างน้อย <b>6 ใน 9 ครั้ง</b>
      </div>
      <div class="btn-row">
        <button class="btn btn-blue" onclick="setColorTestMode(1)">เริ่มสอบสี 9 ครั้ง</button>
        <button class="btn btn-dark" onclick="setColorTestMode(0)">หยุดสอบสี</button>
        <button class="btn btn-red" onclick="resetColorTest()">Reset</button>
      </div>
    </div>

    <div class="card">
      <div class="label">สีที่เครื่องกำลังแสดง</div>
      <div id="colorDisplayBox" class="color-display color-display-idle">
        <div>
          <div id="colorCurrentName" class="color-current-name">READY</div>
          <div id="colorCurrentSub" class="color-current-sub">กดเริ่มสอบสี</div>
        </div>
      </div>
    </div>

    <div class="card">
      <div class="label">ผลการสอบ</div>
      <div id="colorTestResult" class="state-text color-ready">READY</div>
      <div class="color-score-grid">
        <div class="color-score-box">
          <div id="colorRound" class="color-score-num">0</div>
          <div class="color-score-label">ครั้งที่</div>
        </div>
        <div class="color-score-box">
          <div id="colorCorrect" class="color-score-num">0</div>
          <div class="color-score-label">ถูก</div>
        </div>
        <div class="color-score-box">
          <div id="colorWrong" class="color-score-num">0</div>
          <div class="color-score-label">ผิด</div>
        </div>
      </div>
      <div class="color-seq">
        โจทย์: <span id="colorTestSeq">-</span><br>
        ตอบ: <span id="colorAnswerSeq">-</span>
      </div>
    </div>

    <div class="card">
      <div class="label">ปุ่มทดสอบจากหน้าเว็บ</div>
      <div class="btn-row">
        <button class="btn color-btn-red" onclick="pressColor('R')">ตอบ แดง</button>
        <button class="btn color-btn-yellow" onclick="pressColor('Y')">ตอบ เหลือง</button>
        <button class="btn color-btn-green" onclick="pressColor('G')">ตอบ เขียว</button>
      </div>
      <div class="morse-note">
        ใช้ทดสอบแทนปุ่มจริงได้ แต่ตอนใช้งานจริงให้กด SW35/36/37
      </div>
    </div>
  </div>
</section>
<section id="i2c" class="section"><h2 class="section-title">🔎 I2C Scanner</h2><div class="card"><div class="label">กดเมื่อต้องการสแกน ไม่สแกนตลอดเวลา จึงไม่หน่วง</div><div class="btn-row"><button class="btn btn-blue" onclick="scanI2C()">Scan I2C Now</button></div><div id="i2cLog" class="logbox" style="margin-top:14px">ยังไม่ได้สแกน</div></div></section>
    <section id="status" class="section"><h2 class="section-title">✅ System Status</h2><div class="grid"><div class="card"><div class="label">WiFi IP</div><div class="value" style="font-size:26px">192.168.1.15:99</div></div><div class="card"><div class="label">Uptime</div><div class="value" style="font-size:30px"><span id="uptime">0</span>s</div></div><div class="card"><div class="label">Refresh Mode</div><div class="value" style="font-size:26px">API 400ms</div></div></div></section>
    <div class="footer">ESP32-S3 School Project Dashboard V12DD</div>
  </main>
</div>
<script>
let led={r:0,y:0,g:0};let buzzer=0;
function setText(id,v){document.getElementById(id).textContent=v}function setBar(id,v){document.getElementById(id).style.width=Math.max(0,Math.min(100,v*100/4095))+"%"}
function setLedUi(c,s){const m={r:{card:"ledCardR",toggle:"toggleR",text:"ledTextR"},y:{card:"ledCardY",toggle:"toggleY",text:"ledTextY"},g:{card:"ledCardG",toggle:"toggleG",text:"ledTextG"}}[c];const card=document.getElementById(m.card),tog=document.getElementById(m.toggle),txt=document.getElementById(m.text);if(s){card.classList.add("active");tog.classList.add("active");txt.textContent="ON";txt.className="state-text state-on"}else{card.classList.remove("active");tog.classList.remove("active");txt.textContent="OFF";txt.className="state-text state-off"}}
function setSwitchUi(n,p){const card=document.getElementById("swCard"+n),txt=document.getElementById("swText"+n);if(p){card.classList.add("pressed");txt.innerHTML="กดอยู่<small>ตรวจพบการกดปุ่ม</small>"}else{card.classList.remove("pressed");txt.innerHTML="ไม่กด<small>สถานะปุ่ม</small>"}}
function setRgbUi(mode){["rgbDot1","rgbDot2","rgbDot3","rgbDot4"].forEach(id=>document.getElementById(id).classList.remove("on"));let t="OFF";if(mode>0){["rgbDot1","rgbDot2","rgbDot3","rgbDot4"].forEach(id=>document.getElementById(id).classList.add("on"));if(mode==1)t="TEST 4 สี";if(mode==2)t="แดง";if(mode==3)t="เหลือง";if(mode==4)t="เขียว";if(mode==5)t="น้ำเงิน";if(mode==6)t="ขาว"}setText("rgbText",t)}
function setBuzzerUi(s){buzzer=s;const t=document.getElementById("buzzerToggle");if(s){t.classList.add("active");setText("buzzerText","ON")}else{t.classList.remove("active");setText("buzzerText","OFF")}}
async function updateData(){try{const r=await fetch("/api",{cache:"no-store"});const d=await r.json();setText("ldr",d.ldr);setText("pot",d.pot);setBar("ldrBar",d.ldr);setBar("potBar",d.pot);setSwitchUi(1,d.sw1==0);setSwitchUi(2,d.sw2==0);setSwitchUi(3,d.sw3==0);led.r=d.ledR;led.y=d.ledY;led.g=d.ledG;setLedUi("r",led.r==1);setLedUi("y",led.y==1);setLedUi("g",led.g==1);setRgbUi(d.rgbMode);setBuzzerUi(d.buzzer);setText("rotaryCount",d.rotaryCount);setText("rotaryDir",d.rotaryDir);setText("rotaryA",d.rotaryA);setText("rotaryB",d.rotaryB);setText("rotarySw",d.rotarySw==0?"กด":"ไม่กด");setText("rgbRotateDelay",d.rgbRotateDelay);setText("plcModeText",d.plcMode==1?"PLC AUTO":"MANUAL");setText("plcMatchText",d.plcMatched==1?"Rule: MATCH":"Rule: Waiting");document.getElementById("plcMatchText").className=d.plcMatched==1?"state-text state-on":"state-text state-off";setText("plcSavedText","Saved Rule: IF "+d.plcInput+" THEN "+d.plcOutput+" = "+d.plcAction);setText("morsePattern",d.morsePattern);setText("morsePot",d.pot);setText("liveMorseBuffer",d.liveMorseBuffer && d.liveMorseBuffer.length?d.liveMorseBuffer:"-");setText("liveMorseLastLetter",d.liveMorseLastLetter && d.liveMorseLastLetter.length?d.liveMorseLastLetter:"-");setText("liveMorseDecoded",d.liveMorseDecoded && d.liveMorseDecoded.length?d.liveMorseDecoded:"-");setText("colorTestSeq",d.colorTestSeq && d.colorTestSeq.length?d.colorTestSeq:"-");setText("colorAnswerSeq",d.colorAnswerSeq && d.colorAnswerSeq.length?d.colorAnswerSeq:"-");setText("colorTestResult",d.colorTestResult);setText("colorCurrentName",d.colorCurrentName && d.colorCurrentName.length?d.colorCurrentName:"READY");setText("colorCurrentSub",d.colorWaiting==1?"รอผู้สอบกด SW35/36/37":"กดเริ่มสอบสี");setText("colorRound",d.colorRound);setText("colorCorrect",d.colorCorrect);setText("colorWrong",d.colorWrong);let cb=document.getElementById("colorDisplayBox");cb.className="color-display color-display-idle";if(d.colorCurrentName=="แดง")cb.className="color-display color-display-red";if(d.colorCurrentName=="เหลือง")cb.className="color-display color-display-yellow";if(d.colorCurrentName=="เขียว")cb.className="color-display color-display-green";if(d.colorTestResult=="PASS")cb.className="color-display color-display-green";if(d.colorTestResult=="FAIL")cb.className="color-display color-display-red";document.getElementById("colorTestResult").className=d.colorTestResult=="PASS"?"state-text color-pass":(d.colorTestResult=="FAIL"?"state-text color-fail":"state-text color-ready");setText("uptime",d.uptime);setText("wifiText","ONLINE")}catch(e){setText("wifiText","OFFLINE")}}
async function setLed(c,v){await fetch(`/led?${c}=${v}`,{cache:"no-store"});await updateData()}async function toggleLed(c){await setLed(c,led[c]?0:1)}async function setRgb(m){await fetch(`/rgb?mode=${m}`,{cache:"no-store"});await updateData()}async function toggleBuzzer(){await fetch(`/buzzer?state=${buzzer?0:1}`,{cache:"no-store"});await updateData()}async function beepBuzzer(){await fetch("/beep",{cache:"no-store"});await updateData()}async function scanI2C(){const box=document.getElementById("i2cLog");box.textContent="กำลังสแกน...";try{const r=await fetch("/i2c",{cache:"no-store"});const d=await r.json();box.textContent=d.result}catch(e){box.textContent="สแกนไม่สำเร็จ"}}
async function resetRotary(){await fetch("/rotaryReset",{cache:"no-store"});await updateData();}

let plcRule = {input:"NONE", output:"NONE", action:"ON"};

function allowDrop(ev){ev.preventDefault();}
function drag(ev){
  ev.dataTransfer.setData("type", ev.target.dataset.type);
  ev.dataTransfer.setData("value", ev.target.dataset.value);
  ev.dataTransfer.setData("text", ev.target.textContent);
}
function drop(ev, zone){
  ev.preventDefault();
  const type = ev.dataTransfer.getData("type");
  const value = ev.dataTransfer.getData("value");
  const text = ev.dataTransfer.getData("text");

  if(type !== zone){return;}

  if(zone === "input"){
    plcRule.input = value;
    setText("selectedInput", text);
  }else if(zone === "output"){
    plcRule.output = value;
    setText("selectedOutput", text);
  }
  updateRulePreview();
}
function selectAction(action){
  plcRule.action = action;
  setText("selectedAction", "Action: " + action);
  updateRulePreview();
}
function updateRulePreview(){
  setText("rulePreview", "IF " + plcRule.input + " THEN " + plcRule.output + " = " + plcRule.action);
}
async function savePlcRule(){
  try{
    if(plcRule.input === "NONE" || plcRule.output === "NONE"){
      setText("plcMsgText","Status: กรุณาลาก Input และ Output ก่อน");
      document.getElementById("plcMsgText").className="state-text state-off";
      return;
    }

    const url = `/plcRule?input=${encodeURIComponent(plcRule.input)}&output=${encodeURIComponent(plcRule.output)}&action=${encodeURIComponent(plcRule.action)}`;
    const res = await fetch(url,{cache:"no-store"});
    const txt = await res.text();

    setText("plcMsgText","Status: Save Rule สำเร็จ");
    document.getElementById("plcMsgText").className="state-text state-on";
    await updateData();
  }catch(e){
    setText("plcMsgText","Status: Save Rule ไม่สำเร็จ");
    document.getElementById("plcMsgText").className="state-text state-off";
  }
}
async function clearPlcRule(){
  plcRule = {input:"NONE", output:"NONE", action:"ON"};
  setText("selectedInput","ยังไม่ได้เลือก Input");
  setText("selectedOutput","ยังไม่ได้เลือก Output");
  setText("selectedAction","Action: ON");
  updateRulePreview();
  await fetch("/plcRule?input=NONE&output=NONE&action=ON",{cache:"no-store"});
  setText("plcMsgText","Status: Clear Rule แล้ว");
  document.getElementById("plcMsgText").className="state-text state-off";
  await updateData();
}
async function setPlcMode(mode){
  try{
    await fetch(`/plcMode?mode=${mode}`,{cache:"no-store"});
    setText("plcMsgText", mode==1 ? "Status: RUN PLC AUTO แล้ว" : "Status: MANUAL MODE แล้ว");
    document.getElementById("plcMsgText").className= mode==1 ? "state-text state-on" : "state-text state-off";
    await updateData();
  }catch(e){
    setText("plcMsgText","Status: เปลี่ยนโหมดไม่สำเร็จ");
    document.getElementById("plcMsgText").className="state-text state-off";
  }
}


async function playMorsePattern(pattern){
  setText("morsePattern", pattern);
  await fetch(`/morse?pattern=${encodeURIComponent(pattern)}`,{cache:"no-store"});
  await updateData();
}
async function playMorseText(text){
  setText("morsePattern", "sending " + text);
  await fetch(`/morse?text=${encodeURIComponent(text)}`,{cache:"no-store"});
  await updateData();
}
async function sendMorseText(){
  const txt = document.getElementById("morseTextInput").value || "SOS";
  await playMorseText(txt.toUpperCase());
}


async function clearMorseLive(){
  await fetch("/morseClear",{cache:"no-store"});
  await updateData();
}


async function setColorTestMode(mode){
  await fetch(`/colorTestMode?mode=${mode}`,{cache:"no-store"});
  await updateData();
}
async function resetColorTest(){
  await fetch("/colorTestReset",{cache:"no-store"});
  await updateData();
}
async function pressColor(c){
  await fetch(`/colorPress?c=${encodeURIComponent(c)}`,{cache:"no-store"});
  await updateData();
}

setInterval(updateData,400);updateData();
</script>
</body>
</html>
)rawliteral";
  return html;
}

void applyLedState() {
  digitalWrite(LED_R, ledRState ? HIGH : LOW);
  digitalWrite(LED_Y, ledYState ? HIGH : LOW);
  digitalWrite(LED_G, ledGState ? HIGH : LOW);
}

void applyBuzzerState() {
  if (!buzzerState) {
    buzzerPinLevel = false;
    digitalWrite(BUZZER_PIN, LOW);
  } else {
    lastBuzzerToggleUs = micros();
  }
}

void updateBuzzerTone() {
  if (!buzzerState) return;

  unsigned long nowUs = micros();
  if (nowUs - lastBuzzerToggleUs >= BUZZER_HALF_PERIOD_US) {
    lastBuzzerToggleUs = nowUs;
    buzzerPinLevel = !buzzerPinLevel;
    digitalWrite(BUZZER_PIN, buzzerPinLevel ? HIGH : LOW);
  }
}

void beep1kHz(unsigned long durationMs) {
  unsigned long endTime = millis() + durationMs;

  while (millis() < endTime) {
    digitalWrite(BUZZER_PIN, HIGH);
    delayMicroseconds(BUZZER_HALF_PERIOD_US);
    digitalWrite(BUZZER_PIN, LOW);
    delayMicroseconds(BUZZER_HALF_PERIOD_US);
  }
}


void updateRotaryEncoder() {
  int a = digitalRead(ROTARY_A_PIN);

  if (a != lastRotaryA) {

    if (a == LOW) {

      int b = digitalRead(ROTARY_B_PIN);

      if (b == HIGH) {
        rotaryCount++;
        rotaryDirection = "CW";
      } else {
        rotaryCount--;
        rotaryDirection = "CCW";
      }

      if (rotaryCount < 0) rotaryCount = 0;
      if (rotaryCount > 50) rotaryCount = 50;

      rgbRotateMode = true;

      if (rotaryCount == 0) {
        rgbRotateDelay = 0;
      } else {
        rgbRotateDelay = map(rotaryCount, 1, 50, 500, 20);
      }
    }

    lastRotaryA = a;
  }

 
  int sw = digitalRead(ROTARY_SW_PIN);

  if (sw != lastRotarySw) {
    delay(2);

    if (digitalRead(ROTARY_SW_PIN) == sw) {
      lastRotarySw = sw;

      if (sw == LOW) {
        rotaryCount = 0;
        rgbRotateDelay = 0;
        rgbRotateMode = false;

        if (rgbReady) {
          rgbStrip.clear();
          rgbStrip.show();
        }

        rotaryDirection = "STOP";
      }
    }
  }
}

void handleRotaryReset() {
  rotaryCount = 0;
  rotaryDirection = "STOP";
  server.send(200, "application/json", "{\"ok\":true}");
}


bool checkPlcInputCondition() {
  if (plcInput == "SW1") return digitalRead(SW1_PIN) == LOW;
  if (plcInput == "SW2") return digitalRead(SW2_PIN) == LOW;
  if (plcInput == "SW3") return digitalRead(SW3_PIN) == LOW;
  if (plcInput == "LDR_DARK") return analogRead(LDR_PIN) < 1200;
  if (plcInput == "POT_HIGH") return analogRead(POT_PIN) > 3000;
  if (plcInput == "ROTARY_POS") return rotaryCount > 5;
  return false;
}

void setPlcOutput(bool active) {
  bool outState = (plcAction == "ON") ? active : !active;

  if (plcOutput == "LED_R") {
    ledRState = outState;
    applyLedState();
  } else if (plcOutput == "LED_Y") {
    ledYState = outState;
    applyLedState();
  } else if (plcOutput == "LED_G") {
    ledGState = outState;
    applyLedState();
  } else if (plcOutput == "BUZZER") {
    buzzerState = outState;
    applyBuzzerState();
  }
}

void updateMiniPlc() {
  if (!plcMode) return;
  if (millis() - lastPlcMs < 50) return;
  lastPlcMs = millis();

  if (plcInput == "NONE" || plcOutput == "NONE") {
    plcRuleMatched = false;
    return;
  }

  plcRuleMatched = checkPlcInputCondition();
  setPlcOutput(plcRuleMatched);
}

void handlePlcRule() {
  if (server.hasArg("input")) plcInput = server.arg("input");
  if (server.hasArg("output")) plcOutput = server.arg("output");
  if (server.hasArg("action")) plcAction = server.arg("action");

  plcRuleMatched = false;

  String json = "{";
  json += "\"ok\":true,";
  json += "\"input\":\"" + plcInput + "\",";
  json += "\"output\":\"" + plcOutput + "\",";
  json += "\"action\":\"" + plcAction + "\"";
  json += "}";

  server.send(200, "application/json", json);
}

void handlePlcMode() {
  if (server.hasArg("mode")) {
    plcMode = server.arg("mode") == "1";
    plcRuleMatched = false;
  }

  String json = "{";
  json += "\"ok\":true,";
  json += "\"plcMode\":" + String(plcMode ? 1 : 0);
  json += "}";

  server.send(200, "application/json", json);
}


void updateRgbRotaryAnimation() {
  if (!rgbRotateMode) return;
  if (rgbRotateDelay <= 0) return;

  if (millis() - lastRgbRotateMs < (unsigned long)rgbRotateDelay) return;
  lastRgbRotateMs = millis();

  if (!initRgbIfNeeded()) return;

  rgbStrip.clear();

  int p1 = rgbRotateIndex % 4;
  int p2 = (rgbRotateIndex + 1) % 4;
  int p3 = (rgbRotateIndex + 2) % 4;
  int p4 = (rgbRotateIndex + 3) % 4;

  rgbStrip.setPixelColor(p1, rgbStrip.Color(255, 0, 0));
  rgbStrip.setPixelColor(p2, rgbStrip.Color(255, 180, 0));
  rgbStrip.setPixelColor(p3, rgbStrip.Color(0, 255, 0));
  rgbStrip.setPixelColor(p4, rgbStrip.Color(0, 0, 255));

  rgbStrip.show();

  rgbRotateIndex++;
  if (rgbRotateIndex >= 4) rgbRotateIndex = 0;
}


String morseForChar(char c) {
  c = toupper(c);
  if (c == 'A') return ".-";
  if (c == 'B') return "-...";
  if (c == 'C') return "-.-.";
  if (c == 'D') return "-..";
  if (c == 'E') return ".";
  if (c == 'F') return "..-.";
  if (c == 'G') return "--.";
  if (c == 'H') return "....";
  if (c == 'I') return "..";
  if (c == 'J') return ".---";
  if (c == 'K') return "-.-";
  if (c == 'L') return ".-..";
  if (c == 'M') return "--";
  if (c == 'N') return "-.";
  if (c == 'O') return "---";
  if (c == 'P') return ".--.";
  if (c == 'Q') return "--.-";
  if (c == 'R') return ".-.";
  if (c == 'S') return "...";
  if (c == 'T') return "-";
  if (c == 'U') return "..-";
  if (c == 'V') return "...-";
  if (c == 'W') return ".--";
  if (c == 'X') return "-..-";
  if (c == 'Y') return "-.--";
  if (c == 'Z') return "--..";
  if (c == '0') return "-----";
  if (c == '1') return ".----";
  if (c == '2') return "..---";
  if (c == '3') return "...--";
  if (c == '4') return "....-";
  if (c == '5') return ".....";
  if (c == '6') return "-....";
  if (c == '7') return "--...";
  if (c == '8') return "---..";
  if (c == '9') return "----.";
  return "";
}


String charForMorse(String p) {
  if (p == ".-") return "A";
  if (p == "-...") return "B";
  if (p == "-.-.") return "C";
  if (p == "-..") return "D";
  if (p == ".") return "E";
  if (p == "..-.") return "F";
  if (p == "--.") return "G";
  if (p == "....") return "H";
  if (p == "..") return "I";
  if (p == ".---") return "J";
  if (p == "-.-") return "K";
  if (p == ".-..") return "L";
  if (p == "--") return "M";
  if (p == "-.") return "N";
  if (p == "---") return "O";
  if (p == ".--.") return "P";
  if (p == "--.-") return "Q";
  if (p == ".-.") return "R";
  if (p == "...") return "S";
  if (p == "-") return "T";
  if (p == "..-") return "U";
  if (p == "...-") return "V";
  if (p == ".--") return "W";
  if (p == "-..-") return "X";
  if (p == "-.--") return "Y";
  if (p == "--..") return "Z";
  if (p == "-----") return "0";
  if (p == ".----") return "1";
  if (p == "..---") return "2";
  if (p == "...--") return "3";
  if (p == "....-") return "4";
  if (p == ".....") return "5";
  if (p == "-....") return "6";
  if (p == "--...") return "7";
  if (p == "---..") return "8";
  if (p == "----.") return "9";
  return "?";
}

void addLiveMorseSymbol(char s) {
  if (liveMorseBuffer.length() < LIVE_MORSE_MAX_BUFFER) {
    liveMorseBuffer += s;
  }
  lastLiveMorseInputMs = millis();
}

void decodeLiveMorseIfTimeout() {
  if (liveMorseBuffer.length() == 0) return;
  if (millis() - lastLiveMorseInputMs < LIVE_MORSE_LETTER_TIMEOUT_MS) return;

  String letter = charForMorse(liveMorseBuffer);
  liveMorseLastLetter = letter;
  liveMorseDecoded += letter;

  if (liveMorseDecoded.length() > 40) {
    liveMorseDecoded = liveMorseDecoded.substring(liveMorseDecoded.length() - 40);
  }

  liveMorseBuffer = "";
}

void handleMorseClear() {
  liveMorseBuffer = "";
  liveMorseDecoded = "";
  liveMorseLastLetter = "";
  lastMorsePattern = "";
  lastMorseText = "";
  server.send(200, "application/json", "{\"ok\":true}");
}

void morseRgbSymbol(char symbolOn) {
  if (!initRgbIfNeeded()) return;

  rgbStrip.clear();
  if (symbolOn == '.') {
    for (int i = 0; i < RGB_COUNT; i++) {
      rgbStrip.setPixelColor(i, rgbStrip.Color(255, 0, 0));
    }
  } else if (symbolOn == '-') {
    for (int i = 0; i < RGB_COUNT; i++) {
      rgbStrip.setPixelColor(i, rgbStrip.Color(0, 255, 0));
    }
  }
  rgbStrip.show();
}

void morseRgbOff() {
  if (!rgbReady) return;
  rgbStrip.clear();
  rgbStrip.show();
}

int morseVolumeDutyHighUs(int halfPeriodUs) {
  int pot = analogRead(POT_PIN); 
  int dutyPercent = map(pot, 0, 4095, 10, 90);
  int highUs = (halfPeriodUs * 2 * dutyPercent) / 100;
  if (highUs < 30) highUs = 30;
  if (highUs > (halfPeriodUs * 2 - 30)) highUs = halfPeriodUs * 2 - 30;
  return highUs;
}

void morseTone700Hz(unsigned long durationMs) {
  int periodUs = 1000000 / MORSE_FREQ_HZ;       
  int halfUs = periodUs / 2;
  unsigned long endMs = millis() + durationMs;

  while (millis() < endMs) {
    int highUs = morseVolumeDutyHighUs(halfUs);
    int lowUs = periodUs - highUs;

    digitalWrite(BUZZER_PIN, HIGH);
    delayMicroseconds(highUs);
    digitalWrite(BUZZER_PIN, LOW);
    delayMicroseconds(lowUs);

    server.handleClient();
  }
}

void playMorsePattern(String pattern) {
  morseBusy = true;
  lastMorsePattern = pattern;

  for (int i = 0; i < pattern.length(); i++) {
    char s = pattern.charAt(i);

    if (s == '.') {
      morseRgbSymbol('.');
      morseTone700Hz(MORSE_DOT_MS);
      morseRgbOff();
    } else if (s == '-') {
      morseRgbSymbol('-');
      morseTone700Hz(MORSE_DASH_MS);
      morseRgbOff();
    }

    digitalWrite(BUZZER_PIN, LOW);
    delay(MORSE_SYMBOL_GAP_MS);
  }

  morseBusy = false;
}

void playMorseText(String text) {
  morseBusy = true;
  lastMorseText = text;
  lastMorsePattern = "";

  for (int i = 0; i < text.length(); i++) {
    char c = text.charAt(i);

    if (c == ' ') {
      delay(MORSE_DOT_MS * 7);
      lastMorsePattern += " / ";
      continue;
    }

    String pattern = morseForChar(c);
    if (pattern.length() == 0) continue;

    lastMorsePattern += pattern;
    lastMorsePattern += " ";

    for (int j = 0; j < pattern.length(); j++) {
      char s = pattern.charAt(j);

      if (s == '.') {
        morseRgbSymbol('.');
        morseTone700Hz(MORSE_DOT_MS);
        morseRgbOff();
      } else if (s == '-') {
        morseRgbSymbol('-');
        morseTone700Hz(MORSE_DASH_MS);
        morseRgbOff();
      }

      digitalWrite(BUZZER_PIN, LOW);
      delay(MORSE_SYMBOL_GAP_MS);
    }

    delay(MORSE_LETTER_GAP_MS);
  }

  morseBusy = false;
}


void updateMorseButtons() {
  if (colorTestMode) return;
  if (morseBusy) return;

  int dotSw = digitalRead(SW1_PIN);  
  int dashSw = digitalRead(SW2_PIN); 

  if (millis() - lastMorseButtonMs < 120) {
    lastMorseDotSw = dotSw;
    lastMorseDashSw = dashSw;
    return;
  }

  if (lastMorseDotSw == HIGH && dotSw == LOW) {
    lastMorseButtonMs = millis();
    lastMorseText = "SW1 DOT";
    addLiveMorseSymbol('.');
    playMorsePattern(".");
  }

  if (lastMorseDashSw == HIGH && dashSw == LOW) {
    lastMorseButtonMs = millis();
    lastMorseText = "SW2 DASH";
    addLiveMorseSymbol('-');
    playMorsePattern("-");
  }

  lastMorseDotSw = dotSw;
  lastMorseDashSw = dashSw;
}

void handleMorse() {
  if (server.hasArg("pattern")) {
    String pattern = server.arg("pattern");
    lastMorseText = pattern;

    for (int i = 0; i < pattern.length(); i++) {
      char s = pattern.charAt(i);
      if (s == '.' || s == '-') addLiveMorseSymbol(s);
    }

    playMorsePattern(pattern);
  } else if (server.hasArg("text")) {
    String text = server.arg("text");
    playMorseText(text);
  }

  String json = "{";
  json += "\"ok\":true,";
  json += "\"busy\":" + String(morseBusy ? 1 : 0) + ",";
  json += "\"pattern\":\"" + lastMorsePattern + "\"";
  json += "}";

  server.send(200, "application/json", json);
}


void colorTestRgb(uint8_t r, uint8_t g, uint8_t b) {
  if (!initRgbIfNeeded()) return;
  for (int i = 0; i < RGB_COUNT; i++) {
    rgbStrip.setPixelColor(i, rgbStrip.Color(r, g, b));
  }
  rgbStrip.show();
}

String colorNameFromChar(char c) {
  if (c == 'R') return "แดง";
  if (c == 'Y') return "เหลือง";
  if (c == 'G') return "เขียว";
  return "-";
}

void colorTestSetLedOnly(char c) {
  ledRState = false;
  ledYState = false;
  ledGState = false;

  if (c == 'R') {
    ledRState = true;
    colorTestRgb(255, 0, 0);
  } else if (c == 'Y') {
    ledYState = true;
    colorTestRgb(255, 180, 0);
  } else if (c == 'G') {
    ledGState = true;
    colorTestRgb(0, 255, 0);
  }

  applyLedState();
}

void colorTestAllLedOff() {
  ledRState = false;
  ledYState = false;
  ledGState = false;
  applyLedState();
}

void colorTestBeepShort() {
  beep1kHz(120);
}

void colorTestGenerateTargets() {
  colorTestSeq = "";

  colorTestTargets[0] = 'R';
  colorTestTargets[1] = 'R';
  colorTestTargets[2] = 'R';

  colorTestTargets[3] = 'Y';
  colorTestTargets[4] = 'Y';
  colorTestTargets[5] = 'Y';

  colorTestTargets[6] = 'G';
  colorTestTargets[7] = 'G';
  colorTestTargets[8] = 'G';

  for (int i = 8; i > 0; i--) {
    int j = random(0, i + 1);
    char temp = colorTestTargets[i];
    colorTestTargets[i] = colorTestTargets[j];
    colorTestTargets[j] = temp;
  }

  for (int i = 0; i < 9; i++) {
    colorTestSeq += colorTestTargets[i];
  }
}

void colorTestShowCurrent() {
  if (!colorTestMode || colorTestRound >= 9) return;

  char c = colorTestTargets[colorTestRound];
  colorTestCurrentName = colorNameFromChar(c);
  colorTestSetLedOnly(c);
  colorTestWaiting = true;
}

void colorTestStart() {
  randomSeed(micros() + analogRead(POT_PIN) + analogRead(LDR_PIN));

  colorTestMode = true;
  colorTestRound = 0;
  colorTestCorrect = 0;
  colorTestWrong = 0;
  colorTestAnswerSeq = "";
  colorTestResult = "RUNNING";
  colorTestWaiting = false;

  colorTestGenerateTargets();
  colorTestShowCurrent();
}

void colorTestFinish() {
  colorTestMode = false;
  colorTestWaiting = false;

  colorTestAllLedOff();

  if (colorTestCorrect >= 6) {
    colorTestResult = "PASS";
    colorTestCurrentName = "ผ่าน";
    colorTestRgb(0, 255, 0);
    beep1kHz(150);
    delay(80);
    beep1kHz(150);
  } else {
    colorTestResult = "FAIL";
    colorTestCurrentName = "ไม่ผ่าน";
    colorTestRgb(255, 0, 0);
    beep1kHz(500);
  }
}

void colorTestEvaluate(char answer) {
  if (!colorTestMode) return;
  if (!colorTestWaiting) return;
  if (colorTestRound >= 9) return;

  char target = colorTestTargets[colorTestRound];
  colorTestAnswerSeq += answer;

  colorTestBeepShort();

  if (answer == target) {
    colorTestCorrect++;
  } else {
    colorTestWrong++;
  }

  colorTestRound++;

  if (colorTestRound >= 9) {
    colorTestFinish();
    return;
  }

  colorTestShowCurrent();
}

void updateColorTestButtons() {
  int s1 = digitalRead(SW1_PIN);
  int s2 = digitalRead(SW2_PIN);
  int s3 = digitalRead(SW3_PIN);

  if (millis() - colorTestLastMs > 120) {
    if (lastColorSw1 == HIGH && s1 == LOW) {
      colorTestLastMs = millis();
      colorTestEvaluate('R');
    }
    if (lastColorSw2 == HIGH && s2 == LOW) {
      colorTestLastMs = millis();
      colorTestEvaluate('Y');
    }
    if (lastColorSw3 == HIGH && s3 == LOW) {
      colorTestLastMs = millis();
      colorTestEvaluate('G');
    }
  }

  lastColorSw1 = s1;
  lastColorSw2 = s2;
  lastColorSw3 = s3;
}

void handleColorTestMode() {
  if (server.hasArg("mode")) {
    bool startMode = server.arg("mode") == "1";
    if (startMode) {
      colorTestStart();
    } else {
      colorTestMode = false;
      colorTestWaiting = false;
      colorTestResult = "READY";
      colorTestCurrentName = "-";
      colorTestAllLedOff();
      if (rgbReady) {
        rgbStrip.clear();
        rgbStrip.show();
      }
    }
  }

  server.send(200, "application/json", "{\"ok\":true}");
}

void handleColorTestReset() {
  colorTestMode = false;
  colorTestSeq = "";
  colorTestAnswerSeq = "";
  colorTestRound = 0;
  colorTestCorrect = 0;
  colorTestWrong = 0;
  colorTestWaiting = false;
  colorTestCurrentName = "-";
  colorTestResult = "READY";
  colorTestAllLedOff();

  if (rgbReady) {
    rgbStrip.clear();
    rgbStrip.show();
  }

  server.send(200, "application/json", "{\"ok\":true}");
}

void handleColorPress() {
  if (server.hasArg("c")) {
    String cc = server.arg("c");
    if (cc.length() > 0) {
      colorTestEvaluate(cc.charAt(0));
    }
  }
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleRoot() {
  server.send(200, "text/html; charset=utf-8", pageHtml());
}

void handleApi() {
  int ldrValue = analogRead(LDR_PIN);
  int potValue = analogRead(POT_PIN);

  int sw1 = digitalRead(SW1_PIN);
  int sw2 = digitalRead(SW2_PIN);
  int sw3 = digitalRead(SW3_PIN);

  unsigned long uptimeSec = (millis() - bootMillis) / 1000;

  String json = "{";
  json += "\"ldr\":" + String(ldrValue) + ",";
  json += "\"pot\":" + String(potValue) + ",";
  json += "\"sw1\":" + String(sw1) + ",";
  json += "\"sw2\":" + String(sw2) + ",";
  json += "\"sw3\":" + String(sw3) + ",";
  json += "\"ledR\":" + String(ledRState ? 1 : 0) + ",";
  json += "\"ledY\":" + String(ledYState ? 1 : 0) + ",";
  json += "\"ledG\":" + String(ledGState ? 1 : 0) + ",";
  json += "\"rgbMode\":" + String(rgbMode) + ",";
  json += "\"rgbReady\":" + String(rgbReady ? 1 : 0) + ",";
  json += "\"buzzer\":" + String(buzzerState ? 1 : 0) + ",";
  json += "\"rotaryCount\":" + String(rotaryCount) + ",";
  json += "\"rotaryDir\":\"" + rotaryDirection + "\",";
  json += "\"rotaryA\":" + String(digitalRead(ROTARY_A_PIN)) + ",";
  json += "\"rotaryB\":" + String(digitalRead(ROTARY_B_PIN)) + ",";
  json += "\"rotarySw\":" + String(digitalRead(ROTARY_SW_PIN)) + ",";
  json += "\"rgbRotateDelay\":" + String(rgbRotateDelay) + ",";
  json += "\"plcMode\":" + String(plcMode ? 1 : 0) + ",";
  json += "\"plcMatched\":" + String(plcRuleMatched ? 1 : 0) + ",";
  json += "\"plcInput\":\"" + plcInput + "\",";
  json += "\"plcOutput\":\"" + plcOutput + "\",";
  json += "\"plcAction\":\"" + plcAction + "\",";
  json += "\"morseBusy\":" + String(morseBusy ? 1 : 0) + ",";
  json += "\"morsePattern\":\"" + lastMorsePattern + "\",";
  json += "\"morseDotSw\":" + String(digitalRead(SW1_PIN)) + ",";
  json += "\"morseDashSw\":" + String(digitalRead(SW2_PIN)) + ",";
  json += "\"liveMorseBuffer\":\"" + liveMorseBuffer + "\",";
  json += "\"liveMorseDecoded\":\"" + liveMorseDecoded + "\",";
  json += "\"liveMorseLastLetter\":\"" + liveMorseLastLetter + "\",";
  json += "\"colorTestMode\":" + String(colorTestMode ? 1 : 0) + ",";
  json += "\"colorTestSeq\":\"" + colorTestSeq + "\",";
  json += "\"colorAnswerSeq\":\"" + colorTestAnswerSeq + "\",";
  json += "\"colorTestResult\":\"" + colorTestResult + "\",";
  json += "\"colorCurrentName\":\"" + colorTestCurrentName + "\",";
  json += "\"colorRound\":" + String(colorTestRound) + ",";
  json += "\"colorCorrect\":" + String(colorTestCorrect) + ",";
  json += "\"colorWrong\":" + String(colorTestWrong) + ",";
  json += "\"colorWaiting\":" + String(colorTestWaiting ? 1 : 0) + ",";
  json += "\"uptime\":" + String(uptimeSec);
  json += "}";

  server.send(200, "application/json", json);
}

void handleI2C() {
  String result = i2cScan();
  String json = "{\"result\":\"" + jsonEscape(result) + "\"}";
  server.send(200, "application/json", json);
}

void handleLed() {
  if (server.hasArg("r")) ledRState = (server.arg("r") == "1");
  if (server.hasArg("y")) ledYState = (server.arg("y") == "1");
  if (server.hasArg("g")) ledGState = (server.arg("g") == "1");
  applyLedState();
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleRgb() {
  if (server.hasArg("mode")) {
    rgbMode = server.arg("mode").toInt();
    if (rgbMode < 0 || rgbMode > 6) rgbMode = 0;
    applyRgbMode();
  }
  String json = "{\"ok\":true,\"rgbMode\":" + String(rgbMode) + "}";
  server.send(200, "application/json", json);
}

void handleBuzzer() {
  if (server.hasArg("state")) {
    buzzerState = (server.arg("state") == "1");
    applyBuzzerState();
  }
  String json = "{\"ok\":true,\"buzzer\":" + String(buzzerState ? 1 : 0) + "}";
  server.send(200, "application/json", json);
}

void handleBeep() {
  bool oldState = buzzerState;

  buzzerState = false;
  digitalWrite(BUZZER_PIN, LOW);
  beep1kHz(300);

  buzzerState = oldState;
  applyBuzzerState();

  server.send(200, "application/json", "{\"ok\":true}");
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  bootMillis = millis();

  Serial.println();
  Serial.println("ESP32-S3 Dashboard V12DD ADD MORSE CODE Start");

  pinMode(LED_R, OUTPUT);
  pinMode(LED_Y, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  ledRState = false;
  ledYState = false;
  ledGState = false;
  buzzerState = false;
  applyLedState();
  applyBuzzerState();

  pinMode(SW1_PIN, INPUT_PULLUP);
  pinMode(SW2_PIN, INPUT_PULLUP);
  pinMode(SW3_PIN, INPUT_PULLUP);

  lastMorseDotSw = digitalRead(SW1_PIN);
  lastMorseDashSw = digitalRead(SW2_PIN);
  lastColorSw1 = digitalRead(SW1_PIN);
  lastColorSw2 = digitalRead(SW2_PIN);
  lastColorSw3 = digitalRead(SW3_PIN);

  pinMode(ROTARY_A_PIN, INPUT_PULLUP);
  pinMode(ROTARY_B_PIN, INPUT_PULLUP);
  pinMode(ROTARY_SW_PIN, INPUT_PULLUP);
  lastRotaryA = digitalRead(ROTARY_A_PIN);
  lastRotaryB = digitalRead(ROTARY_B_PIN);
  lastRotarySw = digitalRead(ROTARY_SW_PIN);

  analogReadResolution(12);
  Wire.begin(I2C_SDA, I2C_SCL);

  pinMode(RGB_PIN, OUTPUT);
  digitalWrite(RGB_PIN, LOW);
  rgbReady = false;
  rgbMode = 0;

  WiFi.mode(WIFI_STA);
  WiFi.config(local_IP, gateway, subnet, dns);
  WiFi.begin(ssid, password);

  Serial.println("Connecting WiFi...");

  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if (millis() - startTime > 20000) {
      Serial.println();
      Serial.println("WiFi timeout. Restarting...");
      ESP.restart();
    }
  }

  Serial.println();
  Serial.print("Open: http://");
  Serial.print(WiFi.localIP());
  Serial.println(":99");

  server.on("/", handleRoot);
  server.on("/api", handleApi);
  server.on("/i2c", handleI2C);
  server.on("/led", handleLed);
  server.on("/rgb", handleRgb);
  server.on("/buzzer", handleBuzzer);
  server.on("/beep", handleBeep);
  server.on("/rotaryReset", handleRotaryReset);
  server.on("/plcRule", handlePlcRule);
  server.on("/plcMode", handlePlcMode);
  server.on("/morse", handleMorse);
  server.on("/morseClear", handleMorseClear);
  server.on("/colorTestMode", handleColorTestMode);
  server.on("/colorTestReset", handleColorTestReset);
  server.on("/colorPress", handleColorPress);

  server.begin();
  Serial.println("Web server started.");
}

void loop() {
  server.handleClient();

  updateMorseButtons();

  decodeLiveMorseIfTimeout();

  updateColorTestButtons();

  updateRotaryEncoder();

  updateMiniPlc();

  updateRgbRotaryAnimation();

  updateBuzzerTone();
}
