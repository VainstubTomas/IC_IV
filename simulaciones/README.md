# Banco y propuestas de integración mesh

`mesh/nodo_sala_mesh` y `mesh/nodo_gateway_mesh` son adaptadores reales de
infraestructura para revisar con la cátedra. La carpeta los separa del firmware
de nuestro sensor; no son un simulador del central ni generan temperaturas falsas.
Incluyen los headers de protocolo de `dispositivos/E1-PB-LECA-HFR01/` (la fuente
del nodo), así los tres roles usan la misma trama. Requieren coordinación antes
de cargarlos en placas compartidas. El gateway acepta comandos **solo del broker
AURA**; el broker local del dashboard recibe el espejo y nada más.
Atienden una sola placa (MAC y UUID fijos): alcanza para el banco, no reemplaza
al gateway de `infraestructura/` de aura-firmware. Ver `../MESH_INTEGRACION.md` para los acuerdos pendientes.
