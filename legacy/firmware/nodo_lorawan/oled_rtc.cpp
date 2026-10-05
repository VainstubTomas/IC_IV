#include "oled_rtc.h"

// Constructor para pantalla OLED SH1106 128x64 I2C
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);
RTC_DS3231 rtc;

static bool rtc_ok = false;
static bool oled_ok = false;

void inicializarPantallaRTC() {
  Serial.println(F("[OLED/RTC] Iniciando bus I2C en pines D4/D5..."));
  Wire.begin(D4, D5);

  u8g2.begin();
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 15, "Banco LoRaWAN UNRaf");
  u8g2.drawStr(0, 35, "Iniciando...");
  u8g2.sendBuffer();
  oled_ok = true;

  if (rtc.begin()) {
    rtc_ok = true;
    Serial.println(F("[OLED/RTC] Modulo RTC DS3231 detectado."));
    if (rtc.lostPower()) {
      Serial.println(F("[OLED/RTC] Ajustando hora RTC con fecha de compilacion..."));
      rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }
  } else {
    Serial.println(F("[OLED/RTC] No se detecto modulo RTC."));
  }
}

void actualizarPantalla(float temp, uint16_t contador, const char* estadoLoRa) {
  if (!oled_ok) return;

  u8g2.clearBuffer();

  // 1. Título
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 10, "LoRaWAN Heladera");
  u8g2.drawHLine(0, 12, 128);

  // 2. Hora RTC o contador de tiempo
  char bufHora[24];
  if (rtc_ok) {
    DateTime now = rtc.now();
    snprintf(bufHora, sizeof(bufHora), "%02d:%02d:%02d  %02d/%02d",
             now.hour(), now.minute(), now.second(), now.day(), now.month());
  } else {
    unsigned long s = millis() / 1000;
    snprintf(bufHora, sizeof(bufHora), "Uptime: %luh %lum %lus", s / 3600, (s % 3600) / 60, s % 60);
  }
  u8g2.drawStr(0, 24, bufHora);

  // 3. Temperatura
  char bufTemp[24];
  if (isfinite(temp) && temp >= -60.0 && temp <= 130.0) {
    snprintf(bufTemp, sizeof(bufTemp), "Temp: %.2f C", temp);
  } else {
    snprintf(bufTemp, sizeof(bufTemp), "Temp: Error sensor");
  }
  u8g2.drawStr(0, 38, bufTemp);

  // 4. Contador de envíos
  char bufCnt[24];
  snprintf(bufCnt, sizeof(bufCnt), "Uplink Nro: %u", contador);
  u8g2.drawStr(0, 50, bufCnt);

  // 5. Estado de conexión LoRaWAN (Join / ACK / Reintento)
  u8g2.drawStr(0, 62, estadoLoRa ? estadoLoRa : "Standby");

  u8g2.sendBuffer();
}
