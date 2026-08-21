#include "oled_rtc.h"

// Constructor para pantallas SH1106 I2C 128x64
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);
RTC_DS3231 rtc;

void inicializarPantallaRTC() {
  Serial.println("[CPP] -> Iniciando bus I2C...");
  Wire.begin(D4, D5);

  Serial.println("[CPP] -> Iniciando pantalla SH1106...");
  u8g2.begin();
  u8g2.clearBuffer(); // Limpia la memoria estática
  u8g2.sendBuffer();

  Serial.println("[CPP] -> Iniciando modulo RTC...");
  if (!rtc.begin()) {
    Serial.println("[CPP] -> ERROR: No se encontro el modulo RTC");
  } else {
    Serial.println("[CPP] -> EXITO: RTC detectado");
    if (rtc.lostPower()) {
      Serial.println("[CPP] -> Ajustando hora RTC...");
      rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }
  }
}

void actualizarPantallaRTC() {
  DateTime now = rtc.now();

  u8g2.clearBuffer();

  // Título
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 10, "Monitor de Heladera");
  u8g2.drawHLine(0, 13, 128);

  // Fecha
  char bufferFecha[20];
  snprintf(bufferFecha, sizeof(bufferFecha), "Fecha: %02d/%02d/%04d", now.day(), now.month(), now.year());
  u8g2.drawStr(0, 28, bufferFecha);

  // Hora (Fuente más grande)
  u8g2.setFont(u8g2_font_logisoso16_tf);
  char bufferHora[15];
  snprintf(bufferHora, sizeof(bufferHora), "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
  u8g2.drawStr(15, 55, bufferHora);

  // Envía todo el fotograma completo a la pantalla
  u8g2.sendBuffer();
}
