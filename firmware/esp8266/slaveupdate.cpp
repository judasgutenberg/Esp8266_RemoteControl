/////////////////////////////////////////////
//slave reflasher code -- this code, when run from a master, can reflash an I2C slave over I2C!  
//You must set fuses and install my modified twiboot  bootloader on your slave
//the branch of twiboot that this code expects is at: https://github.com/judasgutenberg/twiboot
//Gus Mueller, February 5, 2026
//Twiboot code is Copyright (C) 10/2020 by Olaf Rempel    
/*
the commands would be something like this for Atmega328p
.\avrdude.exe -c usbtiny -p m328p -U lfuse:w:0xC2:m -U hfuse:w:0xD8:m -U efuse:w:0xFD:m
.\avrdude.exe -c usbtiny -p m328p -U flash:w:twiboot.hex:i

*/
/////////////////////////////////////////////
 
#include "slaveupdate.h"
//commands and values the bootloader uses to communicate:
#define CMD_ACCESS_MEMORY      0x02
#define MEMTYPE_FLASH          0x01
#define CMD_SWITCH_APPLICATION 0x01
#define BOOTTYPE_APPLICATION   0x80
#define PAGE_SIZE              128
#define BOOT_MAGIC_VALUE 0xB007

uint32_t baseSlaveAddress = 0;
uint8_t pageBuffer[PAGE_SIZE];
uint32_t currentPageBase = 0xFFFFFFFF;
bool pagePending = false;   // true when current page has unsent data

// helper: convert two hex chars to a byte
uint8_t hexToByte(String hex) {
    return strtoul(hex.c_str(), nullptr, 16);
}

// Send a full 128-byte page to the slave in safe chunks, with retries and automatic chunk reduction
bool sendFlashPage(uint32_t pageAddr, uint8_t *data, bool debug) {
    if (debug) {
        Serial.print("Flashing page at 0x");
        Serial.println(pageAddr);
    }
 
    const int MAX_CHUNK_SIZE = 16;    // starting safe chunk size
    const int MIN_CHUNK_SIZE = 16;     // smallest chunk allowed
    const int MAX_RETRIES = 3;        // retry per chunk
    const int POST_CHUNK_DELAY = 10;   // ms to let slave settle

    int offsetInPage = 0;
  
    if(pageAddr == 0){
      Wire.beginTransmission(ci[SLAVE_I2C]);
      Wire.write(CMD_ACCESS_MEMORY);
      Wire.write(MEMTYPE_FLASH);
      Wire.write(0);
      Wire.write(0);
      Wire.endTransmission();
      delay(20);
    }
                
    while (offsetInPage < PAGE_SIZE) {
        int chunkSize = MAX_CHUNK_SIZE;
        bool chunkSent = false;
        if (debug) {
          Serial.print("In chunk loop at ");
          Serial.println(pageAddr);
        }
        while (!chunkSent && chunkSize >= MIN_CHUNK_SIZE) {
            int bytesThisChunk = min(chunkSize, PAGE_SIZE - offsetInPage);
            bool sent = false;

            for (int attempt = 1; attempt <= MAX_RETRIES && !sent; attempt++) {
                Wire.beginTransmission(ci[SLAVE_I2C]);
                Wire.write(CMD_ACCESS_MEMORY);
                Wire.write(MEMTYPE_FLASH);

                uint16_t byteAddr = pageAddr + offsetInPage;
                if (debug) {
                  Serial.print("Address on slave: ");
                  Serial.println(byteAddr);
                }
                Wire.write((byteAddr >> 8) & 0xFF);
                Wire.write(byteAddr & 0xFF);
                delay(3);
                for (int i = 0; i < bytesThisChunk; i++) {
                    Wire.write(data[offsetInPage + i]);
                }
                delay(2);
                uint8_t err = Wire.endTransmission();
                delay(2);
                if (err == 0) {
                    sent = true;
                    chunkSent = true;
                } else if (debug) {
                    Serial.print(" ERROR sending chunk at offset ");
                    Serial.print(offsetInPage);
                    Serial.print(" (attempt ");
                    Serial.print(attempt);
                    Serial.print("), bytesThisChunk=");
                    Serial.println(bytesThisChunk);
                    
                }
                delay(5 * attempt);  // gradually longer delay on retries
            }

            if (!chunkSent) {
                // reduce chunk size if it keeps failing
                if (chunkSize > MIN_CHUNK_SIZE) {
                    chunkSize /= 2;
                    if (debug) {
                        Serial.print(" Reducing chunk size to ");
                        Serial.println(chunkSize);
                    }
                } else {
                    // cannot reduce further
                    if (debug) {
                        Serial.print(" FAILED chunk at offset ");
                        Serial.println(offsetInPage);
                    }
                    return false; // abort page
                }
            }
        }

        offsetInPage += min(chunkSize, PAGE_SIZE - offsetInPage);
        delay(POST_CHUNK_DELAY);
    }

    if (debug) {
      Serial.println(" OK -- send flash page");
    }
    return true;
}



// Flush the last page at EOF or when page boundary changes
void flushLastPage(bool debug) {
    if (currentPageBase != 0xFFFFFFFF && pagePending) {
        if (debug) {
            Serial.print("Flushing last page at 0x");
            Serial.println(currentPageBase, HEX);
        }
        sendFlashPage(currentPageBase, pageBuffer, debug);
        if (debug) {
          Serial.println("........ OK");
        }
        currentPageBase = 0xFFFFFFFF;
        pagePending = false;
        memset(pageBuffer, 0xFF, PAGE_SIZE);
    }
}

// Process one line of the HEX file
void processHexLine(String line, bool debug) {
    line.trim();
    if (line.length() < 11 || line[0] != ':') return;

    uint8_t len   = hexToByte(line.substring(1,3));
    uint16_t addr = (hexToByte(line.substring(3,5)) << 8) | hexToByte(line.substring(5,7));
    uint8_t type  = hexToByte(line.substring(7,9));

    if (type == 0x01) return;  // EOF
    if (type == 0x04) {         // Extended linear address
        baseSlaveAddress = ((hexToByte(line.substring(9,11)) << 8) | hexToByte(line.substring(11,13))) << 16;
        return;
    }
    if (type != 0x00) return;   // ignore other types

    uint32_t absAddr = baseSlaveAddress + addr;

    for (int i = 0; i < len; i++) {
        uint32_t a = absAddr + i;
        uint32_t pageBase = a & ~(PAGE_SIZE-1);

        // flush previous page if we moved to a new one
        if (pageBase != currentPageBase) {
            flushLastPage(debug);         // flush old page if any
            currentPageBase = pageBase;   // update AFTER flush
        }

        pageBuffer[a - pageBase] = hexToByte(line.substring(9 + i*2, 11 + i*2));
        pagePending = true;              // mark that this page has unsent data
    }
}

// Main streaming loop (from server or local file)
void streamHexFile(Stream *stream, bool debug) {
    while (stream->available()) {
        String line = stream->readStringUntil('\n');
        processHexLine(line, debug);
    }
}

// Update slave firmware from a HEX URL
void updateSlaveFirmware(String url) {
    HTTPClient http;
    bool debug = false;
    http.begin(clientGet, url);
    int httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK) return;

    WiFiClient *stream = http.getStreamPtr();
    streamHexFile(stream, debug);

    finalizeBootloaderUpdate(debug);
}

// Finalize the update: flush last page + jump to application
void finalizeBootloaderUpdate(bool debug) {
    if (debug) {
      Serial.println("Flushing last page if needed...");
    }

    Wire.beginTransmission(ci[SLAVE_I2C]);
    Wire.write(CMD_ACCESS_MEMORY);   // command: access flash
    Wire.write(MEMTYPE_FLASH);
    Wire.write(0x00);               // dummy addr high
    Wire.write(0x00);               // dummy addr low
    Wire.endTransmission();
    
    flushLastPage(debug);
    delay(40);
    if (debug) {
      Serial.println("Requesting slave to jump to application...");
    }

    Wire.beginTransmission(ci[SLAVE_I2C]);
    Wire.write(CMD_SWITCH_APPLICATION);
    Wire.write(BOOTTYPE_APPLICATION);
    Wire.endTransmission();

    if (debug) {
      Serial.println("Jump command sent successfully.");
    }
}

void runSlaveSketch() {
  Wire.beginTransmission(ci[SLAVE_I2C]);
  Wire.write(CMD_SWITCH_APPLICATION); // tells slave to switch app/bootloader
  Wire.write(BOOTTYPE_APPLICATION);   // choose "application" path
  Wire.endTransmission();

  // small delay to allow the slave to react
  delay(10);
}

void enterSlaveBootloader() {
    Wire.beginTransmission(ci[SLAVE_I2C]);
    Wire.write(190); // COMMAND_ENTER_BOOTLOADER
    Wire.endTransmission();
    delay(20);
}

void leaveSlaveBootloader() {
  Wire.beginTransmission(ci[SLAVE_I2C]);
  Wire.write(CMD_SWITCH_APPLICATION);
  Wire.write(BOOTTYPE_APPLICATION);
  Wire.endTransmission();
}
