###  Descripción del repositorio y proyecto

El siguiente repositorio contiene el codigo (firmware (.ino, .h, .cpp), sistema de flujo de información) detallado para el proyecto de automatización de la materia INGENIERIA EN COMPUTACIÓN 4
de la carrera INGENIERIA EN COMPUTACION de la Universidad Nacional de Rafaela

### 🤝 Integrantes
Grassino Facundo, Bernardo Del Barco, Juan Cruz Sotelo y Vainstub Tomás.

### 🛜 Diagrama de flujo del sistema de comunicación
<img width="451" height="772" alt="FlowIC_IV" src="https://github.com/user-attachments/assets/84e0a788-09b7-4444-8743-73d7eac694b9" />

### 📁 Estructura del Repositorio

La arquitectura del proyecto se organiza por dominios de ingeniería (firmware, hardware y software) para mantener desacoplados los módulos físicos, lógicos y de servicios:

```text
.
├── firmware/
│   └── main/                   # Código embebido para el microcontrolador
│       ├── main.ino            # Punto de entrada y loop principal de la placa
│       ├── oled_rtc.cpp        # Implementación del display OLED y módulo de tiempo (RTC)
│       ├── oled_rtc.h          # Cabeceras y definiciones para la pantalla OLED y RTC
│       ├── temp_sensor.cpp     # Lógica de adquisición y lectura del sensor de temperatura
│       └── temp_sensor.h       # Cabeceras y configuración del sensor de temperatura
│
├── hardware/                   # Recursos físicos y diseño mecánico
│   ├── 3d-print/               # Archivos listos para fabricación/impresión (.stl, .gcode)
│   └── cad/                    # Archivos fuente editables del modelado 3D (.step, .f3d)
│
└── software/                   # Ecosistema lógico y servicios
    └── backend/                # Servidor central, API y lógica de negocio
        ├── .env                # Variables de entorno y credenciales (local)
        ├── .gitignore          # Reglas de exclusión para dependencias y binarios
        └── README.md           # Documentación específica del backend y setup local
