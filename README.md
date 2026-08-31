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
