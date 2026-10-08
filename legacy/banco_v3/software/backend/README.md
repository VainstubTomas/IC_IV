# Backend del banco histórico AURA v3

Esta carpeta está archivada. No consume el nodo ESP-WIFI-MESH v4.


API Express, MongoDB y cliente MQTT para el dashboard local de heladera/freezer.
Guía general: [README](../../README.md). Contrato, pendientes y pruebas físicas:
[MESH_INTEGRACION](../../../docs/MESH_INTEGRACION_v3.md).

Desde esta carpeta: `npm ci`, crear `.env` desde `.env.example`, completar
broker/DB/UUID y ejecutar `npm start`. La plantilla sirve dashboard y API en
8080. `npm test` ejecuta `../../tests/backend/` con los modelos y transporte
aislados, sin MongoDB ni AURA reales.

Un único `AURA_DEVICE_ID` representa la placa. Telemetría e historiales usan
`sensor=heladera` o `sensor=freezer`. MQTT se conecta al **broker local del
espejo**, separado de AURA. `ts` conserva la medición UTC; sin él se distingue
hora de recepción. Dedup por `ingest_id` + sonda. Colecciones mesh nuevas
evitan conflictos con índices del historial antiguo; no lo borran ni migran.

El gateway es quien envía mediciones a la API central AURA por REST. Este
backend no reenvía el espejo a AURA. Guarda alertas sensor/energía del contrato
v3 recibidas por el broker local; AURA todavía tiene limitaciones de persistencia
de alertas. Sin hora válida no se garantiza dedup de alertas MQTT.

`/dispositivo/config` devuelve la configuración vigente que reportó la placa
(`status.details.config`, espejado por el gateway). En `mesh` es de **solo
lectura**: POST responde `409`. El broker local es un espejo y el gateway no
acepta comandos desde él; la configuración se cambia en AURA, que así registra
cada cambio y es la única que emite `command_id`. Los `response` espejados de
comandos de AURA no tienen registro local y se ignoran; los umbrales del
dashboard se actualizan con el siguiente reporte de estado del nodo.
POST solo publica en `ICIV_TRANSPORT=lorawan` (legado) con `AURA_CONFIG_EXPERIMENTAL=true`.
`/umbrales?sensor=...` configura emails locales, no el nodo.

Las variables anteriores AURA_FRIDGE_DEVICE_ID/AURA_FREEZER_DEVICE_ID y la
opción de extensiones mesh se retiraron; actualizar el `.env` local conservando
sus secretos. No se sobrescribe ese archivo automáticamente.
