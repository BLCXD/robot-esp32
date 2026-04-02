/*
  Robot AI Assistant - ESP32 WROOM
  ================================
  Wymagane biblioteki (Arduino Library Manager):
  - ArduinoWebsockets by Gil Maimon
  - ESP8266Audio by Earle F. Philhower (obsługuje też ESP32)
  - ArduinoJson by Benoit Blanchon

  Schemat podłączeń:
  ==================
  MOSTEK H (L298N):
    IN1 → GPIO 25
    IN2 → GPIO 26
    IN3 → GPIO 27
    IN4 → GPIO 14
    ENA → GPIO 32 (PWM lewy silnik)
    ENB → GPIO 33 (PWM prawy silnik)
    5V  → zewnętrzne zasilanie silników
    GND → wspólna masa

  I2S DAC (MAX98357A):
    BCLK → GPIO 22
    LRC  → GPIO 21
    DIN  → GPIO 23
    GND  → GND
    VIN  → 3.3V lub 5V
*/

#include <WiFi.h>
#include <ArduinoWebsockets.h>
#include <ArduinoJson.h>
#include <AudioGeneratorMP3.h>
#include <AudioOutputI2S.h>
#include <AudioFileSourceBuffer.h>
#include <AudioFileSourcePROGMEM.h>

using namespace websockets;

// ==============================
// KONFIGURACJA - ZMIEŃ TE WARTOŚCI
// ==============================
const char* WIFI_SSID     = "NAZWA_TWOJEJ_SIECI";
const char* WIFI_PASSWORD = "HASLO_WIFI";
const char* SERVER_IP     = "192.168.1.100"; // IP komputera z serwerem
const int   SERVER_PORT   = 5000;
// ==============================

// Piny silników
#define MOTOR_L_IN1  25
#define MOTOR_L_IN2  26
#define MOTOR_R_IN3  27
#define MOTOR_R_IN4  14
#define MOTOR_L_PWM  32
#define MOTOR_R_PWM  33

// Piny I2S (MAX98357A)
#define I2S_BCLK     22
#define I2S_LRC      21
#define I2S_DOUT     23

// PWM kanały
#define PWM_CH_L     0
#define PWM_CH_R     1
#define PWM_FREQ     1000
#define PWM_RES      8

// Prędkość silników (0-255)
#define SPEED_NORMAL 200
#define SPEED_TURN   180

WebsocketsClient ws;
AudioGeneratorMP3 *mp3 = nullptr;
AudioOutputI2S    *i2s = nullptr;

bool      wsConnected    = false;
bool      audioPlaying   = false;
uint8_t  *audioBuffer    = nullptr;
size_t    audioBufferLen = 0;

// Kolejka komend ruchu
struct MoveCmd {
  String action;
  int    duration;
};
MoveCmd   moveQueue[20];
int       moveQueueHead = 0;
int       moveQueueTail = 0;
bool      moveQueued    = false;
unsigned long moveEndTime = 0;
bool      executingMove   = false;
String    currentAction   = "STOP";

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

void motorForward()  { setMotors(1,0,1,0, SPEED_NORMAL); }
void motorBackward() { setMotors(0,1,0,1, SPEED_NORMAL); }
void motorLeft()     { setMotors(0,1,1,0, SPEED_TURN);   }
void motorRight()    { setMotors(1,0,0,1, SPEED_TURN);   }
void motorStop()     { setMotors(0,0,0,0, 0);             }

void executeAction(const String& action) {
  currentAction = action;
  if      (action == "FORWARD")  motorForward();
  else if (action == "BACKWARD") motorBackward();
  else if (action == "LEFT")     motorLeft();
  else if (action == "RIGHT")    motorRight();
  else                           motorStop();
}

// ──────────────────────────────
// Kolejka ruchu
// ──────────────────────────────
void enqueueMoveCmd(const String& action, int duration) {
  if (moveQueueTail < 20) {
    moveQueue[moveQueueTail++] = {action, duration};
    moveQueued = true;
  }
}

void processMoveQueue() {
  // Jeśli trwa komenda z czasem - czekaj
  if (executingMove && millis() < moveEndTime) return;
  executingMove = false;

  if (!moveQueued || moveQueueHead >= moveQueueTail) {
    moveQueued = false;
    moveQueueHead = moveQueueTail = 0;
    return;
  }

  MoveCmd cmd = moveQueue[moveQueueHead++];
  executeAction(cmd.action);

  if (cmd.duration > 0) {
    executingMove = true;
    moveEndTime   = millis() + cmd.duration;
  }
  if (moveQueueHead >= moveQueueTail) {
    moveQueued    = false;
    moveQueueHead = moveQueueTail = 0;
  }
}

// ──────────────────────────────
// Audio I2S
// ──────────────────────────────
void audioInit() {
  i2s = new AudioOutputI2S();
  i2s->SetPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  i2s->SetGain(0.8);
}

void playAudioFromBase64(const String& b64) {
  // Zatrzymaj poprzednie audio
  if (mp3 && mp3->isRunning()) {
    mp3->stop();
    delete mp3;
    mp3 = nullptr;
  }
  if (audioBuffer) {
    free(audioBuffer);
    audioBuffer = nullptr;
  }

  // Dekoduj base64
  size_t b64Len  = b64.length();
  size_t outLen  = (b64Len * 3) / 4;
  audioBuffer    = (uint8_t*)malloc(outLen);
  if (!audioBuffer) {
    Serial.println("Brak pamięci na audio!");
    return;
  }

  // Prosta dekodacja base64
  const char* b64chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  size_t i = 0, j = 0;
  uint8_t tmp[4];
  audioBufferLen = 0;

  for (size_t k = 0; k < b64Len; k++) {
    char c = b64[k];
    if (c == '=') break;
    const char* pos = strchr(b64chars, c);
    if (!pos) continue;
    tmp[i++] = pos - b64chars;
    if (i == 4) {
      audioBuffer[j++] = (tmp[0] << 2) | (tmp[1] >> 4);
      audioBuffer[j++] = (tmp[1] << 4) | (tmp[2] >> 2);
      audioBuffer[j++] = (tmp[2] << 6) | tmp[3];
      i = 0;
    }
  }
  if (i > 1) audioBuffer[j++] = (tmp[0] << 2) | (tmp[1] >> 4);
  if (i > 2) audioBuffer[j++] = (tmp[1] << 4) | (tmp[2] >> 2);
  audioBufferLen = j;

  // Odtwórz
  auto* source = new AudioFileSourcePROGMEM(audioBuffer, audioBufferLen);
  auto* buf    = new AudioFileSourceBuffer(source, 2048);
  mp3          = new AudioGeneratorMP3();
  if (mp3->begin(buf, i2s)) {
    audioPlaying = true;
    Serial.println("Audio: odtwarzanie...");
  } else {
    Serial.println("Audio: błąd startowania MP3");
    audioPlaying = false;
  }
}

void processAudio() {
  if (!audioPlaying || !mp3) return;
  if (mp3->isRunning()) {
    if (!mp3->loop()) {
      mp3->stop();
      audioPlaying = false;
      Serial.println("Audio: zakończono");
    }
  } else {
    audioPlaying = false;
  }
}

// ──────────────────────────────
// WebSocket
// ──────────────────────────────
void onMessage(WebsocketsMessage msg) {
  Serial.print("WS odebrano: ");
  Serial.println(msg.data().substring(0, 80));

  StaticJsonDocument<512> doc;
  DeserializationError err = deserializeJson(doc, msg.data());
  if (err) {
    Serial.println("JSON błąd");
    return;
  }

  const char* type = doc["type"];

  if (strcmp(type, "audio") == 0) {
    String b64 = doc["data"].as<String>();
    playAudioFromBase64(b64);
  }
  else if (strcmp(type, "move") == 0) {
    // Natychmiastowa komenda (strzałki)
    String action   = doc["action"].as<String>();
    int    duration = doc["duration"] | 0;
    if (duration > 0) {
      enqueueMoveCmd(action, duration);
      enqueueMoveCmd("STOP", 0);
    } else {
      moveQueued = false;
      moveQueueHead = moveQueueTail = 0;
      executingMove = false;
      executeAction(action);
    }
  }
  else if (strcmp(type, "move_sequence") == 0) {
    // Sekwencja komend od AI
    moveQueued = false;
    moveQueueHead = moveQueueTail = 0;
    executingMove = false;
    JsonArray commands = doc["data"]["commands"].as<JsonArray>();
    for (JsonObject cmd : commands) {
      String action   = cmd["action"].as<String>();
      int    duration = cmd["duration"] | 0;
      enqueueMoveCmd(action, duration);
    }
    Serial.print("Kolejka ruchu: ");
    Serial.println(moveQueueTail);
  }
}

void wsConnect() {
  String url = "ws://" + String(SERVER_IP) + ":" + String(SERVER_PORT) + "/ws/esp32";
  Serial.print("Łączę z: ");
  Serial.println(url);

  ws.onMessage(onMessage);
  ws.onEvent([](WebsocketsEvent e, String d) {
    if (e == WebsocketsEvent::ConnectionOpened) {
      wsConnected = true;
      Serial.println("WS: połączono!");
    } else if (e == WebsocketsEvent::ConnectionClosed) {
      wsConnected = false;
      Serial.println("WS: rozłączono");
    }
  });

  wsConnected = ws.connect(SERVER_IP, SERVER_PORT, "/ws/esp32");
}

// ──────────────────────────────
// Setup & Loop
// ──────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(500);

  motorsInit();
  motorStop();
  audioInit();

  // WiFi
  Serial.print("Łączę WiFi: ");
  Serial.println(WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 30) {
    delay(500);
    Serial.print(".");
    tries++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi OK! IP: " + WiFi.localIP().toString());
  } else {
    Serial.println("\nBłąd WiFi!");
    return;
  }

  wsConnect();
}

unsigned long lastPing    = 0;
unsigned long lastReconn  = 0;

void loop() {
  unsigned long now = millis();

  // Odbieraj wiadomości WS
  if (wsConnected) {
    ws.poll();

    // Ping co 10s
    if (now - lastPing > 10000) {
      lastPing = now;
      ws.send("{\"type\":\"ping\"}");
    }
  } else {
    // Reconnect co 5s
    if (now - lastReconn > 5000) {
      lastReconn = now;
      Serial.println("Próba ponownego połączenia...");
      wsConnect();
    }
  }

  // Audio
  processAudio();

  // Ruch (kolejka AI)
  if (moveQueued) {
    processMoveQueue();
  }

  delay(10);
}
