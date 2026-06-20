import streamlit as st
import sqlite3
import pandas as pd
import time
from datetime import datetime
import paho.mqtt.publish as publish # <-- LIBRERÍA AGREGADA PARA ENVIAR ÓRDENES

st.set_page_config(page_title="Dashboard Sistemas Embebidos", layout="wide")
st.title("Monitoreo de Telemetría:")

def load_data():
    conn = sqlite3.connect('data/telemetria_sistemb.db')
    df = pd.read_sql_query("SELECT * FROM datos ORDER BY id DESC LIMIT 50", conn)
    conn.close()
    return df

st.sidebar.header("Configuración")
refresh_rate = st.sidebar.slider("Frecuencia de actualización (seg)", 1, 10, 5)

try:
    df = load_data()
    if not df.empty:
        ultima_lectura = df.iloc[0]
        
        # --- LÓGICA DE HEARTBEAT (Watchdog) ---
        formato = '%Y-%m-%d %H:%M:%S'
        ultima_fecha = datetime.strptime(ultima_lectura['fecha'], formato)
        ahora = datetime.now()
        diferencia_segundos = (ahora - ultima_fecha).total_seconds()
        
        estado_red = "OFFLINE" if diferencia_segundos > 15 else "ONLINE"
        estado_rele = f" {ultima_lectura['rele']}" 
        
        col1, col2, col3, col4 = st.columns(4)
        with col1:
            st.metric("Temperatura", f"{ultima_lectura['temp']:.2f} °C")
        with col2:
            st.metric("Límite Seteado", f"{ultima_lectura['limite']:.2f} °C")
        with col3:
            st.metric("Estado Relé", estado_rele)
        with col4:
            st.metric("Conexión ESP32", estado_red)

        st.subheader("Histórico de Temperatura")
        df['fecha'] = pd.to_datetime(df['fecha'])
        st.line_chart(df.set_index('fecha')['temp'])

        st.subheader("Últimos Registros")
        df_mostrar = df.copy()
        df_mostrar['temp'] = df_mostrar['temp'].round(2)
        st.dataframe(df_mostrar.head(5), use_container_width=True)

        # --- NUEVO: PANEL DE CONTROL BIDIRECCIONAL ---
        st.markdown("---")
        st.subheader("Panel de Control Remoto")
        st.write("Modificá el límite de temperatura de la ESP32 desde acá:")
        
        # Selector numérico que arranca con el valor del límite actual
        nuevo_limite = st.number_input("Nuevo Límite (°C)", value=float(ultima_lectura['limite']), step=1.0)
        
        if st.button("Enviar Orden a ESP32"):
            try:
                # Publica el nuevo número en el tópico de control usando el broker local
                publish.single("facultad/lab_berni/control", payload=str(nuevo_limite), hostname="127.0.0.1")
                st.success(f"¡Orden enviada exitosamente! El límite ahora es {nuevo_limite}°C.")
            except Exception as e:
                st.error(f"Error al enviar la orden: {e}")

    else:
        st.info("Esperando los primeros datos del ESP32...")
except Exception as e:
    st.error(f"Error general: {e}")

time.sleep(refresh_rate)
st.rerun()