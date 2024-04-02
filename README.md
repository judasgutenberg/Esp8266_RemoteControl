# Esp8266 Remote

This is another advancement from my Moxee Hotspot Watchdog system, which was itself an advancement from my ESP8266-Micro-Weather system.  This system still logs weather data and can, if configured correctly, reboot a Moxee cellular hotspot when it craps out and needs a reboot (which is essential for reliably connecting to my off-grid cabin).

But the main feature in this system is that it allows you to remote control devices across the internet.  It does this using a server running PHP/MySQL.  A web-based tool (tool.php) allows you to edit the value column of a table called device_feature, where device_id is equal to locationId specified in config.c.  The ESP8266 regularly polls the server to look for information in those device_feature records, and it automatically copies data from the value column to the pins they're connected to (which is specified in device_type_feature).  These pins can then be used to turn on relays, which allow you to turn on large loads in remote locations. I'm using this system to remotely turn on the boiler, hot water heater, and an electric space heater in my off-grid cabin.

This system is actually multi-user and supports multiple user accounts, each with multiple devices.  Most of the microcontroller work is done and I am in the process of improving the interface for someone performing remote control.
