#ifndef PANTALLA_RTC_H
#define PANTALLA_RTC_H

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "RTClib.h"

// Declaramos las dos funciones que usaremos en tu código principal
void inicializarPantallaRTC();
void actualizarPantallaRTC();

#endif
