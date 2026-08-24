#ifndef PANTALLA_RTC_H
#define PANTALLA_RTC_H

#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
#include "RTClib.h"

void inicializarPantallaRTC();
void actualizarPantallaRTC(float temp);

#endif
