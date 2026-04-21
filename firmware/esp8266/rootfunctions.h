

void notYetDeferred();

void printRTCDate();
void setDateDs1307(byte second,  byte minute, byte hour, byte dayOfWeek, byte dayOfMonth, byte month, byte year);
void sendIr(String rawDataStr);

void clearFramLog();
void displayFramRecord(uint16_t recordIndex);
void displayAllFramRecords();
void dumpFramRecordIndexes();
void hexDumpFRAMAtIndex(uint16_t index, uint8_t bytesPerLine, uint16_t maxLines);
void hexDumpFRAM(uint16_t startAddress, uint8_t bytesPerLine, uint16_t maxLines);
void swapFRAMContents(uint16_t location1, uint16_t location2, uint16_t length);
void addOfflineRecord(std::vector<std::tuple<uint8_t, uint8_t, double>>& record, uint8_t ordinal, uint8_t type, double value);





void rebootEsp();
String weatherDataString(int sensorId, int sensorSubtype, int dataPin, int powerPin, int i2c, int deviceFeatureId, char objectCursor, String sensorName, int ordinalOfOverwrite, int consolidateAllSensorsToOneRecord); 
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
