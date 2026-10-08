# Integración IC IV con AURA v4.0 — ESP-WIFI-MESH

Base: copia pública de aura-firmware y contrato v4.0 fechados 07/10/2026.
El contrato recibido está copiado sin modificaciones en docs/CONTRATO_MQTT.md.
No sustituye la versión normativa privada de aura-app. La comparación online
no estuvo disponible durante esta adaptación: verificar los commits del fork
antes del PR. El firmware, el conector y los pasos de entrega están separados.

## Flujo actual

```text
DS18B20 heladera + freezer → hoja XIAO → ESP-WIFI-MESH → raíz de la cátedra
                                                             │ MQTT hw/<hw_id>/data
                                                             ▼
                                                    AURA + conector Python
                                                             │ commit / descarte
                                                             ▼
hoja ← CONFIRMACION del ingest_id ← raíz ← hw/<hw_id>/ack
```

La hoja no conoce UUID de AURA ni MAC del padre/raíz. El hw_id es la MAC STA de
fábrica con prefijo mac-, minúsculas y sin separadores. La cátedra asigna la placa.
Datos desconocidos pueden quedar en cuarentena y liberar la cola al confirmarse.
No se utiliza LoRaWAN, ESP-NOW ni REST en el firmware actual.

## Qué se implementó

| Pedido | Implementación |
|---|---|
| Interior en ESP-WIFI-MESH | Nodo hoja basado en comun/nodo_mesh.h, con copia compartida sin cambios. |
| Identidad por placa | hw_id impreso; sin tabla MAC → UUID ni UUID en el nodo. |
| Telemetría normativa | Solo temperaturas válidas, ID persistente y ts cuando hay hora. |
| ACK de AURA | Libera solo el envío confirmado; envío aceptado y PUBACK no borran registros. |
| Dos sondas / ajustes remotos | Intervalos y umbrales separados, parche atómico guardado en NVS. |
| Primer dato del corte + FIFO | Slots NVS protegidos y FIFO de 30 por sonda; mensaje en vuelo conservado completo. |
| LED y OLED locales | Alarma de umbral sin red; pantalla apagada en batería/desconexión. |
| Reintentos | Inicial + 3; luego r_s, sin detener escucha ni asociación automática de mesh. |
| Conector Python | Manifiesto físico, descarte/guardado y alertas de umbral por cruce. |
| Pruebas | Conector, SDK, FIFO/NVS, mensaje en vuelo, tipos/rangos, tamaño y compilación. |
| Fork/PR | Instrucciones para el compañero en ../ENTREGA_AURA.md; no se realizó. |

## Configuración remota y tamaño

Los 7 parámetros canónicos son intervalo_s (heladera), f_s (freezer), hlo/hhi,
flo/fhi (°C) y r_s (recuperación). Los valores NVS de la versión anterior se
conservan; cambian los nombres del JSON, no sus valores guardados. La ficha
del nodo contiene rangos, valores iniciales y ejemplos. El conector lee esos
mismos nombres de config_vigente(). Los nombres largos del antiguo protocolo
ESP-NOW no forman parte del nuevo comando.

El reporte completo con valores extremos, alimentación desconocida y contadores
entra en 180 bytes. La respuesta positiva con UUID de comando de 36 caracteres
también. No mandar todos los parámetros más un ID largo sin comprobar el tamaño:
el raíz rechaza comandos grandes y la base puede omitir config en resultados.

intervalo_s representa la heladera y también sirve para el estado offline del
raíz. Se envía un reporte periódico con ese intervalo, incluso con sondas
fallando: el raíz no confunde la ausencia de temperatura con caída de la placa.

## Persistencia y recuperación

Se mantiene el journal iciv_mesh3, índices dobles/CRC y la tabla NVS de 128 KiB.
Cada registro ordinario mide 34 bytes; capacidad 30 por sonda, primeras capturas
protegidas y hora local del corte. iciv_aura4 guarda el mensaje en vuelo completo
y los IDs que agrupa. La cola oficial de LittleFS recibe un solo mensaje por vez.
La capacidad de esa cola no amplía la capacidad del journal del dispositivo.

El adaptador conserva el JSON del UUID publicado antes de entregarlo a la
biblioteca. Si se corta entre el ACK y la limpieza NVS, reenvía ese mismo JSON:
no construye otro grupo de temperaturas con el mismo ingest_id. Un ACK ajeno no
borra nada. Una cola llena descarta solo ordinarias antiguas; la primera captura
del corte y el mensaje en vuelo permanecen protegidos.

Los registros de alertas del firmware previo se pasan por alerts, nunca por
values. Las nuevas fallas y cambios de energía usan la API de alertas de AURA.
Las alertas son RAM en la base y no tienen confirmación durable. No se promete
entrega de alertas durante un apagado o caída prolongada.

Con reloj válido se conserva la hora original de cada lectura/corte; sin hora
no se inventa fecha ni simultaneidad. Un corte con ambas sondas fallando sigue
guardando su hora local, sin generar data vacío.

## Diferencia entre asociación y ACK

radio_conectada() prueba asociación de mesh, no recepción por el raíz ni guardado
en AURA. El protocolo nuevo tiene solo CONFIRMACION final, sin el ACK intermedio
que tenía IC IV. La pantalla/LED distinguen mesh caída de espera de AURA agotada,
pero no pueden separar raíz, broker y backend cuando sigue habiendo asociación.
La libre reconexión de ESP-WIFI-MESH no se apaga después de tres intentos: el
presupuesto aplica a transmisiones de muestras. Después se espacia el envío.

## Pruebas sin placa

```powershell
python -m unittest discover -s conectores/E1-PB-LECA-HFR01/tests -v
python herramientas/validar_conectores.py
python herramientas/validar_dispositivos.py
make -C dispositivos/E1-PB-LECA-HFR01/tests
```

El parser real/tamaño JSON tiene un target json con ARDUINOJSON_INCLUDE; usa la
misma ArduinoJson 7.4.3 del firmware. Los tests estándar C++17 no requieren Arduino.
Las pruebas del SDK/código compartido comprueban la base sin modificarla.
Estas verificaciones no reemplazan el banco físico ni la integración central.

## Pruebas en banco

1. Obtener hw_id, MESH_ID, clave/canal y permiso de banco. Configurar solo la
   hoja; raíz/relevos los maneja la cátedra. Conservar una copia local de NVS y
   LittleFS antes de flashear una placa con datos. No borrar ni cambiar tabla.
2. Sin credenciales: lecturas, OLED, LED de umbral y journal deben funcionar;
   se imprime hw_id pero no hay comunicación. Validar ambas sondas separadas.
3. Con mesh: comprobar MESH_LEAF, unión y hw/<hw_id>/data con temperaturas,
   mismo ingest_id en cada reintento, ts original y ausencia de diagnóstico en values.
4. Con ack_falso.py de cátedra: modo sin_ack debe conservar el envío; ok permite
   drenar. El ACK falso es solo banco y no acredita persistencia central real.
   Confirmaciones equivocadas o resultados no reconocidos no deben borrar datos.
5. En AURA real: asignación de hw_id, conector cargado, deduplicación y ack tras
   commit. Verificar diferencia entre persistido/cuarentena/descartado/rechazado.
6. set_config parcial, inválido y desconocido: aplicado solo tras guardado, rechazo
   atómico, reporte vigente y conservación tras reinicio. Probar heladera/freezer
   con periodos distintos y umbrales que prenden el LED sin conexión.
7. Quitar una sonda / forzar 85 de arranque: no crear temperatura falsa; una
   alerta de falla y otra de recuperación. El conector guarda la otra sonda.
8. Cortar raíz/relevo/AURA por separado: FIFO, presupuesto y recuperación,
   OLED/LED y escucha de comandos. No confundir mesh unida con AURA disponible.
9. Entrada de batería ya validada: primer dato con hora de corte, ambos sensores
   ausentes, overflow y reinicio con envío pendiente. El ID/JSON deben conservarse.
10. Broker/conector: duplicados, datos fuera de rango, cambios de umbral y
    lecturas históricas que no disparan una falsa alarma actual.

## Pendientes para confirmar con la cátedra

| Punto | Por qué |
|---|---|
| Mesh ESP-WIFI-MESH en placas | La base del 07/10 declara el banco pendiente: unión sin clave WiFi, origen a dos saltos y bajada/reenganche. |
| Runtime de conectores / ingesta MQTT v4 | El contrato indica que la versión central previa aún no persiste MQTT ni usa hw/ack. |
| Alta de placa y credenciales de mesh | Las da la cátedra; no se generan UUID ni claves reales en este repo. |
| ACK intermedio / diagnóstico de cada etapa | No existe en el protocolo nuevo: no inventar confirmaciones ni tópicos. |
| Hora del corte en AURA con ambas sondas fallando | Se conserva localmente; el raíz fecha la alerta al publicarla. Acordar cómo propagar el ts original. |
| Hooks públicos de cola/ACK/reintento | El adaptador conserva primeros cortes y JSON/RTC usando structs internos de la base. Proponer API sin modificar comun/ en este PR. |
| Resultado con command_id largo | La biblioteca omite config si no entra; IDs cortos/UUID de 36 caracteres están probados. |
| Alimentación, RTC y autonomía | Circuito real pendiente; radio siempre escuchando y consumo por medir. |
| Alertas durables | La base las mantiene en RAM; no son parte de la cola confirmada de temperaturas. |

El backend/front local y los bancos ESP-NOW anteriores están en legacy/banco_v3, como material
de la versión v3; no consumen este nodo v4. Su actualización no forma parte de
la entrega del nodo/conector. El histórico está señalado en el README.
