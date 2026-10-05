#include "temp_sensor.h"

// pin D2 de la XIAO ESP32-S3 para el bus 1-Wire
#define ONE_WIRE_BUS D2 

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensorTemp(&oneWire);

float temperaturaActual = 0.0;

void inicializarTemperatura() {
  Serial.println("[CPP] -> Iniciando sensor DS18B20...");
  sensorTemp.begin();
}

void leerTemperatura() {
  // sensor lee
  sensorTemp.requestTemperatures();
  // almacena la temperatura en la variable global, indice cero (unico)
  temperaturaActual = sensorTemp.getTempCByIndex(0);
}