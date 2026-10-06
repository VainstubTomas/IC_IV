# Comandos de IC IV

Desde la raíz del repositorio, para el banco local:

```powershell
docker compose up -d
docker compose ps
```

Backend:

```powershell
cd software/backend
npm ci
if (!(Test-Path .env)) { Copy-Item .env.example .env }
# Completar .env antes de iniciar
npm start
```

Desde la misma carpeta, pruebas:

```powershell
npm test
```

Desde la raíz, regenerar las copias de sala y gateway para Arduino IDE (el nodo
no se regenera: `dispositivos/E1-PB-LECA-HFR01/` es su fuente):

```powershell
python herramientas/arduino/generar_mesh_monolitico.py
```

Detener backend con Ctrl+C. Desde la raíz:

```powershell
docker compose down
```

Dashboard: http://localhost:8080/. Swagger: http://localhost:8080/api-docs.
Ajustar URLs si cambia SERVERPORT. Configuración de hardware/AURA y guía de
cortes/reintentos: [MESH_INTEGRACION.md](MESH_INTEGRACION.md).

Pruebas C++ (con make/g++): `make -C dispositivos/E1-PB-LECA-HFR01/tests`
(o `make -C tests/firmware/host`, que las delega).
Entrega AURA y migración de NVS: consultar MESH_INTEGRACION antes de flashear.
