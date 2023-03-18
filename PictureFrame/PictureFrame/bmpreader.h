#include "FS.h"
/*-----------------------------------------
Function  : load_bitmap_for_epd
Input     : fs:FS, String, String, uint8_t*
Output    : bool
Remarks   : none
-------------------------------------------*/
bool load_bitmap_for_epd(fs::FS &fs, String path, String filename, uint8_t* buffer);