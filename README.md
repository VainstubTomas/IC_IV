# ❄️ Sistema Integral IoT de Telemetría - Sistemas de refrigeración (IC_IV)

Monitoreo de heladera y freezer con dos DS18B20, una XIAO ESP32S3, OLED, RTC
y alarmas locales. Cada sonda tiene intervalo y umbrales independientes.
La placa conserva lecturas durante cortes y admite configuración remota sin reflashear.

La versión actual sigue la base AURA v4.0 recibida: nodo hoja ESP-WIFI-MESH,
identidad hw_id y confirmación central de las muestras. El conector Python
valida temperaturas y genera alertas por cruce de umbral. La cátedra mantiene
el raíz, los relevos, el broker y la plataforma AURA.

## 🏗️ Arquitectura General del Sistema

```text
DS18B20 heladera + freezer / OLED / RTC / LEDs
                         │
                  Nodo hoja XIAO
                  Journal NVS + FIFO
                         │ ESP-WIFI-MESH
                  Relevos → raíz de cátedra
                         │ MQTT hw/<hw_id>/data
                  AURA → conector Python → base
                         │ hw/<hw_id>/ack
                  Raíz → confirmación → hoja
```

- Las mediciones contienen solo temperaturas válidas y conservan un ID estable.
- La primera captura del corte está protegida; las lecturas ordinarias usan FIFO.
- El mensaje en vuelo se conserva completo hasta la confirmación de su ID.
- LED de umbral y OLED funcionan localmente; pantalla apagada en batería/offline.
- Intervalos, umbrales y recuperación se validan y guardan en NVS.

La asociación a mesh y el PUBACK del broker no acreditan persistencia en AURA.
La confirmación central puede resolver el ID como persistido, duplicado,
cuarentena, descartado o rechazado. Falta validar el conjunto con placas y AURA real.

## 🧱 Estructura del repositorio

```text
IC_IV/
 ├── dispositivos/E1-PB-LECA-HFR01/  # Firmware fuente, ficha, bibliotecas y tests
 ├── conectores/E1-PB-LECA-HFR01/    # Conector Python, manifiesto y tests
 ├── autocontenido/E1-PB-LECA-HFR01/ # Copia generada para Arduino IDE
 ├── comun/                         # Base AURA y SDK sin modificaciones, para probar aquí
 ├── herramientas/                  # Validadores y generador vigentes de AURA
 ├── docs/                          # Contrato v4, integración, comandos y procedencia
 ├── legacy/                        # LoRa/LoRaWAN, ESP-NOW y banco local anteriores
 └── README.md
```

| Carpeta | Cómo se usa |
|---|---|
| dispositivos/E1-PB-LECA-HFR01 | Código original del nodo. Se edita aquí; incluye los headers de comun/ mediante rutas relativas. |
| conectores/E1-PB-LECA-HFR01 | Código Python que procesa las temperaturas del lado de AURA. |
| autocontenido/E1-PB-LECA-HFR01 | Copia del mismo firmware, con los headers comunes juntos, para Arduino IDE. Se genera automáticamente; no se edita a mano. |

La copia autocontenida es parte del procedimiento de la cátedra: CONTRIBUTING.md
pide ejecutar su generador y el CI comprueba que coincida con las fuentes.
No representa otro nodo ni una versión diferente. Las pruebas actuales viven
junto a sus fuentes y en comun/. El mapa de códigos está en dispositivos/README.md.

El backend/frontend, Docker, el broker y las simulaciones anteriores están juntos
en [legacy/banco_v3](legacy/banco_v3/README.md). El resto de etapas está explicado
en [legacy/README.md](legacy/README.md). No participan del nodo v4.

## 🚀 Guía de Puesta en Marcha

### 1. Preparar el firmware

Arduino IDE, placa XIAO_ESP32S3, core ESP32 3.3.11 y USB CDC On Boot Enabled.
Instalar las versiones exactas de dispositivos/E1-PB-LECA-HFR01/bibliotecas.txt.
Abrir el .ino de esa carpeta conservando comun/ en su ruta relativa.

Crear config_local.h desde config_local.h.example sin sobrescribir uno existente.
La cátedra proporciona MESH_ID, MESH_CLAVE y canal. No se configuran UUID de AURA
ni MAC del padre/raíz. La placa imprime su hw_id para que la cátedra haga el alta.
Sin datos de mesh se puede medir y guardar, pero no comunicar.

Conservar partitions.csv, NVS 128 KiB y flash 8 MiB. Respaldar NVS/LittleFS antes
de flashear una placa con datos; no borrar flash ni cambiar tabla como solución a un error.
Monitor serie: 115200. Pines, módulos y funcionamiento están en la
[ficha del nodo](dispositivos/E1-PB-LECA-HFR01/README.md).

### 2. Alternativa para Arduino IDE

Desde Git Bash o Linux:

```bash
bash herramientas/generar_autocontenidos.sh
```

Abrir autocontenido/E1-PB-LECA-HFR01/E1-PB-LECA-HFR01.ino. Esta carpeta es una copia
del mismo firmware. El generador la reemplaza: conservar cualquier configuración
privada de esa copia antes de regenerar y volver a colocarla localmente después.

### 3. Configuración remota

Por defecto: heladera 60 s y 2..6 °C; freezer 300 s y -25..-15 °C; recuperación 300 s.
Los nombres compactos intervalo_s, f_s, hlo/hhi, flo/fhi y r_s permiten reportar
la configuración completa dentro del límite de 180 bytes de la base.

```json
{"command":"set_config","params":{"intervalo_s":120,"f_s":600},"command_id":"iciv-1"}
```

Se valida el parche completo, se guarda en NVS y se informa lo aplicado.
Rangos y restricciones están en la ficha. La configuración de red y el alta
de la placa se coordinan con la cátedra.

## 🧪 Pruebas y Validación

Python 3.11 o posterior, sin paquetes externos para el conector:

```powershell
python -m unittest discover -s conectores/E1-PB-LECA-HFR01/tests -v
python herramientas/validar_conectores.py
python herramientas/validar_dispositivos.py
```

Con make y compilador C++17:

```bash
make -C dispositivos/E1-PB-LECA-HFR01/tests
make -C comun/tests
```

El conector valida campos físicos, guarda solo los válidos y alerta por cruces de
umbrales. No implementa la predicción opcional. Ver su
[README](conectores/E1-PB-LECA-HFR01/README.md) y [comandos](docs/COMANDOS.md).

Pruebas de software y compilación no reemplazan el banco físico. Validar dos
sondas, RTC, batería, LEDs, cortes, FIFO, reinicios, comandos y confirmaciones.
La [guía de integración](docs/MESH_INTEGRACION.md) detalla qué probar y qué falta
confirmar con la cátedra, incluidos ACK intermedio, alertas y compatibilidad de la base.

## 📦 Entrega a AURA

Este repositorio IC_IV reúne nuestro desarrollo y el histórico en legacy/.
La entrega al repositorio de la cátedra se prepara en un fork de aura-firmware:
el compañero copia los archivos versionables del nodo y del conector, genera
allí el autocontenido y revisa la entrada del mapa y los responsables.
La base comun/ y las herramientas se toman de ese fork actualizado.

**Empezar por [ENTREGA_AURA.md](ENTREGA_AURA.md):** contiene las carpetas exactas,
los pasos de validación y los pendientes que deben mencionar en el PR.

No versionar .env, config_local.h, credenciales.h, respaldos ni ejecutables.
