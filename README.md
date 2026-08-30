# ❄️ Sistema Integral IoT de Telemetría - Heladera Industrial (IC_IV)

Proyecto completo e integrado de adquisición, transporte, procesamiento y visualización de telemetría para refrigeración industrial.

---

## 🏗️ Arquitectura General del Sistema

```
[ Sensor DS18B20 + RTC ]
         │ (1-Wire / I2C)
         ▼
[ Nodo XIAO ESP32-S3 ] ────(LoRa 915 MHz SX1262)────► [ Gateway LoRa / Broker ]
                                                               │ (MQTT: 1883)
                                                               ▼
                                                  [ Docker: Eclipse Mosquitto ]
                                                               │
                                  ┌────────────────────────────┴───────────────────────────┐
                                  │ (MQTT Protocol)                                        │ (WebSockets: 9001)
                                  ▼                                                        ▼
                    [ Backend Node.js / Express ] ────(Socket.IO)────► [ Dashboard Web Industrial ]
                                  │                                    (HTML / CSS Grid / JS)
                                  ▼
                     [ Base de Datos MongoDB ]
                         (Docker: 27017)
```

---

## 📁 Estructura del Repositorio

```text
fridge-telemetry-dashboard/
├── docker-compose.yml              # Orquestación de Broker Mosquitto + Base de Datos MongoDB
├── mosquitto/
│   ├── config/
│   │   └── mosquitto.conf          # Configuración de listeners TCP (1883) y WebSockets (9001)
│   ├── data/                       # Persistencia de mensajes MQTT
│   └── log/                        # Logs del broker
├── frontend/
│   └── index.html                  # Dashboard industrial monocomponente (CSS Grid, Dark Theme)
├── index.html                      # Acceso directo al Dashboard para doble clic
├── software/
│   └── backend/
│       ├── package.json            # Dependencias (Express 5, Mongoose, MQTT, Socket.IO)
│       ├── .env                    # Configuración de entorno local
│       ├── .env.example            # Plantilla de variables de entorno
│       └── src/
│           ├── app.js              # Configuración de Express
│           ├── server.js           # Servidor HTTP + Socket.IO + Conexión DB y MQTT
│           ├── config/
│           │   ├── config.js       # Carga de variables de entorno
│           │   ├── db-connect-config.js  # Conexión Mongoose
│           │   └── mqtt/
│           │       ├── mqtt-config.js    # Conexión resiliente a Mosquitto
│           │       └── mqtt-topics.js    # Tópicos: iciv/#, iciv/value1/analog
│           └── models/
│               └── sensor-data-model.js  # Esquema SensorData (Mongoose)
├── firmware/
│   └── main/
│       ├── main.ino                # Código principal ESP32-S3 + RadioLib LoRa SX1262
│       ├── oled_rtc.cpp / .h       # Controlador de pantalla OLED y reloj RTC
│       └── temp_sensor.cpp / .h    # Controlador de sonda de temperatura DS18B20
└── README.md                       # Documentación técnica completa
```

---

## ⚠️ NOTA CRÍTICA DE HARDWARE PARA EL EQUIPO

> [!IMPORTANT]
> **CONFIGURACIÓN OBLIGATORIA DE BOTÓN FÍSICO (PULL-DOWN)**
> 
> Si se añade un botón/pulsador físico en la placa **XIAO ESP32-S3** para forzar lecturas o transmisiones manuales en campo, este **DEBE ESTAR CABLEADO INDEFECTIBLEMENTE EN CONFIGURACIÓN PULL-DOWN**.
>
> - **Justificación:** Los pines GPIO del ESP32-S3 en entornos industriales con compresores de heladeras son altamente vulnerables al ruido electromagnético. La configuración pull-down asegura el nivel lógico `GND (0V)` en reposo y un flanco de subida limpio (`RISING`) a `3.3V` al presionar.
>
> ```
>        +3.3V (VCC)
>            │
>          ┌─┴─┐
>          │ o │  Pulsador Físico (Normalmente Abierto)
>          │   │
>          └─┬─┘
>            ├───────────────────► GPIO ESP32-S3 (Lectura Digital)
>            │
>          ┌─┴─┐
>          │ R │  Resistencia Pull-Down (10 kΩ)
>          │   │
>          └─┬─┘
>            │
>           GND (0V)
> ```

---

## 🚀 Guía de Puesta en Marcha

### 1. Iniciar Infraestructura con Docker (Broker MQTT + MongoDB)
Desde la raíz del proyecto, ejecuta:

```bash
# Levantar Mosquitto y MongoDB en segundo plano
docker compose up -d

# Verificar estado de los contenedores
docker compose ps

# Ver logs del broker MQTT en tiempo real
docker compose logs -f mqtt-broker
```

### 2. Iniciar el Backend (Node.js)
Accede a la carpeta de software e instala las dependencias:

```bash
cd software/backend

# Instalar paquetes
npm install

# Iniciar servidor
npm start
```
El backend se conectará automáticamente a:
* **MongoDB:** `mongodb://localhost:27017/iciv_db`
* **Broker MQTT:** `mqtt://localhost:1883` (suscrito al tópico `iciv/#`)

### 3. Abrir el Dashboard Frontend
* **Opción directa:** Haz doble clic sobre `frontend/index.html` en tu explorador de archivos para abrirlo en el navegador.
* Cuenta con visualización de temperatura (°C), señal LoRa RSSI (dBm), timestamp RTC, botón de "Forzar lectura" y modo simulación automático si los servicios aún no están enviando datos.

---

## 🧪 Pruebas Rápidas y Validación de la API

Una vez levantados Docker y el Backend, puedes probar los endpoints desde tu terminal (PowerShell, Bash o cURL):

### 1. Probar inserción manual de telemetría (POST)
```bash
# Con cURL
curl -X POST http://localhost:3000/api/v1/telemetria \
  -H "Content-Type: application/json" \
  -d "{\"temperatura\": 4.1, \"rssi\": -80, \"deviceId\": \"Heladera1\"}"

# Con PowerShell
Invoke-RestMethod -Uri "http://localhost:3000/api/v1/telemetria" -Method POST -Headers @{"Content-Type"="application/json"} -Body '{"temperatura": 4.1, "rssi": -80}'
```

### 2. Consultar la última lectura (GET)
```bash
curl http://localhost:3000/api/v1/telemetria/latest
```

### 3. Consultar historial de telemetría (GET)
```bash
curl "http://localhost:3000/api/v1/telemetria/history?limit=10"
```

### 4. Probar forzado de lectura (POST)
```bash
curl -X POST http://localhost:3000/api/v1/leer
```

---

## 📡 Tópicos MQTT y Payloads

| Tópico | Dirección | Descripción / Payload |
| :--- | :--- | :--- |
| `iciv/#` | Suscripción | Tópico base de telemetría recibida del nodo LoRa (auto-persiste en MongoDB). |
| `iciv/value1/analog` | Publicación | Envío de comandos analógicos de control (`publishCommand('analog', valor)`). |

---

## 🛑 Detener Servicios Docker

```bash
docker compose down
```
*(Los datos de MongoDB y la configuración de Mosquitto se conservan en volúmenes persistentes).*
