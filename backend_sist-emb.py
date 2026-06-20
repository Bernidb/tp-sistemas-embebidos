import paho.mqtt.client as mqtt
import sqlite3
import json
from datetime import datetime

def init_db():
    conn = sqlite3.connect('data/telemetria_sistemb.db')
    cursor = conn.cursor()
    cursor.execute('''CREATE TABLE IF NOT EXISTS datos (
                        id INTEGER PRIMARY KEY AUTOINCREMENT,
                        fecha TEXT,
                        aula TEXT,
                        temp REAL,
                        hum REAL,
                        limite REAL,
                        rele TEXT)''')
    conn.commit()
    conn.close()

def on_message(client, userdata, msg):
    try:
        payload = msg.payload.decode('utf-8')
        data = json.loads(payload)
        partes_topico = msg.topic.split('/')
        aula = partes_topico[1] if len(partes_topico) > 1 else "desconocido"
        ahora = datetime.now().strftime('%Y-%m-%d %H:%M:%S')

        print(f"[{ahora}] Datos recibidos: {data}")

        conn = sqlite3.connect('data/telemetria_sistemb.db')
        cursor = conn.cursor()
        cursor.execute("INSERT INTO datos (fecha, aula, temp, hum, limite, rele) VALUES (?, ?, ?, ?, ?, ?)",
                       (ahora, aula, data.get('temp'), data.get('hum'), data.get('limite'), data.get('rele')))
        conn.commit()
        conn.close()
    except Exception as e:
        print(f"Error procesando mensaje: {e}")

init_db()
client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
client.on_message = on_message
client.connect("localhost", 1883, 60)
client.subscribe("facultad/+/telemetria")

print("Servidor escuchando y grabando... (Ctrl+C para salir)")
client.loop_forever()