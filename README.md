# IC_IV — Monitoreo de heladera industrial

DS18B20 + OLED/RTC → XIAO ESP32-S3 / Wio-SX1262 → gateway del aula →
ChirpStack → lorawan-bridge → broker AURA → backend → MongoDB / dashboard.

Solo el lorawan-bridge consume ChirpStack. No levantar un stack LoRaWAN propio.
El proyecto está adaptándose a AURA según los mails docentes. El contrato MQTT
v2.0 aún no fue recibido; no se afirma integración completa con la plataforma.
Ver [propuesta y registro de correcciones](docs/PROPUESTA_AURA.md).

## Firmware

Sketch: firmware/nodo_lorawan/nodo_lorawan.ino. Placa XIAO_ESP32S3,
USB CDC On Boot Enabled, monitor serie 115200. Dependencias: RadioLib 7.7.1
(API verificada), DallasTemperature, OneWire, U8g2 y RTClib.
Copiar credenciales.h.example a credenciales.h y completar claves únicas del
docente. credenciales.h permanece ignorado por Git.

AU915 sub-banda 2, clase A. Por defecto: 20 s, confirmado, DR2 fijo, ADR apagado.
Hasta 3 intentos totales con backoff/jitter. Nonces y sesión persistidos en NVS.
Sonda inválida = 0x7FFF; no se utiliza la temperatura interna como reemplazo.

El codec configuration/codec-heladera.js se instala en profile propio dentro de
la aplicación aura, por la cátedra. Mediciones FPort 1: data = {temp_c} o {} si
no hay lectura. Configuración propuesta FPort 10, reporte propuesto FPort 11;
el mapeo del reporte a status/response sigue pendiente de acordar.

## Backend y dashboard

Desde software/backend: npm ci, copiar .env.example a .env y completar MongoDB,
URL/credenciales del broker AURA y AURA_DEVICE_ID con el UUID real.
Ejecutar npm start; abrir http://localhost:<SERVERPORT>/ (8080 en la plantilla).
Swagger: /api-docs. El dashboard consume REST cada 5 s; backend también emite
eventos Socket.IO. Hora mostrada = recepción, no RTC. Simulación offline identificada.

Suscripciones: devices/+/data, devices/+/status, devices/+/response. Solo se
ingestan values.temp_c numéricos; las consultas del dashboard se filtran por
AURA_DEVICE_ID. No hay tópicos iciv/... ni suscripción application/... en el backend.
Umbrales de email viven en plataforma. Status/response no se interpretan como
ejecución hasta obtener el contrato.

Configuración remota preparada como propuesta experimental, desactivada por defecto
(AURA_CONFIG_EXPERIMENTAL=false). Habilitar solo en banco acordado con la cátedra.
POST /api/v1/dispositivo/config recibe interval_s, confirmed, adr, dr, offset_c.
Un 202 significa publicación MQTT y estado pendiente, no aplicación en nodo.
Lectura forzada deshabilitada hasta definir su comando; clase A recibe luego del uplink.

## Pruebas locales

npm test desde software/backend. Verifica codec, configuración binaria, rangos,
ingesta por UUID y publicaciones MQTT con transporte simulado. No requiere servicios.
Si el entorno restringe subprocesses: node --test --test-isolation=none.
Docker Compose local levanta MongoDB y Mosquitto únicamente para pruebas aisladas;
no equivale al broker de AURA ni al servidor LoRaWAN del aula.

Pendientes: contrato v2.0, aprobación de la propuesta, profile/tags/UUID, conectividad
del bridge, compilación con todas las librerías y prueba física de downlinks/reinicios.
