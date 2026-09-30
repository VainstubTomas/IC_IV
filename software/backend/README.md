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
No interpretar ACK/response como ejecución sin contrato v2.0. npm test ejecuta
pruebas locales con transporte simulado; no verifica AURA ni hardware real.
