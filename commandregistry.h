#ifndef COMMAND_REGISTRY_H
#define COMMAND_REGISTRY_H

#include <Arduino.h>

// Function pointer type
typedef void (*CommandHandler)(String* args, int argCount, bool deferred);

// Struct definition
struct CommandDef {
  const char* name;
  CommandHandler handler;
  int maxArgs;
  bool exactMatch;
  uint8_t configuration;
};

// 👇 Declare (not define!) the array
extern CommandDef commands[];

extern const int commandCount;

#endif