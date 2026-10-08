# Comandos del proyecto actual — AURA v4

Todos parten de la raíz del repositorio. Python 3.11 o posterior; C++17 para
las pruebas del firmware. No requieren placa ni servidor AURA.

## Validar el nodo y el conector

```powershell
python herramientas/validar_dispositivos.py
python herramientas/validar_conectores.py
python -m unittest discover -s conectores/E1-PB-LECA-HFR01/tests -v
python -m unittest discover -s comun/python/tests -v
```

```bash
make -C dispositivos/E1-PB-LECA-HFR01/tests
make -C comun/tests
```

Parser JSON real, opcional: make -C dispositivos/E1-PB-LECA-HFR01/tests json
ARDUINOJSON_INCLUDE=/ruta/ArduinoJson/src (la biblioteca exacta está en bibliotecas.txt).

## Generar el firmware para Arduino IDE

Desde Git Bash o Linux, usando la herramienta de la cátedra:

```bash
bash herramientas/generar_autocontenidos.sh
```

La copia sale en autocontenido/E1-PB-LECA-HFR01. El generador reemplaza la carpeta
autocontenido: guardar cualquier config_local.h de esa copia en un lugar privado
antes de regenerar. Editar siempre las fuentes de dispositivos/, nunca la copia.

Abrir el .ino de dispositivos/E1-PB-LECA-HFR01 o el generado. XIAO_ESP32S3,
ESP32 3.3.11, USB CDC Enabled y versiones de bibliotecas.txt. Revisar particiones,
respaldo NVS/LittleFS y config_local.h antes de flashear. No hay un flasheo automático.

Guía de pruebas y pendientes: [MESH_INTEGRACION.md](MESH_INTEGRACION.md).
Fork/PR: [../ENTREGA_AURA.md](../ENTREGA_AURA.md).
Banco histórico: [../legacy/banco_v3/README.md](../legacy/banco_v3/README.md).
