# Backend IC IV

API Express y cliente MQTT AURA para heladera y freezer. Ver la guía general
en [README](../../README.md) y el alcance del banco en
[MESH_INTEGRACION](../../MESH_INTEGRACION.md).

Desde esta carpeta: `npm ci`, crear `.env` desde `.env.example`, completar
broker/DB/UUID y ejecutar `npm start`. Dashboard y API usan el puerto configurado;
la plantilla usa 8080. `npm test` ejecuta las pruebas locales.

Cada sonda tiene un UUID lógico independiente. Los endpoints de telemetría
aceptan `sensor=heladera` o `sensor=freezer`; sin selector se utiliza heladera.
La hora medida se conserva separada de la recepción cuando se habilita la
extensión acordada. Los mensajes duplicados se detectan por `ingest_id`.

Los reportes mesh y la publicación de ajustes están deshabilitados por defecto
hasta coordinar su protocolo con gateway/AURA. Un acuse de transporte solo no
confirma aplicación. `/umbrales` configura email local; la interfaz mesh envía
los umbrales al nodo mediante `/dispositivo/config` y usa su reporte explícito.
