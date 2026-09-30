# ❄️ Sistema Integral IoT de Telemetría - Sistemas de refrigeración (IC_IV)

Proyecto de adquisición, transporte, procesamiento y visualización de temperatura
para una heladera industrial, desarrollado para su integración con **AURA**.
Incluye firmware del nodo, codec, backend con persistencia y alertas por email,
y dashboard web.

La adaptación sigue el **[contrato MQTT AURA v2.0](CONTRATO_MQTT.md)**, recibido
y cotejado con el código. Se conserva el documento original en la raíz; sus
referencias a FastAPI y enlaces internos describen el repo principal de AURA.
Nuestro backend Node.js/MongoDB es el cliente y banco del proyecto IC_IV; no
reemplaza el backend FastAPI de AURA ni resuelve sus pendientes internos.

El contrato recibido declara pendientes el bridge y la persistencia MQTT de AURA.
Confirmar con la cátedra si ya están implementados antes de probar la cadena completa.
La configuración binaria y el reporte de falla siguen siendo propuestas de IC_IV,
pendientes de aprobación.
---

## 🏗️ Arquitectura General del Sistema

```text
[ Sonda DS18B20 + OLED SH1106 + RTC DS3231 ]
                         │ 1-Wire / I2C
                         ▼
          [ XIAO ESP32-S3 + Wio-SX1262 ]
                         │ LoRaWAN AU915 SB2 / OTAA / clase A
                         ▼
             [ Gateway Milesight del aula ]
                         │ Semtech UDP
                         ▼
             [ ChirpStack de la cátedra ]
                         │ Codec propio + MQTT
                         ▼
                  [ lorawan-bridge ]
                         │ Traducción al contrato AURA
                         ▼
                   [ Broker AURA ]
                         │ devices/<UUID>/data, status, response
                         ▼
             [ Backend Node.js / Express ]
                   │                 │
                   ▼                 ▼
               [ MongoDB ]     [ Dashboard web ]
                               REST cada 5 segundos

Configuración: backend → devices/<UUID>/command → bridge → ChirpStack → nodo
```

**Solo el lorawan-bridge consume el broker de ChirpStack.** El backend del proyecto
se conecta al broker de AURA; no se suscribe a `application/...` ni usa `iciv/...`.
El backend también emite eventos Socket.IO, pero el dashboard actual usa polling REST.
La disponibilidad del bridge y su conexión deben confirmarse con la cátedra.

---

## 🧱 Estructura del repositorio

```
IC_IV/
 ├── firmware/
 │   ├── nodo_lorawan/              # Firmware principal: LoRaWAN AU915, clase A
 │   │   ├── nodo_lorawan.ino       # OTAA, sesión NVS, telemetría y recepción de downlinks
 │   │   ├── config_nodo.h          # Formato binario y validación de configuración
 │   │   ├── credenciales.h.example # Plantilla OTAA; credenciales.h real no se versiona
 │   │   ├── temp_sensor.cpp/.h     # DS18B20; lectura inválida sin reemplazo ficticio
 │   │   └── oled_rtc.cpp/.h        # OLED SH1106 y RTC DS3231
 │   └── main/                      # Firmware legacy de pruebas LoRa P2P
 │       ├── main.ino               # Ejemplo anterior; no usar para integración AURA
 │       ├── temp_sensor.cpp/.h     # Driver del sensor de temperatura DS18B20 (1-Wire)
 │       └── oled_rtc.cpp/.h        # Manejo de display OLED + módulo RTC (hora/fecha local)
 │
 ├── configuration/
 │   └── codec-heladera.js          # Codec del profile propio de ChirpStack
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
 │   │   │   │   │   ├── aura-protocol.js   # Validación de UUID y values.temp_c
 │   │   │   │   │   └── mqtt-topics.js     # Definición de tópicos MQTT del sistema
 │   │   │   │   ├── swagger/              # Configuración OpenAPI / Swagger UI
 │   │   │   │   └── mailer/
 │   │   │   │       └── mailer-config.js   # Configuración SMTP para envío de alertas
 │   │   │   ├── controller/        # Controladores HTTP (parseo de requests/responses)
 │   │   │   │   ├── sensor-data-controller.js
 │   │   │   │   ├── threshold-controller.js
 │   │   │   │   └── alert-email-controller.js
 │   │   │   ├── services/          # Lógica de negocio
 │   │   │   │   ├── device-event-service.js  # Status y response del contrato v2
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
 │   │   │   │   ├── device-status-model.js  # Estado del nodo y LWT del bridge
 │   │   │   │   ├── device-command-model.js # Registro de comandos y estado pendiente
 │   │   │   │   └── alert-email-model.js     # Emails registrados para recibir alertas
 │   │   │   └── routes/            # Definición de endpoints REST bajo /api/v1
 │   │   │       ├── index.js
 │   │   │       ├── sensor-data-routes.js
 │   │   │       ├── threshold-routes.js
 │   │   │       ├── device-config-routes.js # API experimental de configuración remota
 │   │   │       └── alert-email-routes.js
 │   │   ├── scripts/prueba-contrato.ps1 # Prueba local de mensajes y duplicados
 │   │   ├── test/aura.test.js      # Pruebas locales del codec, MQTT e ingesta
 │   │   ├── .env.example           # Plantilla de variables de entorno
 │   │   ├── package.json           # Dependencias (Express, Mongoose, mqtt, socket.io, nodemailer)
 │   │   └── README.md              # Documentación específica del backend
 │   │
 │   └── frontend/                  # Dashboard web estático
 │       ├── index.html             # Vista principal (consulta REST cada 5 segundos)
 │       └── styles.css             # Estilos del dashboard
 │
 ├── docker-compose.yml             # Orquesta broker MQTT (mosquitto) y base de datos (MongoDB)
 ├── CONTRATO_MQTT.md              # Contrato AURA v2.0 recibido de la cátedra
 ├── COMANDOS.md                    # Comandos operativos del proyecto
 └── README.md                      # Documentación general del sistema
```

---

## 🚀 Guía de Puesta en Marcha

### 1. Preparar el entorno

Requisitos: Node.js y npm, Docker con Compose para MongoDB local, y Arduino IDE
con soporte ESP32 para compilar el nodo. Los comandos siguientes usan PowerShell.

```powershell
cd "E:\FACULTAD\Proyectos\IC_IV"
```

Hay dos escenarios:

- **Prueba local sin hardware:** MongoDB y Mosquitto locales, con mensajes de prueba.
- **Prueba en el aula:** gateway y ChirpStack de la cátedra, broker AURA y UUID real.

**No levantar otro stack LoRaWAN/ChirpStack en la computadora del alumno.**
El `docker-compose.yml` de este proyecto contiene únicamente MongoDB y Mosquitto;
su broker local no recibe automáticamente el tráfico del aula ni reemplaza AURA.

### 2. Iniciar la infraestructura necesaria

Para pruebas locales aisladas, desde la raíz:

```powershell
docker compose up -d
docker compose ps
docker compose logs -f mqtt-broker
```

Salir de los logs con `Ctrl+C` no detiene los contenedores.

Si se usa el broker AURA de la cátedra y se necesita únicamente MongoDB local:

```powershell
docker compose up -d mongodb
```

Si la cátedra provee también MongoDB, configurar esa conexión y no levantar
contenedores locales innecesarios.

### 3. Configurar e iniciar el backend

```powershell
cd software/backend
npm ci
if (!(Test-Path .env)) { Copy-Item .env.example .env }
```

Editar `.env` antes de iniciar. La copia condicional conserva una configuración existente.

| Variable | Uso |
|---|---|
| `SERVERPORT` | Puerto HTTP; la plantilla usa `8080` |
| `BDURL` | MongoDB; local: `mongodb://localhost:27017/iciv_db` |
| `MQTTBROKERURL` | URL del broker AURA; en prueba aislada: `mqtt://localhost:1883` |
| `AURA_DEVICE_ID` | UUID del dispositivo AURA; no DevEUI ni `Heladera1` |
| `AURA_BRIDGE_DEVICE_ID` | UUID del bridge para leer su LWT; solicitar a la cátedra |
| `MQTT_CLIENT_ID` | ID estable, exclusivo de esta instancia; conservar al reiniciar |
| `AURA_CONFIG_EXPERIMENTAL` | Mantener `false` hasta acordar la propuesta con la cátedra |
| `BROKERUSERNAME`, `BROKERPASSW` | Credenciales MQTT si el broker requiere autenticación |
| `MQTTBROKERCAPATH` | Ruta al certificado CA cuando corresponda |
| `SMTPHOST`, `SMTPPORT`, `SMTPSECURE`, `SMTPUSER`, `SMTPPASS`, `SMTPFROM` | Configuración para alertas por email |

Para el banco local puede usarse el UUID de ejemplo
`650e8400-e29b-41d4-a716-446655440001` en `AURA_DEVICE_ID`. **No identifica un
dispositivo real de AURA**; en clase reemplazarlo por el UUID asignado.

```powershell
npm start
# Alternativa de desarrollo:
# npm run dev
```

Confirmar en terminal conexión a MongoDB, conexión MQTT y suscripciones
`devices/+/data`, `devices/+/status`, `devices/+/response`.
Se guardan únicamente mediciones numéricas `values.temp_c` del dispositivo configurado.
Las suscripciones y comandos usan **QoS 1**. Sesión persistente (`clean=false`)
y `MQTT_CLIENT_ID` estable permiten que el broker conserve mensajes QoS 1 durante
desconexiones, según su configuración y límites. No usar el mismo ID en dos instancias.
MongoDB e índices se inicializan antes de conectar MQTT; PUBACK espera persistencia.
Si hay un error transitorio de DB, se reintenta el mensaje sin confirmarlo.

`ingest_id` es opcional: si viene como UUID se deduplica con índice único persistente.
Sin ese campo no es posible distinguir dos entregas del mismo evento. Los reintentos
de radio pueden ser eventos distintos; no se deduplican por temperatura ni contador.
Un duplicado no vuelve a disparar el chequeo de alertas.

`status` guarda `online`/`offline` y `details` por UUID. RSSI se lee de
`details.rssi`. El estado offline del bridge tiene prioridad sobre el último online
del nodo. `response` correlaciona UUID + `details.command_id`, conserva historial y
acepta solo `encolado`, `transmitido`, `recibido`, `rechazado`. Se evitan regresiones
por respuestas tardías. Todos mantienen `confirma_ejecucion=false`.

### 4. Abrir el Dashboard Frontend

Con `SERVERPORT=8080`, abrir **http://localhost:8080/**. Si se eligió otro puerto,
cambiarlo en la dirección. El backend sirve `software/frontend/`; abrir el HTML
con doble clic no conecta correctamente las rutas relativas de la API.

El dashboard muestra temperatura, hora de recepción, umbrales de alerta, emails
y configuración remota. Consulta la API cada 5 segundos.

- Sin registros válidos muestra ausencia de telemetría; el backend no inventa una lectura inicial.
- Si falla la consulta, el frontend muestra datos simulados identificados como **Modo Simulación (Offline)**.
- La hora del dashboard corresponde al registro en MongoDB; el RTC se usa en la OLED del nodo.
- RSSI proviene de `status.details.rssi`; queda sin valor si no llegó esa métrica.
- El indicador distingue desconexión del broker, bridge offline, nodo offline y estado desconocido. No supone conexión física por responder la API.
- **Forzar lectura** está deshabilitado hasta definir ese comando con AURA.
- Los umbrales de email viven en la plataforma; no se envían al firmware.

---

## 🧪 Pruebas Rápidas y Validación de la API

### 1. Pruebas automatizadas sin hardware

Desde `software/backend`:

```powershell
npm test
```

Si el entorno impide crear procesos secundarios:

```powershell
node --test --test-isolation=none
```

Las pruebas cubren temperatura positiva/negativa y centinela, configuración
binaria y rangos, UUID y mediciones AURA, MQTT con transporte simulado y servicio
de ingesta, deduplicación, persistencia antes de PUBACK, estados del bridge y correlación de respuestas. Usan dobles de prueba para transporte y DB; no comprueban gateway, AURA, MongoDB real, SMTP ni hardware.

### 2. Verificar API y dashboard con un mensaje de prueba

Requiere backend, MongoDB y Mosquitto local funcionando, y el UUID de ejemplo
configurado en `.env`. Desde la raíz del proyecto, publicar:

```powershell
$mensajePrueba = '{"values":{"temp_c":4.25}}'
$mensajePrueba | docker compose exec -T mqtt-broker mosquitto_pub -q 1 -h localhost -t "devices/650e8400-e29b-41d4-a716-446655440001/data" -s
```

Ese mensaje es **telemetría de prueba**, aunque el dashboard lo muestre como dato
recibido por la API. No demuestra recepción desde la sonda física.

```powershell
Invoke-RestMethod "http://localhost:8080/api/v1/health"
Invoke-RestMethod "http://localhost:8080/api/v1/telemetria/latest"
Invoke-RestMethod "http://localhost:8080/api/v1/telemetria/history?limit=20"
```

La última lectura debe incluir `temperatura: 4.25` y el UUID del tópico.
El dashboard debe actualizarse en el siguiente sondeo. No ejecutar este ejemplo
contra el broker de producción de AURA.

Para probar datos duplicados y estados del bridge en el broker local:

```powershell
cd software/backend
./scripts/prueba-contrato.ps1
```

Usar el UUID de ejemplo configurado en .env y el backend corriendo. El script
publica dos veces el mismo ingest_id con QoS 1 y comprueba un solo registro;
también simula bridge offline. Es tráfico de prueba, no radio real. Opcionalmente
`-ProbarComandos` prueba response con configuración experimental habilitada,
siempre en el banco local y nunca contra AURA real.

### 3. Documentación interactiva y endpoints

Swagger UI está disponible en **http://localhost:8080/api-docs** con el puerto
de la plantilla. Permite consultar y probar las rutas documentadas; las rutas de
configuración remota nuevas se describen aquí y todavía no tienen anotaciones Swagger.

| Método y ruta | Comportamiento actual |
|---|---|
| `GET /api/v1/health` | Salud y tiempo de actividad del backend |
| `GET /api/v1/telemetria/latest` | Última lectura del UUID configurado, o `null` |
| `GET /api/v1/telemetria/history?limit=20` | Histórico del mismo dispositivo |
| `POST /api/v1/telemetria` | Inserción HTTP para pruebas; usar el UUID en `deviceId` |
| `GET /api/v1/umbrales`, `POST /api/v1/umbrales` | Configuración de alertas de plataforma |
| `GET /api/v1/alertas/emails`, `POST /api/v1/alertas/emails` | Destinatarios de alertas |
| `DELETE /api/v1/alertas/emails/:id` | Eliminar destinatario |
| `GET /api/v1/dispositivo/status` | Estado guardado de nodo/bridge y conexión MQTT actual |
| `GET /api/v1/dispositivo/config` | Habilitación experimental y último comando |
| `POST /api/v1/dispositivo/config` | Propuesta de configuración; `409` si está deshabilitada, `202` si se publica |
| `POST /api/v1/leer` | `409`: lectura forzada aún no acordada |

---

## 📡 Nodo LoRaWAN (ChirpStack v4 + AU915)

### 1. Requisitos en Arduino IDE

- Placa **XIAO_ESP32S3**; **USB CDC On Boot: Enabled**.
- Librerías: **RadioLib 7.7.1** como API de referencia, **DallasTemperature**,
  **OneWire**, **U8g2** y **RTClib**.
- Hardware: XIAO ESP32-S3 + Wio-SX1262 B2B, DS18B20 en D2, OLED SH1106 y RTC
  DS3231 por I2C en D4/D5. Conexión de sonda con resistencia pull-up adecuada.

### 2. Configurar claves y alta en el aula

Solicitar a la cátedra claves únicas `JOIN_EUI`, `DEV_EUI`, `APP_KEY`, UUID AURA,
alta en aplicación **aura** y un **device profile propio, clase A** con el codec.
No modificar el profile compartido.

Desde la raíz:

```powershell
if (!(Test-Path firmware/nodo_lorawan/credenciales.h)) {
    Copy-Item firmware/nodo_lorawan/credenciales.h.example firmware/nodo_lorawan/credenciales.h
}
```

Completar el archivo local. `.env` y `credenciales.h` están ignorados por Git.
El dispositivo debe tener tag `aura_device_id=<UUID real>`; sin él el bridge lo
descarta. Confirmar con la cátedra el límite `aura_max_downlink_bytes`.

### 3. Codec y transmisión

La cátedra instala `configuration/codec-heladera.js` en el profile propio.
En FPort 1 el payload conserva 4 bytes big-endian:

| Bytes | Contenido |
|---|---|
| 0–1 | Contador local uint16; no se publica en `values` |
| 2–3 | Temperatura int16 en centésimas; `0x7FFF` indica lectura no disponible |

`decodeUplink` devuelve `data: {"temp_c":4.25}` para una lectura válida.
El bridge lo coloca en `values`. Una falla de sonda devuelve `data: {}` y una
advertencia, sin inventar temperatura. El canal de notificación de esa falla
sigue pendiente de definición en el contrato.

Abrir `firmware/nodo_lorawan/nodo_lorawan.ino`, compilar, flashear y revisar
monitor serie a **115200**: inicialización de radio, join nuevo o sesión restaurada,
y transmisiones. Por defecto: **AU915 SB2**, **DR2/SF10**, ADR apagado, uplinks
confirmados cada **20 s**, hasta **3 intentos totales** con backoff y jitter.
RadioLib usa sub-banda `2`; ChirpStack la identifica como `au915_1`.
Nonces y sesión se restauran mediante los setters de RadioLib y se persisten en NVS.

### 4. Configuración remota preparada para banco acordado

Propuesta pendiente de validar con la cátedra: comando **set_config** con
configuración completa, sin cambios parciales. Parámetros:

| Parámetro | Valores admitidos |
|---|---|
| `interval_s` | Entero de 20 a 3600 segundos |
| `confirmed` | Booleano |
| `adr` | Booleano |
| `dr` | Entero 2–5; usado como DR fijo con ADR apagado |
| `offset_c` | −5 a +5 °C, precisión de 0.01 °C |

`encodeDownlink` produce **7 bytes en FPort 10**: versión (1 byte), intervalo
(2), flags confirmado/ADR (1), DR (1), offset en centésimas con signo (2).
Confirmar que el bridge use ese puerto y que el límite configurado permita el
payload. El firmware valida antes de aplicar y guarda en NVS; si no hay
configuración válida guardada, usa los valores por defecto del banco.

El backend conserva `AURA_CONFIG_EXPERIMENTAL=false` por defecto. Solo tras
acordar la prueba habilitarlo, reiniciar el backend y usar la tarjeta de configuración.
La API genera `command_id` y publica en `devices/<UUID>/command`:

```json
{
  "command": "set_config",
  "params": {
    "interval_s": 300,
    "confirmed": false,
    "adr": false,
    "dr": 2,
    "offset_c": -0.25
  },
  "command_id": "<UUID generado por el backend>"
}
```

Un `202` indica publicación MQTT, **no ejecución en el nodo**. Clase A recibe
el downlink después del próximo uplink; la interfaz sigue los estados del bridge, sin afirmar ejecución.
`command_id` no está incluido en los 7 bytes, por lo que aún no hay correlación
de ejecución de punta a punta.

El reporte binario propuesto usa **FPort 11**, con los mismos 7 bytes de
configuración vigente al arrancar y tras aplicar cambios. Su envío está
deshabilitado por defecto con `ICIV_REPORTE_CONFIG_EXPERIMENTAL=0`; habilitarlo
solo en una compilación de banco acordada. `decodeConfigReport` permite
interpretarlo en pruebas, pero `decodeUplink` rechaza ese puerto para no publicar
configuración como medición. Su traslado a `status`/`response` debe acordarse.

### Propuesta de reporte de falla de sensor (no implementada ni normativa)

El contrato reserva alerts/... y aún no define notificación de falla. No se
publica allí. Para discutir en clase proponemos un uplink diagnóstico separado,
FPort 12, tres bytes: versión 1, sensor 1 (DS18B20), estado 0 (recuperado) o 1
(sin lectura). Se emitiría solo al cambiar de estado. Puerto y bytes están sujetos
a revisión; firmware/codec actuales NO transmiten ni decodifican este formato.

Proponemos que el bridge lo traduzca al status del dispositivo conservando
`status=online` porque falla el sensor, no la comunicación. Ejemplo de extensión
de details para revisión docente (NO emitir hasta incorporarla al contrato):

```json
{"status":"online","details":{"evento":"sensor","sensor":"ds18b20","estado_sensor":"sin_lectura"}}
```

La recuperación usaría `estado_sensor=recuperado`. Debe acordarse una ruta de
diagnósticos que no coloque estos campos en values; el bridge actual especificado
es opaco a las mediciones. El centinela y la omisión de temp_c ya funcionan, pero
no equivalen a esta notificación. Ningún campo propuesto se publica en producción.

### 5. Lista de comprobación en hardware

1. Confirmar join y recepción de `temp_c` por la cadena completa de AURA.
2. Reiniciar el nodo y comprobar sesión restaurada o join válido sin repetir nonce.
3. Desconectar la DS18B20: OLED muestra error y no se publica una temperatura ficticia.
4. Reconectar la sonda y comprobar recuperación de las mediciones.
5. En banco acordado, probar configuración válida, rechazo de valores fuera de rango
   y conservación en NVS tras reiniciar.
6. Verificar demora clase A y revisar con la cátedra reporte de aplicación y falla.
7. Confirmar QoS 1, repetir un ingest_id y revisar un solo registro; probar reinicio
   del backend con la misma sesión MQTT y mensajes publicados mientras estaba caído.
8. Probar status.details.rssi y los cuatro response con su command_id; verificar
   que recibido no se muestre como ejecutado y que rechazado muestre details.motivo.
9. Confirmar UUID del bridge y su LWT offline. Si se cambia el intervalo del nodo,
   coordinar también el uplink interval del device profile: el bridge infiere offline
   con 3 veces ese intervalo, no leyendo automáticamente nuestra configuración NVS.

Para una instalación permanente conviene configurar varios minutos y uplinks no
confirmados; los 20 s confirmados son valores de prueba del aula.

---

## 🛑 Detener Servicios Docker

Detener el backend con `Ctrl+C`. Desde la raíz del proyecto:

```powershell
docker compose down
```

MongoDB conserva su volumen; Mosquitto conserva datos y logs montados en carpetas
locales. No usar `docker compose down -v` si se quieren conservar los datos.
