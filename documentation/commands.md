# Interactive Commands


This document describes the command interface of the ESP8266 Remote
Control system, which is used to interact with a remote microcontroller via a serial connection or across the internet.

## Command syntax

Commands are matched by their registered command name, followed by zero or more
space-delimited arguments.

Arguments containing spaces can be enclosed in double quotes. The command parser
removes the quotes before passing the arguments to the command handler.  Configuration commands can refer to configuration items by name or by number if sent from via http but if sent via socket or serial, they have to refer to the configuration items by number.  

For example:

```text
run slave 160
set 42 123
set serial logging 1 "serial.log"
ir "A1 B2 C3"
```

The registry records, for each command:

- **Command name**
- **Handler**
- **Maximum argument count**
- **Serial/deferred flag**
- **Capability requirements**

The capability byte is encoded as:

```text
bit 7: requires deferment
bit 6: unused
bit 5: unused
bit 4: requires infrared
bit 3: requires RTC
bit 2: requires FRAM
bit 1: requires slave
bit 0: requires LittleFS
```

A command whose required capability is not present is rejected before its
handler is called.

---

# System and reboot commands

## `reboot now`

Immediately reboots the ESP8266.

```text
reboot now
```

No arguments.

This is distinct from `reboot`, which is marked as requiring deferment.

## `reboot`

Requests a deferred ESP8266 reboot.

```text
reboot
```

No arguments.

The deferred handler calls `rebootEsp()`.

## `reboot slave`

Reboots the I2C slave device.

```text
reboot slave
```

No arguments.

Requires a configured slave.

## `watchdog reboot`

Causes the slave/watchdog mechanism to request a reboot of the master.

```text
watchdog reboot
```

No arguments.

Requires a configured slave. The deferred handler sends slave command 134.

## `get preboot`

Displays information retained in the RTC preboot structure, including:

- reboot count
- milliseconds from the previous boot
- previous firmware version
- last command log ID
- last command ID
- last command type
- `useHardcodedConfig`

```text
get preboot
```

No arguments.

## `clear safe mode`

Marks the system as stable and disables startup safe mode, which a device can enter if some configuration isse causes it to crash repeatedly as it attempts to start up.

```text
clear safe mode
```

No arguments.

## `set preboot`

Sets one item in the RTC preboot structure.

```text
set preboot <location> <value>
```

Locations:

| Location | Meaning |
|---:|---|
| 1 | `lastVersion` |
| 2 | `lastCommandId` |
| 3 | `lastCommandType` |
| 4 | `useHardcodedConfig` |
| other | `lastCommandLogId` |

The new RTC structure is written with `rtcWrite()`.

---

# Firmware update commands

## `update firmware`

Updates the master ESP8266 firmware.

```text
update firmware <firmware>
```

The argument may be:

1. A complete `http://` URL.
2. A path beginning with `/`, which is appended to the configured host.
3. A backend firmware identifier/path, which is converted into a secured
   backend download URL.

The update is performed as a deferred operation.

HTTPS URLs are not handled by this code path.

## `local update firmware`

Updates firmware from a file already present in LittleFS.

```text
local update firmware <filename>
```

The deferred handler calls `flashFromLittleFS()` using the supplied filename.

Requires LittleFS.

## `update slave firmware`

Updates firmware on the I2C slave.

```text
update slave firmware <firmware>
```

The firmware argument is interpreted similarly to the master firmware update:

- complete `http://` URL
- `/path` on the configured host
- backend firmware identifier/path

The command verifies that the resulting URL exists, obtains the slave's
current version, enters the slave bootloader, performs the update, then waits
for the slave to report its new version after it boots up.

Requires a slave.

---

# Communications commands

## `set fast com`

Enables or disables the fast communications WebSocket.

```text
set fast com <value>
```

Use `1` to enable and `0` to disable.

When deferred:

- `1` starts the WebSocket with mode `3`.
- `0` stops the WebSocket and returns `outputMode` to `0`.

The command is registered as requiring deferment.

## `reset fast com`

Resets the fast communications WebSocket.

```text
reset fast com
```

No arguments.

Sets the output mode to fast communications mode (`3`), disconnects the
existing WebSocket, and starts a new WebSocket with mode `4`.

## `set serial swap`

Sets the serial swap state.  1 is swapped and 0 is unswapped. Read the details about how that works here: https://forum.arduino.cc/t/nodemcu-serial-communication-serial-swap-explained/643454

```text
set serial swap <value>
```

Calls `serialSwap()` with the integer value.

## `get serial swap`

Reports the current serial swap state.  1 is swapped and 0 is unswapped.

```text
get serial swap
```

The handler reports the value of `serialSwapped`.

> **Registry note:** this command is registered with a maximum argument count
> of 1 even though its handler does not use an argument.

## `set serial logging`

Enables or disables serial logging and optionally sets the log filename.

```text
set serial logging <value> [filename]
```

`0` disables logging; a nonzero value enables it.

If a filename is supplied, it becomes the serial logging filename.

Examples:

```text
set serial logging 0
set serial logging 1
set serial logging 1 serial.log
```

## `get serial logging`

Reports whether serial logging is enabled and, if applicable, its filename.

```text
get serial logging
```

## `reset serial`

Resets the serial interface using the configured baud-rate level.

```text
reset serial
```

No arguments.

---

# GPIO commands

## `get gpio`

Reads an ESP8266 GPIO pin.

```text
get gpio <pin>
```

Returns the value from `digitalRead()`.

## `set gpio`

Sets an ESP8266 GPIO pin.

```text
set gpio <pin> <value>
```

If the pin is not controlled by a device feature, it is configured as an
output and written with the supplied value.

If the pin is present in `pinMap`, the operation is refused because that pin
is controlled by a device feature.

> **Implementation note:** the current handler assigns both `pinNumber` and
> `value` from `param[0]`. Thus the registered second argument is currently
> not actually used by the handler. The intended behavior appears to be
> `param[1]` for the value.

## `get slave gpio`

Reads a GPIO on the current I2C slave.

```text
get slave gpio <pin>
```

Requires a slave.

## `set slave gpio`

Sets a GPIO on the current I2C slave.

```text
set slave gpio <pin> <value>
```

Requires a slave. Pins controlled by a device feature are refused.

## `dump gpio state`

Dumps the raw ESP8266 GPIO state.

```text
dump gpio state
```

The output contains:

```text
GPIO | DIR | VAL
```

For GPIOs 0 through 16, nonexistent GPIOs 6, 7, 8, and 11 are skipped.
GPIO16 is handled separately because its registers differ.

## `dump pin state`

Lists the pins known to the device, including the pin mapping, description,
and pin name.

```text
dump pin state
```

---

# Sensor and weather commands

## `init sensors`

Reinitializes the configured weather sensors.

```text
init sensors
```

No arguments.

## `dump weather data`

Returns weather sensor data.

```text
dump weather data <ordinal>
```

With ordinal `0`, the command obtains the current weather data from the
configured sensor.

For a nonzero ordinal, it obtains the corresponding line from the configured
additional-sensor information.

## `version`

Returns the firmware version.

```text
version
```

## `one pin at a time`

Sets one-pin-at-a-time mode.

```text
one pin at a time <value>
```

The value is converted to a Boolean and stored in `onePinAtATimeMode`.

## `ir`

Sends an IR command.

```text
ir "<IR data>"
```

The handler converts spaces in the supplied argument to commas before passing
the resulting string to `sendIr()`.

Requires an IR capability.

---

# FRAM commands

All FRAM commands require a configured FRAM device.

## `clear fram`

Clears the FRAM log.

```text
clear fram
```

## `dump fram`

Displays all FRAM records.

```text
dump fram
```

## `dump fram hex`

Displays a hexadecimal FRAM dump.

```text
dump fram hex [index]
```

If the argument is omitted or empty, the handler starts at:

```text
2 * FRAM_INDEX_SIZE
```

Otherwise the supplied value is used as the starting index.

## `dump fram hex#`

Displays a hexadecimal dump at a specified FRAM index.

```text
dump fram hex# <index>
```

## `swap fram`

Swaps FRAM contents using the configured FRAM index size and a hard-coded
second location of `554`.

```text
swap fram
```

## `dump fram record`

Displays one FRAM record.

```text
dump fram record <record-number>
```

## `get fram index`

Displays the FRAM record indexes.

```text
get fram index
```

---

# RTC commands

All commands in this section requiring the RTC require a configured RTC.

## `set date`

Sets the DS1307 date/time.

```text
set date <values>
```

The single argument must contain seven comma-delimited values. These are
passed in order to:

```text
setDateDs1307()
```

Example form:

```text
set date 0,0,0,0,0,0,0
```

The source code does not document the meaning of each of the seven fields, so
this document does not assign names to them.

## `get date`

Displays the RTC date/time.

```text
get date
```

---

# Watchdog and slave commands

## `pet watchdog`

Pets the slave watchdog using the current Unix time, keeping the watchdog from biting for the time being.

```text
pet watchdog
```

Requires a slave.

## `get watchdog info`

Returns slave watchdog information.

```text
get watchdog info
```

Requires a slave.

## `get watchdog data`

Returns slave watchdog data.

```text
get watchdog data
```

Requires a slave.

## `run slave sketch`

Runs whatever the sketch is currently on the slave.

```text
run slave sketch
```

Requires a slave.

## `slave bootloader`

Places the slave into its bootloader.

```text
slave bootloader
```

Requires a slave.

## `send slave serial`

Sends serial data to the slave.

```text
send slave serial <data>
```

Requires a slave.

## `get slave serial`

Reads serial data from the slave and returns it.

```text
get slave serial
```

Requires a slave.

## `get slave parsed datum`

Gets a parsed datum from the slave.

```text
get slave parsed datum <ordinal>
```

Requires a slave.

## `set slave time`

Sets the slave's Unix time.

```text
set slave time <unix-time>
```

Requires a slave.

## `get slave time`

Returns the slave's Unix time.

```text
get slave time
```

Requires a slave.

## `init slave serial`

Initializes serial communication on the slave.

```text
init slave serial
```

Requires a slave.

## `read slave eeprom`

Reads up to 500 bytes from the slave EEPROM.

```text
read slave eeprom <address>
```

Requires a slave.

## `dump slave parsed data`

Displays the parsed serial packet assembled on a slave as hexadecimal data.

```text
dump slave parsed data
```

Requires a slave.

## `run slave`

Runs an arbitrary numbered command on the slave.  See https://github.com/judasgutenberg/Arduino_I2C_Slave_With_Commands/blob/main/slave.ino for what numbers indicate what commands.

```text
run slave <command-number> [value]
```

If a value is supplied, it is sent to the slave with `sendLong()`.

If no value is supplied, the command is queried with `requestLong()`.

Requires a slave.

## `get slave`

Gets a numbered slave configuration item.

```text
get slave <ordinal>
```

Requires a slave.

## `set slave`

Sets a numbered slave configuration item.

```text
set slave <ordinal> <value>
```

Requires a slave.

## `set slave parser basis`

Sets a string value in the slave parser basis array.

```text
set slave parser basis <ordinal> <value>
```

Requires a slave.

## `set slave basis`

Sets a numeric value in the slave parser basis array.

```text
set slave basis <ordinal> <value>
```

Requires a slave.

---

# Configuration commands

## `save master config`

Saves the master configuration.

```text
save master config <to-word> [destination]
```

The handler's first argument is retained as `toWord`, although it is not used
by the current implementation.

The optional destination can be:

- `slave`
- `fram`

If no destination is specified, the configured persistence method determines
the destination.

If neither the selected persistence method nor the requested destination can
be used, configuration is saved to local flash.

The currently implemented destinations are slave EEPROM, FRAM, and local
flash. The source contains a commented-out local-flash selection branch for a
specific persistence-method case.

## `save slave config`

Saves the slave configuration to EEPROM.

```text
save slave config
```

Requires a slave.

## `init master defaults`

Initializes master configuration defaults.

```text
init master defaults
```

## `init slave defaults`

Initializes slave configuration defaults.

```text
init slave defaults
```

Requires a slave.

## `dump config`

Loads/displays configuration from the selected persistence source.

```text
dump config <from-word> [source]
```

The first argument is retained as `fromWord`, but the current handler does not
use it.

The optional source can be:

- `slave`
- `fram`

If neither applies, local flash is used.

Requires a slave when accessing slave EEPROM.

## `dump slave config eeprom`

Loads the slave configuration from the slave EEPROM at offset 512.

```text
dump slave config eeprom
```

Requires a slave.

## `get master eeprom used`

Reports the number of bytes used by the master configuration in slave EEPROM.

```text
get master eeprom used
```

Requires a slave.

## `get slave eeprom used`

Reports the number of bytes used by the slave configuration in slave EEPROM,
subtracting the 512-byte master offset.

```text
get slave eeprom used
```

Requires a slave.

## `set`

Sets a numbered configuration item.

```text
set <configuration-index> <value>
```

If the index refers to a numeric configuration item, the value is converted
to an integer.

If the index refers to a string configuration item, the value is stored as a
string.

For `I2C_SPEED`, the I2C clock is also immediately changed with
`Wire.setClock()`.

## `get`

Gets a numbered configuration item.

```text
get <configuration-index>
```

The index determines whether the numeric (`ci[]`) or string (`cs[]`)
configuration array is accessed.

---

# File-system commands

These commands require LittleFS.

## `format file system`

Formats the LittleFS file system.

```text
format file system
```

This is destructive.

## `ls`

Lists files.

```text
ls
```

The file system is intentionally flat. The source contains a commented-out
`mkdir` command and notes that directories were not implemented.

## `rm`

Deletes a file.

```text
rm <filename>
```

## `mv`

Renames a file.

```text
mv <old-name> <new-name>
```

## `download`

Downloads a file.

```text
download <url>
```

The downloaded file is saved using the filename extracted from the URL.

## `upload`

Uploads a local file.

```text
upload <filename>
```

The handler first checks that the file exists in LittleFS and that another
file is not already being uploaded.

## `cat`

Outputs a file to the current user interface.

```text
cat <filename>
```

---

# Serial parser commands

## `dump serial parser blocks`

Displays the configured serial parser blocks.

```text
dump serial parser blocks
```

If no blocks are configured, the command reports that a `serialparser.cfg`
file should be downloaded.

## `init serial parser`

Initializes the serial parser from `serialparser.cfg`.

```text
init serial parser
```

Reports an error if the file is missing or contains no valid configuration.

## `dump parsed data`

Displays the parsed master serial data joined with `*`.

```text
dump parsed data
```

The registry currently marks this command as requiring a slave, although the
handler itself accesses `serialParsedData` directly.

## `dump slave parsed data`

Displays the parsed serial packet assembled on a slave as hexadecimal.

```text
dump slave parsed data
```

Requires a slave.

---

# Diagnostics and status

## `uptime`

Reports how long ago the device last booted.

```text
uptime
```

## `get wifi uptime`

Reports how long WiFi has been up.

```text
get wifi uptime
```

## `get lastpoll`

Reports how long ago the last poll occurred.

```text
get lastpoll
```

## `get lastdatalog`

Reports how long ago the last data log occurred.

```text
get lastdatalog
```

## `timing`

Reports timing and communications statistics, including:

- loop count
- connection count
- logged serial bytes
- time since output mode changed
- serial data parsed
- average serial bytes per parsed item
- milliseconds per loop
- serial bytes per loop
- milliseconds per connection

Some fields are omitted when their corresponding counts are zero or when
serial parsing is configured as command-only.

## `memory`

Dumps memory statistics.

```text
memory
```

## `reset info`

Reports the ESP reset reason and reset information.

```text
reset info
```

## `wifi info`

Reports WiFi information including:

- RSSI
- SSID
- local IP
- MAC address
- gateway
- DNS server
- subnet mask
- WiFi channel
- PHY mode

## `flash chip info`

Reports flash-chip properties:

- flash chip size
- real flash chip size
- flash speed
- flash mode

## `flash info`

Reports:

- free sketch space
- sketch size

## `cpu`

Reports CPU and ESP information including:

- CPU frequency
- cycle count
- chip ID
- core version
- SDK version
- boot version
- boot mode
- VCC voltage

## `anomaly log test`

Adds the supplied argument to the anomaly log.

```text
anomaly log test <value>
```

---

# Serial data and debugging

## `dump master serial data`

The registered command is:

```text
dump parsed data
```

It outputs the current `serialParsedData` array joined with `*`.

## `dump slave parsed data`

The registered command is:

```text
dump slave parsed data
```

It reads the parsed slave serial packet and prints 20 bytes as hexadecimal.

---

# Latency

## `clear latency average`

Clears the accumulated latency count and latency sum.  A running latency average is used by the system to decide how much to offset a time component used to determine whether or not a poll is valid or not.

```text
clear latency average
```

---

# General notes about command execution

The command registry is the authoritative list of commands recognized by the
command dispatcher. Each command has a maximum argument count and a set of
capability flags.

Commands marked with the deferment bit are designed to be deferred. The
command execution machinery can save a command's state and retain the command
text so that it can be executed later.

A command can therefore have both an immediate phase and a deferred phase.
Several handlers deliberately produce a short message during the immediate
phase and perform the actual operation when called with `deferred == true`.

The command registry currently contains **99 commands**.

## Capability summary

| Capability | Registry bit | Meaning |
|---|---:|---|
| LittleFS | `0x01` | Local file system required |
| Slave | `0x02` | I2C slave required |
| FRAM | `0x04` | FRAM required |
| RTC | `0x08` | RTC required |
| IR | `0x10` | IR capability required |
| Deferment | `0x80` | Command requires deferred handling |

The current dispatcher explicitly checks RTC, FRAM, slave, and IR capability
requirements. The LittleFS requirement is represented in the command registry,
but the shown dispatcher does not perform an equivalent `CFG_REQ_FS`
capability check.

## An implementation discrepancy worth knowing

1. **`save master config` and `dump config`:** their first arguments are
   captured as `toWord` / `fromWord` but are not currently used to select the
   operation.
 
