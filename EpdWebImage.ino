/**
   EpdWebImage.ino
    By: Livio Rossani
    Created on: 24.03.2021
    Retrieve a specific PGM image file from a Web server
    and display it on a LilyGo T5 e-paper display
    You can convert any photo using ImageMagick or GraphicsMagick tool "convert":
    convert <INPUT_FILE> -gravity center -resize 960x540 -extent 960x540 -colorspace Gray -sharpen 0x1.5 -dither FloydSteinberg -colors 16 pgm:epd_image
*/

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>
#include "epd_driver.h"
#include "secrets.h"

WiFiMulti wifiMulti;
const char* URL = "http://fotoni.it/public/2021/epd_image";
const int imageOffset = 30;
uint8_t *framebuffer;

void InitialiseDisplay() {
  epd_init();
  framebuffer = (uint8_t *)ps_calloc(sizeof(uint8_t), EPD_WIDTH * EPD_HEIGHT / 2);
  if (!framebuffer) Serial.println("Memory alloc failed!");
  memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);
}

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println();
  Serial.println();

  for (uint8_t t = 4; t > 0; t--) {
    Serial.printf("[SETUP] WAIT %d...\n", t);
    Serial.flush();
    delay(200);
  }
  // #define your WiFi SSID and password in "secrets.h" file
  wifiMulti.addAP(WIFI_SSID, WIFI_PWD);
  InitialiseDisplay();
}

void getImage() {
  // wait for WiFi connection
  if ((wifiMulti.run() == WL_CONNECTED)) {
    HTTPClient http;
    http.begin(URL);
    int httpCode = http.GET();

    if (httpCode > 0) {
      Serial.printf("[HTTP] GET... code: %d\n", httpCode);

      // file found at server
      if (httpCode == HTTP_CODE_OK) {
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
        } while ( headerRows < 2);
        // stream->readBytes(buff, imageOffset);
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

        Serial.println();
        Serial.print("[HTTP] connection closed or file end.\n");
      }
    } else {
      Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end();
  }
}

void edp_update() {
  epd_poweron();      // Switch on EPD display
  epd_clear();
  epd_draw_grayscale_image(epd_full_screen(), framebuffer); // Update the screen
  epd_poweroff_all(); // Switch off all power to EPD
}

void loop() {
  getImage();
  edp_update();
  delay(120000);
}
