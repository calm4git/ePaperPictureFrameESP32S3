/*
  This shall drive a 600x448 pixel 7-Color Epaper Display
  we need to run every 24 hours once and load a new image from sd-card
  also we need to monitor the battery using the provided gas gauge (LC709203 by onsemi)

  Library used:
  * Adafruit LC709203F
  * Adafruit BusIO
  
*/

/* Here we collec all the includes needed for this sketch */
#include "FS.h"
#include "SD_MMC.h"
#include "SD.h"
#include "SPI.h"


#include "Adafruit_LC709203F.h"
#include "epd5in65f.h"
#include "sdhelper.h"
#include "bmpreader.h"
#include "images.h"
/* Here you find the pin definitions for the board */
#define epd_DIN   35
#define epd_CLK   36
#define epd_CS    37
#define epd_DC    8
#define epd_RST   14
#define epd_BUSY  15

#define SD_CLK  12
#define SD_MOSI 11
#define SD_MISO 13
#define SD_CS   10

/* 1bit mode  
  SD_MOSI = SD_CMD
  SD_MISO = SD_D0  
  SD_SCK  = SD_SCK
*/
#define SD_CMD  11 
#define SD_D0   13
#define SD_D1   -1
#define SD_D2   -1
#define SD_D3   10
#define SD_SCK  12

#define LP_DISABLE_IN  18
#define WIFI_ENABLE_IN  17

#define RTC_VCC 5
#define RTC_GND 6

//Sleeptime in us
#define SLEEP_TIME_1s     (1000000ULL)
#define SLEEP_TIME_1m    (60000000ULL)
#define SLEEP_TIME_1h  (3600000000ULL)
#define SLEEP_TIME_1d (86400000000ULL)

#define DBGPRINT Serial1

/* LC709203 gas gauge */
Adafruit_LC709203F lc;

RTC_DATA_ATTR uint32_t image_idx = 0;
RTC_DATA_ATTR uint32_t bootCount = 0;
RTC_DATA_ATTR uint32_t lastboottime = 0;

/* We need an image buffer and assume a 600*448 pixel 4bpp image and display */
uint8_t * imagebuffer_ptr = NULL;

Epd* epd = new Epd(&SPI,epd_DIN, epd_CS, epd_CLK, epd_RST, epd_DC, epd_BUSY );

/* We need to store some information for the battery here as we only querry once */
typedef struct  {
float   percent;
float   voltage;
} battery_t;

battery_t battery;

/* Function prototypes */
void setup_gpio ( void );
bool setup_sdmmc( void );
bool end_sdmmc(void );
void SDCardPower(bool);
void set_next_idx(uint32_t);
uint32_t get_current_idx( void );
void read_image_sdcard( void );
void init_display ( void);
void entersleep( void );
void entersleepinf( void );

/*-----------------------------------------
Function  : entersleep
Input     : uint64 sleeptime 
Output    : none
Remarks   : sleep time in micor seconds
-------------------------------------------*/
void entersleep( void ){
  if( HIGH == digitalRead(LP_DISABLE_IN) ) {
     DBGPRINT.print("Sleep disable by LP_DISABLE_IN");
     delay(1000);
  } else {
    //If we end here we will try to sleep for a while 
    esp_sleep_enable_timer_wakeup( SLEEP_TIME_1d ); //24 hours
    esp_deep_sleep_start();  
  }
}

/*-----------------------------------------
Function  : entersleepinf
Input     : none
Output    : none
Remarks   : sleep without return
-------------------------------------------*/
void entersleepinf( ){
  if(HIGH == digitalRead(LP_DISABLE_IN) ) {
     DBGPRINT.print("Sleep disable by LP_DISABLE_IN");
     delay(1000);
  } else {
    esp_deep_sleep_start();  
  }
}



/*-----------------------------------------
Function  : setup_gpio
Input     : none
Output    : none
Remarks   : Configures GPIO pins
-------------------------------------------*/
void setup_gpio( ){
  //The Epaper displaydriver has its own part to setup the pins needed
  //We will disable everything not needed like the rgb led 
  //Disable power to sd-card
  pinMode(I2C_POWER,OUTPUT);
  digitalWrite(I2C_POWER,LOW);

  pinMode (LP_DISABLE_IN, INPUT_PULLDOWN);

}

/*-----------------------------------------
Function  : setup_sdmmc 
Input     : none
Output    : none
Remarks   : Configures SDHC/MMC Controller
            and mounts SD-Card
-------------------------------------------*/
bool setup_sdmmc( void ){ 
  SDCardPower(true);
  if(false == SD_MMC.setPins(SD_CLK, SD_CMD, SD_D0, SD_D1, SD_D2, SD_D3) ){
    //This is a problem ... sort of
    DBGPRINT.println("SD/MMC Pin setup failed");
    return false;
  } else {
    delay(100); //Card need some time to initalize
    //We now can try to mount the sd-card (20MHz 1bit mode)
    if(!SD_MMC.begin("/sdcard", true, true, 20000, 5)){
          Serial.println("Card Mount Failed");
          return false;
      }
      uint8_t cardType = SD_MMC.cardType();

      if(cardType == CARD_NONE){
          Serial.println("No SD_MMC card attached");
          return false;
      } else {
          DBGPRINT.print("SD_MMC Card Type: ");
          if(cardType == CARD_MMC){
              DBGPRINT.println("MMC");
          } else if(cardType == CARD_SD){
              DBGPRINT.println("SDSC");
          } else if(cardType == CARD_SDHC){
              DBGPRINT.println("SDHC");
          } else {
              DBGPRINT.println("UNKNOWN");
          }
      }
      uint64_t cardSize = SD_MMC.cardSize() / (1024 * 1024);
      DBGPRINT.printf("SD Card Size: %lluMB\n", cardSize);
      return true;
  }

}

bool end_sdmmc( void ){
  SD_MMC.end();
  SDCardPower(false);
  return true;
}

/*-----------------------------------------
Function  : SDCardPower 
Input     : bool
Output    : none
Remarks   : enable / disable power to sdcard
-------------------------------------------*/
void SDCardPower(bool enable ){
  if(true==enable){
    digitalWrite(I2C_POWER,HIGH);
  } else {
    digitalWrite(I2C_POWER,LOW);
  }
}

/*-----------------------------------------
Function  : update_display 
Input     : uint8_t* imgptr
Output    : none
Remarks   : User needs to provide buffer
            with image 
-------------------------------------------*/
void update_display(uint8_t* imgptr){
  wake_display();
  //epd->Clear(EPD_5IN65F_BLUE);
  epd->EPD_5IN65F_Display(imgptr);
  epd->Sleep();
}

/*-----------------------------------------
Function  : loadnextimage 
Input     : uint8_t* imgptr
Output    : none
Remarks   : User needs to provide buffer
            with image size bytes
-------------------------------------------*/
void loadnextimage(uint8_t* imgptr){
  
  
  if(false == read_image_sdcard(imgptr)){
    //We load a dummy image from flash here
    
  } else {
    update_display(imgptr);
  }
  
  
}

bool read_image_sdcard(uint8_t* img_ptr ){
  //we get a listing of all files on the image folder
  //and we than will open one by index if it is a bmp
  //we expect those to be in /images on the card
  uint32_t index = get_current_idx();
  String path = "/images";
  String filename = "";
  bool result = false;
  
  File file = openFileAtIdx(SD_MMC,path.c_str(),index);
  if(!file){
    index=0;    
    DBGPRINT.printf("Failed to open image at %i idx\r\n",index);
    File file = openFileAtIdx(SD_MMC,path.c_str(),0);
    index++;
    set_next_idx(index);
  } else {
    index++;
    set_next_idx(index);
  }
  filename = String(file.name());
  file.close();

  if(filename != ""){ //We can try to read the file    
    result =  load_bitmap_for_epd(SD_MMC, path,filename, img_ptr);
  } else {
    //We have no file to open  
    DBGPRINT.printf("File == NULL\n\r");
    result = false;           
  }
  return result;
  
    

}

void set_next_idx( uint32_t idx){
  image_idx =idx; //Move data into RTC RAM
  DBGPRINT.printf("Set image idx %i \r\n", image_idx);
}

uint32_t get_current_idx( void ){
  uint32_t idx = image_idx;
  DBGPRINT.printf("Get image idx %i \r\n", image_idx);
  return idx;
}

void init_display ( void){
  DBGPRINT.print("Init edp");
  epd->Init();
  DBGPRINT.print("Init done");  
}

void wake_display( void ){
  DBGPRINT.print("wakeup display");
  epd->Wake();
  DBGPRINT.print("wakeup done");
}

void setup() {
  bootCount=bootCount+1;
  Serial1.begin(460800,SERIAL_8N1,-1,TX1);
  if (!lc.begin()) {
    DBGPRINT.println(F("Couldnt find Adafruit LC709203F?\nMake sure a battery is plugged in!"));    
  } else {
    DBGPRINT.println(F("Found LC709203F"));
    DBGPRINT.print("Version: 0x"); 
    DBGPRINT.println(lc.getICversion(), HEX);

    lc.setThermistorB(3950);
    DBGPRINT.print("Thermistor B = "); Serial.println(lc.getThermistorB());

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
  if(ESP.getPsramSize()> (600*448/2) ){
    //Next is to get an image buffer with 600*448*4Bit = 144000 byte from psram 
    //This will be slower than getting some heap but also give more headroom for 
    //other parts that need some buffer like the sd-card reading
    imagebuffer_ptr = (byte*)ps_malloc( (600*448/2) );


  } else if( ESP.getFreeHeap() > (600*448/2) ){
    //This is not the good way to go as we eat up heap as it would be free....
    imagebuffer_ptr = (uint8_t*)malloc( (600*448/2) );    
  }

  if(imagebuffer_ptr == NULL ){
    //Memory allocation failed ...
    epd->Init();
    epd->Wake();
    epd->Clear(EPD_5IN65F_RED);
    epd->Sleep();
  } else {
    //We are good to go 
  }
  
  if (battery.percent<10){
    //Display empty symbol and do a long sleep ( as long as possible )
    load_bitmap_for_epd_array((uint8_t*)_acBatteryEmpty,imagebuffer_ptr);
    init_display();
    update_display(imagebuffer_ptr);
    entersleepinf();  //Sleep forever....
  } 

  
  //Buffers are in place, lets set the io-pins as needed...
  DBGPRINT.println("Setup GPIO");
  setup_gpio();
  
  DBGPRINT.println("Setup SD/MMC");
  if(true == setup_sdmmc() ){
    DBGPRINT.println("Init Display");
    init_display();
    DBGPRINT.println("Load BMP");
    loadnextimage(imagebuffer_ptr);
    end_sdmmc();
    DBGPRINT.println("Update done");
    entersleep(); //Sleep for 24 hours
  } else {
    load_bitmap_for_epd_array((uint8_t*)_acNo_Sd_Card,imagebuffer_ptr);
    init_display();
    update_display(imagebuffer_ptr);
    
    entersleepinf();  //Sleep forever....
  }
    
}

void loop() {
  entersleep(); //Sleep for 24 hours
}

