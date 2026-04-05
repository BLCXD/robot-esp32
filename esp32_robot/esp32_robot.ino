#include <WiFi.h>
#include <WebSocketsClient.h>
#include <SocketIOclient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// --- KONFIGURACJA SIECI ---
const char* ssid = "TWOJA_SIEC_WIFI";
const char* password = "TWOJE_HASLO_WIFI";
const char* server_ip = "192.168.X.X"; // Zmodyfikuj na lokalne IP gdzie dziala server.py
const uint16_t server_port = 5000;

SocketIOclient socketIO;

// --- KONFIGURACJA EKRANIKU I2C ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define INIT_SDA_PIN 27
#define INIT_SCL_PIN 14

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// --- KONFIGURACJA PINOW L298N ---
// Silnik tylny (napęd)
const int ENA = 13;
const int IN1 = 12;
const int IN2 = 26;

// Silnik przedni (skręcanie)
const int ENB = 25;
const int IN3 = 33;
const int IN4 = 32;

// PWM Konfiguracja
const int freq = 5000;
const int resolution = 8;
const int pwmChannelA = 0;
const int pwmChannelB = 1;

// Zmienne do zarzadzania czasem jazdy
unsigned long movementTimer = 0;
unsigned long movementDuration = 0;
bool isMoving = false;


// --- FUNKCJE POMOCNICZE ---

void printToScreen(String line1, String line2) {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println(line1);
  display.println(line2);
  display.display();
}

void setupDisplay() {
  Wire.begin(INIT_SDA_PIN, INIT_SCL_PIN);
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 init failed"));
    for(;;);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(F("Robot Init..."));
  display.display();
}

void setupMotors() {
  pinMode(ENA, OUTPUT);
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  
  pinMode(ENB, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
  
  // Detekcja wersji bibliotek dla ESP32 do obslugi PWM
  #if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
    analogWrite(ENA, 0);
    analogWrite(ENB, 0);
  #else
    ledcSetup(pwmChannelA, freq, resolution);
    ledcAttachPin(ENA, pwmChannelA);
    ledcSetup(pwmChannelB, freq, resolution);
    ledcAttachPin(ENB, pwmChannelB);
    ledcWrite(pwmChannelA, 0);
    ledcWrite(pwmChannelB, 0);
  #endif
}

void setMotorSpeedA(int speed) {
  #if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
    analogWrite(ENA, speed);
  #else
    ledcWrite(pwmChannelA, speed);
  #endif
}

void setMotorSpeedB(int speed) {
  #if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
    analogWrite(ENB, speed);
  #else
    ledcWrite(pwmChannelB, speed);
  #endif
}

void stopMotors() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
  setMotorSpeedA(0);
  setMotorSpeedB(0);
  isMoving = false;
  printToScreen("Status: STOP", "");
}

// --- FUNKCJE RUCHOWE ---

void moveForward(int speed) {
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  setMotorSpeedA(speed);
  
  // Wyprostowanie koł skretnych na wprost  (zerowanie napiecia na skrecie)
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
  setMotorSpeedB(0);
}

void moveBackward(int speed) {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  setMotorSpeedA(speed);

  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
  setMotorSpeedB(0);
}

void turnLeft(int speed) {
  // Typowo serwo samo odbija na sprezynie albo jest to normalny silnik
  // My zaluzmy normalny silnik skretny dzialajacy dwojako
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
  setMotorSpeedB(speed);
  // Podczas skretu jedziemy chwile do przodu a by faktycznie skrecic
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  setMotorSpeedA(speed);
}

void turnRight(int speed) {
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
  setMotorSpeedB(speed);
  // Podczas skretu jedziemy chwile do przodu
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  setMotorSpeedA(speed);
}

void executeCommand(String command, int value) {
  int defaultSpeed = 255; // Pelen gas (PWM 0-255)
  
  // Wartosc przeliczamy na milisekundy. Jesli podano 0 to domyslnie dajemy 1 sekunde ruchu.
  movementDuration = (value > 0) ? (value * 1000) : 1000;
  
  if(command == "przód" || command == "przod") {
    moveForward(defaultSpeed);
    printToScreen("Kierunek: PRZOD", "Czas: " + String(movementDuration) + "ms");
    isMoving = true;
    movementTimer = millis();
  } else if (command == "tył" || command == "tyl") {
    moveBackward(defaultSpeed);
    printToScreen("Kierunek: TYL", "Czas: " + String(movementDuration) + "ms");
    isMoving = true;
    movementTimer = millis();
  } else if (command == "lewo") {
    turnLeft(defaultSpeed);
    printToScreen("Kierunek: LEWO", "Czas: " + String(movementDuration) + "ms");
    isMoving = true;
    movementTimer = millis();
  } else if (command == "prawo") {
    turnRight(defaultSpeed);
    printToScreen("Kierunek: PRAWO", "Czas: " + String(movementDuration) + "ms");
    isMoving = true;
    movementTimer = millis();
  } else if (command == "stop" || command == "zatrzymaj") {
    stopMotors();
  } else {
    printToScreen("Nieznana komenda:", command);
    stopMotors();
  }
}

// --- SOCKET.IO ZDARZENIA ---

void socketIOEvent(socketIOmessageType_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case sIOtype_DISCONNECT:
      Serial.println("[SIO] Rozlaczono!");
      printToScreen("Socket.IO:", "Rozlaczono");
      stopMotors();
      break;
    case sIOtype_CONNECT:
      Serial.printf("[SIO] Polaczono do: %s\n", payload);
      socketIO.send(sIOtype_CONNECT, "/");
      printToScreen("Socket.IO:", "Polaczono!");
      break;
    case sIOtype_EVENT: {
      String msg = (char*)payload;
      DynamicJsonDocument doc(1024);
      DeserializationError error = deserializeJson(doc, payload);
      
      if (error) {
        Serial.print(F("Usterka deserializeJson(): "));
        Serial.println(error.c_str());
        return;
      }
      
      // event name z Socket.IO
      String eventName = doc[0];
      if (eventName == "robot_command") {
        String command = doc[1]["command"] | "stop";
        int value = doc[1]["value"] | 0;
        
        Serial.print("Odebrano komende: "); Serial.println(command);
        executeCommand(command, value);
      }
      break;
    }
  }
}

void setup() {
  Serial.begin(115200);
  
  setupDisplay();
  setupMotors();
  
  WiFi.begin(ssid, password);
  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi polaczone!");
  Serial.print("IP: "); Serial.println(WiFi.localIP());
  
  printToScreen("WiFi OK", WiFi.localIP().toString());
  
  // Polacz z serwerem Flask SocketIO (uzywajac protokolu w wersji 4)
  socketIO.begin(server_ip, server_port, "/socket.io/?EIO=4");
  socketIO.onEvent(socketIOEvent);
}

void loop() {
  socketIO.loop();
  
  // Autostop po danym czasie, zeby robot nie jechal bez konca
  if (isMoving && (millis() - movementTimer >= movementDuration)) {
    stopMotors();
  }
}
