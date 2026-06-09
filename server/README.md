## Files for a Server

These files are to be placed on a server running Apache, PHP, and MySQL (or MariaSQL).  

The server does not have to run the Linux operating system unless you are planning on using utilities that depend on some shell scripts such as full_sql_backup.sh (though they could probably be made to work on non-Linux OSes).

Wherever you end up placing your server backend, the firmware will need to be flashed with this information (found in config.cpp file, though this can be overridden numerous ways with configuration places in a LittleFS file system, in a FRAM or EEPROM, or in the EEPROM on an I2C slave).

Note: none of these files require compilation or other framework-mandated hassles.  If you're anything like me, you often abandon potential code solutions for your problems the moment they start requiring Node.js or Composer. 
