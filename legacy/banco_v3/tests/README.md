# Pruebas del banco histórico v3

Los comandos de este documento parten de legacy/banco_v3/.
Las pruebas actuales están junto al nodo, al conector y a comun/.


En `software/backend`: `npm ci` y `npm test`. Los tests están en `tests/backend/`.
No requieren una DB ni AURA reales: verifican validadores, dedup, persistencia,
API Express, configuración en solo lectura en mesh (comandos solo desde AURA)
y la configuración vigente reportada por el nodo en el dashboard.

Las pruebas del nodo viven con su fuente, en `../firmware/nodo_mesh_v3/tests`
(C++17 y make: `make`). `make -C tests/firmware/host` las delega. Ejecutan 22 static_asserts
de FIFO/ACK/rangos/CRC y tests de MeshStorage con Preferences falsa: registro
individual, ACK sin reescribir la FIFO, reinicios, overflow, corte entre registro
e índice, recuperación CRC y rechazo de NVS antigua sin borrado.
`make ingest ARDUINOJSON_INCLUDE=<ruta/ArduinoJson/src>` comprueba con el parser
real respuestas REST 201, conteos/tipos JSON y errores parciales.
En Windows, los comandos g++ equivalentes están en el README general.

`tests/firmware/esp32/tests_mesh` evalúa los 22 static_asserts al compilar para
XIAO ESP32S3; no ejecuta las pruebas de NVS falsa en la placa.
Esas mismas pruebas son las que corre el CI de aura-firmware sobre la carpeta
del dispositivo: no hay copias que regenerar.

Respaldo anterior a cambiar particiones: desde la raíz,
`python -m unittest discover -s tests/nvs -v` verifica CRC, selección de copia
y que 85 °C histórico no se exporta como medición válida de v3.

`manual/prueba-contrato.ps1` corresponde al contrato/firmware LoRaWAN anterior.
Usarlo solo con ICIV_TRANSPORT=lorawan y un banco aislado; no prueba mesh v3.
Ejecutables, dependencias, credenciales y respaldos NVS locales se excluyen de Git.
Las pruebas locales no sustituyen sondas físicas, alimentación ni aceptación de AURA.
