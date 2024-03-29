/*****************************************************************************
* | File      	:   EPD_5in65f.h
* | Author      :   Waveshare team
* | Function    :   5.65inch e-paper
* | Info        :
*----------------
* |	This version:   V1.0
* | Date        :   2020-07-08
* | Info        :
* -----------------------------------------------------------------------------
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documnetation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to  whom the Software is
# furished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS OR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
#
******************************************************************************/

#ifndef __EPD_5IN65F_H__
#define __EPD_5IN65F_H__

#include <SPI.h>
// Display resolution
#define EPD_WIDTH       600
#define EPD_HEIGHT      448

#define UWORD   unsigned int
#define UBYTE   unsigned char
#define UDOUBLE  unsigned long

/**********************************
Color Index
**********************************/
#define EPD_5IN65F_BLACK   0x0	/// 000
#define EPD_5IN65F_WHITE   0x1	///	001
#define EPD_5IN65F_GREEN   0x2	///	010
#define EPD_5IN65F_BLUE    0x3	///	011
#define EPD_5IN65F_RED     0x4	///	100
#define EPD_5IN65F_YELLOW  0x5	///	101
#define EPD_5IN65F_ORANGE  0x6	///	110
#define EPD_5IN65F_CLEAN   0x7	///	111   unavailable  Afterimage

class Epd {
public:
    Epd(SPIClass* SPI_BUS, int16_t DIN_Pin, int16_t CS_Pin, int16_t SCK_Pin, int16_t RESET_Pin, int16_t DC_Pin, int16_t BUSY_Pin);
    ~Epd();
    int  Init(void);
    void  InitSPI(void);
	  void EPD_5IN65F_BusyHigh(void);
	  void EPD_5IN65F_BusyLow(void);
    void Reset(void);
    void EPD_5IN65F_Display(uint8_t* image);
    void EPD_5IN65F_Display_part(const UBYTE *image, UWORD xstart, UWORD ystart, 
                                 UWORD image_width, UWORD image_heigh);
    void EPD_5IN65F_SendImage(const UBYTE *image);
    void EPD_5IN65F_WaitImageUpdateDone( void );
    void SendCommand(unsigned char command);
    void SendData(unsigned char data);
    void Wake(void);
    void Sleep(void);
    void Clear(UBYTE color);

private:
    int16_t _DIN_Pin;
    int16_t _CS_Pin;
    int16_t _SCK_Pin;
    int16_t _RESET_Pin;
    int16_t _DC_Pin;
    int16_t _BUSY_Pin;
    SPIClass * _SPI_BUS;
    unsigned long width;
    unsigned long height;
    uint8_t linebuffer[300];
};

#endif /* EPD5IN83B_HD_H */

/* END OF FILE */
