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

/* Here you find the pin definitions for the board */
#define epd_DIN 35
#define epd_CLK 36
#define epd_CS 37
#define epd_DC 8
#define epd_RST 14
#define epd_BUSY 15

#define SD_CLK 12
#define SD_MOSI 11
#define SD_MISO 13
#define SD_CS 10

/* 1bit mode  
  SD_MOSI = SD_CMD
  SD_MISO = SD_D0  
  SD_SCK  = SD_SCK
*/
#define SD_CMD 11 
#define SD_D0 13
#define SD_D1 -1
#define SD_D2 -1
#define SD_D3 10
#define SD_SCK 12

//15 second sleep time
#define SLEEP_TIME_MS 15000

/* LC709203 gas gauge */
Adafruit_LC709203F lc;

/* this is some global memory we use inside the RTC domain */
RTC_DATA_ATTR uint8_t status    = 0;
RTC_DATA_ATTR uint32_t image_idx = 0;
RTC_DATA_ATTR uint32_t bootCount = 0;

/* We need an image buffer and assume a 600*448 pixel 4bpp image and display */
uint8_t * imagebuffer_ptr = NULL;

Epd* epd = new Epd(&SPI,epd_DIN, epd_CS, epd_CLK, epd_RST, epd_DC, epd_BUSY );

/* We need to store some information for the battery here as we only querry once */
typedef struct  {
float   percent;
float   voltage;
} battery_t;

battery_t battery;

void setup_gpio ( void );
void setup_sdmmc( void );
void setup_sdcard_spi ( void );
void read_image_sdcard( void );
void set_next_idx( uint32_t idx);
uint32_t get_current_idx( void );

uint32_t u32_swapp_endianess(uint32_t x);
uint16_t u16_swapp_endianess(uint16_t x);

void init_display ( void);

//Loads bitmapfile into a byte buffer for transfer tp edp
//retruns false if file can't be loaded
bool load_bitmap_for_epd(fs::FS &fs, String path,String filename, uint8_t* buffer);


void setup_gpio( ){
  //The Epaper displaydriver has its own part to setup the pins needed
  //We will disable everything not needed like the rgb led 
  //Disable power to sd-card
  pinMode(I2C_POWER,OUTPUT);
  digitalWrite(I2C_POWER,LOW);
}

void SDCardPower(bool enable ){
  if(true==enable){
    digitalWrite(I2C_POWER,HIGH);
  } else {
    digitalWrite(I2C_POWER,LOW);
  }
}

void update_display(uint8_t* imgptr){
  wake_display();
    //epd->Clear(EPD_5IN65F_BLUE);
    epd->EPD_5IN65F_Display(imgptr);
  epd->Sleep();
}

void loadnextimage(uint8_t* imgptr){
  read_image_sdcard(imgptr);
  update_display(imgptr);
  
}

void setup() {
  uint32_t serial_wait=0;
  Serial.begin(115200);
  while(0==Serial.available()){
    serial_wait++   ;
    delay(1);
    if((serial_wait % 512) == 0){
      Serial.print(".");
    }
  }

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
    imagebuffer_ptr = (uint8_t*)malloc( (600*448/2) );    
  }


  if(imagebuffer_ptr == NULL ){
    //Memory allocation failed ...
    // What to do next?
  } else {
    //We are good to go 
  }
  //Buffers are in place, lets set the io-pins as needed...
  setup_gpio();
  //We mount the sd-card and let the second core already read the image 
  /* This could become its own taks and run on the second CPU Core..... */
  Serial.println("Try to mount SD-Card");
  setup_sdmmc();
  
  init_display();
  loadnextimage(imagebuffer_ptr);
  Serial.println("Display update done");
  
  /*
  esp_sleep_enable_timer_wakeup( (uint64_t)(24*60*60*1000000) ); //24 hours
  esp_deep_sleep_start();  
  */
  
}

void setup_sdmmc( void ){
  if(false == SD_MMC.setPins(SD_CLK, SD_CMD, SD_D0, SD_D1, SD_D2, SD_D3) ){
    //This is a problem ... sort of
    Serial.println("SD/MMC Pin setup failed");
  } else {
    
    SDCardPower(true);
    delay(100); //Card need some time to initalize
    //We now can try to mount the sd-card (20MHz 1bit mode)
    if(!SD_MMC.begin("/sdcard", true, true, 20000, 5)){
          Serial.println("Card Mount Failed");
          return;
      }
      uint8_t cardType = SD_MMC.cardType();

      if(cardType == CARD_NONE){
          Serial.println("No SD_MMC card attached");
          return;
      } else {
          Serial.print("SD_MMC Card Type: ");
          if(cardType == CARD_MMC){
              Serial.println("MMC");
          } else if(cardType == CARD_SD){
              Serial.println("SDSC");
          } else if(cardType == CARD_SDHC){
              Serial.println("SDHC");
          } else {
              Serial.println("UNKNOWN");
          }
      }
      uint64_t cardSize = SD_MMC.cardSize() / (1024 * 1024);
      Serial.printf("SD Card Size: %lluMB\n", cardSize);
      
  }

}






void read_image_sdcard(uint8_t* img_ptr ){
  //we get a listing of all files on the image folder
  //and we than will open one by index if it is a bmp
  //we expect those to be in /images on the card
  uint32_t index = get_current_idx();
  String path = "/images";
  String filename = "";
  
  File file = openFileAtIdx(SD_MMC,path.c_str(),index);
  if(!file){
    index=0;    
    Serial.printf("Failed to open image at %i idx\r\n",index);
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
    bool result =  load_bitmap_for_epd(SD_MMC, path,filename, img_ptr);
  } else {
    //We have noe file to open          
  }
  
    

}

void set_next_idx( uint32_t idx){
  image_idx =idx; //Move data into RTC RAM
  Serial.printf("Set image idx %i \r\n", image_idx);
}

uint32_t get_current_idx( void ){
  uint32_t idx = image_idx;
  Serial.printf("Get image idx %i \r\n", image_idx);
  return idx;
}

void init_display ( void){
  Serial.print("Init edp");
  epd->Init();
  Serial.print("Init done");  
}

void wake_display( void ){
  Serial.print("wakeup display");
  epd->Wake();
  Serial.print("wakeup done");
}

uint32_t u32_swapp_endianess(uint32_t x){           
  return ( ( (x>>24) & 0x000000FF ) << 0) | ( ( (x>>16) & 0x000000FF ) << 8) | ( ( (x>>8) & 0x000000FF ) << 16) | ( ( (x>>0) & 0x000000FF ) << 24);
}

uint16_t u16_swapp_endianess(uint16_t x){
  return ( ( (x>>8) & 0x000000FF ) << 0) | ( ( (x>>0) & 0x000000FF ) << 8) ;
}

#define BMP_COMP_BI_RGB             0
#define BMP_COMP_BI_RLE8            1
#define BMP_COMP_BI_RLE4            2
#define BMP_COMP_BI_BITFIELDS       3
#define BMP_COMP_BI_JPEG            4
#define BMP_COMP_BI_PNG             5
#define BMP_COMP_BI_ALPHABITFIELDS  6
#define BMP_COMP_BI_CMYK            11
#define BMP_COMP_BI_CMYKRLE8        12
#define BMP_COMP_BI_CMYKRLE4        13


typedef struct __attribute__((__packed__)){
  char id[2];
  uint32_t FileSize;
  uint16_t Reserved0;
  uint16_t Reserved1;
  uint32_t ImageDataOffset;
} BMP_Header_t;

typedef struct __attribute__((__packed__)){
  uint32_t headersize;
  uint32_t imgwidth;
  uint32_t imgheight;
  uint16_t bitplanes;     
  uint16_t bitperpixel;
  uint32_t compression;
  uint32_t datasize;  
  uint32_t hor_resolution;
  uint32_t vert_resolution;
  uint32_t colorsinpalette;
  uint32_t numberofimportantcolors;
} DIB_Header_t;

typedef struct __attribute__((__packed__)){
  uint32_t color[16]; //16 color in RGB + 1 byte unused
} BMP_Color_Pallette16_t;

bool load_bitmap_for_epd(fs::FS &fs, String path, String filename, uint8_t* buffer){
    // If we could open a file we will print some debug information
    uint32_t start = millis();    
    File file = fs.open(path+"/"+filename);
    if(!file){
      Serial.println("Reader can't open file");
      return false;
    }
    Serial.print("  FILE returned: ");
    Serial.print(file.name());
    Serial.print("  SIZE: ");
    Serial.println(file.size());
    //File needs to be 134518 byte in size

    if((uint32_t)(134518) != file.size() ){
      Serial.println("Filesize != 134518 byte");
      file.close();
      return false;
    }
    //Next is to load the BMP/DIB header
    BMP_Header_t BMPHeader;
    DIB_Header_t DIBHeader;
    BMP_Color_Pallette16_t Palette;
    file.readBytes((char*)(&BMPHeader), sizeof(BMPHeader));
    Serial.printf("Header id %c %c\n\r",BMPHeader.id[0],BMPHeader.id[1]);
    Serial.printf("Filesize = %i\n\r", BMPHeader.FileSize);
    Serial.printf("Data offset = %i\n\r", BMPHeader.ImageDataOffset);
    file.readBytes((char*)(&DIBHeader), sizeof(DIBHeader));
    Serial.printf("Image width %i\n\r", DIBHeader.imgwidth);
    Serial.printf("Image height %i\n\r", DIBHeader.imgheight);
    Serial.printf("Bitperpixel %i\n\r", DIBHeader.bitperpixel); //if this is less than 8 we have a color palette 
    //Next is to read the plattet depeding on the bits per pixel
    //1 bit per pixel means 2 entries
    //4 bit per pixel means 16 entry
    //8 bit per pixel means 256 entry
    if(DIBHeader.bitperpixel!=4){ //May support later 1bpp images?
      Serial.println("We can only process 4 bit per pixel for now");
      file.close();
      return false;
    }
    
    if(DIBHeader.imgwidth!=600){
      Serial.println("With not 600\n\r");
      file.close();
      return false;
    }

    if(DIBHeader.imgheight!=448){
      Serial.println("Height != 448");
      file.close();
      return false;
    }

    file.readBytes((char*)(&Palette), sizeof(Palette));
    //We have now also read the palette, but will ignore it for now
    file.seek(BMPHeader.ImageDataOffset); //Jump to raw data and start reading.....
    //we have only 7 ish colors so we map everything bejond color 7 as transparent (only  0 to 6 are valid colors)
    //every byte holds 2 pixel, we read 300 bytes per line and 448 lines of data
    file.readBytes((char*)(imagebuffer_ptr),(BMPHeader.FileSize-BMPHeader.ImageDataOffset)); //We read all data at once into the buffer
    file.close();    
    //At this point we won't need the SD-Card any longer....
    //No color check we assume the bmp is in valid expected palette
    /*
    for(uint32_t i=0;i<BMPHeader.FileSize-BMPHeader.ImageDataOffset;i++){
          //If color > 7 set color to 7
          if( 0 !=(imagebuffer_ptr[i]&0x80)){
            imagebuffer_ptr[i]=((imagebuffer_ptr[i]&0x0F) | 0x70 );
            //Serial.printf("Byte %i color >7 in high nibble",i);
          }
          //If color > 7 set color to 7
          if( 0 !=(imagebuffer_ptr[i]&0x08)){
            imagebuffer_ptr[i]=((imagebuffer_ptr[i]&0xF0) | 0x07 );
            //Serial.printf("Byte %i color >7 in low nibble",i);
          } 
    }
    */ 
    //We can send the data to the display now...
    Serial.printf("Data loaded into memory (%i ms), read to be send...\n\r",(millis()-start) );
    return true;
}

void loop() {
 static uint32_t timeout = SLEEP_TIME_MS; //5 min in ms
 //We will wait here for 5 minutes and continue later on
 if(timeout>0){
  delay(1);
  timeout--;
 } else {
  loadnextimage(imagebuffer_ptr);
  timeout = SLEEP_TIME_MS;   
 }
 if(0==(timeout%1000)){
   Serial.println("Tick");
 }
 


 
}

