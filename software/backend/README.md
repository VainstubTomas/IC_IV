# 🔌 Backend REST API + MQTT Gateway (IC_IV)

Servidor backend desarrollado en **Node.js (Express 5)** con persistencia en **MongoDB (Mongoose)**, cliente **MQTT (Eclipse Mosquitto)** y comunicación bidireccional en tiempo real con **Socket.IO**.

---

## 🏛️ Arquitectura por Capas

- **`models/sensor-data-model.js`:** Esquema de MongoDB (`temperature`, `rssi`, `deviceId`, `source`, `timestamps`).
- **`repository/sensor-data-repository.js`:** Operaciones de base de datos (`create`, `getLatest`, `getHistory`, `getStats`).
- **`services/sensor-data-service.js`:** Lógica de negocio, validación de rangos térmicos, formateo de timestamps y parser automático de mensajes MQTT (`"Heladera1:3.85"` o JSON).
- **`controller/sensor-data-controller.js`:** Handlers de peticiones HTTP con validaciones y manejo de errores.
- **`routes/sensor-data-routes.js`:** Endpoints REST (`/api/v1/telemetria`, `/api/v1/telemetria/latest`, `/api/v1/telemetria/history`, `/api/v1/leer`, `/api/v1/health`).
- **`config/mqtt/mqtt-config.js`:** Cliente MQTT con auto-ingesta y persistencia directa a la base de datos al recibir mensajes en `iciv/#`.

---

## 🚀 Instalación y Puesta en Marcha

### 1. Requisitos Previos
Asegúrate de tener corriendo los contenedores de Docker (Mosquitto y MongoDB):
```bash
# Desde la raíz del proyecto
docker compose up -d
```

### 2. Instalar Dependencias
```bash
npm install
```

### 3. Iniciar el Servidor
```bash
# Modo producción
npm start

# Modo desarrollo con auto-recarga
npm run dev
```

El servidor quedará escuchando en `http://localhost:3000`.

---

## 📡 Catálogo de Endpoints de la API

### 1. `GET /api/v1/telemetria/latest`
Obtiene la última lectura de telemetría registrada.

**Respuesta (200 OK):**
```json
{
  "id": "66cc48b1...",
  "temperatura": 3.8,
  "temperature": 3.8,
  "rssi": -78,
  "deviceId": "Heladera1",
  "timestamp": "2026-08-26 15:00:12",
  "createdAt": "2026-08-26T18:00:12.345Z"
}
```

---

### 2. `POST /api/v1/telemetria`
Inserta una nueva lectura manualmente o desde un dispositivo HTTP.

**Body (JSON):**
```json
{
  "temperatura": 4.2,
  "rssi": -82,
  "deviceId": "Heladera1"
}
```

---

### 3. `GET /api/v1/telemetria/history?limit=20`
Consulta las últimas lecturas históricas ordenadas cronológicamente.

---

### 4. `POST /api/v1/leer`
Dispara un comando de lectura forzada hacia el nodo a través de MQTT (`iciv/value1/analog`).

---

### 5. `GET /api/v1/health`
Verifica el estado del servidor y tiempo de actividad (uptime).
