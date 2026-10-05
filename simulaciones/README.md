# Banco y propuestas de integración mesh

`mesh/nodo_sala_mesh` y `mesh/nodo_gateway_mesh` son adaptadores reales de
infraestructura para revisar con la cátedra. La carpeta los separa del firmware
de nuestro sensor; no son un simulador del central ni generan temperaturas falsas.
Requieren los headers de `firmware/mesh_comun` y coordinación antes de cargarlos
en placas compartidas. Ver `../MESH_INTEGRACION.md` para los acuerdos pendientes.
