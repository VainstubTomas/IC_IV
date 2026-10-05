# Backend IC IV

API Express, MongoDB y cliente MQTT para el dashboard local de heladera/freezer.
Guía general: [README](../../README.md). Contrato, pendientes y pruebas físicas:
[MESH_INTEGRACION](../../MESH_INTEGRACION.md).

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

`/dispositivo/config` devuelve configuración completa de la placa. POST acepta
un parche de los siete parámetros documentados, sin selector de sonda, solo
si `AURA_CONFIG_EXPERIMENTAL=true`. Los formularios envían únicamente diferencias;
recuperación usa su formulario propio. Solo `aplicado` con `details.config`
completo y coherente con el parche confirma ejecución. Un reporte de estado
permite recuperar los ajustes vigentes aunque se haya perdido la respuesta.
`/umbrales?sensor=...` configura emails locales, no el nodo.

Las variables anteriores AURA_FRIDGE_DEVICE_ID/AURA_FREEZER_DEVICE_ID y la
opción de extensiones mesh se retiraron; actualizar el `.env` local conservando
sus secretos. No se sobrescribe ese archivo automáticamente.
