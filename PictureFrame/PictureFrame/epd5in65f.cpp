/*****************************************************************************
* | File      	:   EPD_5in65f.c
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

#include <stdlib.h>
#include <Arduino.h>
#include "epd5in65f.h"

//#define NOEPD
#define DBGPRINT Serial1
Epd::~Epd() {
};

Epd::Epd(SPIClass * SPI_BUS, int16_t DIN_Pin, int16_t CS_Pin, int16_t SCK_Pin, int16_t RESET_Pin, int16_t DC_Pin, int16_t BUSY_Pin) {
    _DIN_Pin=DIN_Pin;
    _CS_Pin=CS_Pin;
    _SCK_Pin=SCK_Pin;
    _RESET_Pin=RESET_Pin;
    _DC_Pin=DC_Pin;
    _BUSY_Pin=BUSY_Pin;
    _SPI_BUS=SPI_BUS;
    
    width = EPD_WIDTH;
    height = EPD_HEIGHT;
};

/******************************************************************************
function :  Initialize the e-Paper register
parameter:
******************************************************************************/
int Epd::Init(void) {
    /* First setup the SPI Port here */

    this->InitSPI();
    this->Wake();
    this->Sleep();
    return 0;
}

void Epd::InitSPI(void) {
    /* First setup the SPI Port here */
    this->_SPI_BUS->begin(this->_SCK_Pin, -1 , this->_DIN_Pin, -1); //SCLK, ... , MOSI , we use only clock and dout on the esp32-s3
    //We only use Dout on the ESP32 here 
    this->_SPI_BUS->beginTransaction(SPISettings(2000000, MSBFIRST, SPI_MODE0));

}


/**
 *  @brief: basic function for sending commands
 */
void Epd::SendCommand(unsigned char command) {
    digitalWrite(this->_DC_Pin, LOW);
    digitalWrite(this->_CS_Pin, LOW);
    this->_SPI_BUS->transfer(command);
    digitalWrite(this->_CS_Pin, HIGH);
}

/**
 *  @brief: basic function for sending data
 */
void Epd::SendData(unsigned char data) {
    digitalWrite(this->_DC_Pin, HIGH);
    digitalWrite(this->_CS_Pin, LOW);
    this->_SPI_BUS->transfer(data);
    digitalWrite(this->_CS_Pin, HIGH);
    
}

void Epd::EPD_5IN65F_BusyHigh(void)// If BUSYN=0 then waiting
{
    #ifdef NOEPD
      DBGPRINT.print("EPD_5IN65F_BusyHigh() dummy mode (500ms)");
      delay(500); //Will simulate a busy line .... sort of
      return;
    #endif
    uint32_t start_time = millis();
    while(!(digitalRead(this->_BUSY_Pin))){
        if( (millis()-start_time) > (20*1000) ){
          //This can be considered as error......
          DBGPRINT.print("EPD_5IN65F_BusyHigh() > 20s runtime");
          start_time = millis();
        }                
    }
}

void Epd::EPD_5IN65F_BusyLow(void)// If BUSYN=1 then waiting
{
    #ifdef NOEPD
      Serial.print("EPD_5IN65F_BusyLow() dummy mode (500ms)");
      delay(500); //Will simulate a busy line .... sort of
      return;
    #endif
    //We have here a chance to never return
    uint32_t start_time = millis();
    while(digitalRead(this->_BUSY_Pin)){
      if( (millis()-start_time) > (20*1000) ){
          //This can be considered as error......
          DBGPRINT.print("EPD_5IN65F_BusyLow() > 20s runtime");
          start_time = millis();
        }   
    }
}

/**
 *  @brief: module reset.
 *          often used to awaken the module in deep sleep,
 *          see Epd::Sleep();
 */
void Epd::Reset(void) {
    digitalWrite(this->_RESET_Pin, LOW);                //module reset    
    delay(1);
    digitalWrite(this->_RESET_Pin, HIGH);
    delay(200);    
}

/******************************************************************************
function :  Sends the image buffer in RAM to e-Paper and displays
parameter:
******************************************************************************/
void Epd::EPD_5IN65F_Display(uint8_t *image) {
    unsigned long i,j;
    SendCommand(0x61);//Set Resolution setting
    SendData(0x02);
    SendData(0x58);
    SendData(0x01);
    SendData(0xC0);
    SendCommand(0x10);
    for(i=0; i<height; i++) {
        for(j=0; j<width/2; j++) {
          SendData(image[j+((width/2)*i)]);
		    }        
    }
    SendCommand(0x04);//0x04 -> Power On
    EPD_5IN65F_BusyHigh();
    SendCommand(0x12);//0x12 -> Refesh display 
    esp_sleep_enable_timer_wakeup(25*1000 * 1000); //25s cpu sleep
    esp_light_sleep_start();            
    EPD_5IN65F_BusyHigh();
    SendCommand(0x02);  //0x02  -> Power Off
    EPD_5IN65F_BusyLow();
	  delay(200);
}

void Epd::EPD_5IN65F_SendImage(const UBYTE *image) {
    
    SendCommand(0x61);//Set Resolution setting
    SendData(0x02);
    SendData(0x58);
    SendData(0x01);
    SendData(0xC0);
    SendCommand(0x10);
    for(uint32_t i=0; i<134400; i++) {
            SendData(image[i]);
    }
    SendCommand(0x04);//0x04
    EPD_5IN65F_BusyHigh();
    SendCommand(0x12);//0x12
}

void Epd::EPD_5IN65F_WaitImageUpdateDone(){
    EPD_5IN65F_BusyHigh();
    SendCommand(0x02);  //0x02  -> Power Off
    EPD_5IN65F_BusyLow();
	  delay(200);
}



/******************************************************************************
function :  Sends the part image buffer in RAM to e-Paper and displays
parameter:
******************************************************************************/
void Epd::EPD_5IN65F_Display_part(const UBYTE *image, UWORD xstart, UWORD ystart, 
                                        UWORD image_width, UWORD image_heigh)
{
    unsigned long i,j;
    SendCommand(0x61);//Set Resolution setting
    SendData(0x02);
    SendData(0x58);
    SendData(0x01);
    SendData(0xC0);
    SendCommand(0x10);
    for(i=0; i<height; i++) {
        for(j=0; j< width/2; j++) {
            if(i<image_heigh+ystart && i>=ystart && j<(image_width+xstart)/2 && j>=xstart/2) {
              SendData(pgm_read_byte(&image[(j-xstart/2) + (image_width/2*(i-ystart))]));
            }
			else {
				SendData(0x11);
			}
		}
    }
    SendCommand(0x04);//0x04
    EPD_5IN65F_BusyHigh();
    SendCommand(0x12);//0x12
    EPD_5IN65F_BusyHigh();
    SendCommand(0x02);  //0x02
    EPD_5IN65F_BusyLow();
	  delay(200);
}

/******************************************************************************
function : 
      Clear screen
******************************************************************************/
void Epd::Clear(UBYTE color) {
    SendCommand(0x61);//Set Resolution setting
    SendData(0x02);
    SendData(0x58);
    SendData(0x01);
    SendData(0xC0);
    SendCommand(0x10);
    for(int i=0; i<width/2; i++) {
        for(int j=0; j<height; j++) {
            SendData((color<<4)|color);
		}
	}
    SendCommand(0x04);//0x04
    EPD_5IN65F_BusyHigh();
    SendCommand(0x12);//0x12
    EPD_5IN65F_BusyHigh();
    SendCommand(0x02);  //0x02
    EPD_5IN65F_BusyLow();
    delay(500);
}

/**
 *  @brief: After this command is transmitted, the chip would enter the 
 *          deep-sleep mode to save power. 
 *          The deep sleep mode would return to standby by hardware reset. 
 *          The only one parameter is a check code, the command would be
 *          You can use EPD_Reset() to awaken
 */
void Epd::Sleep(void) {
    delay(100);
    SendCommand(0x07);
    SendData(0xA5);
    delay(100);
	  digitalWrite(this->_RESET_Pin, 0); // Reset
}


void Epd::Wake(void){
    pinMode(_CS_Pin,OUTPUT);
    digitalWrite(_CS_Pin, HIGH);
    pinMode(_DC_Pin, OUTPUT);
    pinMode(_RESET_Pin, OUTPUT);
    pinMode(_BUSY_Pin,INPUT_PULLDOWN);
    Reset();
    EPD_5IN65F_BusyHigh();
    SendCommand(0x00);
    SendData(0xEF);
    SendData(0x08);
    SendCommand(0x01);
    SendData(0x37);
    SendData(0x00);
    SendData(0x23);
    SendData(0x23);
    SendCommand(0x03);
    SendData(0x00);
    SendCommand(0x06);
    SendData(0xC7);
    SendData(0xC7);
    SendData(0x1D);
    SendCommand(0x30);
    SendData(0x3C);
    SendCommand(0x41);
    SendData(0x00);
    SendCommand(0x50);
    SendData(0x37);
    SendCommand(0x60);
    SendData(0x22);
    SendCommand(0x61);
    SendData(0x02);
    SendData(0x58);
    SendData(0x01);
    SendData(0xC0);
    SendCommand(0xE3);
    SendData(0xAA);
	
    delay(100); //delay function, here okay as we run on an OS
    
    SendCommand(0x50);
    SendData(0x37);
}
/* END OF FILE */
