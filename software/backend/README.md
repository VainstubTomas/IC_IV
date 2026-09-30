# Backend IC_IV para integración AURA

Consultar el README.md de la raíz: contiene la puesta en marcha y la propuesta de configuración AURA.

- Configurar BDURL, SERVERPORT, MQTTBROKERURL (broker AURA) y AURA_DEVICE_ID.
- npm ci; npm start; dashboard servido en /; Swagger en /api-docs.
- GET /api/v1/telemetria/latest devuelve última lectura del UUID configurado o null.
- GET /api/v1/telemetria/history devuelve histórico del mismo dispositivo.
- MQTT: devices/+/data → values.temp_c. UUID obligatorio en tópico.
- GET/POST /api/v1/umbrales: alertas por email en plataforma, sin downlink.
- GET /api/v1/dispositivo/config: último comando y habilitación experimental.
- POST /api/v1/dispositivo/config: propuesta set_config; 202 = pendiente.
- POST /api/v1/leer: 409, comando aún no acordado.

AURA_CONFIG_EXPERIMENTAL=false por defecto. No consumir broker ChirpStack.
Contrato v2.0 recibido: response acepta encolado/transmitido/recibido/rechazado y ninguno confirma ejecución. npm test ejecuta
pruebas locales con transporte simulado; no verifica AURA ni hardware real.

## Recepción según contrato v2.0

MQTT_CLIENT_ID estable y exclusivo, clean=false, suscripciones QoS 1. Completar
AURA_BRIDGE_DEVICE_ID para LWT. DB e índices listos antes de MQTT; no PUBACK antes
de persistir. ingest_id opcional deduplica en MongoDB y evita alertas repetidas.
Status guarda online/offline y details; RSSI sale de details.rssi. Response usa
details.command_id para correlación, guarda historial y no confirma ejecución.
GET /api/v1/dispositivo/status expone estado de nodo, bridge y conexión MQTT.
Scripts de prueba local: ./scripts/prueba-contrato.ps1; ver README principal.
No sustituye FastAPI de AURA; las pruebas de esa plataforma dependen de la cátedra.
