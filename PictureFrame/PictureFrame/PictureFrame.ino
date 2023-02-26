/*
  This shall drive a 600x448 pixel 7-Color Epaper Display
  we need to run every 24 hours once and load a new image from sd-card
  also we need to monitor the battery using the provided gas gauge (LC709203 by onsemi)

  Library used:
  * Adafruit LC709203F
  * Adafruit BusIO
  
*/

/* Here we collec all the includes needed for this sketch */
#include "Adafruit_LC709203F.h"

/* Here you find the pin definitions for the board */
#define epd_DIN -1
#define epd_CLK -1
#define epd_CS -1
#define epd_DC -1
#define epd_RST -1
#define epd_BUSY -1

#define SDCARD_PWR -1

/* LC709203 gas gauge */
Adafruit_LC709203F lc;

/* this is some global memory we use inside the RTC domain */
RTC_DATA_ATTR uint8_t status    = 0;
RTC_DATA_ATTR uint8_t image_idx = 0;

/* We need an image buffer and assume a 600*448 pixel 4bpp image and display */
uint8_t * imagebuffer_ptr = null;



/* We need to store some information for the battery here as we only querry once */
typedef struct  {
float   percent;
float   voltage;
} battery_t;

battery_t battery;


void setup_gpio( ){
  //The Epaper displaydriver has its own part to setup the pins needed
  //We will disable everything not needed like the rgb led 

  
  //We also have a power pin for the SD-Card to save power 
  pinMode(SDCARD_PWR, OUTPUT);
  //We will enable power to the sdcard
  DigitalWrite(SDCARD_PWR, HIGH);

}




void setup() {
  Serial.begin(115200);
  if (!lc.begin()) {
    Serial.println(F("Couldnt find Adafruit LC709203F?\nMake sure a battery is plugged in!"));    
  } else {
    Serial.println(F("Found LC709203F"));
    Serial.print("Version: 0x"); Serial.println(lc.getICversion(), HEX);

    lc.setThermistorB(3950);
    Serial.print("Thermistor B = "); Serial.println(lc.getThermistorB());

    lc.setPackSize(LC709203F_APA_2000MAH);
    lc.setAlarmVoltage(3.5);
    battery.voltage = lc.cellVoltage();
    battery.percent = lc.cellPercent(); 
    lc.setPowerMode(LC709203F_POWER_SLEEP);
  }
 
  /* At this point we need to decide what we do:
  - If we have more than 10% within the battery we will load the next image
  - If we have 10% and below we will display a battery empty image and after sleep infinite
  */
  
  if (battery.percent<10){
    //Display empty symbol and do a long sleep ( as long as possible )
    
  } 
  
  if(ESP.getPsramSize()> (600*448/2) ){
    //Next is to get an image buffer with 600*448*4Bit = 144000 byte from psram 
    //This will be slower than getting some heap but also give more headroom for 
    //other parts that need some buffer like the sd-card reading
    imagebuffer_ptr = (byte*)ps_malloc( (600*448/2) );


  } else if( ESP.getFreeHeap() > (600*448/2) ){
    //This is not the good way to go as we eat up heap as it would be free....
    imagebuffer_ptr = malloc( (600*448/2) )    
  }


  if(imagebuffer_ptr == null ){
    //Memory allocation failed ...
    // What to do next?
  } else {
    //We are good to go 
  }
  //Buffers are in place, lets set the io-pins as needed...
  setup_gpio();
  //Next is to setup the SPI Interface for the epd


}

void loop() {
 


 
}

