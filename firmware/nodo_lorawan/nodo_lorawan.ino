/*
 * Banco LoRaWAN UNRaf — Nodo de Telemetría (Heladera Industrial)
 *
 * Manda periódicamente el contador de envíos y la temperatura de la sonda DS18B20
 * por LoRaWAN (AU915 OTAA) al gateway Milesight UG de la cátedra, que lo reenvía
 * al Network Server ChirpStack v4.
 *
 * HARDWARE: Seeed XIAO ESP32S3 + Wio-SX1262 (conector B2B)
 * SENSORES: Sonda DS18B20 (1-Wire en pin D2), pantalla OLED SH1106 + RTC DS3231 (I2C en D4/D5)
 *
 * DEPENDENCIAS EN ARDUINO IDE:
 *   - RadioLib (>= 7.0)
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
const bool    USAR_ADR = false;
const uint8_t DATARATE = 2;

// ===== TEMPORIZACIÓN =====
const unsigned long INTERVALO_ENVIO = 20000UL;  // 20 segundos
unsigned long ultimoEnvio = 0;

uint16_t contador = 0;

// ===== CONFIGURACIÓN DE CONFIRMACIÓN Y REINTENTOS =====
const bool          CONFIRMADO   = true;
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
void restaurarBuffer(const char* clave, size_t tam, bool esNonce);
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
  almacen.begin(NVS_ESPACIO, false);
  restaurarBuffer(NVS_NONCES, RADIOLIB_LORAWAN_NONCES_BUF_SIZE, true);
  restaurarBuffer(NVS_SESION, RADIOLIB_LORAWAN_SESSION_BUF_SIZE, false);

  estado = node.activateOTAA();
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
  ultimoEnvio = millis() - INTERVALO_ENVIO;
}

// ===== LOOP PRINCIPAL =====
void loop() {
  unsigned long ahora = millis();

  if (ahora - ultimoEnvio >= INTERVALO_ENVIO) {
    ultimoEnvio = ahora;
    enviarTelemetria();
  }

  delay(50);
}

// ===== ADQUISICIÓN Y ENVÍO DE TELEMETRÍA =====
void enviarTelemetria() {
  // 1. Leer sonda física DS18B20
  leerTemperatura();
  float temp = temperaturaActual;

  // 2. Empaquetar payload binario de 4 bytes:
  //    Bytes 0-1: contador (uint16 big-endian)
  //    Bytes 2-3: temperatura * 100 (int16 big-endian, con signo para temperaturas bajo cero)
  int16_t tempRaw = (int16_t)round(temp * 100.0f);

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
    actualizarPantalla(temp, contador, "Uplink ACK OK");
  } else {
    Serial.println(F("[TX] PERDIDO: Se agotaron los reintentos permitidos."));
    actualizarPantalla(temp, contador, "TX PERDIDO (Sin ACK)");
  }

  contador++;
}

bool intentarEnvio(const uint8_t* payload, size_t tam, uint8_t intento) {
  LoRaWANEvent_t bajada = {};

  // sendReceive realiza la transmisión y abre las ventanas RX1 y RX2
  int estado = node.sendReceive(payload, tam, 1, CONFIRMADO, nullptr, &bajada);

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

  if (CONFIRMADO && !bajada.confirming) {
    Serial.println(F("Sin confirmacion ACK del servidor"));
    return false;
  }

  if (estado > 0) {
    Serial.print(CONFIRMADO ? F("Confirmado, ACK en RX") : F("Enviado + Downlink en RX"));
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
  if (USAR_ADR) {
    node.setADR(true);
    Serial.println(F("Modo ADR activo."));
  } else {
    node.setADR(false);
    node.setDataRate(DATARATE);
    Serial.print(F("Data rate fijo: DR"));
    Serial.print(DATARATE);
    Serial.println(F(" (SF10)"));
  }
}

void guardarBuffer(const char* clave, const uint8_t* buf, size_t tam) {
  if (!buf || tam == 0) return;
  almacen.putBytes(clave, buf, tam);
}

void restaurarBuffer(const char* clave, size_t tam, bool esNonce) {
  if (!almacen.isKey(clave)) return;
  uint8_t* destino = esNonce ? node.getBufferNonces() : node.getBufferSession();
  if (!destino) return;
  almacen.getBytes(clave, destino, tam);
}

void detener() {
  while (true) {
    delay(1000);
  }
}
