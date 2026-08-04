#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#include <TFT_eSPI.h>

#include "project_config.h"
#include "secrets.h"

TFT_eSPI tft;
WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

const char* STATUS_TOPIC = MESHCORE_STATUS_TOPIC;
const char* PACKETS_TOPIC = MESHCORE_PACKETS_TOPIC;

bool repeaterOnline = false;
String repeaterName = "Waiting for status...";
String regionTag = MESHCORE_REGION;
uint32_t packetCount = 0, rxCount = 0, txCount = 0;
int lastRSSI = 0, noiseFloor = 0, queueLength = 0;
float lastSNR = 0;
uint32_t lastPacketMs = 0, lastStatusMs = 0, repeaterUptime = 0;
uint32_t lastWifiTry = 0, lastMqttTry = 0, lastDraw = 0;

// CoreScope dark theme → RGB565
// surface-0 #0f0f23 | surface-1 #1a1a2e | surface-2 #232340 | border #334155
// text #e2e8f0 | muted #d1d5db | accent #4a9eff | green #22c55e | amber #f59e0b | red #ef4444
const uint16_t COL_BG     = 0x0864;  // #0f0f23
const uint16_t COL_PANEL  = 0x18C5;  // #1a1a2e  header / raised
const uint16_t COL_CARD   = 0x2108;  // #232340  cards
const uint16_t COL_EDGE   = 0x320A;  // #334155
const uint16_t COL_ACCENT = 0x4CFF;  // #4a9eff
const uint16_t COL_OK     = 0x262B;  // #22c55e  (approx)
const uint16_t COL_WARN   = 0xF4E1;  // #f59e0b
const uint16_t COL_BAD    = 0xEA28;  // #ef4444
const uint16_t COL_MUTED  = 0xD6BB;  // #d1d5db
const uint16_t COL_TEXT   = 0xE75E;  // #e2e8f0
const uint16_t COL_OK_BG  = 0x0A20;  // deep green chip
const uint16_t COL_BAD_BG = 0x4000;  // deep red chip
const uint16_t COL_ACC_BG = 0x10A6;  // blue-tinted region badge

// Layout — 320x240 landscape, CoreScope-style observer cards
const int SCR_W = 320, SCR_H = 240;
const int M = 6, GAP = 4;
const int HDR_H = 30;
const int NAME_Y = 54, NAME_H = 24;
const int CARD_W = 100, CARD_H = 66;
const int ROW1_Y = 84, ROW2_Y = 154;
const int FOOT_Y = 226;

bool uiBooted = false;
bool prevMqtt = false, prevRptr = false, prevWifi = false;
String prevName, prevPkts, prevRxTx, prevLast, prevRssi, prevSnr, prevNoise, prevFoot;

String elapsed(uint32_t ms) {
  uint32_t s = ms / 1000;
  if (s < 60) return String(s) + "s";
  if (s < 3600) return String(s / 60) + "m";
  return String(s / 3600) + "h";
}
String uptimeFmt(uint32_t s) {
  if (s < 60) return String(s) + "s";
  if (s < 3600) return String(s / 60) + "m";
  if (s < 172800) return String(s / 3600) + "h";
  return String(s / 86400) + "d";
}

uint16_t rssiColor(int r) {
  if (!r) return COL_MUTED;
  if (r < -110) return COL_BAD;
  if (r < -100) return COL_WARN;
  return COL_OK;
}
uint16_t snrColor(float s) {
  if (lastRSSI == 0) return COL_MUTED;
  if (s < 0) return COL_WARN;
  return COL_OK;
}

void drawStatusChip(int x, int y, const char* label, bool ok) {
  const int w = 52, h = 16;
  uint16_t bg = ok ? COL_OK_BG : COL_BAD_BG;
  uint16_t fg = ok ? COL_OK : COL_BAD;
  tft.fillRoundRect(x, y, w, h, 3, bg);
  tft.fillCircle(x + 8, y + 8, 3, fg);
  tft.setTextColor(fg, bg);
  tft.drawString(label, x + 15, y + 4, 1);
}

void drawRegionBadge(int x, int y) {
  tft.fillRoundRect(x, y, 34, 16, 3, COL_ACC_BG);
  tft.drawRoundRect(x, y, 34, 16, 3, COL_ACCENT);
  tft.setTextColor(COL_ACCENT, COL_ACC_BG);
  tft.drawCentreString(regionTag, x + 17, y + 4, 1);
}

void drawHeader(bool force) {
  bool mqttOk = mqtt.connected();
  bool wifiOk = WiFi.status() == WL_CONNECTED;
  if (!force && mqttOk == prevMqtt && repeaterOnline == prevRptr && wifiOk == prevWifi) return;
  prevMqtt = mqttOk;
  prevRptr = repeaterOnline;
  prevWifi = wifiOk;

  tft.fillRect(0, 0, SCR_W, HDR_H, COL_PANEL);
  tft.drawFastHLine(0, 0, SCR_W, COL_ACCENT);
  tft.drawFastHLine(0, HDR_H - 1, SCR_W, COL_EDGE);

  tft.setTextColor(COL_ACCENT, COL_PANEL);
  tft.drawString(MESHPULSE_HEADER_LABEL, M, 7, 2);
  tft.setTextColor(COL_MUTED, COL_PANEL);
  tft.drawString("CoreScope", 62, 10, 1);

  drawRegionBadge(148, 7);
  drawStatusChip(198, 7, "MQTT", mqttOk);
  drawStatusChip(256, 7, "RPTR", repeaterOnline);
}

void drawNameBar(bool force) {
  String name = repeaterName;
  if (name.length() > 26) name = name.substring(0, 26);
  if (!force && name == prevName) return;
  prevName = name;

  tft.fillRoundRect(M, NAME_Y, SCR_W - 2 * M, NAME_H, 5, COL_CARD);
  tft.drawRoundRect(M, NAME_Y, SCR_W - 2 * M, NAME_H, 5, COL_EDGE);

  // left accent bar like CoreScope selected row
  tft.fillRoundRect(M, NAME_Y, 4, NAME_H, 2, COL_ACCENT);

  tft.setTextColor(COL_TEXT, COL_CARD);
  tft.drawString(name, M + 12, NAME_Y + 5, 2);
}

void drawCard(int x, int y, const char* label, const String& value, uint16_t valueColor,
              String& prev, bool force) {
  if (!force && value == prev) return;
  prev = value;

  tft.fillRoundRect(x, y, CARD_W, CARD_H, 6, COL_CARD);
  tft.drawRoundRect(x, y, CARD_W, CARD_H, 6, COL_EDGE);

  tft.setTextColor(COL_MUTED, COL_CARD);
  tft.drawString(label, x + 8, y + 6, 1);
  tft.drawFastHLine(x + 8, y + 18, CARD_W - 16, COL_EDGE);

  int font = value.length() > 8 ? 2 : 4;
  int vy = font == 4 ? y + 26 : y + 32;
  tft.setTextColor(valueColor, COL_CARD);
  tft.drawCentreString(value, x + CARD_W / 2, vy, font);
}

void drawFooter(bool force) {
  String foot;
  if (WiFi.status() == WL_CONNECTED) {
    foot = WiFi.localIP().toString() + "  ·  up " + uptimeFmt(repeaterUptime);
  } else {
    foot = "Wi-Fi connecting...";
  }
  if (!force && foot == prevFoot) return;
  prevFoot = foot;

  tft.fillRect(0, FOOT_Y, SCR_W, SCR_H - FOOT_Y, COL_BG);
  tft.drawFastHLine(0, FOOT_Y, SCR_W, COL_EDGE);
  tft.setTextColor(COL_MUTED, COL_BG);
  tft.drawCentreString(foot, SCR_W / 2, FOOT_Y + 4, 1);
}

void drawChrome() {
  tft.fillScreen(COL_BG);
  drawHeader(true);
  drawNameBar(true);

  int x0 = M;
  int x1 = M + CARD_W + GAP;
  int x2 = M + 2 * (CARD_W + GAP);

  drawCard(x0, ROW1_Y, "PACKETS", "--", COL_ACCENT, prevPkts, true);
  drawCard(x1, ROW1_Y, "RX / TX", "--", COL_TEXT, prevRxTx, true);
  drawCard(x2, ROW1_Y, "LAST PKT", "--", COL_OK, prevLast, true);
  drawCard(x0, ROW2_Y, "RSSI", "--", COL_MUTED, prevRssi, true);
  drawCard(x1, ROW2_Y, "SNR", "--", COL_MUTED, prevSnr, true);
  drawCard(x2, ROW2_Y, "NOISE / Q", "--", COL_TEXT, prevNoise, true);
  drawFooter(true);
  uiBooted = true;
}

void drawUI() {
  if (!uiBooted) drawChrome();

  drawHeader(false);
  drawNameBar(false);

  int x0 = M;
  int x1 = M + CARD_W + GAP;
  int x2 = M + 2 * (CARD_W + GAP);

  String pkts = String(packetCount);
  String rxtx = String(rxCount) + "/" + String(txCount);
  String last = lastPacketMs ? elapsed(millis() - lastPacketMs) : "--";
  String rssi = lastRSSI ? String(lastRSSI) + "dBm" : "--";
  String snr  = lastRSSI ? String(lastSNR, 1) + "dB" : "--";
  String noise = (noiseFloor || queueLength)
                   ? String(noiseFloor) + "/" + String(queueLength)
                   : "--";

  drawCard(x0, ROW1_Y, "PACKETS", pkts, COL_ACCENT, prevPkts, false);
  drawCard(x1, ROW1_Y, "RX / TX", rxtx, COL_TEXT, prevRxTx, false);
  drawCard(x2, ROW1_Y, "LAST PKT", last, COL_OK, prevLast, false);
  drawCard(x0, ROW2_Y, "RSSI", rssi, rssiColor(lastRSSI), prevRssi, false);
  drawCard(x1, ROW2_Y, "SNR", snr, snrColor(lastSNR), prevSnr, false);
  drawCard(x2, ROW2_Y, "NOISE / Q", noise, COL_TEXT, prevNoise, false);
  drawFooter(false);
}

void callback(char* topic, byte* payload, unsigned int len) {
  JsonDocument doc;
  if (deserializeJson(doc, payload, len)) return;
  String t(topic);
  if (t.endsWith("/status")) {
    repeaterOnline = String((const char*)(doc["status"] | "unknown")).equalsIgnoreCase("online");
    repeaterName = String((const char*)(doc["origin"] | "Unknown repeater"));
    JsonObject s = doc["stats"];
    if (!s.isNull()) {
      noiseFloor = s["noise_floor"] | noiseFloor;
      queueLength = s["queue_len"] | queueLength;
      repeaterUptime = s["uptime_secs"] | repeaterUptime;
    }
    lastStatusMs = millis();
  } else if (t.endsWith("/packets")) {
    packetCount++;
    const char* d = doc["direction"] | "";
    if (!strcmp(d, "rx")) {
      rxCount++;
      if (doc["RSSI"].is<const char*>()) lastRSSI = String((const char*)doc["RSSI"]).toInt();
      else lastRSSI = doc["RSSI"] | lastRSSI;
      if (doc["SNR"].is<const char*>()) lastSNR = String((const char*)doc["SNR"]).toFloat();
      else lastSNR = doc["SNR"] | lastSNR;
    } else if (!strcmp(d, "tx")) {
      txCount++;
    }
    lastPacketMs = millis();
  }
}

void initDisplay() {
  Serial.println("MeshPulse boot");
  Serial.println("ESP32-2432S028 MeshPulse");

  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, TFT_BACKLIGHT_ON);

  delay(150);
  tft.init();
  tft.setRotation(1);
  tft.setTextWrap(false);

  tft.fillScreen(TFT_RED);   delay(100);
  tft.fillScreen(TFT_GREEN); delay(100);
  tft.fillScreen(TFT_BLUE);  delay(100);
  tft.fillScreen(COL_BG);

  tft.setTextColor(COL_ACCENT, COL_BG);
  tft.drawCentreString("MeshPulse", 160, 95, 4);
  tft.setTextColor(COL_MUTED, COL_BG);
  tft.drawCentreString("CoreScope dark", 160, 135, 2);
  Serial.println("Display init OK");
}

void connectWiFi() {
  lastWifiTry = millis();
  WiFi.disconnect();
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void connectMQTT() {
  lastMqttTry = millis();
  String id = "MeshPulse-" + String((uint32_t)ESP.getEfuseMac(), HEX);
  bool ok = strlen(MQTT_USERNAME)
              ? mqtt.connect(id.c_str(), MQTT_USERNAME, MQTT_PASSWORD)
              : mqtt.connect(id.c_str());
  if (ok) {
    mqtt.subscribe(STATUS_TOPIC);
    mqtt.subscribe(PACKETS_TOPIC);
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);
  initDisplay();
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(callback);
  mqtt.setBufferSize(2048);
  mqtt.setKeepAlive(30);
  connectWiFi();
  drawChrome();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    if (millis() - lastWifiTry > 10000) connectWiFi();
  } else {
    if (!mqtt.connected() && millis() - lastMqttTry > 5000) connectMQTT();
    if (mqtt.connected()) mqtt.loop();
  }
  if (lastStatusMs && millis() - lastStatusMs > 180000) repeaterOnline = false;
  if (millis() - lastDraw > 400) {
    lastDraw = millis();
    if (lastPacketMs) prevLast = "";
    drawUI();
  }
  delay(5);
}
