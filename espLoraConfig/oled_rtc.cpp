#include "oled_rtc.h"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1 

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
RTC_DS3231 rtc;

void inicializarPantallaRTC() {
  Wire.begin(D4, D5);

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("Error: No se pudo inicializar la OLED"));
  }
  
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  if (!rtc.begin()) {
    Serial.println("Error: No se encontró el módulo RTC");
  }

  if (rtc.lostPower()) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
}

void actualizarPantallaRTC() {
  DateTime now = rtc.now();
  display.clearDisplay();
  
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Monitor de Heladera");
  display.drawLine(0, 10, 128, 10, SSD1306_WHITE);
  
  display.setCursor(0, 18);
  display.print("Fecha: ");
  if (now.day() < 10) display.print('0');
  display.print(now.day(), DEC);
  display.print('/');
  if (now.month() < 10) display.print('0');
  display.print(now.month(), DEC);
  display.print('/');
  display.print(now.year(), DEC);

  display.setTextSize(2);
  display.setCursor(15, 38);
  
  if (now.hour() < 10) display.print('0');
  display.print(now.hour(), DEC);
  display.print(':');
  
  if (now.minute() < 10) display.print('0');
  display.print(now.minute(), DEC);
  display.print(':');
  
  if (now.second() < 10) display.print('0');
  display.println(now.second(), DEC);

  display.display();
}
