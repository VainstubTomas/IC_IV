# Integración ESP-NOW de IC IV — contrato AURA v3.0

Este proyecto mide heladera y freezer con **una placa y un UUID de AURA**.
La base es el encadenamiento fijo sensor → sala → gateway del ejemplo de la
cátedra. No implementa rutas dinámicas. El [contrato recibido](CONTRATO_MQTT.md)
se conserva completo; su versión anterior quedó en `legacy/docs/`.

## Revisión de las correcciones recibidas

| Observación | Resultado en esta versión |
|---|---|
| `errors` interpretado como array | Corregido: se exigen enteros y HTTP 201, `errors == 0` e `inserted + duplicates == 1`. Un evento por POST. Sin flags de éxito alternativo. |
| Hora en `measured_at` dentro de payload | Corregido: el evento REST lleva `ts` UTC arriba, al lado de `payload`. Sin RTC válido se omite. |
| REST + MQTT duplican datos en AURA | Corregido: las mediciones hacia AURA entran solo por REST. El espejo opcional usa otro broker. |
| UUID por sonda | Corregido: un UUID físico y los campos `temp_heladera_c` / `temp_freezer_c`; recuperación común configurada por separado. |
| Reescritura del journal completo | Corregido: registros individuales y dos índices; partición NVS de 128 KiB. La duración indicada en el mail era una estimación sin medir, no una vida útil garantizada. |
| Falla publicada como medición vacía | Corregido: no se envía REST sin temperatura válida. Alerta de falla y recuperación por `alerts/<UUID>/sensor`. |
| POST dentro del loop | Corregido: una tarea FreeRTOS procesa HTTP; el loop sigue atendiendo radio y MQTT. Las conexiones MQTT siguen teniendo timeout acotado. |
| 85 °C de arranque | Corregido: se espera la conversión y se rechazan 85 °C, −127 °C, NaN y lecturas fuera del rango físico. |

### Segunda revisión

| Observación | Resultado en esta versión |
|---|---|
| El gateway acepta comandos del broker local | Corregido: el broker local recibe solo el espejo. El gateway se suscribe a `devices/<id>/command` únicamente en el broker AURA; el dashboard local muestra la configuración en solo lectura y el backend responde `409` al POST en mesh. Todo `command_id` de un `response` lo emitió AURA. |
| La carpeta del dispositivo se presentaba como copia generada | Corregido: `dispositivos/E1-PB-LECA-HFR01/` es la fuente. Se eliminaron `firmware/nodo_mesh`, `firmware/mesh_comun` y `herramientas/aura/preparar_dispositivo.py`; sala, gateway y tests incluyen los headers de esa carpeta. |
| Falta el fork y el PR a aura-firmware | Ver *Entrega al repositorio de la cátedra*: PR solo con la carpeta del dispositivo, su fila en el mapa y CODEOWNERS con los usuarios del grupo. |
| Gateway de una placa y tramas que `infraestructura/` no entiende | El gateway de banco sigue siendo de una placa: alcanza para el banco. La adaptación de `infraestructura/` (tramas v3 y tabla MAC → UUID de §2.4) va en un PR aparte, sin mezclarla con el del dispositivo. |

El contrato v3 está publicado como **propuesta** y contiene cambios marcados
como pendientes de implementación central. Compilar este banco no demuestra
que la infraestructura de la cátedra ya los implemente.

## Flujo, identidad y ACK

```text
2 DS18B20 + RTC + LEDs + OLED
        │
        ▼
sensor ─ESP-NOW─► sala ─ESP-NOW─► gateway ─HTTP REST─► AURA
                                  │
                                  ├─ MQTT AURA: status / command / response / alerts
                                  └─ MQTT LOCAL distinto (optativo) → backend IC IV
                                                                       │
                                                                  MongoDB/dashboard
```

El gateway contiene en `config_local.h` una entrada MAC → `ICIV_DEVICE_ID`.
`ICIV_TENANT_ID` identifica el tenant; `ICIV_GATEWAY_ID` identifica la placa gateway
y su LWT. Las MAC, claves y valores del entorno real no se versionan.
La sonda se distingue por el nombre de la medición, nunca por otro UUID.
El banco permite enlace directo sensor → gateway si las MAC de padre y gateway
coinciden y `ICIV_ROOM_MAC` del gateway es cero.

Cada registro tiene un UUID aleatorio persistido antes de transmitirse. Para
mediciones se reutiliza como `ingest_id` en reintentos y después de reinicios.

| Acuse | Qué acredita | Efecto |
|---|---|---|
| ACK_GATEWAY | El gateway final validó el registro recibido | Permite distinguir caída de radio y de ingesta. No elimina datos. |
| ACK_CENTRAL | REST 201 con conteos enteros correctos acredita medición insertada o duplicada | Retira esa medición de la cola persistente. |
| ACK_ALERT | El gateway terminó la publicación MQTT QoS 1 de una alerta | Retira la alerta pendiente; **no acredita persistencia central**. |

El contrato indica que AURA todavía no persiste alertas como historial durable.
Por eso ACK_ALERT es distinto de ACK_CENTRAL; no se presenta un PUBACK como
prueba de guardado. Un cache RAM de 16 IDs evita repetir publicaciones ya
aceptadas mientras el gateway sigue encendido. Tras reinicios, REST deduplica
mediciones; MQTT puede volver a entregar alertas. El backend local deduplica
alertas con `ts` conocido por placa/tipo/hora/detalles. Sin reloj válido no puede
garantizar esa deduplicación sin agregar campos al contrato.

## Telemetría REST

`POST <ICIV_API_BASE>/api/v1/telemetry/ingest`, con autenticación del entorno.
Se manda **un evento por request** para evitar confirmar un lote parcialmente
rechazado. Cada sonda usa su intervalo propio; no se repite la última lectura
de la otra para completar el mensaje.

```json
{"events":[{
  "tenant_id":"<UUID del tenant>",
  "device_id":"<UUID de la placa>",
  "type":"temperatura",
  "ingest_id":"<UUID persistente de esta medición>",
  "ts":"2026-10-05T14:03:00Z",
  "payload":{"temp_heladera_c":4.5}
}]}
```

Respuesta aceptada: HTTP **201**, `{"inserted":1,"duplicates":0,"errors":0,...}`
o `{"inserted":0,"duplicates":1,"errors":0,...}`. Cualquier otro estado,
conteo, tipo JSON, campo ausente o cuerpo mal formado mantiene la muestra
pendiente. No se requiere `accepted_ingest_ids` ni una extensión de API.

El HTTP corre en una tarea y devuelve el resultado por una cola FreeRTOS.
Solo el loop usa MQTT y envía los ACK de radio. Los callbacks ESP-NOW copian
paquetes a una cola; no ejecutan HTTP, sensores ni escrituras NVS.

## MQTT, estados y alertas

En el broker AURA se usan los tópicos definidos por el contrato, con UUID:

- `devices/<UUID>/status`: QoS 1, retain; `details.evento="reporte"`,
  `transporte="mesh"`, `config` completo, `pendientes`, `descartadas` y
  `alimentacion="red"|"bateria"|"desconocida"`.
- `devices/<UUID>/command`: `set_config` con los parámetros que cambian.
- `devices/<UUID>/response`: `transmitido`, `aplicado` o `rechazado`, con
  `details.command_id` si fue provisto y configuración completa al aplicar.
- `alerts/<UUID>/sensor`: `severity`, `message`, `details.campo` y
  `details.motivo="sin_respuesta"|"fuera_de_rango"|"recuperada"`.
- `alerts/<UUID>/energia`: `details.alimentacion="bateria"|"red"`.
  Las alertas usan QoS 1, sin retain, y `ts` solo con reloj válido.

El nodo genera alertas al cambiar de estado, no en cada lectura fallida.
Mantiene el estado de sondas y alimentación en NVS entre reinicios.
El primer estado de alimentación conocido también se informa; arrancar en
batería no reconstruye la hora de un corte ocurrido con el nodo apagado.
Las alarmas de umbral se calculan localmente para LED 1; no se publica una
extensión de alerta de umbral que el contrato no define.

El gateway infiere offline luego de 3 × el menor intervalo de muestreo, porque
cada sonda provoca uplinks independientes. El siguiente reporte vuelve a online.
El gateway publica su propio LWT offline, no un LWT para cada sonda.

**Espejo local optativo:** `ICIV_LOCAL_MQTT_ENABLED=1` habilita una segunda
conexión con un client ID distinto. Resuelve ambos hosts y bloquea el espejo
si coinciden IP y puerto, o si no se pueden resolver. El espejo es **de solo
salida**: el gateway no se suscribe a `devices/<id>/command` en el broker local.
Si aceptara comandos de ahí, un cambio hecho desde el dashboard no quedaría
registrado en AURA (lo que §2.5 del contrato busca evitar) y el `aplicado` saldría
hacia AURA con un `command_id` que AURA nunca emitió. El backend IC IV debe
usar ese broker local, no el de AURA para recibir las mediciones de mesh.
No es un segundo camino de ingesta central ni una copia durable garantizada:
si el espejo falla, AURA puede tener la lectura aunque el dashboard local no.

## Configuración remota: parches atómicos

| Parámetro | Inicial | Rango |
|---|---|---|
| `intervalo_heladera_s` | 60 | entero 5–86400 s |
| `intervalo_freezer_s` | 300 | entero 5–86400 s |
| `min_heladera_c` / `max_heladera_c` | 2 / 6 | −55 a 125 °C; mínimo < máximo |
| `min_freezer_c` / `max_freezer_c` | −25 / −15 | −55 a 125 °C; mínimo < máximo |
| `recuperacion_s` | 300 | entero 60–86400 s; común a la placa |

Los umbrales admiten dos decimales; la conversión de la DS18B20 está configurada
a 9 bits y resuelve 0,5 °C. Estos valores son ajustes de banco, no recomendaciones
de conservación de alimentos.

```json
{"command":"set_config","params":{"intervalo_heladera_s":120},"command_id":"c-2026-0002"}
```

El gateway compacta el parche con una máscara de parámetros. El nodo combina
con los valores vigentes, valida el conjunto y guarda en NVS. Si un parámetro
es inválido no aplica ninguno. El identificador de comando es texto opcional
de hasta 64 caracteres en este adaptador; no se exige UUID.
`aplicado` con configuración completa viene del resultado explícito del nodo.
`recibido`, el ACK de radio o HTTP 202 de la API local no confirman ejecución.

Los comandos llegan **solo desde AURA**. El dashboard local muestra en solo
lectura la configuración vigente que reporta el nodo (por sonda y la
recuperación común) y no publica comandos: en `mesh`, `POST /dispositivo/config`
responde `409`. Los umbrales de `/umbrales` son de email local, no un downlink.

## Persistencia, capacidad y migración

Hay 30 registros ordinarios por sonda, una primera captura de corte protegida
por sonda y **un registro de inicio del corte separado**. Si ambas sondas fallan,
la transición a batería mantiene su hora sin depender de una temperatura.
Las alertas comparten la FIFO ordinaria y por eso también consumen capacidad.
Con solo mediciones: aproximadamente 30 minutos de heladera y 150 de freezer
con los intervalos iniciales. No es un historial ilimitado.

La FIFO llena descarta el ordinario más antiguo y cuenta la pérdida. Las primeras
capturas no se sobrescriben antes de confirmarse. Confirmadas, quedan como
referencia hasta un corte posterior. Cortes sucesivos durante una caída larga
no tienen una reserva ilimitada: se conserva el primero pendiente y lo posterior
entra en FIFO. Una caída de comunicación sola no activa la protección eléctrica.

El journal RAM mide **1963 bytes**, pero ya no se guarda entero. Cada registro
NVS mide **34 bytes** (30 de datos + CRC); el índice mide **341 bytes**.
Dos bancos por registro y dos índices permiten escribir el registro inactivo,
verificarlo y confirmar después el índice. Un ACK cambia solo el índice.
Una nueva muestra escribe su registro y el índice, sin reescribir otras 60.
La partición `partitions.csv` reserva **128 KiB** de NVS en la XIAO ESP32S3 de
8 MiB, con dos particiones de aplicación. Verificar que la tabla elegida al
compilar es la propia del sketch y corresponde a la placa antes de flashear.

Esto reduce escrituras y da espacio de trabajo a NVS; **no hay una vida útil
medida**. Mantener 5 s para pruebas acelera desgaste y tráfico. La validación
física pendiente incluye cortes durante escritura y pruebas prolongadas.

**Si la placa ya tiene el firmware mesh anterior:** antes de cambiar la tabla,
drenar la cola con la versión anterior si es posible y guardar sus ajustes.
Cargar `herramientas/nvs/exportar_mesh_v2` usando la **tabla anterior** y guardar
el JSON del monitor serie como `respaldo_nvs_v2.json`, archivo local ignorado.
`python herramientas/nvs/verificar_mesh_v2.py respaldo_nvs_v2.json` verifica
CRC y exporta ajustes/cola a `respaldo_nvs_v2.revisado.json`.
No hay importación automática de registros del formato antiguo al nuevo.
Revisar el respaldo y decidir con el grupo el destino de las muestras antes
de reinicializar la NVS o cambiar particiones. **No borrar flash con datos
pendientes sin respaldo validado.** El nodo nuevo detecta `iciv_mesh/q0,q1`
y se detiene sin borrarlos; ambas copias nuevas inválidas también detienen
envíos. Si es la primera carga no se necesita migración.

El backend usa colecciones mesh nuevas (`MeshReading`, `MeshThreshold`,
`MeshAlert`) para no chocar con los índices de las versiones de UUID por sonda.
No borra el historial anterior ni lo reasigna inventando correspondencias;
ese historial requiere una migración explícita después de confirmar el alta.

## Alimentación, offline y pantalla

La entrada de alimentación empieza deshabilitada (`ICIV_POWER_PIN=-1`). Definir
con el responsable del circuito el GPIO, modo de entrada y nivel de corriente
presente, siempre compatible con 3,3 V. El antirrebote dura 200 ms.
Al pasar a batería se guarda la transición y se solicita lectura de ambas
sondas. Su primera captura queda protegida. La hora es la detección disponible,
no una garantía del instante eléctrico exacto; el RTC perdido no se reemplaza
con una hora inventada. RTC en UTC usa offset 0; si mantiene hora Argentina,
usar `ICIV_RTC_UTC_OFFSET_SECONDS=-10800`.

En modo normal: envío inicial + hasta 3 reintentos. Sin ACK_GATEWAY se informa
caída del trayecto de radio; con gateway confirmado pero sin ACK_CENTRAL, falta
ingesta central. LED 2 se enciende, OLED se apaga y los intentos de entrega se
espacian (300 s iniciales). La radio sigue **escuchando comandos**, como
requiere §3.3; se limita el envío, no la recepción. Las sondas y LED 1 siguen
funcionando: no es deep sleep. OLED también se apaga por batería aunque haya comunicación.
Un ACK de alerta no acredita que la ingesta REST haya vuelto.
Sin mediciones válidas ni pendientes, no se puede probar persistencia REST
mediante una alerta: el estado de esa ingesta puede quedar desconocido.
Los snapshots se reservan al ciclo de recuperación o a un comando recibido
durante offline, para no transmitir a cada lectura mientras dura la caída.
El consumo real de recepción y la autonomía necesitan medirse en placa.

Recuperación realiza un barrido de canales 1–11 y un intento por ciclo.
Sala tiene canal fijo; gateway usa el canal de su AP. Un cambio de AP requiere
coordinar la sala; no hay búsqueda automática de nuevas rutas.

## Entrega al repositorio de la cátedra y pendientes

`dispositivos/E1-PB-LECA-HFR01/` es **la fuente** del nodo, no una copia: sketch
del mismo nombre, protocolo, ficha, bibliotecas versionadas, tabla NVS y tests.
Es la carpeta que se edita y se mantiene, acá y en aura-firmware.
Ese código ya figura como heladera-freezer IC IV en el ejemplo recibido,
pero ubicación final y alta UUID deben confirmarse en clase. No se crea un
registro real a partir del ejemplo.

Ya no existen `firmware/nodo_mesh`, `firmware/mesh_comun` ni el generador del
paquete: sala y gateway de banco incluyen los headers de esa carpeta. No subir
`config_local.h` ni los ejecutables de tests. La carpeta no necesita cambiar
`comun/` del repo AURA.

Para la entrega que hace el grupo:

1. Hacer fork de aura-firmware y crear la rama elegida.
2. Copiar **solo** `dispositivos/E1-PB-LECA-HFR01/` al fork, con los usuarios de
   GitHub del grupo en la ficha y la ubicación confirmada. Mantener la entrada de su mapa o ajustarla con
   la cátedra; agregar el grupo en `.github/CODEOWNERS`.
3. Ejecutar el validador, tests de carpeta y compilación con las bibliotecas
   de la ficha. Regenerar `autocontenido/` con la herramienta del repo AURA.
4. Abrir el PR de dispositivo para revisión, sin credenciales.
5. Proponer los cambios de radio/sala/gateway en **otro issue o PR**, como pide
   CONTRIBUTING. No mezclar infraestructura en el PR de la heladera.

**Bloqueo de integración actual:** la infraestructura recibida usa su protocolo
de aplicación v1; nuestro nodo usa IC IV v3. No son intercambiables aunque
ambos usen ESP-NOW. Los adaptadores de banco de `simulaciones/mesh` implementan
el protocolo requerido, pero su instalación y su adaptación multi-dispositivo
deben revisarse con la cátedra. No cambiar un gateway compartido por este
adaptador de una sola placa.

| Punto a confirmar/probar con la cátedra | Motivo |
|---|---|
| Código/ubicación/UUID único, tenant y MAC del nodo | Alta real y tabla del gateway. |
| Adaptador de infraestructura y protocolo IC IV v3 | La base v1 no entiende nuestras muestras/comandos/ACK. |
| API/autenticación y respuestas 201 reales | Se verificó el parser local; no se llamó al servidor de clase. |
| Ingesta MQTT central activa y endpoints corregidos | §10.1 indica que aún puede publicar al vacío; bloquea estados/comandos/alertas en AURA. |
| `aplicado`, reportes `config` y alertas del contrato propuesto | El contrato marca trabajo central/adaptadores pendiente. |
| Broker distinto para dashboard local y permisos de ambos | Evitar otro camino de telemetría en AURA. |
| PMK/LMK y política de cifrado ESP-NOW | El banco conserva el transporte del ejemplo; no configura seguridad campus. |
| Circuito de alimentación, RTC y particiones reales | No se dispone de hardware ni esquema confirmado del circuito. |
| Ensayo de duración y fallos NVS en placa | No se prometen meses de vida útil a partir de una estimación. |

## Protocolo del banco IC IV

Versión de aplicación **3**; no confundir con versión del contrato ni de
ESP-NOW. Cabecera de 17 bytes, payload máximo de 180. Sala reenvía sin fabricar
confirmaciones finales. Acuses requieren origen/destino configurados e ID
coincidente; uno atrasado no borra otra muestra.

| Tipo | Número | Payload |
|---|---|---|
| TELEMETRIA | 1 | Muestra de 30 bytes; tipo medición, alerta sensor o energía |
| COMANDO | 2 | ID texto 65 bytes + máscara 2 + configuración 20 (87 bytes) |
| ACK_SALTO | 3 | Reservado; no confirma ingesta |
| ACK_GATEWAY / ACK_CENTRAL | 4 / 5 | UUID 16 bytes |
| PROBE / PROBE_ACK | 6 / 7 | Vacío |
| CONFIG_RESULT | 8 | Comando + resultado (88 bytes), configuración completa |
| CONFIG_SNAPSHOT | 9 | Configuración y diagnóstico |
| ACK_ALERT | 10 | UUID 16 bytes; publicación MQTT, no ingesta durable |

## Qué probar en el banco

1. Comparar sondas con referencia y revisar RTC UTC. Probar −127/85, sonda
   desconectada y recuperación: no REST vacío, una alerta por transición.
2. Completar MAC/canal de los tres roles compatibles. Confirmar ACK_GATEWAY y
   ACK_CENTRAL con 201 válido; probar errores=1, 200, timeout y JSON inválido:
   deben mantener la muestra pendiente.
3. Verificar que **no hay data mesh en el broker AURA** y sí en el local cuando
   está habilitado. Intentar configurar ambos al mismo broker: espejo bloqueado.
4. Cambiar un intervalo o umbral por vez, solo el parámetro cambiado. Probar
   recuperación común aparte, min ≥ max y campo desconocido: rechazo atómico.
   Reiniciar y revisar NVS y `status.details.config`.
5. Detener gateway y después solo API: distinguir ambos estados, 3 reintentos,
   LED 2, OLED y recuperación espaciada. LED 1 sigue evaluando ambas sondas.
6. Simular entrada de batería: corte registrado aunque ambas sondas fallen.
   Reiniciar antes/después de la captura y revisar primeras lecturas protegidas.
7. Superar 30 registros por sonda sin conexión: descartes ordinarios, protección
   del primer corte. Restaurar y revisar IDs estables, dedup y orden por `ts`.
8. Hacer un ensayo controlado de corte durante escrituras con datos de banco
   respaldados. Revisar la recuperación de índice y que no se envía sin NVS.

Pruebas locales: `npm test` en backend, Makefile de host para FIFO/NVS, prueba
REST con ArduinoJson y compilación Arduino de roles/paquete. No sustituyen
aceptación de AURA ni pruebas con sondas, batería y red del aula.
La guía PDF anterior es de la versión previa: usar este procedimiento v3.
