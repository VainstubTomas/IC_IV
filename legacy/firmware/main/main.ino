#include <RadioLib.h>

// modulos locales
#include "oled_rtc.h"
#include "temp_sensor.h" // agregada la sonda

// Pines correctos para el conector B2B del XIAO ESP32-S3 + Wio LoRa
const int PIN_CS    = 41;
const int PIN_DIO1  = 39;
const int PIN_RESET = 42;
const int PIN_BUSY  = 40;

SX1262 radio = new Module(PIN_CS, PIN_DIO1, PIN_RESET, PIN_BUSY);

// --- VARIABLES PARA MILLIS() ---
unsigned long tiempoAnterior = 0;
const unsigned long intervaloLoRa = 5000; 

void setup() {
  Serial.begin(115200);
  
  while (!Serial) {
    delay(10);
  }
  
  Serial.println("\n--- INICIANDO TEST LORA ---");
  
  Serial.println("Inicializando modulo OLED/RTC...");
  inicializarPantallaRTC(); 

  // iniciar la sonda de temperatura
  inicializarTemperatura();

  Serial.println("[LoRa] Configurando hardware...");
  int state = radio.begin(915.0, 125.0, 9, 7, 18, 10, 8, 1.6, false);

  if (state == RADIOLIB_ERR_NONE) {
    Serial.println("[LoRa] Inicializacion exitosa! El chip responde.");
  } else {
    Serial.print("[LoRa] Fallo de inicio. Codigo de error: ");
    Serial.println(state);
    while (true); 
  }
}

void loop() {
  // 1. lee la temperatura
  leerTemperatura();

  // 2. actualizamos la pantalla pasandole el valor
  actualizarPantallaRTC(temperaturaActual);

  // 3. control de tiempo y transmisión LoRa sin frenar el micro
  unsigned long tiempoActual = millis();
  
  if (tiempoActual - tiempoAnterior >= intervaloLoRa) {
    tiempoAnterior = tiempoActual; 

    // Armamos un payload simple con la temperatura
    char payload[32];
    snprintf(payload, sizeof(payload), "Heladera1:%.2f", temperaturaActual);

    Serial.print("[LoRa] Enviando paquete: ");
    Serial.println(payload);
    
    int state = radio.transmit(payload);

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
  }
}