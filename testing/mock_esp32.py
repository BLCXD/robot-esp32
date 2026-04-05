import socketio
import time

sio = socketio.Client(logger=False, engineio_logger=False)

@sio.event
def connect():
    print('=============================================')
    print('[ESP32 MOCK] Połączono z serwerem Flask Socket.IO!')
    print('[ESP32 MOCK] Symulator oczekuje na komendy ruchu...')
    print('=============================================\n')

@sio.event
def disconnect():
    print('[ESP32 MOCK] Rozłączono z serwerem.')

@sio.on('robot_command')
def on_robot_command(data):
    print(f"---> [Wiadomość z serwera]: Otrzymano {data}")
    command = data.get('command', 'stop')
    value = data.get('value', 0)
    
    # Symulacja ekraniku OLED
    print(f" [Ekran OLED] Kierunek: {command.upper()} | Wartość: {value}")
    
    # Symulacja podwozia L298N
    if command in ['przód', 'przod']:
        print(" [L298N] Włączanie silnika tył (prędkość pełna do przodu). Oś skrętu wyśrodkowana.")
    elif command in ['tył', 'tyl']:
        print(" [L298N] Włączanie silnika tył (prędkość pełna do tyłu). Oś skrętu wyśrodkowana.")
    elif command == 'lewo':
        print(" [L298N] Skręt osi przedniej w lewo. Krótki ruch silnika tył do przodu.")
    elif command == 'prawo':
        print(" [L298N] Skręt osi przedniej w prawo. Krótki ruch silnika tył do przodu.")
    elif command in ['stop', 'zatrzymaj']:
        print(" [L298N] Zatrzymywanie wszystkich silników.")
    else:
        print(f" [L298N] Nieznana komenda: {command}")
        
    print("---> [ESP32 MOCK] Wykonano. Czekam na kolejne rozkazy...\n")

if __name__ == '__main__':
    print("Uruchamianie emulatora ESP32 (Python)...")
    try:
        # Próba połączenia na port serwera Flask
        sio.connect('http://localhost:5000')
        sio.wait()
    except Exception as e:
        print("Błąd połączenia. Upewnij się, że 'server.py' jest najpierw uruchomiony na porcie 5000.")
        print(f"Treść błędu: {e}")
