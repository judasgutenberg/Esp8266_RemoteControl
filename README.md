# Esp8266 Remote

This is another advancement from my Moxee Hotspot Watchdog system, which was itself an advancement from my ESP8266-Micro-Weather system.  This system still logs weather data and can, if configured correctly, reboot a Moxee cellular hotspot when it craps out and needs a reboot (which is essential for reliably connecting to my off-grid cabin).

But the main feature in this system is that it allows you to remote control devices across the internet.  It does this using a server running PHP/MySQL.  A web-based tool (tool.php) allows you to edit the value column of a table called device_feature, where device_id is equal to locationId specified in config.c.  The ESP8266 regularly polls the server to look for information in those device_feature records, and it automatically copies data from the value column to the pins they're connected to (which is specified in device_type_feature).  These pins can then be used to turn on relays, which allow you to turn on large loads in remote locations. I'm using this system to remotely turn on the boiler, hot water heater, and two different outlets connected to electric space heaters in my off-grid cabin.

One caveat: this system is one where the server tells the microcontroller what pins do what. For some pins (notably GPIO10 and GPIO9 on an ESP8266, which are used for accessing on-board flash storage), setting them to outputs and forcing them to take a value will cause the ESP8266 to crash and restart.  So definitely test all your pin control arrangements while local to the ESP8266 before using it remotely.

To expand the number of pins usable for remote control, you can add a slave Arduino with my slave software:
https://github.com/judasgutenberg/Generic_Arduino_I2C_Slave and just add the I2C address of the slave Arduino to the device_type_feature record.

This system is actually multi-user and supports multiple user accounts, each with potentially multiple devices.  Most of the microcontroller work is done and I am in the process of improving the interface for someone performing remote control.  Until that is complete, here's an overview of how to set up control for a particular device:

1. Connect the device to the ESP8266 somehow.  Usually this involves a relay and a relay driver circuit such as the ULN2003 (there are lots of examples of this online, for example https://microcontrollerslab.com/relay-driver-circuit-using-uln2003/).
2. Add a record to the device table to represent your particular ESP8266.  At this point device_id and location_id are the same. Your ESP8266 will send that id to the server whenever it polls it to get updates on what values its pins should have.  You will need to set locationId to this value in config.c before compiling the code for you ESP8266.
3. Add a record to the device_type table describing your particular type of device (in this case, ESP8266).  Details for this don't matter much.
4. Add a record to the device_type_feature table describing the pin (mostly the pin number and perhaps the I2C address if it is on a slave).
5. Add a record to the device_feature table describing the specific pin (here a human readable name would be useful).
6. Now you can change the state of a particular ESP8266's pin by changing the state of the value column in the device_feature record.  Make sure to set enabled to 1 as well.

Here is the user interface, which allows you to turn items on and off in the list by checking the on column.
![alt text](remotecontrolscreen.jpg?raw=true)

This system actually tracks whether or not data makes it to the ESP8266 that it is sent to via the last_known_device_value and last_known_device_modified columns.  This is important when a remote control action needs to be verified as having happened. Otherwise you end up looking for a change of temperature or power consumption at your off-grid cabin for such confirmation.  I use a string hash table as a very simple database to store this information on the microcontroller, which might be overkill. But the ESP8266 has enough storage and memory to be a little wasteful of resources.

In addition to supporting the changing of pin states using a server, this system now also implements a local API with two ESP8266-served endpoints (first you have to know its local address, now being saved in the device table as ip_address). These endpoints are /readLocalData and /writeLocalData. The former gives a JSON object with info about the pins and their current state.  The other accepts two querystring params:  id and on.  id is usually the pin number, unless it's a pin on a slave Arduino, in which case it is [I2C address of slave].[pin number].  On is just a 1 for on and 0 for off.  Using that second API, you can turn on circuits directly and then the ESP8266 will update the server with the new value information.  This is useful when building a local control panel, which is my next project.

index.h has the HTML for a locally-served front-end to take advantage of the local API, though my intention is to build an actual device to do this instead of relying on the massive computational overhead of a modern web browser. 
