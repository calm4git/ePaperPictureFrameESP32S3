#include "sdhelper.h"

#define DBGPRINT Serial

void listDir(fs::FS &fs, const char * dirname, uint8_t levels){
  DBGPRINT.printf("Listing directory: %s\n", dirname);

  File root = fs.open(dirname);
  if(!root){
    DBGPRINT.println("Failed to open directory");
    return;
  }
  if(!root.isDirectory()){
    DBGPRINT.println("Not a directory");
    return;
  }

  File file = root.openNextFile();
  while(file){
    if(file.isDirectory()){
      DBGPRINT.print("  DIR : ");
      DBGPRINT.println(file.name());
      if(levels){
        listDir(fs, file.name(), levels -1);
      }
    } else {
      DBGPRINT.print("  FILE: ");
      DBGPRINT.print(file.name());
      DBGPRINT.print("  SIZE: ");
      DBGPRINT.println(file.size());
    }
    file = root.openNextFile();
  }
}

void createDir(fs::FS &fs, const char * path){
  Serial.printf("Creating Dir: %s\n", path);
  if(fs.mkdir(path)){
    DBGPRINT.println("Dir created");
  } else {
    DBGPRINT.println("mkdir failed");
  }
}

void removeDir(fs::FS &fs, const char * path){
  DBGPRINT.printf("Removing Dir: %s\n", path);
  if(fs.rmdir(path)){
    DBGPRINT.println("Dir removed");
  } else {
    DBGPRINT.println("rmdir failed");
  }
}

void readFile(fs::FS &fs, const char * path){
  Serial.printf("Reading file: %s\n", path);

  File file = fs.open(path);
  if(!file){
    DBGPRINT.println("Failed to open file for reading");
    return;
  }

  Serial.print("Read from file: ");
  while(file.available()){
    DBGPRINT.write(file.read());
  }
  file.close();
}

void writeFile(fs::FS &fs, const char * path, const char * message){
  DBGPRINT.printf("Writing file: %s\n", path);

  File file = fs.open(path, FILE_WRITE);
  if(!file){
    DBGPRINT.println("Failed to open file for writing");
    return;
  }
  if(file.print(message)){
    DBGPRINT.println("File written");
  } else {
    DBGPRINT.println("Write failed");
  }
  file.close();
}

void appendFile(fs::FS &fs, const char * path, const char * message){
  DBGPRINT.printf("Appending to file: %s\n", path);

  File file = fs.open(path, FILE_APPEND);
  if(!file){
    Serial.println("Failed to open file for appending");
    return;
  }
  if(file.print(message)){
      DBGPRINT.println("Message appended");
  } else {
    DBGPRINT.println("Append failed");
  }
  file.close();
}

void renameFile(fs::FS &fs, const char * path1, const char * path2){
  Serial.printf("Renaming file %s to %s\n", path1, path2);
  if (fs.rename(path1, path2)) {
    DBGPRINT.println("File renamed");
  } else {
    DBGPRINT.println("Rename failed");
  }
}

void deleteFile(fs::FS &fs, const char * path){
  DBGPRINT.printf("Deleting file: %s\n", path);
  if(fs.remove(path)){
    DBGPRINT.println("File deleted");
  } else {
    DBGPRINT.println("Delete failed");
  }
}

File openFileAtIdx(fs::FS &fs, const char * path, uint32_t idx){
  uint32_t current_idx=0;
  File file;
  DBGPRINT.printf("Open at path %s at index %u", path, idx);
  File root = fs.open(path);
    if(!root){
        DBGPRINT.println("Failed to open directory");
        return file;
    }
    if(!root.isDirectory()){
        DBGPRINT.println("Not a directory");
        return file;
    }
    
    file = root.openNextFile(); //opens file in readmode
    while(file){
        if(file.isDirectory()){
          //we ignore directory entry
          file = root.openNextFile();
        } else {
            if(current_idx < idx){
              current_idx++;
              file = root.openNextFile();
            } else {
              DBGPRINT.print("  FILE: ");
              DBGPRINT.print(file.name());
              DBGPRINT.print("  SIZE: ");
              DBGPRINT.println(file.size());
              break; //exit while() and retun file obj
            }
        }        
    }
    return file;
}
