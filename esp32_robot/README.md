# ESP32 Robot Node

Katalog ten zawiera oprogramowanie dla ESP32, który obsługuje łączność z robotem, interpretuje komendy z serwera po Socket.IO i przekazuje je na koła (L298N). Kod przygotowano zgodnie z wymogami środowiska Arduino IDE.

## Zależności / Biblioteki

Aby poprawnie skompilować `esp32_robot.ino`, musisz zainstalować z poziomu **Menedżera Bibliotek** (Arduino IDE -> Szkic -> Dołącz bibliotekę -> Zarządzaj bibliotekami) następujące pozycje:

1. **WebSockets by Links2004** – biblioteka komunikacyjna dla Socket.IO
2. **ArduinoJson by Benoit Blanchon** (starsza wersja v6 lub najnowsza kompatybilna, w kodzie użyto `DynamicJsonDocument` znanego z gałęzi 6.x)
3. **Adafruit SSD1306** (wraz z jego wymaganym wsparciem czyli **Adafruit GFX Library** i **Adafruit BusIO**) – do sterowania ekranikiem OLED.

## Konfiguracja
1. Otwórz `esp32_robot.ino`
2. W sekcji `KONFIGURACJA SIECI` uzupełnij z czym ma się połączyć ESP32:
   - `ssid` - Twoja nazwa WIFI
   - `password` - Hasło
   - `server_ip` - Lokalne IP Twojego komputera, na którym odpalasz `server.py` dla Flask WebSockets. Podłączenie do np. `192.168...` (nie używaj `localhost` - ESP musi znać adres fizyczny!).

## Wgrywanie i debugowanie
Program kompiluj wybierając w Arduino IDE płytkę `DOIT ESP32 DEVKIT V1` lub inną z serii WROOM zależnie od precyzyjnego modułu. Po wgraniu otwórz **Monitor portu szeregowego (Serial Monitor)** używając szybkości *115200 baud* aby ujrzeć dokładne logi z podłączania do WiFi i ewentualnych odbieranych wiadomości.

Po uruchomieniu ekran Oled wskaże też parametry i aktualnie odebraną komendę ruchu.
