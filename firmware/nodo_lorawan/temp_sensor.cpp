#include "temp_sensor.h"

// Pin D2 de la XIAO ESP32-S3 para el bus 1-Wire
#define ONE_WIRE_BUS D2 

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensorTemp(&oneWire);

float temperaturaActual = 0.0;

void inicializarTemperatura() {
  Serial.println(F("[DS18B20] Iniciando sonda de temperatura 1-Wire en pin D2..."));
  sensorTemp.begin();
  sensorTemp.setWaitForConversion(true);
}

void leerTemperatura() {
  sensorTemp.requestTemperatures();
  float t = sensorTemp.getTempCByIndex(0);
  
  // Validar si la sonda respondió correctamente (-127 = DEVICE_DISCONNECTED_C)
  if (t > -55.0 && t < 125.0) {
    temperaturaActual = t;
  } else {
    // Si la sonda física no está conectada, fallback a la temperatura interna del ESP32
    temperaturaActual = temperatureRead();
  }
}
