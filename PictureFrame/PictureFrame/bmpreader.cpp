#include "bmpreader.h"

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

#define DBGPRINT Serial1

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

typedef struct{
  uint8_t b;
  uint8_t g;  
  uint8_t r;
  uint8_t unused;  
} palette_color_t;

typedef union{
  uint32_t colordword;
  palette_color_t color;
} colorentry_t;

typedef struct __attribute__((__packed__)){
  colorentry_t entry[16]; //16 color in RGB + 1 byte unused
} BMP_Color_Pallette16_t;

typedef struct{
  palette_color_t input;
  uint8_t output;
} color_lut_entry_t;

/* this needs to be index input -> color output */
/*
  Display has following color arangement currently:
  0x00 -> Balck
  0x01 -> White
  0x02 -> Green  
  0x03 -> Blue
  0x04 -> Red
  0x05 -> Yellow
  0x06 -> Orange
  0x07 -> Transparent
*/
color_lut_entry_t color_lut[16];

#define epd_black 0x00
#define epd_white 0x01
#define epd_green 0x02
#define epd_blue 0x03
#define epd_red 0x04
#define epd_yellow 0x05
#define epd_orange 0x06
#define epd_transparent 0x07

#define rgb_red (0x00ed1c24)
#define rgb_black (0x00000000)
#define rgb_green (0x0022b14c)
#define rgb_yellow (0x00fff200)
#define rgb_blue (0x003f48cc)
#define rgb_transparent (0x00ffe282)
#define rgb_white (0x00ffffff)



bool load_bitmap_for_epd(fs::FS &fs, String path, String filename, uint8_t* buffer){
    // If we could open a file we will print some debug information
    uint32_t start = millis();    
    File file = fs.open(path+"/"+filename);
    if(!file){
      DBGPRINT.println("Reader can't open file");
      return false;
    }
    DBGPRINT.print("  FILE returned: ");
    DBGPRINT.print(file.name());
    DBGPRINT.print("  SIZE: ");
    DBGPRINT.println(file.size());

    //Next is to load the BMP/DIB header
    BMP_Header_t BMPHeader;
    DIB_Header_t DIBHeader;
    BMP_Color_Pallette16_t Palette;
    file.readBytes((char*)(&BMPHeader), sizeof(BMPHeader));
    DBGPRINT.printf("Header id %c %c\n\r",BMPHeader.id[0],BMPHeader.id[1]);
    DBGPRINT.printf("Filesize = %i\n\r", BMPHeader.FileSize);
    DBGPRINT.printf("Data offset = %i\n\r", BMPHeader.ImageDataOffset);
    file.readBytes((char*)(&DIBHeader), sizeof(DIBHeader));
    DBGPRINT.printf("Image width %i\n\r", DIBHeader.imgwidth);
    DBGPRINT.printf("Image height %i\n\r", DIBHeader.imgheight);
    DBGPRINT.printf("Bitperpixel %i\n\r", DIBHeader.bitperpixel); //if this is less than 8 we have a color palette 
    //Next is to read the plattet depeding on the bits per pixel
    //1 bit per pixel means 2 entries
    //4 bit per pixel means 16 entry
    //8 bit per pixel means 256 entry
    if(DIBHeader.bitperpixel!=4){ //May support later 1bpp images?
      DBGPRINT.println("We can only process 4 bit per pixel for now");
      file.close();
      return false;
    }
    file.readBytes((char*)(&Palette), sizeof(Palette));
    for(uint32_t i=0;i<16;i++){
      DBGPRINT.printf("Color entry %i\n\r", i);
      DBGPRINT.printf("Color dword 0x%08x -> ",Palette.entry[i].colordword );
      DBGPRINT.printf("R: [ 0x%02x / %i ] ",Palette.entry[i].color.r, Palette.entry[i].color.r  );      
      DBGPRINT.printf("G: [ 0x%02x / %i ] ",Palette.entry[i].color.g, Palette.entry[i].color.g );      
      DBGPRINT.printf("B: [ 0x%02x / %i ]\n\r",Palette.entry[i].color.b, Palette.entry[i].color.b );      
      
      color_lut[i].input=Palette.entry[i].color;
      switch (Palette.entry[i].colordword){

        case rgb_red:{
          color_lut[i].output=epd_red;        
        }break;

        case rgb_black:{
          color_lut[i].output=epd_black;
        }break;
        
        case rgb_green:{
          color_lut[i].output=epd_green;
        }break;

        case rgb_yellow:{
          color_lut[i].output=epd_yellow;
        }break;

        case rgb_blue :{
          color_lut[i].output=epd_blue;
        }break;

        case rgb_transparent:{
          color_lut[i].output=epd_transparent;
        }break;
        
        case rgb_white:{
          color_lut[i].output=epd_white;
        }break;

        default:{
          color_lut[i].output=epd_transparent;
        }break;

      }
      


    }


    
    if(DIBHeader.imgwidth!=600){
      DBGPRINT.println("With not 600\n\r");
      file.close();
      return false;
    }

    if(DIBHeader.imgheight!=448){
      DBGPRINT.println("Height != 448");
      file.close();
      return false;
    }

    file.seek(BMPHeader.ImageDataOffset); //Jump to raw data and start reading.....
    //we have only 7 ish colors so we map everything bejond color 7 as transparent (only  0 to 6 are valid colors)
    //every byte holds 2 pixel, we read 300 bytes per line and 448 lines of data
    file.readBytes((char*)(buffer),(BMPHeader.FileSize-BMPHeader.ImageDataOffset)); //We read all data at once into the buffer
    file.close();   

    //At this point we won't need the SD-Card any longer....
    //We need to change color according to a color palette we have in RAM
    uint32_t bytesperline = DIBHeader.imgwidth/2;
    for(uint32_t x=0;x<(DIBHeader.imgheight);x++){  
      for(uint32_t i=0;i<(bytesperline);i++){
       
        uint32_t offset = bytesperline*x;
        
        uint8_t pixel_a[2];
        uint8_t pixel_b[2];
        pixel_a[0]=(buffer[i+offset]>>4)&0x0F;
        pixel_a[1]=(buffer[i+offset]>>0)&0x0F;
        pixel_a[0]=color_lut[pixel_a[0]].output;
        pixel_a[1]=color_lut[pixel_a[1]].output;
        buffer[offset+i]= ( (pixel_a[0]<<4) &0xF0) | (pixel_a[1]&0x0F);

        
      }
    }
    //image is color corrected in ram but we need also to mirror it vertically
    
    for(uint32_t x=0;x<(DIBHeader.imgheight);x++){  
      for(uint32_t i=0;i<(bytesperline/2);i++){
       
        uint32_t offset = bytesperline*x;
        
        uint8_t pixel_a[2];
        uint8_t pixel_b[2];
        pixel_a[0]=(buffer[i+offset]>>4)&0x0F;
        pixel_a[1]=(buffer[i+offset]>>0)&0x0F;
        
        pixel_b[0]=(buffer[offset+bytesperline-1-i]>>4)&0x0F;
        pixel_b[1]=(buffer[offset+bytesperline-1-i]>>0)&0x0F;
        

        
        buffer[offset+bytesperline-1-i]= ( (pixel_a[1]<<4) &0xF0) | (pixel_a[0]&0x0F);
        buffer[offset+i] = ( (pixel_b[1]<<4) &0xF0) | (pixel_b[0]&0x0F);
        
      }
    }
    
    
    //We can send the data to the display now...
    DBGPRINT.printf("Data loaded into memory (%i ms), read to be send...\n\r",(millis()-start) );
    return true;
}

bool load_bitmap_for_epd_array(uint8_t* data, uint8_t* buffer){
   // If we could open a file we will print some debug information
    uint32_t start = millis();    
    if(!data){
      DBGPRINT.println("Can't open data, NULL ptr");
      return false;
    }
    
    //Next is to load the BMP/DIB header
    BMP_Header_t BMPHeader;
    DIB_Header_t DIBHeader;
    BMP_Color_Pallette16_t Palette;
    memcpy( (void*)(&BMPHeader), (void*)(data), sizeof(BMPHeader));
    DBGPRINT.printf("Header id %c %c\n\r",BMPHeader.id[0],BMPHeader.id[1]);
    DBGPRINT.printf("Filesize = %i\n\r", BMPHeader.FileSize);
    DBGPRINT.printf("Data offset = %i\n\r", BMPHeader.ImageDataOffset);
    memcpy((char*)(&DIBHeader), (void*)(data+ sizeof(BMPHeader) ), sizeof(DIBHeader));
    DBGPRINT.printf("Image width %i\n\r", DIBHeader.imgwidth);
    DBGPRINT.printf("Image height %i\n\r", DIBHeader.imgheight);
    DBGPRINT.printf("Bitperpixel %i\n\r", DIBHeader.bitperpixel); //if this is less than 8 we have a color palette 
    //Next is to read the plattet depeding on the bits per pixel
    //1 bit per pixel means 2 entries
    //4 bit per pixel means 16 entry
    //8 bit per pixel means 256 entry
    if(DIBHeader.bitperpixel!=4){ //May support later 1bpp images?
      DBGPRINT.println("We can only process 4 bit per pixel for now");
    
      return false;
    }
    memcpy((void*)(&Palette), (void*)(data+sizeof(BMPHeader)+sizeof(DIBHeader)), sizeof(Palette));
    for(uint32_t i=0;i<16;i++){
      DBGPRINT.printf("Color entry %i\n\r", i);
      DBGPRINT.printf("Color dword 0x%08x -> ",Palette.entry[i].colordword );
      DBGPRINT.printf("R: [ 0x%02x / %i ] ",Palette.entry[i].color.r, Palette.entry[i].color.r  );      
      DBGPRINT.printf("G: [ 0x%02x / %i ] ",Palette.entry[i].color.g, Palette.entry[i].color.g );      
      DBGPRINT.printf("B: [ 0x%02x / %i ]\n\r",Palette.entry[i].color.b, Palette.entry[i].color.b );      
      
      color_lut[i].input=Palette.entry[i].color;
      switch (Palette.entry[i].colordword){

        case rgb_red:{
          color_lut[i].output=epd_red;        
        }break;

        case rgb_black:{
          color_lut[i].output=epd_black;
        }break;
        
        case rgb_green:{
          color_lut[i].output=epd_green;
        }break;

        case rgb_yellow:{
          color_lut[i].output=epd_yellow;
        }break;

        case rgb_blue :{
          color_lut[i].output=epd_blue;
        }break;

        case rgb_transparent:{
          color_lut[i].output=epd_transparent;
        }break;
        
        case rgb_white:{
          color_lut[i].output=epd_white;
        }break;

        default:{
          color_lut[i].output=epd_transparent;
        }break;

      }
      


    }
   
    if(DIBHeader.imgwidth!=600){
      DBGPRINT.println("With not 600\n\r");

      return false;
    }

    if(DIBHeader.imgheight!=448){
      DBGPRINT.println("Height != 448");

      return false;
    }

    //we have only 7 ish colors so we map everything bejond color 7 as transparent (only  0 to 6 are valid colors)
    //every byte holds 2 pixel, we read 300 bytes per line and 448 lines of data
    memcpy((void*)(buffer),(void*)(data+BMPHeader.ImageDataOffset), ( BMPHeader.FileSize-BMPHeader.ImageDataOffset ) );
    //At this point we won't need the SD-Card any longer....
    //We need to change color according to a color palette we have in RAM
    uint32_t bytesperline = DIBHeader.imgwidth/2;
    for(uint32_t x=0;x<(DIBHeader.imgheight);x++){  
      for(uint32_t i=0;i<(bytesperline);i++){
       
        uint32_t offset = bytesperline*x;
        
        uint8_t pixel_a[2];
        uint8_t pixel_b[2];
        pixel_a[0]=(buffer[i+offset]>>4)&0x0F;
        pixel_a[1]=(buffer[i+offset]>>0)&0x0F;
        pixel_a[0]=color_lut[pixel_a[0]].output;
        pixel_a[1]=color_lut[pixel_a[1]].output;
        buffer[offset+i]= ( (pixel_a[0]<<4) &0xF0) | (pixel_a[1]&0x0F);

        
      }
    }
    //image is color corrected in ram but we need also to mirror it vertically
    
    for(uint32_t x=0;x<(DIBHeader.imgheight);x++){  
      for(uint32_t i=0;i<(bytesperline/2);i++){
       
        uint32_t offset = bytesperline*x;
        
        uint8_t pixel_a[2];
        uint8_t pixel_b[2];
        pixel_a[0]=(buffer[i+offset]>>4)&0x0F;
        pixel_a[1]=(buffer[i+offset]>>0)&0x0F;
        
        pixel_b[0]=(buffer[offset+bytesperline-1-i]>>4)&0x0F;
        pixel_b[1]=(buffer[offset+bytesperline-1-i]>>0)&0x0F;
        

        
        buffer[offset+bytesperline-1-i]= ( (pixel_a[1]<<4) &0xF0) | (pixel_a[0]&0x0F);
        buffer[offset+i] = ( (pixel_b[1]<<4) &0xF0) | (pixel_b[0]&0x0F);
        
      }
    }
    
    
    //We can send the data to the display now...
    DBGPRINT.printf("Data loaded into memory (%i ms), read to be send...\n\r",(millis()-start) );
    return true;
}


