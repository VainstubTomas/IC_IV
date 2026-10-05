# Contrato MQTT — Proyecto AURA

**Estado**: normativo. Ante una discrepancia entre este documento y otro, **manda este**.
**Versión del contrato**: 2.0 · **Última actualización**: 2026-09-25
**Alcance**: mensajería MQTT entre los dispositivos IoT y la plataforma (backend FastAPI).
Desde la v2.0 los dispositivos llegan por **LoRaWAN**, a través de ChirpStack y del
`lorawan-bridge` ([ADR-002](adr/ADR-002-lorawan-reemplaza-a-la-mesh-espnow.md)). No cubre la API
REST salvo donde se la menciona como camino alternativo.

Este documento se escribió **relevando el código**, no de memoria. Cada afirmación normativa
apunta al archivo y la función que la implementa hoy, o está marcada explícitamente como
**pendiente**. Las secciones marcadas ⚠️ describen desajustes reales entre las partes.

> **Estado de la v2.0.** El lado del backend (tópicos, regexes, validaciones) está
> implementado. **El `lorawan-bridge` todavía no existe**: todo lo que este documento dice de
> él es especificación, marcada 🔧. Y la ingesta MQTT del backend **no persiste nada hoy**
> (§10, pendiente 1): hasta que eso se arregle, ningún emisor —ni el bridge ni otro— llega a
> la base.

---

## 0. Por qué existe este documento

El contrato MQTT es lo único que une el codec de un dispositivo LoRaWAN con una expresión
regular en Python corriendo en un contenedor. No hay compilador, ni tipos, ni tests que crucen
esa frontera: si un lado cambia el nombre de un tópico o un campo, el otro **no falla con un
error** — simplemente deja de reconocer el mensaje y lo descarta con un `logger.warning`. El
sistema sigue "funcionando" y los datos dejan de llegar.

LoRaWAN agrega una segunda frontera con el mismo modo de falla: casi todo lo que está mal
configurado en ChirpStack descarta los paquetes **sin una sola línea de log** (ver el
`README.md` del repo `IC-lorawan-test`). Es exactamente el modo de falla que la cátedra ya
evalúa en los TPs de los alumnos (ver `../../tps/CLAUDE.md`, sección de cruce
firmware↔simulador). Este documento es para que el proyecto cumpla lo que le pide a los
alumnos.

---

## 1. Brokers y conexión

Hay **dos brokers** con responsabilidades separadas, y **solo el `lorawan-bridge` habla con
los dos**:

```
nodos LoRaWAN ─► gateway ─► [broker LoRaWAN] ◄─► lorawan-bridge ◄─► [broker AURA] ◄─► backend
                            interno de ChirpStack                    este contrato
```

| Broker | Qué circula | Quién lo usa |
|---|---|---|
| **LoRaWAN** (el Mosquitto del stack ChirpStack) | tópicos propios de ChirpStack (§2.2) | gateway-bridge, ChirpStack y el `lorawan-bridge`. **Ningún otro componente de AURA se suscribe acá.** |
| **AURA** | `devices/…`, `alerts/…`, `system/…` (§2.1) | el `lorawan-bridge` y el backend |

Parámetros del broker AURA:

| Parámetro | Valor | Dónde está definido |
|---|---|---|
| Host / puerto (backend) | `localhost:1883` por defecto | `MQTTService.__init__` en `app/backend/services/mqtt_service.py`. ⚠️ Nadie lo instancia con otro valor: la parametrización por entorno es parte del pendiente 1 de §10 |
| TLS | no | — |
| Autenticación | ninguna | — |
| Cliente backend | `paho-mqtt` | `app/backend/services/mqtt_service.py` |
| Cliente bridge | 🔧 pendiente | — |

⚠️ **Sin TLS ni credenciales.** Aceptable en la red de laboratorio; **no** en el despliegue
del campus. Cualquiera en la red puede publicar en `devices/+/command` y accionar
dispositivos, o en `devices/+/data` e inyectar mediciones. Ver §10, pendiente 8.

Del lado LoRaWAN la radio sí está cifrada: cada dispositivo tiene su propia clave AES (OTAA)
y el gateway no puede leer ni falsificar el contenido. La debilidad está del bridge para acá.

### Last Will and Testament 🔧

El bridge tiene su propio `device_id` en AURA (`BRIDGE_DEVICE_ID`) y registra un LWT antes de
conectar al broker AURA:

- **Tópico**: `devices/<BRIDGE_DEVICE_ID>/status`
- **Payload**: `{"status":"offline"}`
- **Retain**: `true` · **QoS**: `1`

Un `offline` retenido del bridge significa que **ningún** dispositivo LoRaWAN está llegando,
aunque cada uno siga apareciendo `online` con su último estado retenido. El backend debe
leerlo así.

El `offline` **de cada dispositivo** no puede venir de un LWT, porque el nodo no tiene
conexión MQTT propia: lo infiere el bridge (§3.2).

---

## 2. Árbol de tópicos

### 2.1 Broker AURA — lo que ve el backend

**Regla de oro: el `device_id` viaja en el tópico, no en el payload.** El backend lo extrae
con `match.group(1)` sobre la regex del tópico (`DeviceIntegrationService`, en
`app/backend/services/device_integration.py`). Un payload que traiga `device_id` adentro no es
un error, pero ese campo **se ignora**.

El `device_id` es un **UUID** de AURA, no el DevEUI del dispositivo ni un nombre legible.

```
devices/<device_id>/data        ← bridge publica   → backend consume
devices/<device_id>/status      ← bridge publica   → backend consume   [retain]
devices/<device_id>/command     ← backend publica  → bridge consume
devices/<device_id>/response    ← bridge publica   → backend consume
alerts/<device_id>/<tipo>       ← (sin emisor hoy) → backend
system/health                   ← (sin emisor hoy) → backend
```

**Sin prefijo `aura/`.** Los tópicos arrancan directamente en `devices/`. Esto contradice
otros documentos del repo; ver §8.

**El árbol no cambió respecto de la v1.x.** Cambió quién lo emite. Para el backend, el bridge
es un gateway más que publica en nombre de muchos dispositivos, igual que lo era el gateway de
la mesh ESP-NOW.

Regexes exactas que reconoce el backend (`DeviceIntegrationService.__init__`):

```python
"device_status":   r"^devices/([^/]+)/status$"
"device_data":     r"^devices/([^/]+)/data$"
"device_command":  r"^devices/([^/]+)/command$"
"device_response": r"^devices/([^/]+)/response$"
"alert":           r"^alerts/([^/]+)/([^/]+)$"
"system_health":   r"^system/health$"
```

Un tópico que no matchee ninguna se descarta con `logger.warning(f"Topic no reconocido")`
y `process_mqtt_message` devuelve `False`. **No hay excepción ni reintento.**

### 2.2 Broker LoRaWAN — lo que consume el bridge 🔧

Tópicos de la integración MQTT de ChirpStack v4 (JSON, `json=true` en `chirpstack.toml`):

```
application/<appId>/device/<devEUI>/event/up       uplink con el payload ya decodificado
application/<appId>/device/<devEUI>/event/join     el dispositivo hizo OTAA
application/<appId>/device/<devEUI>/event/status   batería y margen del enlace
application/<appId>/device/<devEUI>/event/txack    el gateway transmitió un downlink
application/<appId>/device/<devEUI>/event/ack      el nodo acusó un downlink confirmado
application/<appId>/device/<devEUI>/event/log      errores de ChirpStack sobre el device
```

El bridge se suscribe a `application/<appId>/device/+/event/+`, con **un solo `appId`**: el de
la aplicación `aura`. Con esa suscripción cubre todos los dispositivos, presentes y futuros.

Los tópicos `<región>/gateway/<gatewayEUI>/…` (gateway-bridge ↔ ChirpStack) son internos del
stack LoRaWAN y **nadie de AURA los toca**.

### 2.3 Identidad: de DevEUI a `device_id` 🔧

ChirpStack identifica por DevEUI; AURA, por UUID. El puente entre ambos es un **tag del
dispositivo en ChirpStack**:

```
aura_device_id = <uuid del dispositivo en AURA>
```

ChirpStack incluye los tags del dispositivo en cada evento (`deviceInfo.tags`), así que el
bridge traduce sin tabla propia ni consulta a la base: el mapeo vive donde se da de alta el
dispositivo.

| Concepto de ChirpStack | Qué es en AURA |
|---|---|
| Tenant | uno: la UNRaf |
| Application | una: `aura` |
| Device profile | un **tipo** de nodo: codec, versión LoRaWAN, clase A/C, límite de downlink (§3.3) |
| Device | cada placa, con su tag `aura_device_id` |

**Agregar un dispositivo no requiere tocar ni el bridge ni el backend**: se lo da de alta en
ChirpStack con su tag y en AURA con el mismo UUID.

Un evento de un dispositivo **sin** `aura_device_id` se descarta con `logger.warning` y suma a
un contador de huérfanos. **No se publica en ningún tópico de AURA**, ni siquiera con el DevEUI
en lugar del UUID: publicarlo rompería la regla de que el `device_id` es un UUID de AURA.

Para la bajada (§3.3) el bridge necesita el mapeo inverso, UUID → DevEUI. Lo arma con los tags
que ve en los uplinks y, si un UUID no aparece, lo consulta a la API de ChirpStack.

---

## 3. Mensajes

### 3.1 `devices/<device_id>/data` — telemetría

**Emisor**: bridge, uno por cada `event/up` con payload decodificado · **QoS 1** ·
**retain `false`**

```json
{ "values": { "temp_c": 21.5, "lux": 123.4 },
  "ingest_id": "5f0c1b1e-8a6d-4a55-9f2b-7c3e2d1a0b99" }
```

| Campo | Obligatorio | Tipo | Notas |
|---|---|---|---|
| `values` | **sí** | objeto | El campo `object` del uplink, tal como lo devolvió el codec del device profile. Único campo requerido (`_validate_data_payload`). Sin él el mensaje se descarta. |
| `ingest_id` | no | UUID | El `deduplicationId` que ChirpStack asigna al uplink (§5). ⚠️ El backend **todavía no lo lee** por MQTT. |
| `unit` | no | objeto | El backend lo lee (`_process_device_data`); ningún emisor lo manda. Default `{}`. |
| `quality` | no | entero | Leído, nadie lo manda. Default `100`. |

**`values` es opaco para el bridge.** Su forma la define el codec de cada device profile, no
el bridge. Un codec nuevo no requiere cambiar el bridge.

**Solo mediciones.** En `values` no van RSSI, SNR, contadores de trama ni nada que describa el
enlace o el estado del nodo: eso va a `status` (§3.2). Mezclarlos fue el problema de `pend` y
`desc` en la v1.x: un panel que graficaba "todas las variables del dispositivo" graficaba el
buffer de la mesh como si fuera un sensor.

Un uplink **sin** `object` (el device profile no tiene codec, o el codec falló) **no se
publica en `data`**. El bridge lo registra con `logger.warning`: publicar los bytes crudos en
base64 dentro de `values` metería basura en la serie temporal.

⚠️ **La hora del dato es la de llegada al backend.** `_process_device_data` no lee ningún
timestamp del payload. ChirpStack sabe cuándo se recibió el uplink (`time`), pero ese dato
todavía no tiene campo en el contrato. Con uplinks cada pocos minutos el error es chico; con
una cola retenida tras una caída del backend (§5) deja de serlo.

### 3.2 `devices/<device_id>/status` — estado del dispositivo

**Emisor**: bridge · **QoS 1** · **retain `true`** ← distinto de `data`

El bridge publica un `status` por cada uplink, join o evento de estado de ChirpStack. Todo lo
que no es `status` va **anidado bajo `details`**, que es lo que el backend persiste:

```json
{
  "status": "online",
  "details": {
    "evento": "up", "rssi": -87, "snr": 6.5, "gateway_eui": "24e124fffefafc09",
    "fcnt": 142, "dr": 5, "f_port": 1
  }
}
```

| Campo | Obligatorio | Notas |
|---|---|---|
| `status` | **sí** | Único requerido (`_validate_status_payload`). Valores: `"online"`, `"offline"`. |
| `details` | no | El backend lee `payload.get("details", {})` en `_process_device_status`. |

Qué pone el bridge en `details` según el evento de ChirpStack 🔧:

| Evento | `details` |
|---|---|
| `event/up` | `evento: "up"`, `rssi` y `snr` del mejor gateway, `gateway_eui`, `fcnt`, `dr`, `f_port` |
| `event/join` | `evento: "join"` |
| `event/status` | `evento: "status"`, `bateria` y `margen` tal como los informa ChirpStack |

**`offline` por inferencia 🔧.** Como no hay LWT por dispositivo, el bridge publica
`{"status":"offline","details":{"motivo":"sin_uplinks"}}` (retain) cuando un dispositivo pasa
`3 ×` su intervalo de uplink sin transmitir. El intervalo sale del campo *uplink interval* del
device profile en ChirpStack, que ChirpStack también usa para marcar el dispositivo como
inactivo. El siguiente uplink lo vuelve a `online`.

`retain: true` es deliberado: un backend que arranca después recibe el último estado conocido
de cada dispositivo de inmediato, sin esperar el próximo uplink, que en LoRaWAN pueden ser
minutos.

### 3.3 `devices/<device_id>/command` — comando hacia el dispositivo

**Emisor**: backend · **Consumidor**: bridge · **QoS 1**

```json
{ "command": "turn_on", "params": { "canal": 1 }, "command_id": "c-2026-0001" }
```

| Campo | Obligatorio | Notas |
|---|---|---|
| `command` | **sí** | `_validate_command_payload` |
| `params` | no | Default `{}` |
| `command_id` | no | 🔧 Si viene, el bridge lo devuelve en cada `response` de ese comando (§3.4). Sin él, las respuestas no se pueden correlacionar con el comando. |

**Camino del comando 🔧.** El bridge busca el DevEUI del `device_id` (§2.3) y encola el
downlink con la **API de ChirpStack** (`EnqueueDeviceQueueItem`), pasando el JSON como
`object` para que lo codifique el `encodeDownlink` del device profile. Se usa la API y no el
tópico `…/command/down` porque la API devuelve el `id` del ítem encolado, y ese `id` es lo que
permite atar el `txack` y el `ack` posteriores a este comando.

**Límite de tamaño 🔧.** El límite real es en **bytes en el aire después del codec** y depende
del *data rate*. Por eso **no lo valida el backend** (quitado de `_process_device_command` en
la v2.0): el backend no puede saberlo. Lo valida el bridge contra el tag
`aura_max_downlink_bytes` del **device profile**. Si el comando no entra, o el profile no tiene
ese tag, el bridge **no lo encola** y publica `rechazado` en `response` con el motivo. Ningún
comando se descarta sin respuesta.

**Latencia.** Un dispositivo **clase A** solo abre la ventana de recepción después de
transmitir: el comando sale con su próximo uplink, que puede tardar minutos. Los dispositivos
que tienen que ejecutar un comando en segundos (actuadores de iluminación, cerraduras) van en
**clase C**, que escucha todo el tiempo y es viable porque están alimentados de red. La clase
es un atributo del device profile, no del comando.

### 3.4 `devices/<device_id>/response` — confirmación

**Emisor**: bridge · **QoS 1** · **retain `false`**

```json
{ "status": "transmitido", "details": { "command_id": "c-2026-0001", "queue_item_id": "…" } }
```

| `status` | Qué significa | De dónde sale 🔧 |
|---|---|---|
| `encolado` | ChirpStack aceptó el downlink en la cola del dispositivo | respuesta de `EnqueueDeviceQueueItem` |
| `transmitido` | el gateway lo emitió por radio | `event/txack` |
| `recibido` | el nodo acusó recibo (solo en downlinks confirmados) | `event/ack` con `acknowledged: true` |
| `rechazado` | no se encoló, o el nodo no lo acusó | error de la API, tamaño excedido (§3.3), o `event/ack` con `acknowledged: false`; el motivo va en `details.motivo` |

El backend acepta **solo** estos cuatro estados (`RESPONSE_STATES` en
`DeviceIntegrationService`). Cualquier otro, incluido `enviado_a_mesh` de la v1.x, se rechaza
con `logger.error`.

**Ninguno confirma ejecución.** `recibido` significa que el downlink llegó a la radio del
nodo, no que el firmware hizo lo que pedía el comando. El registro de `response` incluye
`confirma_ejecucion: False` en todos los casos, para que ningún consumidor lo lea como acuse de
ejecución. Confirmar ejecución requiere que el firmware lo informe en un uplink posterior, y
eso no está especificado todavía.

### 3.5 `alerts/<device_id>/<tipo>` y `system/health`

Declarados en las regexes del backend, **sin emisor**. Reservados; no usar hasta que se
especifiquen acá.

---

## 4. Handlers del despacho

| Handler | Tópico que lo dispara | Estado |
|---|---|---|
| `_process_device_status` | `devices/<id>/status` | ✅ llama al Device Service para persistir (ver ⚠️ de §10.1) |
| `_process_device_data` | `devices/<id>/data` | ✅ llama al Device Service para persistir (ver ⚠️ de §10.1) |
| `_process_device_command` | `devices/<id>/command` | ✅ valida y registra en memoria |
| `_process_device_response` | `devices/<id>/response` | ✅ valida y registra en memoria |
| `_process_alert` | `alerts/<id>/<tipo>` | ✅ valida y registra en memoria |
| `_process_system_health` | `system/health` | ✅ valida y registra en memoria |

Los cuatro últimos **solo validan y registran en memoria** (`last_command`, `last_response`,
`last_alert`, `last_system_health`). Ese estado se pierde al reiniciar el proceso.

Validaciones que aplican:

| Tópico | Requiere | Rechaza |
|---|---|---|
| `devices/<id>/command` | `command` | — (el tamaño lo valida el bridge, §3.3) |
| `devices/<id>/response` | `status` | `status` fuera de `RESPONSE_STATES` |
| `alerts/<id>/<tipo>` | — (`severity` default `unknown`) | `severity` fuera de `{info, warning, high, critical, unknown}` |
| `system/health` | `status` | — |

Además, `process_mqtt_message` descarta con `warning` cualquier payload que no sea un objeto
JSON, antes de despachar.

---

## 5. Entrega, duplicados e idempotencia

**Todo el sistema usa QoS 1: "al menos una vez".** El broker puede entregar el mismo mensaje
más de una vez, así que todo consumidor debe ser idempotente.

| Capa | Mecanismo | Alcance |
|---|---|---|
| Radio → ChirpStack | ChirpStack junta las recepciones del mismo uplink por varios gateways y emite **un** evento con un `deduplicationId` | varios gateways, un solo dato |
| Nodo → ChirpStack | uplinks confirmados con reintentos (firmware de `IC-lorawan-test`) | ⚠️ cómo trata ChirpStack un reintento con el mismo `fCnt` está **sin verificar en banco** |
| Bridge → AURA | `ingest_id` = `deduplicationId` en `data` 🔧 | sobrevive a reinicios del bridge: el id lo genera ChirpStack, no el bridge |
| Backend (REST) | `ingest_id` con `unique=True` en `ts_telemetry` (`models/telemetry.py`) | deduplicación real en base |
| Backend (MQTT) | ⚠️ **ninguno**: `_process_device_data` no lee `ingest_id` | pendiente 3 de §10 |

La v1.x tenía la asimetría inversa: el `ingest_id` solo existía en el camino REST y por MQTT la
deduplicación dependía de un array en la RAM del gateway de la mesh. Con LoRaWAN el
identificador existe de origen y viaja por MQTT. Falta que el backend lo use.

### Mensajes durante una caída ⚠️

MQTT no tiene colas: el broker entrega a quien esté suscripto **en ese momento**. Para que
guarde los mensajes QoS 1 mientras un consumidor está caído, el consumidor tiene que conectar
con un **`client_id` fijo y sesión persistente** (`clean_session=False`).

Hoy el backend hace lo contrario: `client_id=f"aura-mqtt-service-{uuid4().hex[:8]}"`, distinto
en cada arranque (`MQTTService._create_client`). **Cada reinicio del backend es un hueco en la
serie.** Lo mismo vale para el bridge del lado de ChirpStack: si se cae, pierde uplinks que
LoRaWAN no retransmite.

Si algún día hay más de una réplica del backend, la suscripción pasa a ser compartida
(`$share/aura-backend/devices/+/data`), para que cada mensaje lo procese una sola réplica.

---

## 6. El camino REST alternativo

`POST /api/v1/telemetry/ingest` (`app/backend/api/endpoints/telemetry.py`) sigue existiendo y
es el único camino que hoy deduplica en base. **Desde la v2.0 no tiene emisor**: lo usaba el
gateway de la mesh en modo `MODO_SIN_BACKEND=0`, y la mesh quedó congelada (§8).

La integración HTTP de ChirpStack, que podría apuntar a este endpoint, se descartó: postea el
formato de eventos de ChirpStack y no el de AURA, así que igual hacía falta un traductor, y
ataba el backend al formato de un proveedor. Ver
[ADR-002](adr/ADR-002-lorawan-reemplaza-a-la-mesh-espnow.md).

---

## 7. Versionado y evolución

El mensaje MQTT **no lleva ninguna marca de versión**. La v1.x tenía `AURA_PROTO_VERSION`, que
versionaba la trama ESP-NOW entre nodos de la mesh y nunca el mensaje MQTT; con la mesh fuera
de uso, no queda nada versionado.

Reglas de evolución de este contrato:

1. **Agregar un campo opcional** al payload no rompe nada: no requiere subir versión.
2. **Renombrar o eliminar un campo, o cambiar su tipo, unidad o conjunto de valores
   válidos**, es incompatible: subir la versión mayor de este documento y coordinar el
   despliegue.
3. **Cambiar el árbol de tópicos** es lo más caro: los dispositivos ya instalados siguen
   publicando en el viejo. Si pasa, el backend debe consumir ambos durante la transición.
4. Todo cambio incompatible se anota en §9.

Con LoRaWAN la regla 3 es más barata que con la mesh, porque los tópicos de AURA los arma el
bridge y no el firmware: se cambian sin reflashear nada. Lo que sí queda grabado en los nodos
es el **formato de sus bytes**, que interpreta el codec del device profile. Un cambio de
formato en el firmware requiere un device profile nuevo, no editar el codec del existente.

**Pendiente**: agregar un campo `v` al payload MQTT para que el backend pueda distinguir
generaciones de emisores sin adivinar por la forma del JSON.

---

## 8. ⚠️ Documentos y código que contradicen este contrato

**Este documento es el normativo**; los demás están desactualizados y hay que corregirlos:

| Documento | Qué dice | Estado |
|---|---|---|
| **Este documento** | `devices/<id>/data`, emitido por el `lorawan-bridge` | ✅ coincide con el backend; el bridge está pendiente |
| `docs/CONTRATOS_APIS.md` | `devices/+/data`, `devices/+/status`, `alerts/#` | ✅ coincide |
| `docs/Arquitectura_IoT_AURA.md:74-108` | `aura/devices/<id>/sensors/<tipo>/data` | ❌ jerarquía profunda que no existe |
| `CLAUDE.md` | remite a este documento | ✅ |
| `.cursorrules` | `devices/<device_id>/{data,status,command,response}`, remite a este documento | ✅ |
| `recursos/circutor_reader/mqtt_messages.json` | `aura/devices/circutor_…/telemetry` | ❌ mock data con otro esquema |
| `firmware/` de este repo y el repo público `aura-firmware` | mesh ESP-NOW con `enviado_a_mesh` y límite de 180 B | ⏸ **congelado**: implementa la v1.x. No es un error a corregir, es el estado de la mesh al congelarla (ADR-002) |

**El caso Circutor sigue abierto** y la v2.0 lo vuelve más claro. El medidor se integra por
Modbus TCP en `10.0.1.3:502`, su lector vive en el repositorio propio `aura-circutor`
([ADR-001](adr/ADR-001-repo-separado-para-el-medidor-circutor.md)) y todavía no publica en
ningún lado. Con el `lorawan-bridge`, el patrón queda establecido: **un adaptador por
protocolo que publica en `devices/<uuid>/…`**. La opción recomendada de ADR-001 (que el
Circutor adopte `devices/<device_id>/data`) es exactamente ese patrón. Mientras no se decida,
sus mensajes se descartan: `aura/devices/circutor_10.0.1.3/telemetry` no matchea ninguna regex
de §2.1.

---

## 9. Historial de cambios

| Versión | Fecha | Cambio |
|---|---|---|
| 2.0 | 2026-09-25 | **Los dispositivos pasan a LoRaWAN** ([ADR-002](adr/ADR-002-lorawan-reemplaza-a-la-mesh-espnow.md)). El árbol de tópicos no cambia; cambia el emisor: el `lorawan-bridge` reemplaza al gateway de la mesh. **Incompatible**: `response` pasa a `encolado`/`transmitido`/`recibido`/`rechazado` (se rechaza `enviado_a_mesh`); el backend deja de limitar el comando a 180 B, porque el límite pasa al bridge por device profile. Nuevos: §2.2 (tópicos de ChirpStack), §2.3 (tag `aura_device_id`), `ingest_id` en `data`, `details` de radio en `status`, `command_id`. Quedan sin objeto los campos `pend`, `desc` y `lux_sim` y el modo REST del gateway. |
| 1.2 | 2026-09-25 | §8: `CLAUDE.md` y `.cursorrules` alineados con este contrato; el lector Circutor ya vive en `aura-circutor`. |
| 1.1 | 2026-09-06 | Implementados los cuatro handlers faltantes de §4, con validación de longitud de comando y de severidad de alerta, y guardia de payload no-dict. |
| 1.0 | 2026-09-06 | Primera redacción, relevada del código en `main` tras la consolidación de ramas. Documenta el estado tal cual está, incluidos los desajustes conocidos. |

---

## 10. Pendientes

Ordenados por impacto:

1. ⚠️ **Que la ingesta MQTT persista algo.** Hoy no persiste nada: `mqtt_service.start()` no
   se llama nunca, `_on_message` crea tareas asyncio desde un thread sin event loop, y los dos
   endpoints del Device Service a los que llama `device_integration` no existen o esperan otro
   esquema. El diagnóstico completo y el plan están en
   `docs/superpowers/plans/2026-09-08-ingesta-mqtt-p0.md`. **Bloquea todo lo demás**: sin
   esto, el bridge publicaría al vacío.
2. **Implementar el `lorawan-bridge`** según §1-§5 (todo lo marcado 🔧).
3. **Que el backend lea `ingest_id` por MQTT** y deduplique igual que el camino REST (§5).
4. **Sesión persistente con `client_id` fijo** en backend y bridge (§5), para no perder los
   mensajes que lleguen durante un reinicio.
5. **Timestamp del dato** (§3.1): llevar la hora de recepción de ChirpStack al payload de
   `data`.
6. **Persistir `command`, `response`, `alerts` y `system/health`** (§4): hoy solo viven en
   memoria.
7. **Verificar en banco** qué hace ChirpStack con un uplink confirmado reintentado con el mismo
   `fCnt` (§5).
8. **Credenciales y ACL en el broker AURA** (§1): que solo el bridge pueda publicar en
   `devices/+/data`, `status` y `response`, y solo el backend en `devices/+/command`.
9. **Corregir los documentos de §8** para que dejen de contradecir al código.
10. **Un test de contrato**: publicar los payloads de ejemplo de este documento y verificar que
    `process_mqtt_message` los acepta. Es lo único que evita que este documento se
    desactualice como los de §8.

Pendientes de la v1.x que quedan **sin objeto** con la mesh congelada: el destino de los
diagnósticos del gateway (resuelto: van a `details`), `lux_sim`, mover `pend`/`desc` a
`status`, y parametrizar `TENANT_ID` / `GATEWAY_DEVICE_ID` del firmware de la mesh.
