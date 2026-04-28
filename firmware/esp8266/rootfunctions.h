void compileAndSendDeviceData(const String& weatherData,const String& whereWhenData, const String& powerData, bool doPinCursorChanges, uint16_t fRAMOrdinal);
void startWeatherSensors(int sensorIdLocal, int sensorSubTypeLocal, int i2c, int pinNumber, int powerPin);
void cleanup();


void notYetDeferred();

void printRTCDate();
void setDateDs1307(byte second,  byte minute, byte hour, byte dayOfWeek, byte dayOfMonth, byte month, byte year);
void sendIr(String rawDataStr);

void rebootEsp();
String weatherDataString(int sensorId, int sensorSubtype, int dataPin, int powerPin, int i2c, int deviceFeatureId, char objectCursor, String sensorName, int ordinalOfOverwrite, int consolidateAllSensorsToOneRecord); 




int freeMemory();
