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
  WiFiClient client;
  HTTPClient http;
  textOut(F("Downloading: "));
  textOut(String(url) + "\n");
  if (!http.begin(client, url)) {
    textOut(F("HTTP begin failed\n"));
    return false;
  }

  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    textOut(F("HTTP GET failed\n"));
    http.end();
    return false;
  }
  WiFiClient* stream = http.getStreamPtr();
  File file = LittleFS.open(localPath, "w");
  if (!file) {
    textOut(F("File open failed\n"));
    http.end();
    return false;
  }
  uint8_t buf[256];
  int total = 0;
  while (http.connected() && (stream->available() || stream->connected())) {
    int avail = stream->available();
    if (!avail) {
      delay(2);
      yield();
      continue;
    }
    int toRead = min(avail, (int)sizeof(buf));
    int c = stream->readBytes(buf, toRead);

    if (c > 0) {
      size_t w = file.write(buf, c);

      if (w != c) {
        textOut(F("Write mismatch!\n"));
        break;
      }
      total += w;
      // ?? critical: force periodic flash commit
      if (total % 4096 == 0) {
        file.flush();
        yield();
      }
    }
  }
  file.flush();
  file.close();
  http.end();
  textOut(F("Written bytes: "));
  textOut(String(total) + F("\n"));
  File verify = LittleFS.open(localPath, "r");
  size_t size = verify.size();
  verify.close();
  textOut(F("Verified file size: "));
  textOut(String(size) + F("\n"));
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
  while (dir.next()) {
    textOut(F("FILE: ") + String(dir.fileName()) + " (" + String(dir.fileSize()) + F(" bytes)\n"));
    yield();
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
