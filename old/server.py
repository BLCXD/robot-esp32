

import os
import re
import json
import base64
import threading
from io import BytesIO
from flask import Flask, render_template, request, jsonify
from flask_sock import Sock
from gtts import gTTS
from dotenv import load_dotenv

load_dotenv()

# ──────────────────────────────
# Konfiguracja z .env
# ──────────────────────────────
AI_PROVIDER = os.getenv("AI_PROVIDER", "anthropic").lower()
AI_MODEL    = os.getenv("AI_MODEL", "claude-opus-4-20250514")
SERVER_HOST = os.getenv("SERVER_HOST", "0.0.0.0")
SERVER_PORT = int(os.getenv("SERVER_PORT", 5000))
TTS_LANG    = os.getenv("TTS_LANG", "pl")
NVIDIA_API_KEY = os.getenv("NVIDIA_API_KEY")

# ──────────────────────────────
# Inicjalizacja klienta AI
# ──────────────────────────────
def build_client():
    if AI_PROVIDER == "anthropic":
        import anthropic
        key = os.getenv("ANTHROPIC_API_KEY")
        if not key:
            raise ValueError("Brak ANTHROPIC_API_KEY w .env")
        return anthropic.Anthropic(api_key=key)

    elif AI_PROVIDER in ("openai", "groq", "ollama", "nvidia"):
        import openai
        if AI_PROVIDER == "openai":
            key = os.getenv("OPENAI_API_KEY")
            if not key:
                raise ValueError("Brak OPENAI_API_KEY w .env")
            return openai.OpenAI(api_key=key)
        elif AI_PROVIDER == "groq":
            key = os.getenv("GROQ_API_KEY")
            if not key:
                raise ValueError("Brak GROQ_API_KEY w .env")
            return openai.OpenAI(
                api_key=key,
                base_url="https://api.groq.com/openai/v1"
            )
        elif AI_PROVIDER == "ollama":
            base_url = os.getenv("OLLAMA_BASE_URL", "http://localhost:11434/v1")
            return openai.OpenAI(api_key="ollama", base_url=base_url)
        elif AI_PROVIDER == "nvidia":
            key = os.getenv("NVIDIA_API_KEY")
            if not key:
                raise ValueError("Brak NVIDIA_API_KEY w .env")
            return openai.OpenAI(
                api_key=key,
                base_url="https://integrate.api.nvidia.com/v1"
            )
    else:
        raise ValueError(f"Nieznany AI_PROVIDER: '{AI_PROVIDER}'. Opcje: anthropic | openai | groq | ollama | nvidia")

client = build_client()
print(f"AI: {AI_PROVIDER.upper()} | Model: {AI_MODEL}")

# ──────────────────────────────
# Flask
# ──────────────────────────────
app  = Flask(__name__)
sock = Sock(app)

esp32_clients = set()
web_clients   = set()

SYSTEM_PROMPT = """Jestes robotem-asystentem AI na kolkach. Odpowiadasz po polsku, zwiezle (max 2-3 zdania).

Jesli uzytkownik prosi Cie o ruch lub nawigacje, na koncu odpowiedzi dodaj JSON z komendami:
[MOVE:{"commands":[{"action":"FORWARD","duration":1000},{"action":"STOP","duration":0}]}]

Dostepne akcje: FORWARD, BACKWARD, LEFT, RIGHT, STOP
duration = czas w milisekundach (0 = natychmiastowe)

Jesli pytanie nie dotyczy ruchu, nie dodawaj JSON.
"""

# ──────────────────────────────
# Wywolanie AI (obsluguje oba API)
# ──────────────────────────────
def call_ai(user_message: str) -> str:
    if AI_PROVIDER == "anthropic":
        import anthropic
        response = client.messages.create(
            model=AI_MODEL,
            max_tokens=512,
            system=SYSTEM_PROMPT,
            messages=[{"role": "user", "content": user_message}]
        )
        return response.content[0].text
    else:
        # OpenAI-compatible (openai / groq / ollama)
        response = client.chat.completions.create(
            model=AI_MODEL,
            max_tokens=512,
            messages=[
                {"role": "system", "content": SYSTEM_PROMPT},
                {"role": "user",   "content": user_message}
            ]
        )
        return response.choices[0].message.content

# ──────────────────────────────
# Helpers
# ──────────────────────────────
def text_to_audio_base64(text: str) -> str:
    tts = gTTS(text=text, lang=TTS_LANG, slow=False)
    buf = BytesIO()
    tts.write_to_fp(buf)
    buf.seek(0)
    return base64.b64encode(buf.read()).decode("utf-8")

def parse_move_commands(response_text: str):
    match = re.search(r'\[MOVE:(.*?)\]', response_text, re.DOTALL)
    if match:
        try:
            commands = json.loads(match.group(1))
            clean    = response_text[:match.start()].strip()
            return clean, commands
        except json.JSONDecodeError:
            pass
    return response_text, None

def send_to_esp32(data: dict):
    msg  = json.dumps(data)
    dead = set()
    for ws in esp32_clients:
        try:
            ws.send(msg)
        except Exception:
            dead.add(ws)
    esp32_clients -= dead

def send_to_web(data: dict):
    msg  = json.dumps(data)
    dead = set()
    for ws in web_clients:
        try:
            ws.send(msg)
        except Exception:
            dead.add(ws)
    web_clients -= dead

# ──────────────────────────────
# Endpointy
# ──────────────────────────────
@app.route("/")
def index():
    return render_template("index.html")

@app.route("/ask", methods=["POST"])
def ask():
    data         = request.json
    user_message = data.get("message", "").strip()
    if not user_message:
        return jsonify({"error": "Brak wiadomosci"}), 400

    def process():
        try:
            full_response             = call_ai(user_message)
            clean_text, move_commands = parse_move_commands(full_response)

            audio_b64 = text_to_audio_base64(clean_text)
            send_to_esp32({"type": "audio", "data": audio_b64})

            if move_commands:
                send_to_esp32({"type": "move_sequence", "data": move_commands})

            send_to_web({
                "type":         "ai_response",
                "text":         clean_text,
                "has_movement": move_commands is not None
            })
        except Exception as e:
            send_to_web({"type": "error", "text": f"Blad: {str(e)}"})

    threading.Thread(target=process, daemon=True).start()
    return jsonify({"status": "processing"})

@app.route("/control", methods=["POST"])
def control():
    data     = request.json
    action   = data.get("action", "STOP").upper()
    duration = data.get("duration", 0)
    if action not in ["FORWARD", "BACKWARD", "LEFT", "RIGHT", "STOP"]:
        return jsonify({"error": "Nieprawidlowa komenda"}), 400
    send_to_esp32({"type": "move", "action": action, "duration": duration})
    return jsonify({"status": "ok", "action": action})

@sock.route("/ws/web")
def websocket_web(ws):
    web_clients.add(ws)
    try:
        while True:
            if ws.receive() is None:
                break
    except Exception:
        pass
    finally:
        web_clients.discard(ws)

@sock.route("/ws/esp32")
def websocket_esp32(ws):
    esp32_clients.add(ws)
    print(f"ESP32 polaczony! Aktywne: {len(esp32_clients)}")
    try:
        while True:
            data = ws.receive()
            if data is None:
                break
            try:
                msg = json.loads(data)
                print(f"ESP32: {msg}")
                send_to_web({"type": "esp32_status", "data": msg})
            except Exception:
                pass
    except Exception:
        pass
    finally:
        esp32_clients.discard(ws)
        print(f"ESP32 rozlaczony. Aktywne: {len(esp32_clients)}")

if __name__ == "__main__":
    print(f"Serwer: http://{SERVER_HOST}:{SERVER_PORT}")
    app.run(host=SERVER_HOST, port=SERVER_PORT, debug=False)