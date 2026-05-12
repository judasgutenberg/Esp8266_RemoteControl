#include "filefunctions.h"
#include <Updater.h>
#include <LittleFS.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>

/////////////////////////////////////////////
//file system routines
/////////////////////////////////////////////

//file uploads:

// Posts a LittleFS file as base64 in a single HTTP POST.
//
// POST variables:
//   mode=upload
//   device_id=<deviceId>
//   data=<base64 encoded file contents>
//
// Returns true on success.
bool postFileUpload(const char* localPath, const char* deviceId) {
    String host = String(cs[HOST_GET]);
    String path = String(cs[URL_GET]);
    if (!LittleFS.begin()) {
        textOut(F("LittleFS mount failed\n"));
        return false;
    }
    File f = LittleFS.open(localPath, "r");
    if (!f) {
        textOut(F("Failed to open file\n"));
        return false;
    }
    uint32_t totalSize = f.size();
    uint32_t encodedSize = ((totalSize + 2) / 3) * 4;
    String fullPath = path + "?mode=upload&filename=" + urlEncode(localPath, false) + "&total_size=" + String(totalSize) + "&device_id=" + urlEncode(deviceId, false);
    textOut(F("Connecting\n"));
    WiFiClient client;
    if (!client.connect(host.c_str(), 80)) {
        textOut(F("Connection failed\n"));
        f.close();
        return false;
    }
    textOut(F("Starting upload\n"));
    client.print(String("POST ") + fullPath + " HTTP/1.1\r\n");
    client.print(String("Host: ") + host + "\r\n");
    client.print(F("Content-Type: text/plain\r\n"));
    client.print(String("Content-Length: ") + encodedSize + "\r\n");
    client.print(F("Connection: close\r\n"));
    client.print(F("\r\n"));
    const size_t chunkSize = 384;
    uint8_t inbuf[chunkSize];
    int loopCount = 0;
    while (f.available()) {
        size_t len = f.read(inbuf, chunkSize);
        String encoded = base64::encode(inbuf, len);
        client.print(encoded);
        yield();
        loopCount++;
        //textOut(String(chunkSize * loopCount) + "\n");
    }
    f.close();
    textOut(F("Waiting for response\n"));
    uint32_t timeout = millis();
    while (client.connected() && !client.available()) {
        if (millis() - timeout > 10000) {
            textOut(F("Response timeout\n"));
            client.stop();
            return false;
        }
        yield();
    }
    String statusLine = client.readStringUntil('\n');
    statusLine.trim();
    textOut(F("Response: "));
    textOut(statusLine);
    textOut(F("\n"));
    int httpCode = -1;
    if (statusLine.startsWith("HTTP/1.1 ")) {
        httpCode = statusLine.substring(9, 12).toInt();
    }
    textOut(F("HTTP code: "));
    textOut(String(httpCode));
    textOut(F("\n"));
    while (client.connected()) {
        String line = client.readStringUntil('\n');
        line.trim();
        if (line.length() == 0) {
            break;
        }
        //textOut(line);
        //textOut(F("\n"));
    }
    //textOut(F("Response body:\n"));
    while (client.available() || client.connected()) {
        while (client.available()) {
            char c = client.read();
            //textOut(c);
        }
        yield();
    }
    client.stop();
    return (httpCode > 0 && httpCode < 400);
}

//not used; too slow!:
String packetString(String fileToUpload, uint32_t fileUploadPosition) {
    const size_t packetSize = 512;

    if (!LittleFS.begin()) {
        return "";
    }

    File f = LittleFS.open(fileToUpload, "r");
    if (!f) {
        return "";
    }

    // If position is beyond file, return empty
    if (fileUploadPosition >= f.size()) {
        f.close();
        //when bad things happen, we return this way
        return "";
    }

    // Move to desired position
    f.seek(fileUploadPosition, SeekSet);

    // Determine how many bytes we can actually read
    size_t bytesToRead = packetSize;
    if (fileUploadPosition + packetSize > f.size()) {
        bytesToRead = f.size() - fileUploadPosition;
    }

    uint8_t buffer[packetSize];
    size_t bytesRead = f.read(buffer, bytesToRead);

    f.close();

    if (bytesRead == 0) {
        return "";
    }

    // Base64 encode
    String encoded = base64::encode(buffer, bytesRead);

    return encoded;
}


//end file uploads

bool flashFromLittleFS(const char* path) {
  File firmware = LittleFS.open(path, "r");
  if (!firmware) {
    textOut("Failed to open firmware file");
    return false;
  }
  String bytesString =  F(" bytes\n");
  size_t size = firmware.size();
  textOut(F("Firmware size: ") + String(size) + bytesString);

  if (!Update.begin(size)) {
    textOut(F("Not enough space for OTA"));
    firmware.close();
    return false;
  }

  size_t written = Update.writeStream(firmware);

  if (written != size) {
    textOut(F("Only wrote ") + String(written) +  "/" + String(size) + bytesString);
    firmware.close();
    //Update.abort();
    return false;
  }

  if (!Update.end()) {
    String errorString = F("unknown");
    //errorString = Update.errorString();
    textOut(F("Update failed:  ") + errorString + "\n");
    firmware.close();
    return false;
  }

  firmware.close();

  textOut(F("Update complete. Rebooting..."));
  ESP.restart();
  return true;
}

//does not seem to work for files larger than about 28 kilobytes
bool downloadFile(const char* url, const char* localPath) {
  if (!LittleFS.begin()) {
    textOut(F("LittleFS mount failed\n"));
    return false;
  }
  textOut(F("Downloading: "));
  textOut(String(url) + "\n");
  WiFiClient client;
  HTTPClient http;
  if (!http.begin(client, url)) {
    textOut(F("HTTP begin failed\n"));
    return false;
  }
  // Prevent chunked transfer weirdness
  http.useHTTP10(true);
  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    textOut(F("HTTP GET failed\n"));
    textOut(F("HTTP code: "));
    textOut(String(httpCode) + "\n");
    http.end();
    return false;
  }
  int expectedSize = http.getSize();
  textOut(F("Expected size: "));
  textOut(String(expectedSize) + "\n");
  File file = LittleFS.open(localPath, "w");
  if (!file) {
    textOut(F("File open failed\n"));
    http.end();
    return false;
  }
  WiFiClient* stream = http.getStreamPtr();
  uint8_t buf[256];
  int total = 0;
  while (http.connected() || stream->available()) {
    size_t available = stream->available();
    if (available) {
      int len = stream->readBytes(buf, min(available, sizeof(buf)));
      if (len > 0) {
        size_t written = file.write(buf, len);
        if (written != (size_t)len) {
          textOut(F("Write mismatch!\n"));
          textOut(F("Expected write: "));
          textOut(String(len) + "\n");
          textOut(F("Actual write: "));
          textOut(String(written) + "\n");
          break;
        }
        total += len;
        if ((total % 4096) == 0) {
          file.flush();
        }
      }
    }
    yield();
  }
  file.flush();
  file.close();
  http.end();
  textOut(F("Written bytes: "));
  textOut(String(total) + "\n");
  File verify = LittleFS.open(localPath, "r");
  size_t size = 0;
  if (verify) {
    size = verify.size();
    verify.close();
  }
  textOut(F("Verified file size: "));
  textOut(String(size) + "\n");
  if (expectedSize >= 0) {
    textOut(F("Expected vs actual: "));
    textOut(String(expectedSize));
    textOut(F(" / "));
    textOut(String(size) + "\n");
    if (size != (size_t)expectedSize) {
      textOut(F("WARNING: size mismatch\n"));
    }
  }
  return (size > 0);
}





bool makeDir(const char* path) {
  if (!LittleFS.begin()) {
    textOut(F("LittleFS mount failed\n"));
    return false;
  }
  // Ensure leading slash
  if (path[0] != '/') {
    textOut(F("Path must start with /\n"));
    return false;
  }
  // Check if already exists
  if (LittleFS.exists(path)) {
    textOut(F("Folder already exists (or a file blocks it): "));
    textOut(path);
    textOut("\n");
    return true;  // treat as success
  }
  // Create placeholder file to force the directory to exist
  String placeholder = String(path) + "/.placeholder";
  File f = LittleFS.open(placeholder.c_str(), "w");
  if (f) {
    f.close();
    textOut(F("Created folder with placeholder: "));
    textOut(path);
    textOut("\n");
    return true;
  } else {
    textOut(F("Failed to create folder placeholder: "));
    textOut(path);
    textOut("\n");
    return false;
  }
}

char* ensureLeadingSlash(const char *in) {
  static char buf[128];
  if (!in) {
    buf[0] = '/';
    buf[1] = '\0';
    return buf;
  }
  if (in[0] == '/') {
    strncpy(buf, in, sizeof(buf));
    buf[sizeof(buf) - 1] = '\0';
    return buf;
  }
  snprintf(buf, sizeof(buf), "/%s", in);
  return buf;
}

void renameFile(const char* originalFileName, const char* newFileName) {
  LittleFS.rename(originalFileName, newFileName);
  textOut(String(originalFileName) + " renamed to " + String(newFileName) + "\n");
}

bool deleteFile(const char* path) {
  if (!LittleFS.begin()) {
    textOut(F("LittleFS mount failed\n"));
    return false;
  }
  if (!LittleFS.exists(path)) {
    textOut(F("file does not exist: "));
    textOut(path);
    textOut("\n");
    return false;
  }
  if (LittleFS.remove(path)) {
    textOut(F("deleted: "));
    textOut(path);
    textOut("\n");
    return true;
  } else {
    textOut(F("failed to delete: "));
    textOut(path);
    textOut("\n");
    return false;
  }
}

void dumpFile(const char* filename) {
    if (!LittleFS.begin()) {
        return;
    }
    File f = LittleFS.open(filename, "r");
    if (!f) {
        textOut(F("File open failed\n"));
        return;
    }
    const int BUF_SIZE = 128;
    char buffer[BUF_SIZE + 1];
    textOut("\n");
    while (f.available()) {

        size_t n = f.readBytes(buffer, BUF_SIZE);
        buffer[n] = 0;  // null terminate

        textOut(buffer);  // ?? send this chunk immediately
    }
    textOut("\n");
    f.close();
}

void formatFileSystem() {
  LittleFS.format();
  LittleFS.begin();
  textOut(F("File system formatted\n"));
}

void listFiles() {
  if(!LittleFS.begin()) {
    return;
  }
  Dir dir = LittleFS.openDir("/");
  bool filesWereFound = false;
  while (dir.next()) {
    textOut(F("FILE: ") + String(dir.fileName()) + " (" + String(dir.fileSize()) + F(" bytes)\n"));
    filesWereFound = true;
    yield();
  }
  if(!filesWereFound) {
    textOut(F("No files were found.\n");
  }
}

int loadAllConfigFromFlash(int mode, uint16_t param) { //can also be used to recover values from FLASH
    if (!LittleFS.begin()) {
        return -1;
    }
    File f = LittleFS.open(F("/config.cfg"), "r");
    if (!f) {
        return -1;
    }
    int*  activeCi;
    char** activeCs;
    int totalConfigItems       = CONFIG_TOTAL_COUNT;
    int totalStringConfigItems = CONFIG_STRING_COUNT;
    //Serial.println("----------SIZES");
    //Serial.println(totalConfigItems);
    //Serial.println(totalStringConfigItems);
    activeCi = ci;
    activeCs = cs;
    // ============================================================
    // 1. Read marker (must be "DATA")
    // ============================================================
    char marker[5];
    if (f.readBytes(marker, 5) != 5) {
        f.close();
        return -1;
    }
    
    marker[4] = '\0';
 
    if (strcmp(marker, "DATA") != 0) {
        f.close();
        return -1;
    }
 
    // ============================================================
    // 2. Read all integer configs (int16)
    // ============================================================
    for (int i = 0; i < totalConfigItems; i++) {
        uint8_t lo = f.read();
        uint8_t hi = f.read();
        int16_t v = (hi << 8) | lo;
        //Serial.print("value for #" + String(i) + ": ");
        //Serial.println(v);
        if (mode == 0) {
            activeCi[i] = v;
        } else if (mode == 1) {
            textOut(String(v));
            textOut("\n");
        }
        yield();
    }

    // ============================================================
    // 3. Read all string configs
    // ============================================================
    for (int i = 0; i < totalStringConfigItems; i++) {
        char buffer[128];
        int pos = 0;

        while (pos < 127 && f.available()) {
            char c = f.read();
            buffer[pos++] = c;
            if (c == 0) break;
        }
        buffer[127] = 0;
        yield();
        if (mode == 0) {
            size_t len = strlen(buffer);

            if (!activeCs[i]) {
                activeCs[i] = (char*)malloc(len + 1);
            } else if (strlen(activeCs[i]) < len) {
                char* tmp = (char*)realloc(activeCs[i], len + 1);
                if (tmp) activeCs[i] = tmp;
            }

            if (activeCs[i]) {
                memcpy(activeCs[i], buffer, len + 1);
            }
            //Serial.print("string value for #" + String(i) + ": ");
            //Serial.println(buffer);
        } else if (mode == 1) {
            textOut(buffer);
            textOut("\n");
        }
    }

    f.close();

    if (mode == 0) {
      return CONFIG_PERSIST_METHOD_FLASH;
    }
    return -1;
}

void saveAllConfigToFlash(uint16_t param) {
    int* activeCi;
    char** activeCs;
    uint32_t slaveConfigLocation = requestLong(ci[SLAVE_I2C], 164);
    int totalConfigItems = CONFIG_TOTAL_COUNT;
    int totalStringConfigItems = CONFIG_STRING_COUNT;
    activeCi = ci;
    activeCs = cs;
    if(false && param >= slaveConfigLocation) {
        totalConfigItems = CONFIG_SLAVE_TOTAL_COUNT;
        totalStringConfigItems = CONFIG_SLAVE_STRING_COUNT;
        activeCi = cis;
        activeCs = css;
    }
    if(!LittleFS.begin()) {
        return;
    }
    File f = LittleFS.open(F("/config.cfg"), "w");
    if(!f) {
        return;
    }

    // ============================================================
    // 1. Write marker "DATA\0"
    // ============================================================

    f.write((const uint8_t*)"DATA", 4);
    f.write((uint8_t)0);

    // ============================================================
    // 2. Write integer configs (2 bytes each)
    // ============================================================

    for(int i = 0; i < totalConfigItems; i++) {
        //Serial.println(i);
        int16_t v = activeCi[i];

        uint8_t lo = v & 0xFF;
        uint8_t hi = (v >> 8) & 0xFF;

        f.write(lo);
        f.write(hi);
        yield();
    }
    // ============================================================
    // 3. Write strings (null terminated)
    // ============================================================
    for(int i = 0; i < totalStringConfigItems; i++) {
        //Serial.println(i);
        const char* s = activeCs[i];
        if(s == NULL) s = "";
    
        //Serial.print("Value: ");
        //Serial.println(s);
        if(s == NULL) {
          s = "";
        }
        size_t len = strlen(s);
        f.write((const uint8_t*)s, len);
        f.write((uint8_t)0);
        yield();
    }
    f.close();
}

//serve a file from the LittleFS file system via http 
void handleFileRequest() {
  String path = server.uri(); // "/file.txt"
  if (path.startsWith("/")) {
    path = path.substring(1); // "file.txt"
  }
  if (path.length() == 0 || !LittleFS.exists("/" + path)) {
    // File doesn't exist, pass control to default 404
    server.send(404, F("text/plain"), F("Not Found"));
    return;
  }
  File file = LittleFS.open("/" + path, "r");
  if (!file) {
    server.send(500, F("text/plain"), F("Failed to open file"));
    return;
  }
  // Basic MIME type detection
  String contentType = F("text/plain");
  if (path.endsWith(F(".html"))) {
    contentType = "text/html";
  } else if (path.endsWith(F(".css"))){
    contentType = F("text/css");
  } else if (path.endsWith(".js")) {
    contentType = F("application/javascript");
  } else if (path.endsWith(F(".png"))) {
    contentType = F("image/png");
  } else if (path.endsWith(F(".jpg")) || path.endsWith(F(".jpeg"))) {
    contentType = F("image/jpeg");
  }
  server.streamFile(file, contentType);
  file.close();
}


//debugging
//////////////////////////////////////////////////////
void anomalyLog(const String &line) {
  //Serial.println(ci[ANOMALY_LOG_LEVEL]);
  // Logging disabled? vanish like a ghost 👻
  if (ci[ANOMALY_LOG_LEVEL] == 0) {
    return;
  }
  // Ensure filesystem is mounted (safe even if already mounted)
  if (!LittleFS.begin()) {
    //Serial.println("FS Begin Fail");
    // If FS fails, there's nowhere to log... so we just bail
    return;
  }
  File f = LittleFS.open(F("/anomaly.txt"), "a"); // append mode
  if (!f) {
    //Serial.println("FS Open Fail");
    return; // failed to open file
  }
  // Timestamp from the global timeClient
  unsigned long ts = timeClient.getEpochTime();
  // Format: epoch|your message
  f.print(ts);
  f.print(":");
  f.println(line);
  f.flush();
  f.close();
  //Serial.println("*" + line + "*");
  //Serial.println("Did the whole thing");
}
