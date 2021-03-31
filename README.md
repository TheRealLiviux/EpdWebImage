# EpdWebImage
Retrieve an image file from a server and display it on a LilyGo T5 e-paper display

The file must be a 960x540 PGM binary greyscale image.
You can convert any image using ImageMagick or GraphicsMagick tool "convert":
    `convert <INPUT_FILE> -gravity center -resize 960x540 -extent 960x540 -colorspace Gray -sharpen 0x1.8 -dither FloydSteinberg -colors 16 pgm:epd_image`


## TODO
Show different images when different buttons are pressed.
This way, with static images or a script on the server, one could cycle between weather display, "facts of the day" on Wikipedia, updated news, photos etc.
