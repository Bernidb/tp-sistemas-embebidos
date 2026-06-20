#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <LiquidCrystal_I2C.h>
#include <ArduinoJson.h>
#include <esp_wifi.h> 

const char* ssid = "";          // <-- REVISAR TU WIFI
const char* password = ""; // <-- REVISAR TU CONTRASEÑA
const char* mqtt_server = ""; // <-- PONER LA IP DEL COMANDO 'hostname -I'

#define DHTPIN 4       
#define DHTTYPE DHT11
#define PIN_LED 15

const int rowPins[4] = {13, 12, 14, 27}; 
const int colPins[4] = {26, 25, 33, 32}; 
const char keys[4][4] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};

DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal_I2C lcd(0x27, 16, 2); 
WiFiClient espClient;
PubSubClient client(espClient);

unsigned long tiempoAnterior = 0;
const long intervalo = 5000;

float limiteTemp = 28.0; 
String inputTeclado = ""; 
bool modoConfig = false; 

// Variables de control avanzado
bool modoAuto = true;        
bool releActivo = false;     
float tempFiltrada = 0.0;    
bool primeraLectura = true;
bool forzarDibujo = true; 

void setup_wifi() {
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(500); 
  
  // <-- LÍNEA AGREGADA: Activa el ahorro de energía del módem Wi-Fi
  esp_wifi_set_ps(WIFI_PS_MIN_MODEM); 
}

// <-- FUNCIÓN AGREGADA: Escucha los mensajes que llegan desde el Dashboard
void callback(char* topic, byte* payload, unsigned int length) {
  String mensaje = "";
  for (int i = 0; i < length; i++) {
    mensaje += (char)payload[i];
  }
  
  // Convertimos el mensaje recibido a número y pisamos el límite actual
  limiteTemp = mensaje.toFloat();
  
  // Forzamos a que la pantalla se actualice al instante para mostrar el nuevo límite
  forzarDibujo = true; 
  Serial.println("Nuevo limite recibido desde la web: " + String(limiteTemp));
}

void reconnect() {
  while (!client.connected()) {
    if (client.connect("ESP32_Berni_SistEmb")) {
      // <-- LÍNEA AGREGADA: Suscripción al tópico de control bidireccional
      client.subscribe("facultad/lab_berni/control"); 
      break;
    }
    delay(5000);
  }
}

char leerTeclado() {
  for (int r = 0; r < 4; r++) {
    digitalWrite(rowPins[r], HIGH); 
    for (int c = 0; c < 4; c++) {
      if (digitalRead(colPins[c]) == HIGH) { 
        delay(50); 
        while(digitalRead(colPins[c]) == HIGH); 
        digitalWrite(rowPins[r], LOW);
        return keys[r][c];
      }
    }
    digitalWrite(rowPins[r], LOW); 
  }
  return '\0'; 
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, LOW);
  dht.begin();
  
  for (int i = 0; i < 4; i++) {
    pinMode(rowPins[i], OUTPUT);
    digitalWrite(rowPins[i], LOW);
    pinMode(colPins[i], INPUT_PULLDOWN); 
  }

  lcd.init();      
  lcd.backlight(); 
  lcd.print("Iniciando...");
  setup_wifi();
  
  client.setServer(mqtt_server, 1883);
  // <-- LÍNEA AGREGADA: Vincula el cliente MQTT con la función receptora
  client.setCallback(callback); 
  
  lcd.clear();
}

void loop() {
  if (!client.connected()) reconnect();
  client.loop();

  bool actualizarPantalla = forzarDibujo; 
  forzarDibujo = false; 
  unsigned long tiempoActual = millis();

  // 1. LECTURA Y FILTRO EMA (Cada 5s)
  if (tiempoActual - tiempoAnterior >= intervalo) {
    tiempoAnterior = tiempoActual;
    float t_cruda = dht.readTemperature(); 
    float h = dht.readHumidity();
    
    if (!isnan(t_cruda) && !isnan(h)) {
      if (primeraLectura) {
        tempFiltrada = t_cruda;
        primeraLectura = false;
      } else {
        tempFiltrada = (0.3 * t_cruda) + (0.7 * tempFiltrada); 
      }

      // Lógica de Histéresis
      if (modoAuto) {
        if (tempFiltrada >= limiteTemp) {
          releActivo = true;
        } else if (tempFiltrada <= (limiteTemp - 1.0)) {
          releActivo = false;
        }
      }
      digitalWrite(PIN_LED, releActivo ? HIGH : LOW);
      actualizarPantalla = true; 

      // Armado y envío del JSON
      StaticJsonDocument<200> doc;
      doc["temp"] = tempFiltrada; 
      doc["hum"] = h;
      doc["limite"] = limiteTemp; 
      doc["rele"] = releActivo ? (modoAuto ? "ON (Auto)" : "ON (Man)") : (modoAuto ? "OFF (Auto)" : "OFF (Man)"); 

      char buffer[256];
      serializeJson(doc, buffer);
      client.publish("facultad/lab_berni/telemetria", buffer); 
    }
  }

  // 2. TECLADO Y MENÚ HMI
  char tecla = leerTeclado();
  if (tecla != '\0') {
    actualizarPantalla = true; 
    
    if (!modoConfig) {
      if (tecla == '#') { modoConfig = true; inputTeclado = ""; lcd.clear(); }
      else if (tecla == 'A') { modoAuto = true; } 
      else if (tecla == 'B') { modoAuto = false; } 
      else if (tecla == 'C' && !modoAuto) { 
        releActivo = !releActivo; 
        digitalWrite(PIN_LED, releActivo ? HIGH : LOW);
      }
    } else {
      if (tecla >= '0' && tecla <= '9') { inputTeclado += tecla; } 
      else if (tecla == '#') {
        if (inputTeclado.length() > 0) limiteTemp = inputTeclado.toFloat(); 
        modoConfig = false; inputTeclado = ""; 
        lcd.clear(); lcd.print("Lim. Guardado!"); delay(1000); lcd.clear(); forzarDibujo = true;
      } 
      else if (tecla == '*') {
        modoConfig = false; inputTeclado = ""; 
        lcd.clear(); lcd.print("Cancelado"); delay(1000); lcd.clear(); forzarDibujo = true;
      }
    }
  }

  // 3. ACTUALIZACIÓN LCD
  if (actualizarPantalla) {
    if (!modoConfig) {
      lcd.setCursor(0, 0);
      lcd.print("T:" + String(tempFiltrada, 1) + " L:" + String(limiteTemp, 0) + "  "); 
      lcd.setCursor(0, 1);
      String modoStr = modoAuto ? "AUTO" : "MAN ";
      String estadoStr = releActivo ? "ON " : "OFF";
      lcd.print(modoStr + " -> " + estadoStr + "    "); 
    } else {
      lcd.setCursor(0, 0); lcd.print("Ingrese limite: "); 
      lcd.setCursor(0, 1); lcd.print("> " + inputTeclado + "             "); 
    }
  }
}