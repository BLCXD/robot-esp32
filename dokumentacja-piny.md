### ESP32
- `VIN` - `VCC` [[Dokumentacja - piny#Ekranik|Ekranik]] __&__ `5V` [[Dokumentacja - piny#Mostek L298N|Mostek L298N]]
- `GND` - `GND` [[Dokumentacja - piny#Ekranik|Ekranik]] __&__ `GND` [[Dokumentacja - piny#Akumulator|Akumulator]] __&__ `GND` [[Dokumentacja - piny#ESP32|ESP32]]
- `D14` - `SCL` [[Dokumentacja - piny#Ekranik|Ekranik]]
- `D27` - `SDA` [[Dokumentacja - piny#Ekranik|Ekranik]]
### Silniki (autko)
##### Silnik przedni (skręcanie)
- `PIN1` - `OUT3` [[Dokumentacja - piny#Mostek L298N|Mostek L298N]]
- `PIN2` - `OUT4` [[Dokumentacja - piny#Mostek L298N|Mostek L298N]]
##### Silnik tylny (napęd)
- `PIN1` - `OUT1` [[Dokumentacja - piny#Mostek L298N|Mostek L298N]]
- `PIN2` - `OUT2` [[Dokumentacja - piny#Mostek L298N|Mostek L298N]]
### Mostek L298N
- `12V` - `VCC` [[Dokumentacja - piny#Akumulator|Akumulator]]
- `5V` - `VIN` [[Dokumentacja - piny#ESP32|ESP32]] __&__ `VCC` [[Dokumentacja - piny#Ekranik|Ekranik]]
- `GND` - `GND` [[Dokumentacja - piny#Akumulator|Akumulator]] __&__ `GND` [[Dokumentacja - piny#ESP32|ESP32]] __&__ `GND` [[Dokumentacja - piny#Ekranik|Ekranik]]
- `OUT1` - `PIN1` [[Dokumentacja - piny#Silnik tylny (napęd)|Silnik tylny (napęd)]]
- `OUT2` - `PIN2` [[Dokumentacja - piny#Silnik tylny (napęd)|Silnik tylny (napęd)]]
- `OUT3` - `PIN1` [[Dokumentacja - piny#Silnik przedni (skręcanie)|Silnik przedni (skręcanie)]]
- `OUT4` - `PIN2` [[Dokumentacja - piny#Silnik przedni (skręcanie)|Silnik przedni (skręcanie)]]
- `ENA` - `D13` [[Dokumentacja - piny#ESP32|ESP32]]
- `IN1` - `D12` [[Dokumentacja - piny#ESP32|ESP32]]
- `IN2` - `D26` [[Dokumentacja - piny#ESP32|ESP32]]
- `ENB` - `D25` [[Dokumentacja - piny#ESP32|ESP32]]
- `IN3` - `D33` [[Dokumentacja - piny#ESP32|ESP32]]
- `IN4` - `D32` [[Dokumentacja - piny#ESP32|ESP32]]
### Akumulator
- `VCC` - `12V` [[Dokumentacja - piny#Mostek L298N|Mostek L298N]]
- `GND` - `GND` [[Dokumentacja - piny#ESP32|ESP32]] __&__ `GND` [[Dokumentacja - piny#Mostek L298N|Mostek L298N]] __&__ `GND` [[Dokumentacja - piny#Ekranik|Ekranik]]
### Ekranik
- `VCC` - `VIN` [[Dokumentacja - piny#ESP32|ESP32]] __&__ `5V` [[Dokumentacja - piny#Mostek L298N|Mostek L298N]]
- `GND` - `GND` [[Dokumentacja - piny#ESP32|ESP32]]
- `SCL` - `D14` [[Dokumentacja - piny#ESP32|ESP32]]
- `SDA` - `D27` [[Dokumentacja - piny#ESP32|ESP32]]
