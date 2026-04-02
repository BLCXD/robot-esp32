# Robot AI Assistant

## Struktura plików

```
robot/
├── server.py                  ← Serwer Python (uruchamiasz na komputerze)
├── templates/
│   └── index.html             ← Panel webowy (Flask serwuje automatycznie)
└── robot_esp32/
    └── robot_esp32.ino        ← Kod dla ESP32 (Arduino IDE)
```

---

## 1. Instalacja — Serwer Python

```bash
pip install flask flask-sock anthropic gtts
```

Ustaw klucz API Anthropic:
```bash
# Linux/Mac
export ANTHROPIC_API_KEY=sk-ant-...

# Windows (CMD)
set ANTHROPIC_API_KEY=sk-ant-...
```

Uruchom serwer:
```bash
python server.py
```

Sprawdź swoje IP (potrzebujesz go dla ESP32):
```bash
# Linux/Mac
ip addr | grep 192.168

# Windows
ipconfig
```

Otwórz przeglądarkę: **http://localhost:5000**

---

## 2. Instalacja — ESP32 (Arduino IDE)

### Biblioteki (Tools → Manage Libraries):
- `ArduinoWebsockets` by Gil Maimon
- `ESP8266Audio` by Earle F. Philhower III
- `ArduinoJson` by Benoit Blanchon

### Konfiguracja w pliku .ino:
```cpp
const char* WIFI_SSID     = "NAZWA_TWOJEJ_SIECI";
const char* WIFI_PASSWORD = "HASLO_WIFI";
const char* SERVER_IP     = "192.168.X.X";  // IP komputera
```

### Ustawienia Arduino IDE:
- Board: **ESP32 Dev Module**
- Upload Speed: 921600
- Flash Size: 4MB

---

## 3. Podłączenia sprzętowe

### Mostek H (L298N) → ESP32:
| L298N | ESP32  |
|-------|--------|
| IN1   | GPIO 25|
| IN2   | GPIO 26|
| IN3   | GPIO 27|
| IN4   | GPIO 14|
| ENA   | GPIO 32|
| ENB   | GPIO 33|
| GND   | GND    |
| +5V   | osobne zasilanie (baterie do silników) |

### DAC I2S (MAX98357A) → ESP32:
| MAX98357A | ESP32  |
|-----------|--------|
| BCLK      | GPIO 22|
| LRC       | GPIO 21|
| DIN       | GPIO 23|
| GND       | GND    |
| VIN       | 3.3V   |

Głośnik (4Ω lub 8Ω, min 2W) → zaciski OUT+ OUT− MAX98357A

---

## 4. Użytkowanie

### Panel webowy (http://IP_KOMPUTERA:5000):
- **Pole tekstowe** → pytania do AI lub komendy ruchu (np. "jedź do przodu")
- **Strzałki** → manualne sterowanie (trzymaj wciśnięty przycisk)
- **Klawiatura** → WASD lub strzałki (gdy focus poza polem tekstowym)
- **Spacja** → STOP

### AI rozumie komendy takie jak:
- "Jedź do przodu przez 3 sekundy"
- "Obróć się w lewo o 90 stopni"
- "Podjedź do przodu, zatrzymaj się, powiedz mi co widzisz"
- "Kim jesteś?" (bez ruchu, tylko odpowiedź głosowa)

---

## 5. Troubleshooting

**ESP32 nie łączy się z serwerem:**
- Sprawdź czy komputer i ESP32 są w tej samej sieci WiFi
- Sprawdź IP w Serial Monitor (Arduino IDE → Tools → Serial Monitor, 115200 baud)
- Upewnij się że firewall nie blokuje portu 5000

**Brak dźwięku:**
- Sprawdź podłączenie MAX98357A (szczególnie BCLK/LRC/DIN)
- Głośnik musi mieć min. 1W, 4-8Ω
- Sprawdź `i2s->SetGain(0.8)` — możesz zwiększyć do 1.0

**Silniki nie działają:**
- Sprawdź zasilanie L298N (osobne od ESP32!)
- Sprawdź piny IN1-IN4 i ENA/ENB
- Przetestuj w Serial Monitor czy komenda dociera do ESP32
