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

Desde la raíz, regenerar copias de Arduino IDE:

```powershell
python firmware/generar_mesh_monolitico.py
```

Detener backend con Ctrl+C. Desde la raíz:

```powershell
docker compose down
```

Dashboard: http://localhost:8080/. Swagger: http://localhost:8080/api-docs.
Ajustar URLs si cambia SERVERPORT. Configuración de hardware/AURA y guía de
cortes/reintentos: [MESH_INTEGRACION.md](MESH_INTEGRACION.md).
