#include "commandregistry.h"
#include "commandhandlers.h"

//that bit array on the end is a set of requirements for the command
//bits specifying requirements in the leftmost byte:  <requires_deferment><unused><unused><requires_ir> 
//bits specifying requirements in the rightmost byte:  <requires_rtc><requires_fram><requires_slave><requires_fs>

CommandDef commands[] = {
  {"reboot now",            cmdRebootEsp, 0,        true,         0b00000000},
  {"reboot slave",          cmdRebootSlave, 0,      true,         0b00000010},
  {"watchdog reboot",       cmdRebootMasterFromSlave, 0, true,    0b10000010},
  {"reboot",                cmdDeferredReboot, 0,   true,         0b10000000},
  {"update firmware",       cmdUpdateFirmware, 1,   true,         0b10000000},
  {"local update firmware", cmdLocalUpdateFirmware, 1,   true,    0b10000001},

  {"get bad reboot info",       cmdBadReboots, 0,      true,          0b00000000},
  {"clear safe mode",       cmdQuitSafeMode, 0,      true,        0b00000000},
  
  {"init sensors",          cmdInitSensors, 0,      true,         0b00000000},
  {"version",               cmdVersion, 0,          true,         0b00000000},
  {"run slave sketch",      cmdRunSlaveSketch, 0,   true,         0b00000010},
  {"slave bootloader",      cmdRunSlaveBootloader, 0, true,       0b00000010},
  {"pet watchdog",          cmdPetWatchdog, 0,      true,         0b00000010},
  {"get weather sensors",   cmdGetWeatherSensors, 0, true,        0b00000000},
  {"one pin at a time",     cmdOnePinAtATime, 0, false,           0b00000000},
  {"clear latency average", cmdClearLatencyAverage, 0, true,      0b00000000},
  {"ir",                    cmdIr, 1,               false,        0b00010000},
  {"clear fram",            cmdClearFram, 0,        true,         0b00000100},
  {"dump fram",             cmdDumpFram, 0,         true,         0b00000100},
  {"dump fram hex",         cmdDumpFramHex, 1,      false,        0b00000100}, 
  {"dump fram hex#",        cmdDumpFramHexAt, 1,    false,        0b00000100}, 
  {"swap fram",             cmdSwapFram, 0,         true,         0b00000100},
  {"dump fram record",      cmdDumpFramRecord, 1,   false,        0b00000100},
  {"get fram index",        cmdGetFramIndex, 0,     true,         0b00000100},
  {"set date",              cmdSetDate, 1,          false,        0b00001000},
  {"get date",              cmdGetDate, 0,          true,         0b00001000},
  {"get watchdog info",     cmdGetWatchdogInfo, 0,  true,         0b00000010},
  {"get watchdog data",     cmdGetWatchdogData, 0,  true,         0b00000010},

  {"save master config",    cmdSaveMasterConfig, 2, false,        0b00000000},
  {"save slave config",     cmdSaveSlaveConfig, 0,  true,         0b00000010},
  {"init master defaults",  cmdInitMasterDefaults, 0, true,       0b00000000}, 
  {"init slave defaults",   cmdInitSlaveDefaults, 0, true,        0b00000010},
  {"uptime",                cmdGetUptime, 0,        true,         0b00000000},
  {"get wifi uptime",       cmdGetWifiUptime, 0,    true,         0b00000000},
  {"get lastpoll",          cmdGetLastpoll, 0,      true,         0b00000000},
  {"get lastdatalog",       cmdGetLastdatalog, 0,   true,         0b00000000},
  {"memory",                cmdMemory, 0,           true,         0b00000000},
  {"dump parsed serial packet", cmdDumpSerialPacket, 0, true,     0b00000010},
  {"format file system",    cmdFormatFileSystem, 0, true,         0b00000001},
  ///////////
  {"anomaly log test",      cmdAnomalyLogTest, 1,   false,        0b00000001},
  {"ls",                    cmdListFiles, 0,        true,         0b00000001},
  {"rm",                    cmdDel, 1,              false,        0b00000001},
  {"mv",                    cmdRenameFile, 2,       false,        0b00000001},
  {"download",              cmdDownload, 1,         false,        0b00000001},
  //{"mkdir", cmdMkdir, 1, false, 0b00000000}, //i never got mkdir to work, so the file system is flat, like on a Commodore 64
  {"upload",                cmdUpload, 1,           false,        0b00000001},
  {"cat",                   cmdCat, 1,              false,        0b00000001},
  {"read slave eeprom",     cmdReadSlaveEeprom, 1,  false,        0b00000010},
  {"reset serial",          cmdResetSerial, 0,      false,        0b00000000},
  {"dump config",           cmdDumpConfig, 2,       false,        0b00000010},
  {"dump slave config eeprom", cmdDumpSlaveEeprom, 0, false,      0b00000010},
  {"send slave serial",     cmdSendSlaveSerial, 1,  false,        0b00000010},
  {"set slave time",        cmdSetSlaveTime, 1,     false,        0b00000010},
  {"get slave time",        cmdGetSlaveTime, 0,     false,        0b00000010},
  {"init slave serial",     cmdInitSlaveSerial, 0,  false,        0b00000010},
  {"get slave serial",      getSlaveSerial, 0,      false,        0b00000010},
  {"get slave parsed datum", getSlaveParsedDatum, 1, false,       0b00000010},
  {"update slave firmware", updateSlaveFirmware, 1, false,        0b00000010},
  {"get master eeprom used", getMasterEepromUsed, 0, false,       0b00000010},
  {"get slave eeprom used", getSlaveEepromUsed, 0,  false,        0b00000010},
  {"get slave",             getSlave, 1, false,                   0b00000010},
  {"set slave parser basis", setSlaveParserBasis, 2, false,       0b00000010},
  {"set slave basis",       setSlaveBasis, 2,       false,        0b00000010},
  {"set slave",             setSlave, 2,            false,        0b00000010},
  {"run slave",             runSlave, 2,            false,        0b00000010},
  {"set",                   cmdSet, 2,              false,        0b00000000},
  {"get",                   cmdGet, 1,              false,        0b00000000},
  // add more here…
};


const int commandCount = sizeof(commands) / sizeof(commands[0]);