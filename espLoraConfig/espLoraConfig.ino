#include <RadioLib.h>

// Pines preconfigurados para el Wio SX1262 con XIAO ESP32S3
// Los pines SPI (SCK=D8, MISO=D9, MOSI=D10) son manejados por defecto en hardware
const int PIN_CS    = D3;
const int PIN_DIO1  = D5;
const int PIN_RESET = D4;
const int PIN_BUSY  = D6;

// Instanciar el módulo
SX1262 radio = new Module(PIN_CS, PIN_DIO1, PIN_RESET, PIN_BUSY);

void setup() {
  Serial.begin(115200);
  
  // Breve delay crucial en macOS para dar tiempo a que se enumere el puerto USB
  delay(3000); 
  Serial.println("[LoRa] Iniciando hardware...");

  // Inicialización: Frecuencia (MHz), Ancho de banda (kHz), Spreading Factor, Coding Rate
  // IMPORTANTE: Cambia 915.0 a 868.0 si estás en Europa o a 433.0 según tu antena/región
  int state = radio.begin(915.0, 125.0, 9, 7, 18, 10, 8, 1.6, false);

  if (state == RADIOLIB_ERR_NONE) {
    Serial.println("[LoRa] Inicializacion exitosa!");
  } else {
    Serial.print("[LoRa] Fallo de inicio. Codigo de error: ");
    Serial.println(state);
    while (true); // Detener ejecución si falla el chip
  }
}

void loop() {
  Serial.println("[LoRa] Enviando paquete de prueba...");
  
  // Transmitir un mensaje en texto plano
  int state = radio.transmit("Test de conexion ESP32-S3 a LoRa");

  if (state == RADIOLIB_ERR_NONE) {
    // El paquete fue enviado exitosamente
    Serial.println("[LoRa] Transmision OK!");
  } else if (state == RADIOLIB_ERR_PACKET_TOO_LONG) {
    Serial.println("[LoRa] Error: Paquete demasiado largo.");
  } else if (state == RADIOLIB_ERR_TX_TIMEOUT) {
    Serial.println("[LoRa] Error: Timeout en transmision.");
  } else {
    Serial.print("[LoRa] Error desconocido. Codigo: ");
    Serial.println(state);
  }

  delay(5000); // Esperar 5 segundos antes del siguiente envío
}
