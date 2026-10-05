# Pruebas del proyecto

En `software/backend`: `npm ci` y `npm test`. Los tests están en `tests/backend/`.
No requieren una DB ni AURA reales: verifican validadores, dedup, persistencia,
API Express, confirmación aplicado, parches y conservación de ediciones del dashboard.

En `tests/firmware/host`, con C++17 y make: `make`. Ejecuta 22 static_asserts
de FIFO/ACK/rangos/CRC y tests de MeshStorage con Preferences falsa: registro
individual, ACK sin reescribir la FIFO, reinicios, overflow, corte entre registro
e índice, recuperación CRC y rechazo de NVS antigua sin borrado.
`make ingest ARDUINOJSON_INCLUDE=<ruta/ArduinoJson/src>` comprueba con el parser
real respuestas REST 201, conteos/tipos JSON y errores parciales.
En Windows, los comandos g++ equivalentes están en el README general.

`tests/firmware/esp32/tests_mesh` evalúa los 22 static_asserts al compilar para
XIAO ESP32S3; no ejecuta las pruebas de NVS falsa en la placa.
El paquete `dispositivos/E1-PB-LECA-HFR01/tests` contiene las pruebas propias
autocontenidas del nodo, generadas para el CI del repo AURA.

Respaldo anterior a cambiar particiones: desde la raíz,
`python -m unittest discover -s tests/nvs -v` verifica CRC, selección de copia
y que 85 °C histórico no se exporta como medición válida de v3.

`manual/prueba-contrato.ps1` corresponde al contrato/firmware LoRaWAN anterior.
Usarlo solo con ICIV_TRANSPORT=lorawan y un banco aislado; no prueba mesh v3.
Ejecutables, dependencias, credenciales y respaldos NVS locales se excluyen de Git.
Las pruebas locales no sustituyen sondas físicas, alimentación ni aceptación de AURA.
