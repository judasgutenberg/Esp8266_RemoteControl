#ifndef FILEFUNCTIONS_H
#define FILEFUNCTIONS_H
#include "utilities.h"
#include "i2cslave.h"
#include "config.h"
#include "globals.h"

bool flashFromLittleFS(const char* path);
bool downloadFile(const char* url, const char* localPath);
bool makeDir(const char* path);
char* ensureLeadingSlash(const char *in);
void renameFile(const char* originalFileName, const char* newFileName);
bool deleteFile(const char* path);
void dumpFile(const char* filename);
void formatFileSystem();
void listFiles();
int loadAllConfigFromFlash(int mode, uint16_t param);
void saveAllConfigToFlash(uint16_t param);
void handleFileRequest();
void anomalyLog(const String &line);



#endif
