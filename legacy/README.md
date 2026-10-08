# Archivo de versiones anteriores

Todo lo que está aquí pertenece a etapas anteriores del proyecto. La entrega
actual está en dispositivos/E1-PB-LECA-HFR01 y conectores/E1-PB-LECA-HFR01,
fuera de esta carpeta. No copiar legacy/ al fork de aura-firmware.

| Ubicación | Etapa / contenido |
|---|---|
| firmware/main | Prototipo LoRa punto a punto y sus módulos de pantalla/sonda. |
| firmware/nodo_lorawan | Nodo LoRaWAN anterior. |
| configuration | Codec LoRaWAN conservado para regresión. |
| firmware/nodo_mesh_v3 | Copia del sensor ESP-NOW previo a la migración v4, con sus tests. |
| banco_v3 | Backend, frontend, Docker, broker, simulaciones ESP-NOW, herramientas y pruebas anteriores. |
| docs | Contratos MQTT v2/v3 y guía de integración v3. |
| software/backend | Fragmentos archivados de una etapa anterior del backend; no es el banco completo. |

Para orientarse y reproducir las pruebas antiguas, leer
[banco_v3/README.md](banco_v3/README.md). Dentro de esa carpeta las dependencias
del banco permanecen juntas; su sensor se toma de firmware/nodo_mesh_v3.
Las guías históricas conservan el contexto de su versión y no describen AURA v4.

No se eliminaron fuentes ni configuraciones privadas al reorganizar. Las claves,
.env, config_local.h, respaldos y binarios siguen ignorados por Git.
