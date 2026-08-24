#ifndef TEMP_SENSOR_H
#define TEMP_SENSOR_H

#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>

extern float temperaturaActual;

void inicializarTemperatura();
void leerTemperatura();

#endif