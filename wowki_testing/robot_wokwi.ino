/*
  Robot AI Assistant - WERSJA WOKWI + SERWER
  ===========================================
  WiFi: Wokwi-GUEST (wbudowana siec symulatora, bez hasla)
  Polaczenie z serwerem przez WebSocket - tak samo jak fizyczny ESP32.

  JAK URUCHOMIC:
  1. Wejdz na wokwi.com, nowy projekt ESP32
  2. Wgraj diagram.json i ten plik
  3. Zmien SERVER_IP na IP swojego komputera
  4. Uruchom serwer Python: python server.py
  5. Kliknij Start w Wokwi

  Biblioteki (libraries.txt):
    ArduinoWebsockets
    ArduinoJson
*/

#include <WiFi.h>
#include <ArduinoWebsockets.h>
#include <ArduinoJson.h>

using namespace websockets;

// ──────────────────────────────
// KONFIGURACJA
// ──────────────────────────────
const char* WIFI_SSID     = "Wokwi-GUEST";  // stala siec symulatora - nie zmieniaj
const char* WIFI_PASSWORD = "";              // brak hasla
const char* SERVER_IP     = "YOUR_IP";       // <- WPISZ IP SWOJEGO KOMPUTERA (np. 192.168.1.100)
const int   SERVER_PORT   = 5000;

// Piny silnikow
#define MOTOR_L_IN1  25
#define MOTOR_L_IN2  26
#define MOTOR_R_IN3  27
#define MOTOR_R_IN4  14
#define MOTOR_L_PWM  32
#define MOTOR_R_PWM  33
#define BUZZER_PIN   22  // zamiast MAX98357A w symulatorze

// PWM
#define PWM_CH_L     0
#define PWM_CH_R     1
#define PWM_FREQ     1000
#define PWM_RES      8
#define SPEED_NORMAL 200
#define SPEED_TURN   180

WebsocketsClient ws;
bool wsConnected = false;

struct MoveCmd { String action; int duration; };
MoveCmd       moveQueue[20];
int           moveQueueHead = 0;
int           moveQueueTail = 0;
bool          moveQueued    = false;
unsigned long moveEndTime   = 0;
bool          executingMove = false;

// ──────────────────────────────
// Silniki
// ──────────────────────────────
void motorsInit() {
  ledcSetup(PWM_CH_L, PWM_FREQ, PWM_RES);
  ledcSetup(PWM_CH_R, PWM_FREQ, PWM_RES);
  ledcAttachPin(MOTOR_L_PWM, PWM_CH_L);
  ledcAttachPin(MOTOR_R_PWM, PWM_CH_R);
  pinMode(MOTOR_L_IN1, OUTPUT);
  pinMode(MOTOR_L_IN2, OUTPUT);
  pinMode(MOTOR_R_IN3, OUTPUT);
  pinMode(MOTOR_R_IN4, OUTPUT);
}

void setMotors(int l1, int l2, int r1, int r2, int speed) {
  digitalWrite(MOTOR_L_IN1, l1);
  digitalWrite(MOTOR_L_IN2, l2);
  digitalWrite(MOTOR_R_IN3, r1);
  digitalWrite(MOTOR_R_IN4, r2);
  ledcWrite(PWM_CH_L, speed);
  ledcWrite(PWM_CH_R, speed);
}

void executeAction(const String& action) {
  Serial.println("[RUCH] " + action);
  if      (action == "FORWARD")  setMotors(1,0,1,0, SPEED_NORMAL);
  else if (action == "BACKWARD") setMotors(0,1,0,1, SPEED_NORMAL);
  else if (action == "LEFT")     setMotors(0,1,1,0, SPEED_TURN);
  else if (action == "RIGHT")    setMotors(1,0,0,1, SPEED_TURN);
  else                           setMotors(0,0,0,0, 0);
}

// ──────────────────────────────
// Kolejka ruchow
// ──────────────────────────────
void enqueueMoveCmd(const String& action, int duration) {
  if (moveQueueTail < 20) {
    moveQueue[moveQueueTail++] = {action, duration};
    moveQueued = true;
  }
}

void processMoveQueue() {
  if (executingMove && millis() < moveEndTime) return;
  executingMove = false;
  if (!moveQueued || moveQueueHead >= moveQueueTail) {
    moveQueued = false; moveQueueHead = moveQueueTail = 0;
    return;
  }
  MoveCmd cmd = moveQueue[moveQueueHead++];
  Serial.printf("[KOLEJKA] %s %dms\n", cmd.action.c_str(), cmd.duration);
  executeAction(cmd.action);
  if (cmd.duration > 0) { executingMove = true; moveEndTime = millis() + cmd.duration; }
  if (moveQueueHead >= moveQueueTail) { moveQueued = false; moveQueueHead = moveQueueTail = 0; }
}

// ──────────────────────────────
// Buzzer - zastepuje TTS w symulatorze
// ──────────────────────────────
void beepShort() { tone(BUZZER_PIN, 1000, 120); delay(170); noTone(BUZZER_PIN); }

// ──────────────────────────────
// WebSocket
// ──────────────────────────────
void onMessage(WebsocketsMessage msg) {
  Serial.println("[WS RX] " + msg.data().substring(0, 80));

  StaticJsonDocument<1024> doc;
  if (deserializeJson(doc, msg.data())) { Serial.println("[ERR] JSON"); return; }

  const char* type = doc["type"];

  if (strcmp(type, "audio") == 0) {
    Serial.println("[AUDIO] Odebrano audio z serwera (buzzer zamiast TTS)");
    beepShort(); delay(80); beepShort();
  }
  else if (strcmp(type, "move") == 0) {
    String action = doc["action"].as<String>();
    int duration  = doc["duration"] | 0;
    moveQueued = false; moveQueueHead = moveQueueTail = 0; executingMove = false;
    if (duration > 0) { enqueueMoveCmd(action, duration); enqueueMoveCmd("STOP", 0); }
    else executeAction(action);
  }
  else if (strcmp(type, "move_sequence") == 0) {
    moveQueued = false; moveQueueHead = moveQueueTail = 0; executingMove = false;
    JsonArray commands = doc["data"]["commands"].as<JsonArray>();
    Serial.printf("[SEKWENCJA] %d komend\n", commands.size());
    for (JsonObject c : commands) enqueueMoveCmd(c["action"].as<String>(), c["duration"] | 0);
  }
}

void wsConnect() {
  ws.onMessage(onMessage);
  ws.onEvent([](WebsocketsEvent e, String d) {
    if (e == WebsocketsEvent::ConnectionOpened) {
      wsConnected = true;
      Serial.println("[WS] Polaczono z serwerem!");
      beepShort();
    } else if (e == WebsocketsEvent::ConnectionClosed) {
      wsConnected = false;
      Serial.println("[WS] Rozlaczono");
    }
  });
  String url = "/ws/esp32";
  Serial.printf("[WS] Lacze z %s:%d%s\n", SERVER_IP, SERVER_PORT, url.c_str());
  wsConnected = ws.connect(SERVER_IP, SERVER_PORT, url);
  if (!wsConnected) Serial.println("[WS] Blad polaczenia!");
}

// ──────────────────────────────
// Setup & Loop
// ──────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(500);
  motorsInit();
  executeAction("STOP");
  pinMode(BUZZER_PIN, OUTPUT);

  Serial.println("=== Robot AI Wokwi + Serwer ===");
  Serial.printf("Serwer: %s:%d\n", SERVER_IP, SERVER_PORT);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("[WiFi] Lacze z Wokwi-GUEST");
  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 30) {
    delay(500); Serial.print("."); tries++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[WiFi] OK! IP: " + WiFi.localIP().toString());
    wsConnect();
  } else {
    Serial.println("\n[WiFi] BLAD!");
  }
}

unsigned long lastPing = 0, lastReconn = 0;

void loop() {
  unsigned long now = millis();
  if (wsConnected) {
    ws.poll();
    if (now - lastPing > 10000) { lastPing = now; ws.send("{\"type\":\"ping\"}"); }
  } else {
    if (now - lastReconn > 5000) { lastReconn = now; wsConnect(); }
  }
  if (moveQueued) processMoveQueue();
  delay(10);
}
