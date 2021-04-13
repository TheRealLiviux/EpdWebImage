# EpdWebImage
Retrieve images from a Web server and display it on a LilyGo T5 4.7" e-paper display.

The image is reloaded and refreshed every 5 minutes. In the meantime, if no buttons are pressed, the ESP32 goes to deep sleep in order to reduce consumption.

Pressing the rightmost button on the rear of the screen (GPIO 39) switches to the next in a set of 4 images (easy to increment, if needed). This allows to have any kind of visuals prepared on the server with any method (for example, periodically screen scraping a website) and to choose between them.

Pressing the leftmost button resets the ESP32, refreshing the display with the first image.

![image from website](meteo_venezia.jpg)
## Image format

For simplicity, the images are resized and converted to a simple uncompressed format on the server: 960x540 PGM binary greyscale (8 bit).
You can convert any image using ImageMagick or GraphicsMagick tool "convert".

For example:

    `convert <INPUT_FILE> -gravity center -resize 960x540 -extent 960x540 -colorspace Gray -sharpen 0x1.8 -dither FloydSteinberg -colors 16 pgm:epd_image_0.pgm`

Reducing the file to 16 grey levels with Floyd Steinberg dithering gives excellent results on the hi-dpi screen of the LilyGo, even for B&W high contrast photos.
![weather image](fine_art.jpg)

## TODO

- Keep the old image on the screen if downloading the new one fails. In place of the fullscreen error sign, show the error on the bottom banner.
- Compute a checksum of the downloaded image. Compare with the previous one kept in non-volatile RTC_DATA. If they match, leave the screen off and don't refresh it.
- Find a way to wake up the ESP32 with any of the four buttons (excluding "reset") and associate each with a single image, removing the need to (slowly!) cycle through all the images.
- Experiment with JPEG instead of PGM, using Bodmer library: https://github.com/Bodmer/TJpg_Decoder

# KNOWN ISSUES

Some PGM images are displayed with an offset to the right, as the code seems to slightly underestimate the length of the header.
