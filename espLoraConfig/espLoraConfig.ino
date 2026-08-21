#include <RadioLib.h>

// Pines correctos para el conector B2B del XIAO ESP32-S3 + Wio LoRa
const int PIN_CS    = 41;
const int PIN_DIO1  = 39;
const int PIN_RESET = 42;
const int PIN_BUSY  = 40;

// Instanciar el módulo SX1262
SX1262 radio = new Module(PIN_CS, PIN_DIO1, PIN_RESET, PIN_BUSY);

void setup() {
  Serial.begin(115200);
  
  // ¡ESTA LÍNEA ES LA MAGIA PARA EL ESP32-S3!
  // El código se quedará congelado aquí hasta que abras el Monitor Serie.
  while (!Serial) {
    delay(10);
  }
  
  Serial.println("\n--- INICIANDO TEST LORA ---");
  Serial.println("[LoRa] Configurando hardware...");

  // Inicialización (915MHz)
  int state = radio.begin(915.0, 125.0, 9, 7, 18, 10, 8, 1.6, false);

  if (state == RADIOLIB_ERR_NONE) {
    Serial.println("[LoRa] Inicializacion exitosa! El chip responde.");
  } else {
    Serial.print("[LoRa] Fallo de inicio. Codigo de error: ");
    Serial.println(state);
    while (true); // Detener ejecución
  }
}

void loop() {
  Serial.println("[LoRa] Enviando paquete de prueba...");
  
  int state = radio.transmit("Test LoRa S3!");

  if (state == RADIOLIB_ERR_NONE) {
    Serial.println("[LoRa] Transmision OK!");
  } else if (state == RADIOLIB_ERR_PACKET_TOO_LONG) {
    Serial.println("[LoRa] Error: Paquete muy largo.");
  } else if (state == RADIOLIB_ERR_TX_TIMEOUT) {
    Serial.println("[LoRa] Error: Timeout en transmision.");
  } else {
    Serial.print("[LoRa] Error desconocido. Codigo: ");
    Serial.println(state);
  }

  delay(5000);
}
