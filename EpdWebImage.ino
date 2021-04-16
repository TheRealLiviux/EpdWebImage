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

enum imageState {
  IMAGE_UNCHANGED,
  IMAGE_CHANGED,
  IMAGE_ERROR
};

// Names to display on the banner when updating the display
char* screenNames[4]={"FOTO","OGGI","METEO","NOTIZIE"};
int vref = 1100;

WiFiMulti wifiMulti;
// Address of the images. File names go from "epd_image_0.pgm" to "epd_image_3.pgm"
String URL = "http://fotoni.it/public/2021/epd_image";  // Suffixed with "_[screen_num].pgm"

uint8_t *framebuffer;
#define uS_TO_S_FACTOR 1000000ULL  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  5*60        /* Time ESP32 will go to sleep (in seconds) */
RTC_DATA_ATTR uint8_t screenNum = 0;
RTC_DATA_ATTR uint16_t imageCrc[4] = {0, 0, 0, 0};

// This is actually executed at each wakeup
void setup() {
  wifiMulti.addAP(WIFI_SSID, WIFI_PWD);
  epd_init();
  framebuffer = (uint8_t *)ps_calloc(sizeof(uint8_t), EPD_WIDTH * EPD_HEIGHT / 2);
  if (framebuffer) {
    memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);
  }
  esp_sleep_wakeup_cause_t wakeup_reason;
  wakeup_reason = esp_sleep_get_wakeup_cause();
  // If button has been pressed, switch to the next image
  if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT1) {
    screenNum = (++screenNum) & 3;
    imageCrc[screenNum] = 0;
  }
}

int getImage(String imageUrl) {
  // wait for WiFi connection
  if ((wifiMulti.run() != WL_CONNECTED)) {
    return IMAGE_ERROR;
  }
  HTTPClient http;
  http.begin((const char *)(imageUrl.c_str()));
  int httpCode = http.GET();
  // file found at server
  if (httpCode != HTTP_CODE_OK) {
    return IMAGE_ERROR;
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
  stream->read();
  stream->read();
  uint16_t crc = 0;
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
        crc ^= (uint16_t)(*(buff + p));
      }
      if (len > 0) {
        len -= c;
      }
    }
    delay(1);
  }
  http.end();
  if (crc == imageCrc[screenNum]) {
    return IMAGE_UNCHANGED;
  }
  imageCrc[screenNum] = crc;
  return IMAGE_CHANGED;
}

void edp_update() {
  epd_poweron();      // Switch on Electronic Paper Display
  epd_clear();
  epd_draw_grayscale_image(epd_full_screen(), framebuffer); // Update the screen
  epd_poweroff(); // Switch off all power to EPD
  delay(250);
  epd_poweroff_all();
}

FontProperties fontP = { 0xFF, 0x80, 33, 0};

int cursor_x = (EPD_WIDTH - EPD_HEIGHT) / 2 + 88;
int cursor_y = EPD_HEIGHT / 2 + 9;


void epd_banner(char *text) {
  cursor_x = 14;
  cursor_y = EPD_HEIGHT - 2;
  epd_clear_area((Rect_t) {
    2, EPD_HEIGHT - 20, EPD_WIDTH - 4, 18
  });
  write_string((GFXfont *)&OpenSans12B, text, &cursor_x, &cursor_y, NULL);
}

void epd_cancel_banner() {
  uint8_t *bannerBuffer = framebuffer + EPD_WIDTH * (EPD_HEIGHT - 20) / 2;
  Rect_t area = { 0, EPD_HEIGHT - 20, EPD_WIDTH, 19 };
  epd_clear_area_cycles(area, 2, 250);
  delay(50);
  epd_draw_grayscale_image(area, bannerBuffer);
  delay(50);
  epd_draw_grayscale_image(area, bannerBuffer);
  delay(50);
  epd_draw_grayscale_image(area, bannerBuffer);
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
  char *message = (char *)(String("Batt:") + String(battery) + String("% - ") + String(screenNames[screenNum]) ).c_str();
  epd_banner(message);
  switch (getImage(imageUrl)) {
    case IMAGE_ERROR:
      // Draw an error message over the image
      // edp_errorSign();
      epd_banner("ERRORE DI CONNESSIONE");
      break;
    case IMAGE_CHANGED:
      // Redraw the whole screen
      edp_update();
      break;
    case IMAGE_UNCHANGED:
      // Redraw only the part of the image covered by the banner
      epd_cancel_banner();
      break;
  }
  delay(250);
  prepareSleep();
  esp_deep_sleep_start();
}
