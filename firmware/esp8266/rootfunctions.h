void compileAndSendDeviceData(const String& weatherData,const String& whereWhenData, const String& powerData, bool doPinCursorChanges, uint16_t fRAMOrdinal);
void startWeatherSensors(int sensorIdLocal, int sensorSubTypeLocal, int i2c, int pinNumber, int powerPin);
void cleanup();
void notYetDeferred();
void sendIr(String rawDataStr);
void rtcWrite(RTCBootInfo &d);
String weatherDataString(int sensorId, int sensorSubtype, int dataPin, int powerPin, int i2c, int deviceFeatureId, char objectCursor, String sensorName, int ordinalOfOverwrite, int consolidateAllSensorsToOneRecord); 

void rtcMarkStable();

void rebootEsp();
int freeMemory();
