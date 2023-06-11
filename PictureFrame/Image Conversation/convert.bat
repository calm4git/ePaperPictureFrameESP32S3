rem magick ./raw/fr_507.jpg -dither FloydSteinberg -define dither:diffusion-amount=85% -remap eink-7color.png BMP3:./converted/0008.bmp
rem for /f %f in ('dir /b raw') do 	echo magick ./raw/%f -dither FloydSteinberg -define dither:diffusion-amount=85% -remap eink-7color.png BMP3:./converted/%f.bmp
rem for /f %%f in ('dir /b raw') do magick ./raw/%%f -dither FloydSteinberg -define dither:diffusion-amount=85%% -remap eink-7color.png BMP3:./converted/%%~nf.bmp
for /f %%f in ('dir /b scaled') do magick ./scaled/%%f -dither FloydSteinberg -define dither:diffusion-amount=85%% -remap eink-7color.bmp BMP3:./converted/%%~nf.bmp
	