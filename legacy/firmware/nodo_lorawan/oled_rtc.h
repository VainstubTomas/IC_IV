#ifndef OLED_RTC_H
#define OLED_RTC_H

#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
#include "RTClib.h"

void inicializarPantallaRTC();
void actualizarPantalla(float temp, uint16_t contador, const char* estadoLoRa);

#endif
