# Banco histórico IC IV — AURA v3 / ESP-NOW

Este conjunto corresponde al backend/dashboard y al banco ESP-NOW anteriores.
Se conserva como referencia y para reproducir pruebas. El nodo actual usa
ESP-WIFI-MESH y MQTT v4: este banco no lo recibe ni reemplaza a AURA.

## Carpetas

| Carpeta | Contenido |
|---|---|
| software/backend | API Node.js, MongoDB, MQTT, email y alertas de la versión v3. |
| software/frontend | Dashboard local de la versión v3. |
| simulaciones/mesh | Sala y gateway ESP-NOW del banco anterior. |
| herramientas/arduino | Generador y copias de esos roles para Arduino. |
| herramientas/nvs | Exportación y validación de respaldos antiguos v2. |
| tests | Pruebas del backend, gateway, respaldo NVS y scripts manuales antiguos. |
| mosquitto / docker-compose.yml | Broker y MongoDB del banco local. |
| README_anterior.md | README de main anterior a la adaptación v4, conservado como documento histórico. |

El sensor v3 se conserva en ../firmware/nodo_mesh_v3/. Los contratos v2/v3 y
la guía anterior están en ../docs/. Las rutas de código que salían del banco
se ajustaron a la nueva ubicación; las relaciones backend/tests/frontend se mantienen.

## Reproducir el banco

Desde la raíz del repo:

```powershell
cd legacy/banco_v3
docker compose up -d
cd software/backend
npm ci
if (!(Test-Path .env)) { Copy-Item .env.example .env }
# Completar .env localmente antes de iniciar.
npm test
npm start
```

Desde legacy/banco_v3/:

```powershell
python herramientas/arduino/generar_mesh_monolitico.py
python -m unittest discover -s tests/nvs -v
docker compose down
```

Tests C++ del sensor archivado: make -C legacy/firmware/nodo_mesh_v3/tests
desde la raíz del repo. El target de tests/firmware/host también los delega.
Gateway REST: target ingest con ARDUINOJSON_INCLUDE apuntando a ArduinoJson/src.

README_anterior.md y la guía v3 describen el diseño anterior: sus rutas relativas
reflejan la organización de aquella versión. Usar este README para la ubicación actual.
No enviar este banco en el PR del nodo/conector a aura-firmware. Configuraciones
privadas, node_modules, datos del broker y ejecutables siguen excluidos de Git.
