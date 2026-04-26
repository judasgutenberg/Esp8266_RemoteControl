#include "filefunctions.h"
#include <LittleFS.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>

/////////////////////////////////////////////
//file system routines
/////////////////////////////////////////////

bool downloadFile(const char* url, const char* localPath) {
  if (!LittleFS.begin()) {
    textOut("LittleFS mount failed\n");
    return false;
  }
  WiFiClient client;
  HTTPClient http;
  textOut("Downloading: ");
  textOut(String(url) + "\n");
  if (!http.begin(client, url)) {
    textOut("HTTP begin failed\n");
    return false;
  }
  int httpCode = http.GET();
  //Serial.println(http.header("Content-Encoding"));
  //Serial.println(http.header("Content-Type"));
  if (httpCode != HTTP_CODE_OK) {
    textOut("HTTP GET failed, code: " + String(httpCode) + "\n");
    http.end();
    return false;
  }
  File file = LittleFS.open(localPath, "w");
  if (!file) {
    textOut("Failed to open local file for writing\n");
    http.end();
    return false;
  }
  WiFiClient *stream = http.getStreamPtr();
  uint8_t buffer[128];
  int len = http.getSize();
  int total = 0;
  while (http.connected() || stream->available()) {
    size_t available = stream->available();
    if (available) {
      int c = stream->readBytes(buffer, min(available, sizeof(buffer)));
      file.write(buffer, c);
      total += c;
    } else {
      delay(1);  // give network time to breathe
    }
    yield();
  }
  file.close();
  http.end();
  textOut("Downloaded " + String(total) + " bytes\n");
  return true;
}

bool makeDir(const char* path) {
  if (!LittleFS.begin()) {
    textOut("LittleFS mount failed\n");
    return false;
  }
  // Ensure leading slash
  if (path[0] != '/') {
    textOut("Path must start with /\n");
    return false;
  }
  // Check if already exists
  if (LittleFS.exists(path)) {
    textOut("Folder already exists (or a file blocks it): ");
    textOut(path);
    textOut("\n");
    return true;  // treat as success
  }
  // Create placeholder file to force the directory to exist
  String placeholder = String(path) + "/.placeholder";
  File f = LittleFS.open(placeholder.c_str(), "w");
  if (f) {
    f.close();
    textOut("Created folder with placeholder: ");
    textOut(path);
    textOut("\n");
    return true;
  } else {
    textOut("Failed to create folder placeholder: ");
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
    textOut("LittleFS mount failed\n");
    return false;
  }
  if (!LittleFS.exists(path)) {
    textOut("file does not exist: ");
    textOut(path);
    textOut("\n");
    return false;
  }
  if (LittleFS.remove(path)) {
    textOut("deleted: ");
    textOut(path);
    textOut("\n");
    return true;
  } else {
    textOut("failed to delete: ");
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
        textOut("File open failed\n");
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
  textOut("File system formatted\n");
}

void listFiles() {
  if(!LittleFS.begin()) {
    return;
  }
  Dir dir = LittleFS.openDir("/");
  while (dir.next()) {
    textOut("FILE: " + String(dir.fileName()) + " (" + String(dir.fileSize()) + " bytes)\n");
    yield();
  }
}

int loadAllConfigFromFlash(int mode, uint16_t param) { //can also be used to recover values from FLASH
    if (!LittleFS.begin()) {
        return 0;
    }
    File f = LittleFS.open("/config.cfg", "r");
    if (!f) {
        return 0;
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
        return 0;
    }
    
    marker[4] = '\0';
 
    if (strcmp(marker, "DATA") != 0) {
        f.close();
        return 0;
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

    if (mode == 0) return 1;
    return 0;
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
    File f = LittleFS.open("/config.cfg", "w");
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
    server.send(404, "text/plain", "Not Found");
    return;
  }
  File file = LittleFS.open("/" + path, "r");
  if (!file) {
    server.send(500, "text/plain", "Failed to open file");
    return;
  }
  // Basic MIME type detection
  String contentType = "text/plain";
  if (path.endsWith(".html")) {
    contentType = "text/html";
  } else if (path.endsWith(".css")){
    contentType = "text/css";
  } else if (path.endsWith(".js")) {
    contentType = "application/javascript";
  } else if (path.endsWith(".png")) {
    contentType = "image/png";
  } else if (path.endsWith(".jpg") || path.endsWith(".jpeg")) {
    contentType = "image/jpeg";
  }
  server.streamFile(file, contentType);
  file.close();
}