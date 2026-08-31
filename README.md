# ❄️ Sistema Integral IoT de Telemetría - Sistemas de refrigeración (IC_IV)

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

## 🧱 Estructura del repositorio

```
IC_IV/
 ├── firmware/
 │   └── main/                      # Firmware del nodo sensor (Arduino, XIAO ESP32-S3)
 │       ├── main.ino               # Lógica principal: lectura de sensor y transmisión LoRa
 │       ├── temp_sensor.cpp/.h     # Driver del sensor de temperatura DS18B20 (1-Wire)
 │       └── oled_rtc.cpp/.h        # Manejo de display OLED + módulo RTC (hora/fecha local)
 │
 ├── mosquitto/                     # Configuración y datos del broker MQTT (Eclipse Mosquitto)
 │   ├── config/
 │   │   └── mosquitto.conf         # Listeners MQTT (1883) y WebSocket (9001), persistencia
 │   ├── data/                      # Persistencia interna del broker (volumen Docker)
 │   └── log/                       # Logs del broker (volumen Docker)
 │
 ├── software/
 │   ├── backend/                   # API REST + WebSocket (Node.js / Express)
 │   │   ├── src/
 │   │   │   ├── app.js             # Configuración de la app Express (CORS, estáticos, rutas)
 │   │   │   ├── server.js          # Bootstrap del servidor HTTP + Socket.IO
 │   │   │   ├── config/
 │   │   │   │   ├── config.js              # Variables de entorno y configuración general
 │   │   │   │   ├── db-connect-config.js   # Conexión a MongoDB (Mongoose)
 │   │   │   │   ├── mqtt/
 │   │   │   │   │   ├── mqtt-config.js     # Cliente MQTT (conexión, credenciales, TLS)
 │   │   │   │   │   └── mqtt-topics.js     # Definición de tópicos MQTT del sistema
 │   │   │   │   └── mailer/
 │   │   │   │       └── mailer-config.js   # Configuración SMTP para envío de alertas
 │   │   │   ├── controller/        # Controladores HTTP (parseo de requests/responses)
 │   │   │   │   ├── sensor-data-controller.js
 │   │   │   │   ├── threshold-controller.js
 │   │   │   │   └── alert-email-controller.js
 │   │   │   ├── services/          # Lógica de negocio
 │   │   │   │   ├── sensor-data-service.js   # Parseo de mensajes MQTT/HTTP y persistencia
 │   │   │   │   ├── threshold-service.js     # Gestión de umbrales por dispositivo
 │   │   │   │   ├── alert-service.js         # Orquestación de alertas por umbral excedido
 │   │   │   │   └── alert-email-service.js   # Envío de notificaciones por correo
 │   │   │   ├── repository/        # Acceso a datos (capa sobre los modelos Mongoose)
 │   │   │   │   ├── sensor-data-repository.js
 │   │   │   │   ├── threshold-repository.js
 │   │   │   │   └── alert-email-repository.js
 │   │   │   ├── models/            # Esquemas Mongoose
 │   │   │   │   ├── sensor-data-model.js     # Lecturas de temperatura (deviceId, valor, origen)
 │   │   │   │   ├── threshold-model.js       # Umbrales min/max por dispositivo
 │   │   │   │   └── alert-email-model.js     # Emails registrados para recibir alertas
 │   │   │   └── routes/            # Definición de endpoints REST bajo /api/v1
 │   │   │       ├── index.js
 │   │   │       ├── sensor-data-routes.js
 │   │   │       ├── threshold-routes.js
 │   │   │       └── alert-email-routes.js
 │   │   ├── .env.example           # Plantilla de variables de entorno
 │   │   ├── package.json           # Dependencias (Express, Mongoose, mqtt, socket.io, nodemailer)
 │   │   └── README.md              # Documentación específica del backend
 │   │
 │   └── frontend/                  # Dashboard web estático
 │       ├── index.html             # Vista principal (consume REST + WebSocket en tiempo real)
 │       └── styles.css             # Estilos del dashboard
 │
 ├── docker-compose.yml             # Orquesta broker MQTT (mosquitto) y base de datos (MongoDB)
 ├── COMANDOS.md                    # Comandos operativos del proyecto
 └── README.md                      # Documentación general del sistema
```

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
* **Opción directa:** Haz doble clic sobre `software/frontend/index.html` en tu explorador de archivos para abrirlo en el navegador.
* Cuenta con visualización de temperatura (°C), señal LoRa RSSI (dBm), timestamp RTC, botón de "Forzar lectura" y modo simulación automático si los servicios aún no están enviando datos.

---

## 🧪 Pruebas Rápidas y Validación de la API

Swagger (proximamente):

---

## 🛑 Detener Servicios Docker

```bash
docker compose down
```
*(Los datos de MongoDB y la configuración de Mosquitto se conservan en volúmenes persistentes).*
