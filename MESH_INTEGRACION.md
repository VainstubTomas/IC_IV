# Integración ESP-NOW de IC IV

Este documento describe la adaptación de `aura-firmware` para heladera y freezer.
La base es su encadenamiento fijo de nodos, no una red con rutas dinámicas:

```text
XIAO con 2 sondas → ESP-NOW → sala → ESP-NOW → gateway → Wi-Fi → AURA
```

Se puede probar sensor → gateway directamente configurando ambas MAC iguales
en el sensor y dejando `ICIV_ROOM_MAC` en cero en el gateway.

## Alcance y relación con el contrato

El ejemplo descargado conserva el firmware mesh antiguo. El contrato MQTT v2.0
incluido en el proyecto describe LoRaWAN y marca esa mesh como congelada.
La nueva consigna de IC IV vuelve a ESP-NOW: **esta adaptación requiere acordar
el transporte y las extensiones con AURA**. El contrato original se conserva
sin alteraciones; no se presenta esta propuesta como un contrato aprobado.

Los sketches nuevos están separados de los prototipos y del firmware LoRaWAN.
El proyecto no cambia el código ni los servidores centrales de la cátedra.
Los adaptadores de sala y gateway son archivos para revisar/cargar de forma
coordinada; no reemplazar un gateway compartido sin autorización de su administrador.

## Identidad y confirmaciones

- Cada sonda se representa con un UUID lógico independiente en AURA.
  Ambas comparten la MAC física del sensor; el campo binario `sensor` distingue
  heladera (0) y freezer (1). Confirmar esta alta con el administrador de AURA.
- Cada muestra tiene un UUID aleatorio persistido antes de enviarse. Es el
  `ingest_id` del evento, estable al reintentar o reiniciar cualquier placa.
- **ACK_GATEWAY:** el gateway final validó la muestra. La sala solo reenvía;
  su recepción no se confunde con llegada al gateway.
- **ACK_CENTRAL:** la ingesta AURA confirmó persistencia para ese `ingest_id`.
  Solo este acuse permite retirar la muestra pendiente del sensor.
- La confirmación de radio/MAC de ESP-NOW y el PUBACK MQTT no son ACK central.

La trama mantiene la cabecera de 17 bytes y el payload máximo de 180 del ejemplo,
pero utiliza **versión de aplicación 2**, con payloads binarios específicos de IC IV.
No es la versión de radio ESP-NOW. Los tres roles deben usar la adaptación;
el ejemplo original no interpreta estas tramas.

| Tipo | Número | Payload |
|---|---|---|
| TELEMETRIA | 1 | `Muestra`, 28 bytes |
| COMANDO | 2 | `ConfigCommand`, configuración por sonda y UUID de comando |
| ACK_SALTO | 3 | Reservado del ejemplo; no elimina lecturas |
| ACK_GATEWAY | 4 | UUID de muestra, 16 bytes |
| ACK_CENTRAL | 5 | UUID de muestra, 16 bytes |
| PROBE / PROBE_ACK | 6 / 7 | Sin payload, búsqueda del gateway por canal |
| CONFIG_RESULT | 8 | Comando y resultado de validación/guardado |
| CONFIG_SNAPSHOT | 9 | Configuración vigente de ambas sondas y diagnóstico local |

El sensor solo acepta acuses del gateway configurado, recibidos por su padre
configurado y coincidentes con el UUID que está esperando. Un acuse antiguo
no borra una muestra diferente. Los callbacks Wi-Fi solo copian tramas a una
cola FreeRTOS; lectura de sensores, persistencia y HTTP se ejecutan fuera de ellos.

## Alimentación y registro del corte

`ICIV_POWER_PIN=-1` deshabilita la detección hasta conocer el circuito de batería.
Configurar pin, modo de entrada y nivel que significa **corriente presente**
en `config_local.h`. La señal debe ser lógica compatible con 3,3 V; no conectar
tensión de red ni una alimentación de 5 V directamente al GPIO.

El sensor filtra cambios durante 200 ms. Al detectar paso a batería registra
la hora del RTC y solicita una lectura de ambas sondas, independientemente de
sus intervalos. El estado de captura pendiente también se guarda: un reinicio
antes de completar la conversión no elimina la obligación de capturarla.
La conversión a 9 bits tarda aproximadamente 94 ms y se atiende sin bloquear la
recepción de mensajes. La resolución de temperatura es 0,5 °C aunque el formato
del paquete use centésimas.

La hora indica **detección del cambio de alimentación**, con la latencia del
circuito y del antirrebote; no permite garantizar el instante eléctrico exacto.
Si el nodo arranca ya en batería sin estado anterior, es la primera detección
disponible, no una reconstrucción de cuándo ocurrió un corte mientras estaba apagado.
Si hubo un reinicio entre detección y conversión, la captura es la primera
disponible al volver, no una medición recuperada del instante anterior.

El RTC debe estar ajustado. `ICIV_RTC_UTC_OFFSET_SECONDS=0` supone hora UTC;
usar `-10800` si el DS3231 guarda hora local de Argentina. El dashboard presenta
los timestamps UTC en la zona del navegador. Si el RTC perdió alimentación o
no tiene fecha válida, se conserva el evento sin inventar su hora. El ajuste
desde la fecha de compilación es una utilidad optativa y aproximada, apagada
por defecto; verificar la hora antes de ensayar cortes.

La pantalla se apaga al usar batería o estar offline. Un corte de corriente no
implica necesariamente un corte de comunicación: si gateway y AURA siguen
respondiendo, el sensor puede entregar sus datos mientras usa batería.

## Cola persistente y capacidad

Hay **30 lecturas ordinarias por sonda**, más una primera lectura de corte
protegida por sonda. Cada muestra ocupa 28 bytes. El journal completo ocupa
menos de 2 KB; NVS mantiene dos copias con generación y CRC32 para elegir la
última válida al arrancar. Los blobs de almacenamiento no ocupan el stack del loop.

Con los valores iniciales, la FIFO conserva aproximadamente 30 minutos de
heladera y 150 minutos de freezer. Cuando se llena, descarta la ordinaria más
antigua para aceptar la nueva y cuenta las pérdidas. **No es historial ilimitado.**
La primera muestra de corte no participa de ese reemplazo: sigue guardada
incluso después de confirmarse y puede reemplazarse en el siguiente corte.
Si ocurre otro corte antes de confirmar el anterior, se protege el primero y
se cuenta la captura posterior que no pudo ocupar ese lugar; las muestras nuevas
siguen entrando en la FIFO. No se conserva un archivo ilimitado de todos los cortes.

Durante una caída solo de comunicación, sin cambio de alimentación detectado,
se conserva la FIFO ordinaria. La protección especial corresponde al corte
eléctrico solicitado y no establece una hora exacta de falla del gateway.
Las mediciones pendientes se retiran tras ACK central; los reenvíos conservan ID.

Los guardados utilizan NVS y su distribución de desgaste, pero muestrear cada
5 segundos aumenta las escrituras. Ese intervalo es para banco; los valores
iniciales de 60/300 segundos reducen las escrituras y el tráfico.
Si la persistencia falla o ambas copias existentes son inválidas, el firmware
informa error y no envía lecturas que no quedaron guardadas. No borra la cola
automáticamente. La partición debe admitir ambos blobs y el espacio de trabajo de NVS.

## Offline, alarmas y configuración

Un ciclo normal permite envío inicial más **3 reintentos**. Espera ACK de gateway
(4 s) y luego central (hasta 12 s desde el envío). Al agotarlos distingue:

- Sin ACK final de gateway: falla del recorrido sensor/sala/gateway.
- Gateway confirmó pero falta central: falla de ingesta/recorrido hacia AURA.

Se enciende LED 2, se apaga OLED y se detiene la radio Wi-Fi del sensor entre
ciclos de recuperación. **No es deep sleep:** sigue leyendo sondas y controlando
LED 1. El primer intento de recuperación ocurre tras 300 s por defecto.
Cada ciclo realiza búsqueda por canales 1–11 y un envío de la lectura pendiente;
si no obtiene ambas confirmaciones espera otro intervalo largo.
Al lograr ambas, vuelve al funcionamiento normal y drena lo pendiente.

La sala usa un canal configurado fijo. El gateway toma el canal de su red Wi-Fi;
sensor, sala y gateway deben coincidir. Si cambia el canal del AP, actualizar
la sala: el barrido del sensor no mueve automáticamente al relay. No se implementa
descubrimiento de rutas ni tolerancia automática a la caída del nodo de sala.

LED 1 se enciende si cualquier lectura válida está debajo de su mínimo o encima
de su máximo. Los límites son independientes y persisten en el nodo. Una sonda
inválida se informa como falla; no se usa la temperatura del ESP32 como sustituto.

| Configuración | Predeterminado | Rango |
|---|---|---|
| Heladera: intervalo | 60 s | 5–86400 s |
| Freezer: intervalo | 300 s | 5–86400 s |
| Heladera: mínimo / máximo | 2 / 6 °C | −55 a 125 °C, mínimo < máximo |
| Freezer: mínimo / máximo | −25 / −15 °C | −55 a 125 °C, mínimo < máximo |
| Recuperación común | 300 s | 60–86400 s |

Los límites predeterminados son valores de prueba configurables, no una
recomendación de conservación de alimentos. La resolución de sonda es distinta
de la precisión de 0,01 °C permitida para especificar los umbrales.

El comando MQTT conserva `command`, `params` y `command_id` y se compacta en
binario al entrar a ESP-NOW. El nodo valida, guarda y devuelve `CONFIG_RESULT`.
El snapshot informa ajustes vigentes al arrancar/reconectar y después de medir
o recibir configuración. `recibido` por sí solo sigue sin confirmar ejecución.
Nuestro backend solo confirma aplicación al recibir el reporte explícito
`details.iciv.applied=true` y comprobar que coincide con el comando enviado.

## Adaptación que debe acordarse con AURA

El gateway usa el endpoint del ejemplo:

```text
POST <API_BASE>/api/v1/telemetry/ingest
```

Envía un único evento por solicitud, con `tenant_id`, `device_id`, `type`,
`ingest_id` y `payload`. El tipo es configurable mediante `ICIV_TELEMETRY_TYPE`
(predeterminado `temperature`); comprobarlo contra el esquema real de AURA.
No se probó este endpoint ni su autenticación contra el servidor de la cátedra.

**Propuesta de respuesta durable:** HTTP 200/201 y
`{"accepted_ingest_ids":["<UUID persistido o ya existente>"]}`.
El gateway solo acusa el ID incluido. Este campo no está confirmado como parte
de la API actual: se propone para distinguir éxito completo y rechazo parcial.
Si AURA ya garantiza que 200/201 de una solicitud individual significa persistencia
o duplicado durable, puede habilitarse `ICIV_HTTP_SUCCESS_IS_DURABLE=1` después
de verificar ese acuerdo. Errores/rechazos explícitos impiden confirmar.
El nodo mantiene los datos si no existe esa garantía.

El gateway espejo publica `devices/<UUID>/data` con `values.temp_c` e `ingest_id`
después de confirmar la ingesta REST. Esto permite usar el dashboard del proyecto.
El espejo MQTT es auxiliar: si falla, no invalida una persistencia ya confirmada
en AURA y no tiene su propia cola durable. Verificar deduplicación entre MQTT
y REST en AURA antes de habilitar ambos caminos en el servidor compartido.

Extensiones propuestas, **desactivadas por defecto**:

- Metadata fuera de `values` en el espejo MQTT: `measured_at`, `power_cut_at`,
  `power_first` y `on_battery`. El backend IC IV las conserva y ordena por hora
  de medición, con hora de recepción separada.
- Esos metadatos, más `clock_valid`, `power_known` y `sensor_valid`, en el payload
  REST. Su almacenamiento/visualización central requiere soporte de AURA.
- `status.details.iciv`: configuración vigente por sonda, validez del sensor,
  alimentación conocida, pendientes, descartes y último corte.
- `response.details.iciv`: parámetros reportados y confirmación de aplicación.

No colocar estos diagnósticos en `values` ni usar tópicos nuevos de AURA.
Para habilitar el banco acordado:

| Componente | Configuración |
|---|---|
| Gateway | `ICIV_AURA_INGEST_ENABLED=1` y red/API/UUID reales en archivo ignorado |
| Gateway | `ICIV_AURA_EXTENSIONS_ENABLED=1`, después de acordar los campos |
| Gateway | Semántica de ACK central verificada o `accepted_ingest_ids` |
| Backend IC IV | `AURA_MESH_EXTENSIONS_ENABLED=true` |
| Backend IC IV | `AURA_CONFIG_EXPERIMENTAL=true` para enviar ajustes |

Mientras estos acuerdos no estén resueltos, el código es un banco preparado,
**no una integración mesh de producción ya validada con AURA**.

## Prueba física recomendada

1. Completar MAC/canal y cargar los tres roles adaptados; confirmar los dos ACK.
2. Comparar las sondas con una referencia y verificar que no se intercambian.
3. Cambiar intervalos y umbrales desde cada formulario; reiniciar el sensor y
   verificar el snapshot de NVS. LED 1 debe responder sin depender del central.
4. Detener el gateway: agotar los reintentos, comprobar LED 2/OLED y recuperación espaciada.
5. Mantener gateway activo y detener ingesta AURA: distinguir falta de ACK central.
6. Simular la señal lógica de batería, verificar primera lectura de ambas sondas
   y hora del RTC; reiniciar antes/después de captura y revisar conservación.
7. Generar más de 30 muestras por sonda durante la caída: comprobar descartes
   ordinarios y que la primera del corte no se sobrescribe.
8. Restaurar la cadena: revisar IDs estables, deduplicación y orden por hora
   original, sin mostrar una lectura atrasada como la más reciente.

Referencias técnicas: [ESP-NOW y callbacks](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/network/esp_now.html),
[Preferences/NVS](https://docs.espressif.com/projects/arduino-esp32/en/latest/tutorials/preferences.html).
Base de aplicación: ejemplo local `aura-firmware` compartido por la cátedra.
