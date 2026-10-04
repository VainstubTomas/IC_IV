# ❄️ Sistema Integral IoT de Telemetría - Sistemas de refrigeración (IC_IV)

Sistema de adquisición, transporte, almacenamiento y visualización de temperatura
para heladera y freezer. Utiliza dos sondas DS18B20, un nodo XIAO ESP32-S3,
una malla ESP-NOW basada en el ejemplo de AURA, MongoDB y un dashboard web.

El nodo mide y controla alarmas localmente, permite configurar cada sonda
desde la interfaz y conserva mediciones pendientes cuando falla la comunicación.
También registra la primera lectura del paso a batería mediante una entrada
de alimentación configurable.

La integración externa se prepara para **AURA**. El contrato MQTT v2.0 incluido
describe LoRaWAN; el retorno a mesh y sus extensiones necesitan un acuerdo de
integración. El alcance y los puntos pendientes están en
[MESH_INTEGRACION.md](MESH_INTEGRACION.md). El backend Node.js del proyecto
es un cliente y banco de visualización separado del servidor central de AURA.

---

## 🏗️ Arquitectura General del Sistema

```text
[ DS18B20 heladera + DS18B20 freezer + OLED + RTC + LEDs ]
                             │
                             ▼
                   [ Sensor XIAO ESP32-S3 ]
                   Cola persistente en NVS
                             │ ESP-NOW
                             ▼
                       [ Nodo de sala ]
                             │ ESP-NOW
                             ▼
                      [ Gateway ESP32 ]
                             │ Wi-Fi
                    ┌────────┴────────┐
                    ▼                 ▼
               [ API AURA ]     [ Broker MQTT AURA ]
               Ingesta REST             │
                                   [ Backend IC IV ]
                                       │       │
                                       ▼       ▼
                                   [ MongoDB ] [ Dashboard ]

ACK gateway y ACK central: gateway → sala → sensor
Configuración: dashboard → backend → MQTT → gateway → sala → sensor
```

- **Dos sondas:** temperatura de heladera y freezer, con intervalos y límites propios.
- **Sensor:** alarma local por umbral, detección de batería, OLED y cola persistente.
- **Sala:** relay de ruta fija; reenvía mensajes sin fabricar confirmaciones finales.
- **Gateway:** relaciona MAC/sonda con UUID AURA y adapta radio, REST y MQTT.
- **Backend/MongoDB:** guarda lecturas, cortes y estados, y gestiona alertas por correo.
- **Dashboard:** consulta la API cada cinco segundos y permite ajustar cada sonda.

El sensor conserva los datos hasta obtener confirmación del central. Que el
gateway reciba una trama o MQTT confirme una publicación no demuestra que
AURA haya guardado el evento. Gateway y sala necesitan la adaptación compatible;
el firmware mesh original no incorpora estos dos ACK finales.

---

## 🧱 Estructura del repositorio

```text
IC_IV/
 ├── firmware/
 │   ├── nodo_mesh/                  # Firmware principal del sensor de dos sondas
 │   │   ├── nodo_mesh.ino           # Punto de entrada Arduino
 │   │   ├── app.h                   # Muestreo, LEDs, OLED/RTC, energía y comunicación
 │   │   └── config_local.h.example # Plantilla de MAC, canal y pines
 │   ├── nodo_sala_mesh/             # Adaptación del relay ESP-NOW
 │   ├── nodo_gateway_mesh/          # Adaptador hacia AURA; requiere coordinar su carga
 │   ├── mesh_comun/
 │   │   ├── mesh_core.h             # Tramas, configuración, FIFO y validación
 │   │   ├── mesh_radio.h            # ESP-NOW y cola de recepción
 │   │   └── mesh_storage.h          # Journal NVS con dos copias y CRC
 │   ├── monolitico/                 # Copias autocontenidas generadas para Arduino IDE
 │   ├── generar_mesh_monolitico.py  # Regenera copias sin incluir credenciales
 │   ├── tests_host/                 # Pruebas C++ de FIFO, CRC y política de ACK
 │   ├── tests_mesh/                 # Evalúa las mismas pruebas al compilar para ESP32
 │   ├── nodo_lorawan/               # Versión LoRaWAN anterior, conservada como referencia
 │   └── main/                       # Prototipo antiguo LoRa punto a punto
 ├── configuration/
 │   └── codec-heladera.js           # Codec de la versión LoRaWAN; no interviene en mesh
 ├── mosquitto/
 │   ├── config/mosquitto.conf       # Broker MQTT del banco local
 │   ├── data/                      # Datos persistentes, excluidos de Git
 │   └── log/                       # Logs locales, excluidos de Git
 ├── software/
 │   ├── backend/
 │   │   ├── src/
 │   │   │   ├── app.js, server.js   # API Express, frontend y arranque de servicios
 │   │   │   ├── config/            # Entorno, MongoDB, MQTT, SMTP y validación mesh
 │   │   │   ├── controller/        # Solicitudes HTTP
 │   │   │   ├── services/          # Telemetría, energía, estados, umbrales y alertas
 │   │   │   ├── repository/        # Acceso a datos
 │   │   │   ├── models/            # Lecturas, cortes, estados, comandos y correos
 │   │   │   └── routes/            # Endpoints bajo /api/v1
 │   │   ├── scripts/               # Herramientas de banco MQTT
 │   │   ├── test/                  # Pruebas del protocolo, ingesta y API
 │   │   ├── .env.example           # Plantilla de configuración
 │   │   └── package.json           # Dependencias y comandos Node.js
 │   └── frontend/
 │       ├── index.html             # Dashboard de heladera y freezer
 │       ├── dashboard.js           # Consultas, formularios y estados
 │       └── styles.css             # Estilos de la interfaz
 ├── docker-compose.yml             # Mosquitto y MongoDB para pruebas locales
 ├── CONTRATO_MQTT.md              # Contrato AURA v2.0 recibido, sin alteraciones
 ├── MESH_INTEGRACION.md            # Especificación de adaptación y guía de banco físico
 ├── COMANDOS.md                    # Comandos operativos
 └── README.md
```

---

## 🚀 Guía de Puesta en Marcha

Requisitos: Node.js y npm compatibles con las dependencias, Docker con Compose
para servicios locales y Arduino IDE con soporte ESP32. Los comandos usan
PowerShell y parten de la raíz del repositorio, salvo indicación contraria.

### 1. Iniciar Infraestructura con Docker (Broker MQTT + MongoDB)

Para un banco local sin hardware:

```powershell
docker compose up -d
docker compose ps
docker compose logs -f mqtt-broker
```

MongoDB queda en `mongodb://localhost:27017/iciv_db` y MQTT en
`mqtt://localhost:1883`. El puerto 9001 admite MQTT sobre WebSocket, aunque
el dashboard actual consulta REST. Compose no inicia el servidor central AURA
ni convierte el broker local en un gateway mesh.

Al usar el broker AURA externo y una base de datos local:

```powershell
docker compose up -d mongodb
```

### 2. Iniciar el Backend (Node.js)

```powershell
cd software/backend
npm ci
if (!(Test-Path .env)) { Copy-Item .env.example .env }
```

Completar `.env` con la configuración del entorno:

| Variable | Uso |
|---|---|
| `SERVERPORT` | Puerto HTTP; la plantilla usa 8080 |
| `BDURL` | URL de MongoDB |
| `MQTTBROKERURL` | Broker AURA o banco MQTT aislado |
| `AURA_FRIDGE_DEVICE_ID` | UUID lógico de heladera |
| `AURA_FREEZER_DEVICE_ID` | UUID lógico de freezer, distinto del anterior |
| `AURA_GATEWAY_DEVICE_ID` | UUID del gateway para su estado y LWT |
| `ICIV_TRANSPORT` | `mesh` por defecto; `lorawan` conserva validación de comandos anteriores |
| `MQTT_CLIENT_ID` | ID estable y exclusivo de esta instancia |
| `AURA_MESH_EXTENSIONS_ENABLED` | Lectura de metadatos y reportes mesh; por defecto `false` |
| `AURA_CONFIG_EXPERIMENTAL` | Publicación de ajustes acordados; por defecto `false` |
| `BROKERUSERNAME`, `BROKERPASSW`, `MQTTBROKERCAPATH` | Autenticación/CA MQTT cuando corresponda |
| `SMTPHOST`, `SMTPPORT`, `SMTPSECURE`, `SMTPUSER`, `SMTPPASS`, `SMTPFROM` | Alertas por correo |

Los aliases anteriores `AURA_DEVICE_ID` y `AURA_BRIDGE_DEVICE_ID` siguen
disponibles para heladera y gateway. Los UUID deben corresponder al alta real
en AURA; las MAC identifican placas y no reemplazan esos UUID.
En un banco aislado pueden utilizarse UUID ficticios válidos.

```powershell
npm start
# Desarrollo:
# npm run dev
```

El backend inicializa MongoDB e índices antes de conectarse a MQTT. Se suscribe
con QoS 1 a `devices/+/data`, `status` y `response`, filtrando los dispositivos
configurados. La sesión persistente necesita conservar `MQTT_CLIENT_ID` y
depende de los límites del broker. `ingest_id` permite deduplicar en MongoDB;
el acuse de ingesta MQTT espera a que termine el guardado.

### 3. Abrir el Dashboard Frontend

Abrir **http://localhost:8080/** con el puerto de la plantilla. El backend sirve
el frontend; no abrir el HTML mediante doble clic.

La interfaz muestra ambas temperaturas, sus historiales, diagnóstico de
gateway/central y cortes eléctricos reportados. Cada sonda tiene un formulario
de intervalo y umbrales; la recuperación offline es común al nodo. En un banco
acordado, habilitar las dos opciones mesh del backend y la adaptación del gateway
según [MESH_INTEGRACION.md](MESH_INTEGRACION.md).

Los ajustes enviados se muestran pendientes hasta el reporte explícito del
nodo. El snapshot permite consultar la configuración vigente después de
reiniciar. Las lecturas acumuladas conservan hora original cuando está disponible,
con hora de recepción separada; un dato atrasado no sustituye al más reciente.
Sin conexión a la API se conserva lo último visible con aviso, sin inventar
temperaturas de simulación. El sondeo de cinco segundos no cambia el muestreo.

---

## 🧪 Pruebas Rápidas y Validación de la API

La documentación OpenAPI/Swagger está en **http://localhost:8080/api-docs**.
Los endpoints de configuración y energía se describen aquí; no todos cuentan
todavía con anotaciones Swagger.

Desde `software/backend`:

```powershell
npm test
```

Las pruebas verifican codecs anteriores, MQTT, deduplicación, metadatos de corte,
rangos por sonda, reportes explícitos y envío de configuración mediante la API.
Utilizan transporte/base simulados, además de una API Express local; no comprueban
el servidor AURA, SMTP ni sensores físicos.

| Método y ruta | Función |
|---|---|
| `GET /api/v1/health` | Salud del backend |
| `GET /api/v1/telemetria/latest?sensor=heladera` | Última lectura de heladera; usar `freezer` para la otra sonda |
| `GET /api/v1/telemetria/history?sensor=freezer&limit=20` | Historial por sonda |
| `POST /api/v1/telemetria` | Inserción HTTP de pruebas con UUID en `deviceId` |
| `GET /api/v1/umbrales`, `POST /api/v1/umbrales` | Umbrales de email de plataforma; no envían ajustes al micro |
| `GET /api/v1/alertas/emails`, `POST /api/v1/alertas/emails` | Destinatarios de alertas |
| `DELETE /api/v1/alertas/emails/:id` | Eliminar destinatario |
| `GET /api/v1/dispositivo/status` | Estados de sondas, gateway y conexión MQTT |
| `GET /api/v1/dispositivo/config?sensor=freezer` | Configuración reportada y último comando por sonda |
| `POST /api/v1/dispositivo/config` | Publicar ajustes; `202` no significa ejecución |
| `GET /api/v1/dispositivo/energia` | Primeras lecturas de cortes recibidas |
| `POST /api/v1/leer` | `409`: lectura forzada no definida |

Prueba MQTT aislada, con el UUID de ejemplo configurado como heladera. Desde la raíz:

```powershell
$mensajePrueba = '{"values":{"temp_c":4.25}}'
$mensajePrueba | docker compose exec -T mqtt-broker mosquitto_pub -q 1 -h localhost -t "devices/650e8400-e29b-41d4-a716-446655440001/data" -s
Invoke-RestMethod "http://localhost:8080/api/v1/telemetria/latest?sensor=heladera"
```

La lectura de prueba debe aparecer en el dashboard; no demuestra recepción
desde el hardware. No enviar estos ejemplos al broker compartido de AURA.

Las pruebas C++ de FIFO/CRC/ACK se evalúan al compilar `firmware/tests_mesh`.
Con un compilador de host compatible también se pueden ejecutar:

```powershell
g++ -std=c++17 firmware/tests_host/test_mesh.cpp -o firmware/tests_host/test_mesh.exe
./firmware/tests_host/test_mesh.exe
```

---

## 📡 Nodo Mesh (ESP-NOW + AURA)

### 1. Requisitos en Arduino IDE

- **Placa:** XIAO_ESP32S3, **USB CDC On Boot: Enabled**. Referencia de compilación:
  core Arduino ESP32 3.3.11.
- **Sensor:** OneWire, DallasTemperature, U8g2 y RTClib.
- **Gateway:** ArduinoMqttClient y ArduinoJson 7.
- **Sala:** sin bibliotecas externas al core ESP32.

| Componente del sensor | Pin inicial configurable |
|---|---|
| Sonda heladera | D2, bus 1-Wire propio |
| Sonda freezer | D3, bus 1-Wire propio |
| LED 1: fuera de rango | D0 |
| LED 2: desconexión | D1 |
| OLED SH1106 y RTC DS3231 | I2C D4/D5 |
| Presencia de alimentación | Deshabilitada (`-1`) hasta definir el circuito |

Cada bus 1-Wire requiere su pull-up y cada LED una resistencia adecuada.
Configurar únicamente una señal lógica compatible para detectar alimentación.

### 2. Configurar y Cargar el Firmware

Copiar la plantilla junto al sketch que se va a cargar:

```powershell
if (!(Test-Path firmware/nodo_mesh/config_local.h)) {
    Copy-Item firmware/nodo_mesh/config_local.h.example firmware/nodo_mesh/config_local.h
}
```

Completar MAC de padre/gateway y canal. Sala y gateway tienen sus propias
plantillas de configuración. En el gateway completar red, broker, API,
tenant y UUID de ambas sondas. `config_local.h` está excluido de Git;
no versionar claves, tokens ni direcciones privadas del entorno.

Cargar **firmware/nodo_mesh/nodo_mesh.ino** para el sensor. Los sketches de
sala y gateway requieren coordinar su carga con el responsable de infraestructura.
Los tres deben utilizar la misma versión de aplicación y el mismo canal.
Abrir monitor serie a **115200 baudios** y revisar NVS, MAC, muestras y ambos ACK.

Si el IDE presenta problemas con includes entre carpetas, usar las copias de
`firmware/monolitico/`, con `config_local.h` al lado del sketch elegido.
Se regeneran desde los originales y nunca se editan a mano:

```powershell
python firmware/generar_mesh_monolitico.py
```

### 3. Funcionamiento y Configuración Remota

Valores iniciales: heladera cada **60 s**, freezer cada **300 s**, recuperación
offline cada **300 s**. Cada intervalo de muestreo admite 5–86400 segundos.
Los límites iniciales son 2/6 °C y −25/−15 °C respectivamente, ajustables desde
la interfaz y guardados en NVS. LED 1 evalúa ambas sondas localmente.

Ejemplo de comando de heladera:

```json
{
  "command": "set_config",
  "params": {
    "sensor": "heladera",
    "interval_s": 60,
    "min_c": 2,
    "max_c": 6,
    "recovery_s": 300
  },
  "command_id": "<UUID generado por el backend>"
}
```

El gateway convierte el comando a binario. El nodo valida cada parámetro,
guarda y reporta lo aplicado. No utiliza DR, ADR ni el módulo SX1262.
La pantalla se apaga en batería/offline; la alarma local sigue funcionando.
Tras envío inicial y tres reintentos sin confirmación, se enciende LED 2 y
se pasa a recuperación espaciada. El sensor conserva **30 lecturas por sonda**
y la primera del corte protegida; al llenar la FIFO descarta ordinarias antiguas.
El detalle de capacidad, timestamps y aceptación central está en la guía mesh.

### 4. Validación en Hardware

Comprobar sondas independientes, ajustes conservados tras reiniciar, LED de
alarma sin red, caída de gateway y de central por separado, detección de batería,
primera muestra protegida y reenvío sin duplicados. La entrada de alimentación
y la hora del RTC deben verificarse antes de ensayar un corte eléctrico.
La compilación y las pruebas locales no sustituyen este banco físico ni la
validación de API/contrato con AURA.

---

## 🛑 Detener Servicios Docker

Detener el backend con `Ctrl+C`. Desde la raíz:

```powershell
docker compose down
```

MongoDB conserva su volumen y Mosquitto sus carpetas de datos/logs.
No utilizar `docker compose down -v` para conservar los datos.
