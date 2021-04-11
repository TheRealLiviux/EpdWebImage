/**
   EpdWebImage.ino
    By: Livio Rossani
    Created on: 24.03.2021
    Retrieve a specific PGM image file from a Web server
    and display it on a LilyGo T5 e-paper display
    You can convert any image file using ImageMagick or GraphicsMagick tool "convert" like this:
    convert <INPUT_FILE> -gravity center -resize 960x540 -extent 960x540 -colorspace Gray -sharpen 0x1.5 -dither FloydSteinberg -colors 16 pgm:epd_image.pgm
*/

#include <Arduino.h>
#include "epd_driver.h"
#include "opensans12b.h"
#include "esp_adc_cal.h"
#include <WiFi.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>
#include "secrets.h"
// Define your WiFi SSID and password in "secrets.h" file
#if !defined(WIFI_SSID)
#define WIFI_SSID "*************"
#endif
#if !defined(WIFI_PWD)
#define WIFI_PWD "*************"
#endif

int vref = 1100;
WiFiMulti wifiMulti;
String URL = "http://fotoni.it/public/2021/epd_image";  // Suffixed with "[screen_num].pgm"

uint8_t *framebuffer;
#define uS_TO_S_FACTOR 1000000ULL  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  5*60        /* Time ESP32 will go to sleep (in seconds) */
RTC_DATA_ATTR uint8_t screenNum = 0;

void InitialiseDisplay() {
  epd_init();
  framebuffer = (uint8_t *)ps_calloc(sizeof(uint8_t), EPD_WIDTH * EPD_HEIGHT / 2);
  if (framebuffer) {
    memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);
  }
}

void setup() {
  wifiMulti.addAP(WIFI_SSID, WIFI_PWD);
  InitialiseDisplay();

  esp_sleep_wakeup_cause_t wakeup_reason;
  wakeup_reason = esp_sleep_get_wakeup_cause();
  // If button has been pressed, cycle through 4 images
  if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT1) {
    screenNum = (++screenNum) & 3;
  }

}

int getImage(String imageUrl) {
  // wait for WiFi connection
  if ((wifiMulti.run() != WL_CONNECTED)) {
    return 0;
  }
  HTTPClient http;
  http.begin((const char *)(imageUrl.c_str()));
  int httpCode = http.GET();
  // file found at server
  if (httpCode != HTTP_CODE_OK) {
    return 0;
  }
  uint32_t frameoffset = 0;
  int len = http.getSize();
  // create buffer for read
  uint8_t buff[128] = { 0 };

  // get tcp stream
  WiFiClient * stream = http.getStreamPtr();

  // Skip the image header
  uint8_t headerRows = 0;
  do {
    headerRows += (stream->read() == 0x0a);
  } while ( headerRows < 2 && stream->available());

  // read all data from server, until the framebuffer is filled
  while (http.connected() && (len > 0 || len == -1) && (frameoffset < EPD_WIDTH * EPD_HEIGHT / 2)) {
    // get available data size
    size_t size = stream->available();

    if (size) {
      // read up to 128 byte
      int c = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));

      // Convert 8-bit pixels of the input buffer to 4-bit pixels in the framebuffer
      for (uint8_t p = 0; p < c ; p += 2) {
        *(framebuffer + frameoffset) = (buff[p + 1] & 0xf0) | (buff[p] >> 4);
        frameoffset++;
      }
      if (len > 0) {
        len -= c;
      }
    }
    delay(1);
  }
  http.end();
  return 1;
}

void edp_update() {
  epd_poweron();      // Switch on EPD display
  epd_clear();
  epd_draw_grayscale_image(epd_full_screen(), framebuffer); // Update the screen
  epd_poweroff(); // Switch off all power to EPD
}

FontProperties fontP = { 0xFF, 0x80, 33, 0};

int cursor_x = (EPD_WIDTH - EPD_HEIGHT) / 2 + 88;
int cursor_y = EPD_HEIGHT / 2 + 9;

void edp_errorSign() {
  epd_fill_circle(EPD_WIDTH / 2, EPD_HEIGHT / 2, EPD_HEIGHT / 2 - 20, 0, framebuffer);
  epd_fill_circle(EPD_WIDTH / 2, EPD_HEIGHT / 2, EPD_HEIGHT / 2 - 60, 0xFF, framebuffer);
  epd_fill_rect((EPD_WIDTH - EPD_HEIGHT) / 2 + 30, EPD_HEIGHT / 2 - 20, EPD_HEIGHT - 50, 40, 0, framebuffer);
  cursor_x = (EPD_WIDTH - EPD_HEIGHT) / 2 + 88;
  cursor_y = EPD_HEIGHT / 2 + 9;
  char *errMsg = "CONNESSIONE NON RIUSCITA";
  //  write_string((GFXfont *)&OpenSans12B, errMsg, &cursor_x, &cursor_y, framebuffer);
  write_mode((GFXfont *)&OpenSans12B, errMsg, &cursor_x, &cursor_y, framebuffer, WHITE_ON_WHITE, &fontP);
}

void epd_banner(char *text) {
  cursor_x = 14;
  cursor_y = EPD_HEIGHT - 2;
  epd_clear_area((Rect_t){2, EPD_HEIGHT - 20, EPD_WIDTH-4, 18});
  epd_clear_area((Rect_t){2, EPD_HEIGHT - 20, EPD_WIDTH-4, 18});
  write_string((GFXfont *)&OpenSans12B, text, &cursor_x, &cursor_y, NULL);
}

void prepareSleep() {
  //  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
  //  gpio_pullup_dis(GPIO_SEL_34);
  //  gpio_pulldown_en(GPIO_SEL_34);
  //  gpio_pullup_dis(GPIO_SEL_35);
  //  gpio_pulldown_en(GPIO_SEL_35);
  //  gpio_pullup_dis(GPIO_SEL_39);
  //  gpio_pulldown_en(GPIO_SEL_39);
  //  esp_sleep_enable_ext1_wakeup(GPIO_SEL_34 | GPIO_SEL_35 | GPIO_SEL_39, ESP_EXT1_WAKEUP_ANY_HIGH);
  esp_sleep_enable_ext1_wakeup(GPIO_SEL_39, ESP_EXT1_WAKEUP_ALL_LOW);
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  epd_poweroff_all();
}


int batteryCharge() {
  uint8_t percentage = 100;
  esp_adc_cal_characteristics_t adc_chars;
  esp_adc_cal_value_t val_type = esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100, &adc_chars);
  if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
    vref = adc_chars.vref;
  }
  float voltage = analogRead(36) / 4096.0 * 6.566 * (vref / 1000.0);
  if (voltage > 1 ) { // Only display if there is a valid reading
    percentage = 2836.9625 * pow(voltage, 4) - 43987.4889 * pow(voltage, 3) + 255233.8134 * pow(voltage, 2) - 656689.7123 * voltage + 632041.7303;
    if (voltage >= 4.20) percentage = 100;
    if (voltage <= 3.20) percentage = 0;  // orig 3.5
  }
  return percentage;
}

void loop() {
  int battery = batteryCharge();
  String imageUrl = URL + "_" + screenNum + ".pgm";
  char *message = (char *)((String("SHOW IMAGE #") + String(screenNum+1) + " - Battery:" + String(battery)).c_str());
  epd_banner(message);
  if (!getImage(imageUrl)) {
    edp_errorSign();
  }
  edp_update();
  delay(100);
  prepareSleep();
  esp_deep_sleep_start();
}
