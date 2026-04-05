/*
  Robot AI Assistant - ESP32 WROOM
  ================================
  Wymagane biblioteki (Arduino Library Manager):
  - ArduinoWebsockets by Gil Maimon
  - ESP32-audioI2S by schreibfaul1
  - ArduinoJson by Benoit Blanchon

  Schemat podłączeń:
  ==================
  SERWA (2x servo 3-pin, np. MG90S):
    Lewe servo:   PWM → GPIO 25 | VCC → 5V | GND → GND
    Prawe servo:  PWM → GPIO 26 | VCC → 5V | GND → GND

    UWAGA: Serwa ciągłe (continuous rotation) - obracają się zamiast
    ustawiać kąt.  - Jeśli masz zwykłe serwa, trzeba je przerobić
    na continuous (mechanicznie lub kupić gotowe).
    Środek (1500µs) = STOP, <1500µs = jedna strona, >1500µs = druga.

  GŁOŚNIK (jack 3.5mm lub bezpośrednio):
    GPIO 25 → kabel jack (L lub R) → głośnik
    GND     → kabel jack (masa)    → głośnik
    Opcjonalnie: kondensator 10µF między GPIO25 a jackiem (filtruje DC)
    Opcjonalnie: wzmacniacz PAM8403 jeśli głośnik za cichy

  UWAGA na GPIO 25:
    GPIO 25 to jednocześnie DAC1 (audio) i tutaj pin PWM lewego serwa.
    Używamy go NA PRZEMIAN - podczas ruchu audio jest wyłączone,
    podczas audio serwa stoją. W kodzie przełączamy tryb pinu.
*/

#include <WiFi.h>
#include <ArduinoWebsockets.h>
#include <ArduinoJson.h>
#include <Audio.h>
#include <ESP32Servo.h>

using namespace websockets;

// ──────────────────────────────
// KONFIGURACJA
// ──────────────────────────────
const char* WIFI_SSID     = "NAZWA_SIECI";
const char* WIFI_PASSWORD = "HASLO_WIFI";
const char* SERVER_IP     = "localhost"; 
const int   SERVER_PORT   = 5000;

// Piny serw
#define SERVO_L_PIN   25   // lewe servo  (uwaga: dzielony z DAC audio!)
#define SERVO_R_PIN   26   // prawe servo

// Audio DAC
#define DAC_PIN       25   // GPIO25 = DAC1 (dzielony z lewym servo)

// Prędkości serwa (µs) - dla serw continuous rotation
// Dostosuj STOP_US jeśli servo się kręci w spoczynku (zwykle 1500)
#define SERVO_STOP_US    1500
#define SERVO_FWD_L_US   1700   // lewe do przodu
#define SERVO_FWD_R_US   1300   // prawe do przodu (odwrócone montażowo)
#define SERVO_BCK_L_US   1300
#define SERVO_BCK_R_US   1700
#define SERVO_TURN_US    1650   // mniej agresywny skręt

// ──────────────────────────────
// Obiekty
// ──────────────────────────────
WebsocketsClient ws;
Servo servoL, servoR;
Audio audio;

bool wsConnected  = false;
bool audioPlaying = false;
bool servoActive  = false;

// Kolejka ruchów
struct MoveCmd { String action; int duration; };
MoveCmd       moveQueue[20];
int           moveQueueHead = 0;
int           moveQueueTail = 0;
bool          moveQueued    = false;
unsigned long moveEndTime   = 0;
bool          executingMove = false;

// ──────────────────────────────
// Przełączanie trybu GPIO25
// GPIO25 = servo LUB DAC - nie mogą działać jednocześnie
// ──────────────────────────────
void enableServos() {
  if (servoActive) return;
  audio.stopSong();
  audioPlaying = false;
  delay(10);
  servoL.attach(SERVO_L_PIN, 1000, 2000);
  servoR.attach(SERVO_R_PIN, 1000, 2000);
  servoActive = true;
}

void enableAudio() {
  if (audioPlaying) return;
  servoL.write(SERVO_STOP_US);
  servoR.write(SERVO_STOP_US);
  delay(50);
  servoL.detach();
  // servoR zostaje (GPIO26 nie koliduje z DAC)
  servoActive = false;
  // DAC na GPIO25
  audio.setPinout(0, 0, DAC_PIN);   // bclk=0, lrc=0, dout=DAC_PIN (tryb DAC)
  audio.setVolume(18);               // 0-21
}

// ──────────────────────────────
// Ruch
// ──────────────────────────────
void motorStop() {
  servoL.writeMicroseconds(SERVO_STOP_US);
  servoR.writeMicroseconds(SERVO_STOP_US);
  Serial.println("[RUCH] STOP");
}

void executeAction(const String& action) {
  enableServos();
  Serial.println("[RUCH] " + action);

  if (action == "FORWARD") {
    servoL.writeMicroseconds(SERVO_FWD_L_US);
    servoR.writeMicroseconds(SERVO_FWD_R_US);
  } else if (action == "BACKWARD") {
    servoL.writeMicroseconds(SERVO_BCK_L_US);
    servoR.writeMicroseconds(SERVO_BCK_R_US);
  } else if (action == "LEFT") {
    servoL.writeMicroseconds(SERVO_BCK_L_US);  // lewe do tyłu
    servoR.writeMicroseconds(SERVO_FWD_R_US);  // prawe do przodu
  } else if (action == "RIGHT") {
    servoL.writeMicroseconds(SERVO_FWD_L_US);
    servoR.writeMicroseconds(SERVO_BCK_R_US);
  } else {
    motorStop();
  }
}

// ──────────────────────────────
// Kolejka ruchów
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

  if (cmd.duration > 0) {
    executingMove = true;
    moveEndTime   = millis() + cmd.duration;
  }
  if (moveQueueHead >= moveQueueTail) {
    moveQueued = false; moveQueueHead = moveQueueTail = 0;
  }
}

// ──────────────────────────────
// Audio - odtwórz MP3 z base64
// ──────────────────────────────
void playAudioFromBase64(const String& b64) {
  // Dekoduj base64 → bufor
  size_t   b64Len = b64.length();
  size_t   outLen = (b64Len * 3) / 4 + 4;
  uint8_t* buf    = (uint8_t*)malloc(outLen);
  if (!buf) { Serial.println("[AUDIO] Brak pamięci!"); return; }

  const char* chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  size_t i = 0, j = 0;
  uint8_t tmp[4];
  size_t decoded = 0;

  for (size_t k = 0; k < b64Len; k++) {
    char c = b64[k];
    if (c == '=') break;
    const char* p = strchr(chars, c);
    if (!p) continue;
    tmp[i++] = p - chars;
    if (i == 4) {
      buf[j++] = (tmp[0] << 2) | (tmp[1] >> 4);
      buf[j++] = (tmp[1] << 4) | (tmp[2] >> 2);
      buf[j++] = (tmp[2] << 6) | tmp[3];
      i = 0;
    }
  }
  if (i > 1) buf[j++] = (tmp[0] << 2) | (tmp[1] >> 4);
  if (i > 2) buf[j++] = (tmp[1] << 4) | (tmp[2] >> 2);
  decoded = j;

  enableAudio();

  // Zapisz do SPIFFS tymczasowo i odtwórz
  // (ESP32-audioI2S nie obsługuje bezpośrednio bufora w pamięci)
  File f = SPIFFS.open("/audio.mp3", FILE_WRITE);
  if (!f) { Serial.println("[AUDIO] Błąd SPIFFS"); free(buf); return; }
  f.write(buf, decoded);
  f.close();
  free(buf);

  audio.connecttoFS(SPIFFS, "/audio.mp3");
  audioPlaying = true;
  Serial.println("[AUDIO] Odtwarzanie...");
}

// ──────────────────────────────
// WebSocket
// ──────────────────────────────
void onMessage(WebsocketsMessage msg) {
  Serial.println("[WS] " + msg.data().substring(0, 60));

  StaticJsonDocument<1024> doc;
  if (deserializeJson(doc, msg.data())) return;

  const char* type = doc["type"];

  if (strcmp(type, "audio") == 0) {
    playAudioFromBase64(doc["data"].as<String>());
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
    for (JsonObject c : commands)
      enqueueMoveCmd(c["action"].as<String>(), c["duration"] | 0);
    Serial.printf("[SEKWENCJA] %d komend\n", moveQueueTail);
  }
}

void wsConnect() {
  ws.onMessage(onMessage);
  ws.onEvent([](WebsocketsEvent e, String d) {
    if (e == WebsocketsEvent::ConnectionOpened) {
      wsConnected = true; Serial.println("[WS] Połączono!");
    } else if (e == WebsocketsEvent::ConnectionClosed) {
      wsConnected = false; Serial.println("[WS] Rozłączono");
    }
  });
  wsConnected = ws.connect(SERVER_IP, SERVER_PORT, "/ws/esp32");
  if (!wsConnected) Serial.println("[WS] Błąd!");
}

// Callback ESP32-audioI2S - woła gdy skończy odtwarzać
void audio_eof_mp3(const char* info) {
  Serial.println("[AUDIO] Zakończono");
  audioPlaying = false;
}

// ──────────────────────────────
// Setup & Loop
// ──────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(500);

  // SPIFFS dla tymczasowych plików audio
  if (!SPIFFS.begin(true)) Serial.println("[SPIFFS] Błąd!");

  // Serwa
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  servoL.setPeriodHertz(50);
  servoR.setPeriodHertz(50);
  enableServos();
  motorStop();

  // WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("[WiFi] Łączę");
  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 30) {
    delay(500); Serial.print("."); tries++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[WiFi] OK " + WiFi.localIP().toString());
    wsConnect();
  } else {
    Serial.println("\n[WiFi] BŁĄD");
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

  if (audioPlaying) audio.loop();
  if (moveQueued)   processMoveQueue();

  delay(5);
}
