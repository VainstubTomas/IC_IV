/*
 * Banco LoRaWAN UNRaf — Nodo de Telemetría (Heladera Industrial)
 *
 * Manda la temperatura DS18B20; conserva contador local y recibe configuracion remota
 * por LoRaWAN (AU915 OTAA) al gateway Milesight UG de la cátedra, que lo reenvía
 * al Network Server ChirpStack v4.
 *
 * HARDWARE: Seeed XIAO ESP32S3 + Wio-SX1262 (conector B2B)
 * SENSORES: Sonda DS18B20 (1-Wire en pin D2), pantalla OLED SH1106 + RTC DS3231 (I2C en D4/D5)
 *
 * DEPENDENCIAS EN ARDUINO IDE:
 *   - RadioLib 7.7.1 (API de referencia comprobada)
 *   - DallasTemperature & OneWire
 *   - U8g2 & RTClib
 *
 * CONFIGURACIÓN DE IDE:
 *   - Placa: "XIAO_ESP32S3"
 *   - USB CDC On Boot: ENABLED (imprescindible para salida serie por USB nativo)
 *
 * ANTES DE COMPILAR:
 *   1. Copiar credenciales.h.example a credenciales.h
 *   2. Completar con JOIN_EUI, DEV_EUI y APP_KEY otorgadas por el docente.
 */

#include <RadioLib.h>
#include <Preferences.h>
#include "credenciales.h"
#include "temp_sensor.h"
#include "oled_rtc.h"
#include "config_nodo.h"

// Activar solo en banco acordado: el contrato aun no define el reporte FPort 11.
#ifndef ICIV_REPORTE_CONFIG_EXPERIMENTAL
#define ICIV_REPORTE_CONFIG_EXPERIMENTAL 0
#endif

// ===== PERSISTENCIA EN NVS =====
Preferences almacen;
const char* NVS_ESPACIO = "lorawan";
const char* NVS_NONCES  = "nonces";
const char* NVS_SESION  = "sesion";

// ===== PINES DEL KIT WIO-SX1262 (CONECTOR B2B) =====
#define PIN_NSS    41
#define PIN_DIO1   39
#define PIN_NRST   42
#define PIN_BUSY   40
#define PIN_ANT_SW 38   // Habilita el camino de RF de la antena (HIGH)

SX1262 radio = new Module(PIN_NSS, PIN_DIO1, PIN_NRST, PIN_BUSY);

// ===== CONFIGURACIÓN LORAWAN =====
// Argentina: AU915, sub-banda 2 (canales 8-15 + 65)
// RadioLib numera desde 1 -> SUBBANDA = 2 (corresponde a "au915_1" en ChirpStack)
const LoRaWANBand_t Region   = AU915;
const uint8_t       SUBBANDA = 2;

LoRaWANNode node(&radio, &Region, SUBBANDA);

// Data rate fijo para validaciones de enlace
// DR2 = SF10 a 125 kHz (cumple dwell-time en AU915 con margen)
ConfigNodo configNodo = configPorDefecto();
bool reportePendiente = true;
uint8_t puertoEnvio = 1;
bool confirmadoEnvio = true;

// ===== TEMPORIZACIÓN =====
unsigned long intervaloEnvio() { return uint32_t(configNodo.intervaloSeg) * 1000UL; }
unsigned long ultimoEnvio = 0;

uint16_t contador = 0;

// ===== CONFIGURACIÓN DE CONFIRMACIÓN Y REINTENTOS =====
const uint8_t       MAX_INTENTOS = 3;
const unsigned long ESPERA_BASE  = 2000UL;
const unsigned long ESPERA_AZAR  = 1000UL;

// ===== DECLARACIÓN DE FUNCIONES =====
void fijarDatarate();
void enviarTelemetria();
bool intentarEnvio(const uint8_t* payload, size_t tam, uint8_t intento);
void esperarReintento(uint8_t intento);
void reunirse();
void guardarBuffer(const char* clave, const uint8_t* buf, size_t tam);
bool restaurarBuffer(const char* clave, size_t tam, bool esNonce);
void cargarConfig();
void procesarConfig(const uint8_t* datos, size_t tam);
void enviarReporte();
void detener();

// ===== SETUP =====
void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 5000) { }

  Serial.println();
  Serial.println(F("=============================================="));
  Serial.println(F("=== Banco LoRaWAN UNRaf - Heladera IoT     ==="));
  Serial.println(F("=============================================="));

  // 1. Inicializar pantalla OLED y módulo RTC
  inicializarPantallaRTC();
  actualizarPantalla(0.0, 0, "Iniciando...");

  // 2. Inicializar sensor físico DS18B20
  inicializarTemperatura();

  // 3. Activar switch de RF de la antena
  pinMode(PIN_ANT_SW, OUTPUT);
  digitalWrite(PIN_ANT_SW, HIGH);

  Serial.print(F("Iniciando radio SX1262... "));
  int estado = radio.begin();
  if (estado != RADIOLIB_ERR_NONE) {
    Serial.print(F("FALLO, codigo "));
    Serial.println(estado);
    Serial.println(F("Verifique conexion B2B entre XIAO y Wio-SX1262."));
    actualizarPantalla(0.0, 0, "Fallo Radio SX1262");
    detener();
  }
  Serial.println(F("OK"));

  // Wio-SX1262 usa DIO2 para conmutación automática TX/RX
  radio.setDio2AsRfSwitch(true);

  // 4. Configurar activación OTAA (LoRaWAN 1.0.x usa nullptr en nwkKey)
  Serial.print(F("Uniendo a la red LoRaWAN (OTAA)... "));
  actualizarPantalla(0.0, 0, "Uniendo OTAA...");
  node.beginOTAA(JOIN_EUI, DEV_EUI, nullptr, APP_KEY);

  // Restaurar nonces y sesión previa desde NVS
  if (!almacen.begin(NVS_ESPACIO, false)) detener();
  cargarConfig();
  restaurarBuffer(NVS_NONCES, RADIOLIB_LORAWAN_NONCES_BUF_SIZE, true);
  restaurarBuffer(NVS_SESION, RADIOLIB_LORAWAN_SESSION_BUF_SIZE, false);

  estado = node.activateOTAA();
  guardarBuffer(NVS_NONCES, node.getBufferNonces(), RADIOLIB_LORAWAN_NONCES_BUF_SIZE);
  if (estado != RADIOLIB_LORAWAN_NEW_SESSION && estado != RADIOLIB_LORAWAN_SESSION_RESTORED) {
    Serial.print(F("FALLO, codigo "));
    Serial.println(estado);
    Serial.println(F("Si no conecta:"));
    Serial.println(F("  - Comprobar credenciales.h"));
    Serial.println(F("  - Si el DevNonce ya se uso, reiniciar nonces en ChirpStack."));
    actualizarPantalla(0.0, 0, "Fallo Join OTAA");
    detener();
  }

  if (estado == RADIOLIB_LORAWAN_SESSION_RESTORED) {
    Serial.println(F("OK - Sesion previa restaurada"));
    actualizarPantalla(0.0, 0, "Sesion Restaurada");
  } else {
    Serial.println(F("OK - Join nuevo exitoso"));
    actualizarPantalla(0.0, 0, "Join OK");
  }

  // Persistir DevNonce inmediatamente
  guardarBuffer(NVS_NONCES, node.getBufferNonces(), RADIOLIB_LORAWAN_NONCES_BUF_SIZE);

  fijarDatarate();
  Serial.println();

  // Forzar primer envío de inmediato
  ultimoEnvio = millis() - intervaloEnvio();
}

// ===== LOOP PRINCIPAL =====
void loop() {
  unsigned long ahora = millis();

  if (ahora - ultimoEnvio >= intervaloEnvio()) {
    ultimoEnvio = ahora;
    if (ICIV_REPORTE_CONFIG_EXPERIMENTAL && reportePendiente) enviarReporte();
    else enviarTelemetria();
  }

  delay(50);
}

// ===== ADQUISICIÓN Y ENVÍO DE TELEMETRÍA =====
void enviarTelemetria() {
  // 1. Leer sonda física DS18B20
  leerTemperatura();
  float temp = temperaturaActual;
  if (isfinite(temp)) temp += configNodo.offsetCentesimas / 100.0f;
  puertoEnvio = 1;
  confirmadoEnvio = configNodo.confirmado;

  // 2. Empaquetar payload binario de 4 bytes:
  //    Bytes 0-1: contador (uint16 big-endian)
  //    Bytes 2-3: temperatura * 100 (int16 big-endian, con signo para temperaturas bajo cero)
  int16_t tempRaw = isfinite(temp) ? (int16_t)round(temp * 100.0f) : 0x7FFF;

  uint8_t payload[4];
  payload[0] = (contador >> 8) & 0xFF;
  payload[1] = contador & 0xFF;
  payload[2] = (tempRaw >> 8) & 0xFF;
  payload[3] = tempRaw & 0xFF;

  char bufEstado[24];
  snprintf(bufEstado, sizeof(bufEstado), "TX #%u...", contador);
  actualizarPantalla(temp, contador, bufEstado);

  bool entregado = false;
  for (uint8_t intento = 1; intento <= MAX_INTENTOS && !entregado; intento++) {
    if (intento > 1) {
      esperarReintento(intento);
      snprintf(bufEstado, sizeof(bufEstado), "Reintento %u/%u", intento, MAX_INTENTOS);
      actualizarPantalla(temp, contador, bufEstado);
    }

    Serial.print(F("[TX] Contador="));
    Serial.print(contador);
    Serial.print(F(" | Temp="));
    Serial.print(temp);
    Serial.print(F(" C | Intento "));
    Serial.print(intento);
    Serial.print('/');
    Serial.print(MAX_INTENTOS);
    Serial.print(F(" ... "));

    entregado = intentarEnvio(payload, sizeof(payload), intento);
  }

  if (entregado) {
    actualizarPantalla(temp, contador, confirmadoEnvio ? "Uplink ACK OK" : "Uplink enviado");
  } else {
    Serial.println(F("[TX] PERDIDO: Se agotaron los reintentos permitidos."));
    actualizarPantalla(temp, contador, "TX PERDIDO (Sin ACK)");
  }

  contador++;
}

bool intentarEnvio(const uint8_t* payload, size_t tam, uint8_t intento) {
  LoRaWANEvent_t bajada = {};

  // sendReceive realiza la transmisión y abre las ventanas RX1 y RX2
  uint8_t datosBajada[242] = {};
  size_t tamBajada = sizeof(datosBajada);
  const bool confirmadoActual = confirmadoEnvio;
  int estado = node.sendReceive(payload, tam, puertoEnvio, datosBajada, &tamBajada, confirmadoActual, nullptr, &bajada);
  if (estado > 0 && bajada.fPort == PUERTO_CONFIG && tamBajada > 0) {
    procesarConfig(datosBajada, tamBajada);
  }

  // Guardar la sesión en NVS tras cada intento para mantener FCnt sincronizado
  guardarBuffer(NVS_SESION, node.getBufferSession(), RADIOLIB_LORAWAN_SESSION_BUF_SIZE);

  if (estado == RADIOLIB_ERR_NETWORK_NOT_JOINED) {
    Serial.println(F("Sin sesion activa, solicitando join..."));
    reunirse();
    return false;
  }

  if (estado < RADIOLIB_ERR_NONE) {
    Serial.print(F("ERROR, codigo "));
    Serial.println(estado);
    return false;
  }

  if (confirmadoActual && !bajada.confirming) {
    Serial.println(F("Sin confirmacion ACK del servidor"));
    return false;
  }

  if (estado > 0) {
    Serial.print(confirmadoEnvio ? F("Confirmado, ACK en RX") : F("Enviado + Downlink en RX"));
    Serial.println(estado);
  } else {
    Serial.println(F("Enviado correctamente"));
  }
  return true;
}

void esperarReintento(uint8_t intento) {
  unsigned long espera = (ESPERA_BASE << (intento - 2)) + random(ESPERA_AZAR);
  unsigned long minimo = node.timeUntilUplink();
  if (espera < minimo) {
    espera = minimo;
  }

  Serial.print(F("     Reintentando en "));
  Serial.print(espera / 1000.0, 1);
  Serial.println(F(" s"));
  delay(espera);
}

void reunirse() {
  Serial.print(F("     Rehaciendo Join OTAA... "));
  int estado = node.activateOTAA();
  guardarBuffer(NVS_NONCES, node.getBufferNonces(), RADIOLIB_LORAWAN_NONCES_BUF_SIZE);

  if (estado == RADIOLIB_LORAWAN_NEW_SESSION) {
    Serial.println(F("OK"));
    fijarDatarate();
  } else {
    Serial.print(F("FALLO, codigo "));
    Serial.println(estado);
  }
}

void fijarDatarate() {
  node.setADR(configNodo.adr);
  if (!configNodo.adr && node.setDatarate(configNodo.dr) != RADIOLIB_ERR_NONE) {
    Serial.println(F("Data rate rechazado: nodo detenido para no informar configuracion falsa"));
    detener();
  }
}

void cargarConfig() {
  uint8_t bytes[TAM_CONFIG];
  ConfigNodo guardada;
  if (almacen.getBytesLength("config") == TAM_CONFIG &&
      almacen.getBytes("config", bytes, TAM_CONFIG) == TAM_CONFIG &&
      decodificarConfig(bytes, TAM_CONFIG, guardada)) configNodo = guardada;
}

void procesarConfig(const uint8_t* datos, size_t tam) {
  ConfigNodo candidata;
  if (!decodificarConfig(datos, tam, candidata)) {
    Serial.println(F("[CONFIG] Rechazada: longitud/version/rango invalido"));
    return;
  }
  // Aplicar radio antes de persistir; restaurar el estado previo si algo falla.
  node.setADR(candidata.adr);
  if (!candidata.adr && node.setDatarate(candidata.dr) != RADIOLIB_ERR_NONE) {
    fijarDatarate();
    Serial.println(F("[CONFIG] Rechazada por RadioLib"));
    return;
  }
  if (almacen.putBytes("config", datos, tam) != tam) {
    fijarDatarate();
    Serial.println(F("[CONFIG] Error NVS: no aplicada"));
    return;
  }
  configNodo = candidata;
  reportePendiente = true;
  Serial.println(F("[CONFIG] Validada, guardada y aplicada; reporte pendiente"));
}

void enviarReporte() {
  uint8_t bytes[TAM_CONFIG];
  codificarConfig(configNodo, bytes);
  puertoEnvio = PUERTO_REPORTE;
  confirmadoEnvio = configNodo.confirmado;
  reportePendiente = false;
  bool entregado = false;
  for (uint8_t intento = 1; intento <= MAX_INTENTOS && !entregado; ++intento) {
    if (intento > 1) esperarReintento(intento);
    entregado = intentarEnvio(bytes, sizeof(bytes), intento);
  }
  // Un downlink recibido durante el reporte puede dejar otro reporte pendiente.
  // Tras agotar intentos se sigue midiendo; no bloquear la telemetria.
  Serial.println(entregado ? F("[CONFIG] Reporte enviado") : F("[CONFIG] Reporte sin confirmar"));
}

void guardarBuffer(const char* clave, const uint8_t* buf, size_t tam) {
  if (!buf || tam == 0) return;
  if (almacen.putBytes(clave, buf, tam) != tam) {
    Serial.println(F("Error NVS: persistencia LoRaWAN fallida"));
    detener();
  }
}

bool restaurarBuffer(const char* clave, size_t tam, bool esNonce) {
  if (almacen.getBytesLength(clave) != tam) return false;
  uint8_t buffer[RADIOLIB_LORAWAN_SESSION_BUF_SIZE > RADIOLIB_LORAWAN_NONCES_BUF_SIZE
    ? RADIOLIB_LORAWAN_SESSION_BUF_SIZE : RADIOLIB_LORAWAN_NONCES_BUF_SIZE];
  if (tam > sizeof(buffer) || almacen.getBytes(clave, buffer, tam) != tam) return false;
  int16_t estado = esNonce ? node.setBufferNonces(buffer) : node.setBufferSession(buffer);
  return estado == RADIOLIB_ERR_NONE;
}

void detener() {
  while (true) {
    delay(1000);
  }
}
