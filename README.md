# EpdWebImage
Retrieve an image file from a server and display it on a LilyGo T5 e-paper display

The file must be a 960x540 PGM greyscale image.
You can convert any image using ImageMagick or GraphicsMagick tool "convert":
    `convert <INPUT_FILE> -gravity center -resize 960x540 -extent 960x540 -colorspace Gray -sharpen 0x1.5 -dither FloydSteinberg -colors 16 pgm:epd_image`

## TODO
Deep sleep between refreshes

Maybe GIF decoder and image scaling line-by-line. Maybe it's better to leave it to the server.
