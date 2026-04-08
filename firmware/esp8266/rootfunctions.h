 
bool downloadFile(const char* url, const char* localPath);
bool makeDir(const char* path);
bool deleteFile(const char* path);
void dumpFile(const char* filename);
void formatFileSystem();
void listFiles();
int loadAllConfigFromFlash(int mode, uint16_t param);
void saveAllConfigToFlash(uint16_t param);
void handleFileRequest();


int freeMemory();




