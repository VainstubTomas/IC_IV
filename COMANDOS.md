# Comandos IC_IV

Desde software/backend:
```powershell
npm ci
npm test
npm start
```
Pruebas sin procesos secundarios si el entorno lo requiere:
```powershell
node --test --test-isolation=none
```
Dashboard: http://localhost:8080/ si SERVERPORT=8080. Swagger: /api-docs.

Solo para infraestructura de pruebas aisladas, desde la raíz:
```powershell
docker compose up -d
docker compose down
```
Esto NO levanta AURA ni ChirpStack y NO recibe automáticamente el tráfico del aula.
En clase usar broker AURA según datos del docente, UUID real y profile propio.
Ver docs/PROPUESTA_AURA.md antes de habilitar configuración experimental.
