# Comandos útiles - IC_IV

## 1. Conectarse a la base de datos (MongoDB)

La base corre en el contenedor Docker `iciv_mongodb`, base de datos `iciv_db`.

```bash
docker exec -it iciv_mongodb mongosh iciv_db
```

Alternativa, si tenés `mongosh` instalado localmente:

```bash
mongosh "mongodb://localhost:27017/iciv_db"
```

Dentro del shell, confirmar la base y ver las colecciones:

```js
use iciv_db
show collections
```

## 2. Buscar registros existentes

La colección de telemetría es `sensordatas` (nombre generado por Mongoose a partir del modelo `SensorData`).

```js
// Contar cuántos registros hay
db.sensordatas.countDocuments()

// Ver todos los registros
db.sensordatas.find().pretty()

// Ver los últimos 10, más reciente primero
db.sensordatas.find().sort({ createdAt: -1 }).limit(10)

// Filtrar por dispositivo
db.sensordatas.find({ deviceId: "Heladera1" })

// Ver solo el último registro
db.sensordatas.find().sort({ createdAt: -1 }).limit(1)
```

## 3. Formato JSON para POST a la API (Insomnia / Postman / curl)

**Endpoint:** `POST http://localhost:8080/api/v1/telemetria`
**Header:** `Content-Type: application/json`

**Body (todos los campos salvo `temperature` son opcionales):**

```json
{
  "deviceId": "Heladera1",
  "temperature": 4.2,
  "rssi": -65,
  "source": "manual"
}
```

Notas de los campos:
- `temperature` (o alternativamente `temperatura`): número, **obligatorio**.
- `deviceId`: string, default `"Heladera1"` si se omite.
- `rssi`: número, default `null` si se omite.
- `source`: uno de `"lora" | "mqtt" | "http" | "manual"`, default `"http"` si se omite.

Equivalente con `curl`:

```bash
curl -X POST http://localhost:8080/api/v1/telemetria \
  -H "Content-Type: application/json" \
  -d '{"deviceId": "Heladera1", "temperature": 4.2, "rssi": -65, "source": "manual"}'
```
