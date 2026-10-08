# Banco histórico ESP-NOW — AURA v3

Estos sketches son los adaptadores anteriores de sala y gateway del banco IC IV.
Usan ESP-NOW y el protocolo del sensor archivado en
[legacy/firmware/nodo_mesh_v3](../../firmware/nodo_mesh_v3/AVISO.md).

| Carpeta | Función anterior |
|---|---|
| mesh/nodo_sala_mesh | Reenvío del sensor hacia el gateway mediante ESP-NOW. |
| mesh/nodo_gateway_mesh | Adaptador del banco hacia MQTT/REST v3. |

Las copias generadas están en ../herramientas/arduino/monolitico. El generador
se ejecuta desde legacy/banco_v3 como explica [../README.md](../README.md).

El flujo actual usa ESP-WIFI-MESH. La cátedra mantiene su raíz y relevos en
aura-firmware/infraestructura; estos adaptadores no se usan ni se entregan con
la hoja v4. Nodo y conector actuales están fuera de legacy, en la raíz del repo.
