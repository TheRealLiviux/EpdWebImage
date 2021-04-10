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
#include <WiFi.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>
#include "epd_driver.h"
#include "opensans12b.h"
#include "secrets.h"

WiFiMulti wifiMulti;
const char* URL = "http://fotoni.it/public/2021/epd_image.pgm";
uint8_t *framebuffer;
#define uS_TO_S_FACTOR 1000000ULL  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  5*60        /* Time ESP32 will go to sleep (in seconds) */

void InitialiseDisplay() {
  epd_init();
  framebuffer = (uint8_t *)ps_calloc(sizeof(uint8_t), EPD_WIDTH * EPD_HEIGHT / 2);
  if (framebuffer) {
    memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);
  }
}

void setup() {
  // #define your WiFi SSID and password in "secrets.h" file
  wifiMulti.addAP(WIFI_SSID, WIFI_PWD);
  InitialiseDisplay();
}

int getImage() {
  // wait for WiFi connection
  if ((wifiMulti.run() != WL_CONNECTED)) {
    return 0;
  }
  HTTPClient http;
  http.begin(URL);
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
  epd_poweroff_all(); // Switch off all power to EPD
}

FontProperties fontP = { 0xFF, 0x80, 33, 0};

void edp_errorSign() {
  epd_fill_circle(EPD_WIDTH / 2, EPD_HEIGHT / 2, EPD_HEIGHT / 2 - 20, 0, framebuffer);
  epd_fill_circle(EPD_WIDTH / 2, EPD_HEIGHT / 2, EPD_HEIGHT / 2 - 60, 0xFF, framebuffer);
  epd_fill_rect((EPD_WIDTH - EPD_HEIGHT) / 2 + 30, EPD_HEIGHT / 2 - 20, EPD_HEIGHT - 50, 40, 0, framebuffer);
  int cursor_x = (EPD_WIDTH - EPD_HEIGHT) / 2 + 88;
  int cursor_y = EPD_HEIGHT / 2 + 9;
  char *errMsg = "CONNESSIONE NON RIUSCITA";
//  write_string((GFXfont *)&OpenSans12B, errMsg, &cursor_x, &cursor_y, framebuffer);
  write_mode((GFXfont *)&OpenSans12B, errMsg, &cursor_x, &cursor_y, framebuffer, WHITE_ON_WHITE, &fontP);
}

void loop() {
  delay(1000);
  if (!getImage()) {
    edp_errorSign();
  }
  edp_update();
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  esp_deep_sleep_start();
}
