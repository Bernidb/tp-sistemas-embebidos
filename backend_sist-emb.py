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

# Se ejecuta CADA VEZ que se conecta al broker
def on_connect(client, userdata, flags, reason_code, properties=None):
    print("Conectado al broker MQTT. Suscribiendo al tópico...")
    client.subscribe("facultad/lab_berni/telemetria")

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

# Mantengo la inicialización con CallbackAPIVersion que venías usando
client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
client.on_connect = on_connect  # Asignamos el evento de reconexión
client.on_message = on_message

client.connect("127.0.0.1", 1883, 60)
client.loop_forever()