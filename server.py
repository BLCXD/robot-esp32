import os
import json
from flask import Flask, render_template, request, jsonify
from flask_socketio import SocketIO, emit
from dotenv import load_dotenv
from openai import OpenAI

load_dotenv()

app = Flask(__name__)
app.config['SECRET_KEY'] = os.environ.get('FLASK_SECRET_KEY', 'default_secret')
# Pozwalamy na logowanie różnych klientów WebSockets 
socketio = SocketIO(app, cors_allowed_origins="*")

import httpx

# Inicjalizacja klienta OpenAI zgodnego z NVIDIA API
try:
    nvidia_client = OpenAI(
        base_url="https://integrate.api.nvidia.com/v1",
        api_key=os.getenv("NVIDIA_API_KEY", "dummy_key"),
        http_client=httpx.Client()
    )
except Exception as e:
    print("Błąd inicjalizacji NVIDIA API:", e)
    nvidia_client = None

@app.route('/')
def index():
    return render_template('index.html')

@app.route('/test-api')
def test_api():
    return render_template('test_api.html')

@app.route('/api/test-nvapi', methods=['POST'])
def handle_test_api():
    if not nvidia_client:
        return jsonify({"error": "Błąd klienta - brak nvidia_client."}), 500
    try:
        data = request.get_json()
        user_prompt = data.get("prompt", "Hej!")
        
        response = nvidia_client.chat.completions.create(
            model="meta/llama-3.1-8b-instruct",
            messages=[{"role": "user", "content": user_prompt}],
            max_tokens=100
        )
        return jsonify({"response": response.choices[0].message.content})
    except Exception as e:
        return jsonify({"error": str(e)}), 500

@app.route('/api/prompt', methods=['POST'])
def handle_prompt():
    data = request.get_json()
    user_message = data.get('message', '')
    
    if not nvidia_client:
        return jsonify({"error": "Brak integracji z NVIDIA API."}), 500
        
    system_prompt = """
Jesteś sztuczną inteligencją sterującą robotem typu łazik.
Użytkownik podaje w naturalnym języku polecenia lub zadaje pytania.
Twoim zadaniem jest parsowanie intencji użytkownika. Zwracaj ZAWSZE odpowiedź w czystym formacie JSON bez dopisków i bez znaczników markdown.
Format dla pytań/luźnej rozmowy:
{
  "type": "chat",
  "response": "Treść twojej odpowiedz na pytanie użytkownika"
}
Format dla komend ruchu (rozpoznane kierunki: przód, tył, lewo, prawo):
{
  "type": "command",
  "direction": "przód",
  "value": 2,
  "response": "Komunikat potwierdzający np. Jadę do przodu 2 metry"
}
Jeśli zapytanie połączone jest z niezrozumiałym ruchem, zapytaj o doprecyzowanie jako 'chat'.
"""
    try:
        response = nvidia_client.chat.completions.create(
            model="meta/llama-3.1-8b-instruct",
            messages=[
                {"role": "system", "content": system_prompt},
                {"role": "user", "content": user_message}
            ],
            temperature=0.1,
            max_tokens=200
        )
        
        raw_content = response.choices[0].message.content.strip()
        # Wyczyszczenie jeśli model dodał backticki Markdown
        if raw_content.startswith("`" * 3):
            raw_content = raw_content[raw_content.find('{'):raw_content.rfind('}')+1]
            
        try:
            parsed = json.loads(raw_content)
        except json.JSONDecodeError:
            parsed = {"type": "chat", "response": raw_content}
            
        if parsed.get('type') == 'command':
            # Wysyłka komendy po WebSocket
            socketio.emit('robot_command', {
                'command': parsed.get('direction', 'przód'),
                'value': parsed.get('value', 0)
            })
            
        return jsonify(parsed)
        
    except Exception as e:
        return jsonify({"error": str(e)}), 500

@app.route('/api/move', methods=['POST'])
def handle_move():
    data = request.get_json()
    direction = data.get('direction', 'przód')
    
    # Wysyłka komendy po WebSocket, domyślnie 1 jednostka ruchu
    socketio.emit('robot_command', {
        'command': direction,
        'value': 1
    })
    
    return jsonify({"status": "ok", "direction": direction})

# Logika WebSockets
@socketio.on('connect')
def test_connect():
    print('Klient podłączony')
    emit('server_response', {'data': 'Połączono z serwerem Flask!'})

@socketio.on('disconnect')
def test_disconnect():
    print('Klient odłączony')

if __name__ == '__main__':
    print("Uruchamianie serwera na porcie 5000...")
    socketio.run(app, debug=True, host='0.0.0.0', port=5000, allow_unsafe_werkzeug=True)
