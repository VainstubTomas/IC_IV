# ❄️ Sistema Integral IoT de Telemetría - Sistemas de refrigeración (IC_IV)

Sistema de adquisición, transporte, almacenamiento y visualización de temperatura
para heladera y freezer. Utiliza dos sondas DS18B20, un nodo XIAO ESP32-S3,
una malla ESP-NOW basada en el ejemplo de AURA, MongoDB y un dashboard web.

El nodo mide y controla alarmas localmente, permite configurar cada sonda
desde la interfaz y conserva mediciones pendientes cuando falla la comunicación.
También registra la primera lectura del paso a batería mediante una entrada
de alimentación configurable.

La integración sigue el **contrato AURA v3.0** recibido: una placa, un UUID,
dos mediciones independientes y telemetría central solo por REST. Las alertas,
estados y comandos usan MQTT. El backend del proyecto y su dashboard usan
un **broker local separado**, con espejo opcional del gateway.
La adaptación de infraestructura y lo pendiente de validar en clase están en
[MESH_INTEGRACION.md](MESH_INTEGRACION.md).

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
               [ API AURA ]     [ Broker MQTT local ]
               Ingesta REST             │
                                   [ Backend IC IV ]
                                       │       │
                                       ▼       ▼
                                   [ MongoDB ] [ Dashboard ]

Gateway ↔ Broker MQTT AURA: estados, comandos, respuestas y alertas.
Las mediciones mesh hacia AURA entran solo por la API REST.

ACK gateway y ACK central: gateway → sala → sensor
Configuración: dashboard → backend → MQTT → gateway → sala → sensor
```

- **Dos sondas:** temperatura de heladera y freezer, con intervalos y límites propios.
- **Sensor:** alarma local por umbral, detección de batería, OLED y cola persistente.
- **Sala:** relay de ruta fija; reenvía mensajes sin fabricar confirmaciones finales.
- **Gateway:** relaciona MAC con un UUID AURA y adapta radio, REST y MQTT.
- **Backend/MongoDB:** guarda lecturas, cortes y estados, y gestiona alertas por correo.
- **Dashboard:** consulta la API cada cinco segundos y permite ajustar cada sonda.

El sensor conserva los datos hasta obtener confirmación del central. Que el
gateway reciba una trama o MQTT confirme una publicación no demuestra que
AURA haya guardado el evento. Gateway y sala necesitan la adaptación compatible;
el firmware mesh original no incorpora estos dos ACK finales. Las alertas usan
un ACK distinto de publicación, que no promete persistencia central.

---

## 🧱 Estructura del repositorio

```text
IC_IV/
 ├── firmware/                       # Firmware actual de nuestro nodo
 │   ├── nodo_mesh/                  # .ino, app.h y plantilla config_local.h.example
 │   └── mesh_comun/                 # Protocolo, radio, FIFO y persistencia compartidos
 ├── dispositivos/E1-PB-LECA-HFR01/   # Entrega autocontenida para el repo AURA (generada)
 ├── software/
 │   ├── backend/                    # API, MongoDB, MQTT y alertas
 │   └── frontend/                   # Dashboard de heladera y freezer
 ├── tests/                          # Todas las pruebas del proyecto
 │   ├── backend/                    # Pruebas Node.js; se ejecutan con npm test
 │   ├── firmware/
 │   │   ├── host/                   # Comprobaciones C++ de FIFO/ACK/CRC
 │   │   └── esp32/tests_mesh/        # Las mismas comprobaciones al compilar en Arduino
 │   └── manual/                     # Scripts de banco MQTT local
 ├── legacy/                         # Versiones anteriores, fuera del flujo mesh actual
 │   ├── firmware/main/              # Prototipo LoRa punto a punto
 │   ├── firmware/nodo_lorawan/       # Firmware LoRaWAN anterior
 │   ├── configuration/              # Codec LoRaWAN anterior
 │   └── docs/                       # Contrato v2 archivado
 ├── simulaciones/mesh/              # Banco/propuestas para revisión de la cátedra
 │   ├── nodo_sala_mesh/             # Adaptador real del relay ESP-NOW
 │   └── nodo_gateway_mesh/          # Adaptador real hacia AURA
 ├── herramientas/nvs/               # Respaldo/validación NVS anterior antes de migrar
 ├── herramientas/aura/              # Genera el paquete de dispositivo
 ├── herramientas/arduino/
 │   ├── generar_mesh_monolitico.py  # Regenera copias; no copia credenciales
 │   └── monolitico/                 # Copias autocontenidas opcionales para Arduino IDE
 ├── mosquitto/config/               # Broker del banco local
 ├── docker-compose.yml             # MQTT y MongoDB para pruebas locales
 ├── CONTRATO_MQTT.md                # Contrato recibido, sin modificaciones
 ├── MESH_INTEGRACION.md             # Integración v3, pruebas y pendientes con AURA
 ├── COMANDOS.md                     # Comandos operativos
 └── README.md
```


El `.ino` del sensor es la entrada de Arduino; `app.h` contiene su funcionamiento
y `mesh_comun` contiene archivos que ese programa necesita. Los archivos locales
`.env`, `config_local.h` y `credenciales.h` no se versionan.

La carpeta `simulaciones/mesh` reúne la propuesta de infraestructura para el banco:
contiene firmware real de sala y gateway, no mediciones ficticias ni un simulador
de AURA. Su carga se coordina con la cátedra. `herramientas/arduino/monolitico`
contiene copias generadas alternativas: no se editan a mano. `legacy` conserva
lo anterior; sus archivos no intervienen al compilar el nodo mesh actual.

Los tests se mantienen versionados para poder repetir la validación. Sus
ejecutables y archivos de compilación no se suben. Ver [tests/README.md](tests/README.md).
La guía PDF de pruebas anterior usa la distribución previa de carpetas; consultar
[MESH_INTEGRACION.md](MESH_INTEGRACION.md) para el procedimiento **v3**; también
cambiaron identidad, comandos y aceptación de ingesta.

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
| `MQTTBROKERURL` | Broker local del espejo, separado de AURA |
| `AURA_DEVICE_ID` | Único UUID AURA de la placa con las dos sondas |
| `AURA_GATEWAY_DEVICE_ID` | UUID del gateway para su estado y LWT |
| `ICIV_TRANSPORT` | `mesh` por defecto; `lorawan` conserva validación de comandos anteriores |
| `MQTT_CLIENT_ID` | ID estable y exclusivo de esta instancia |
| `AURA_CONFIG_EXPERIMENTAL` | Publicación de ajustes acordados; por defecto `false` |
| `BROKERUSERNAME`, `BROKERPASSW`, `MQTTBROKERCAPATH` | Autenticación/CA MQTT cuando corresponda |
| `SMTPHOST`, `SMTPPORT`, `SMTPSECURE`, `SMTPUSER`, `SMTPPASS`, `SMTPFROM` | Alertas por correo |

`AURA_BRIDGE_DEVICE_ID` sigue como alias del gateway. Las variables UUID por
sonda de la versión previa deben reemplazarse por `AURA_DEVICE_ID`. Los UUID deben corresponder al alta real
en AURA; las MAC identifican placas y no reemplazan esos UUID.
En un banco aislado pueden utilizarse UUID ficticios válidos.

```powershell
npm start
# Desarrollo:
# npm run dev
```

El backend inicializa MongoDB e índices antes de conectarse a MQTT. Se suscribe
con QoS 1 a `devices/+/data`, `status`, `response` y `alerts/+/+`, filtrando los dispositivos
configurados. La sesión persistente necesita conservar `MQTT_CLIENT_ID` y
depende de los límites del broker. `ingest_id` + sonda permiten deduplicar en MongoDB;
el acuse de ingesta MQTT espera a que termine el guardado.

### 3. Abrir el Dashboard Frontend

Abrir **http://localhost:8080/** con el puerto de la plantilla. El backend sirve
el frontend; no abrir el HTML mediante doble clic.

La interfaz muestra ambas temperaturas, sus historiales, diagnóstico de
gateway/central y cortes eléctricos reportados. Cada sonda tiene un formulario
de intervalo y umbrales; la recuperación offline es común al nodo. En un banco
acordado, habilitar los comandos del backend y el espejo local del gateway
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

Las pruebas verifican codecs anteriores, MQTT, deduplicación, timestamps y alertas de corte,
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
| `GET /api/v1/dispositivo/config` | Configuración completa reportada y último comando de la placa |
| `POST /api/v1/dispositivo/config` | Publicar ajustes; `202` no significa ejecución |
| `GET /api/v1/dispositivo/energia` | Alertas de cortes con hora original cuando existe |
| `POST /api/v1/leer` | `409`: lectura forzada no definida |

Prueba MQTT aislada, con el UUID de ejemplo configurado como placa. Desde la raíz:

```powershell
$mensajePrueba = '{"values":{"temp_heladera_c":4.25,"temp_freezer_c":-18}}'
$mensajePrueba | docker compose exec -T mqtt-broker mosquitto_pub -q 1 -h localhost -t "devices/650e8400-e29b-41d4-a716-446655440001/data" -s
Invoke-RestMethod "http://localhost:8080/api/v1/telemetria/latest?sensor=heladera"
```

La lectura de prueba debe aparecer en el dashboard; no demuestra recepción
desde el hardware. No enviar estos ejemplos al broker compartido de AURA.

Las pruebas C++ de FIFO/CRC/ACK se evalúan al compilar `tests/firmware/esp32/tests_mesh`.
Con un compilador de host compatible también se pueden ejecutar, junto con
las pruebas de NVS con fallos simulados:

```powershell
g++ -std=c++17 tests/firmware/host/test_mesh.cpp -o tests/firmware/host/test_mesh.exe
./tests/firmware/host/test_mesh.exe
g++ -std=c++17 -Itests/firmware/host/fakes tests/firmware/host/test_storage.cpp -o tests/firmware/host/test_storage.exe
./tests/firmware/host/test_storage.exe
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

Completar MAC de padre/gateway y canal. Los roles en `simulaciones/mesh/` tienen sus propias
plantillas de configuración. En el gateway completar red, broker, API,
tenant, UUID único de la placa y UUID del gateway. El espejo local tiene
host/puerto/credenciales propios y empieza apagado. `config_local.h` está excluido de Git;
no versionar claves, tokens ni direcciones privadas del entorno.

Cargar **firmware/nodo_mesh/nodo_mesh.ino** para el sensor. Los sketches de
`simulaciones/mesh/nodo_sala_mesh` y `simulaciones/mesh/nodo_gateway_mesh` requieren coordinar su carga con el responsable de infraestructura.
Los tres deben utilizar la misma versión de aplicación y el mismo canal.
Abrir monitor serie a **115200 baudios** y revisar NVS, MAC, muestras y ambos ACK.
El sensor incluye `partitions.csv` con NVS de 128 KiB para flash de 8 MiB.
Si ya tenía el mesh anterior, respaldar y revisar su NVS **antes** de cambiar
la tabla; seguir la migración de la guía. No se borra ni convierte la cola anterior automáticamente.

Si el IDE presenta problemas con includes entre carpetas, usar las copias de
`herramientas/arduino/monolitico/`, con `config_local.h` al lado del sketch elegido.
Se regeneran desde los originales y nunca se editan a mano:

```powershell
python herramientas/arduino/generar_mesh_monolitico.py
```

### 3. Funcionamiento y Configuración Remota

Valores iniciales: heladera cada **60 s**, freezer cada **300 s**, recuperación
offline cada **300 s**. Cada intervalo de muestreo admite 5–86400 segundos.
Los límites iniciales son 2/6 °C y −25/−15 °C respectivamente, ajustables desde
la interfaz y guardados en NVS. LED 1 evalúa ambas sondas localmente.

Ejemplo de cambio del intervalo de heladera (los demás parámetros se conservan):

```json
{
  "command": "set_config",
  "params": {"intervalo_heladera_s": 120},
  "command_id": "<UUID generado por el backend>"
}
```

El gateway convierte el comando a binario. El nodo valida cada parámetro,
guarda y reporta lo aplicado. No utiliza DR, ADR ni el módulo SX1262.
La pantalla se apaga en batería/offline; la alarma local sigue funcionando.
Tras envío inicial y tres reintentos sin confirmación, se enciende LED 2 y
se pasa a recuperación espaciada. El sensor conserva **30 lecturas por sonda**
(alertas también ocupan lugar), las primeras del corte protegidas y un
registro de transición a batería independiente de las sondas; al llenar la FIFO descarta ordinarias antiguas.
El detalle de capacidad, timestamps y aceptación central está en la guía mesh.
Una sonda inválida no produce medición; falla/recuperación se informa mediante
alertas del contrato. Se rechaza también el centinela de arranque de 85 °C.

Para entregar al repo de la cátedra, regenerar
`python herramientas/aura/preparar_dispositivo.py` y copiar la carpeta del
dispositivo. Sala y gateway van como propuesta de infraestructura separada,
como requiere CONTRIBUTING del repo AURA. La ficha y sus tests viajan con el nodo.

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
