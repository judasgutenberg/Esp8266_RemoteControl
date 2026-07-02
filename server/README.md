## Files for a Server

These files are to be placed on a server running some sort of web daemon, PHP, and MySQL (or MariaSQL).  I like that tech stack because it is an easy setup, the performance is fine, and the stack is mature enough that updates don't generally break things.

The server does not have to run the Linux operating system unless you are planning on using utilities that depend on some shell scripts such as full_sql_backup.sh (though they could probably be made to work on non-Linux OSes).

Wherever you end up placing your server backend, the firmware will need to be flashed with the correct server information (found in config.cpp file, though this can be overridden numerous ways with configuration places in a LittleFS file system, in a FRAM or EEPROM, or in the EEPROM on an I2C slave).

Note: none of these files require compilation or other framework-mandated hassles.  If you're anything like me, you often abandon potential code solutions that seem useful the moment they start requiring Node.js or Composer.  Why should a simple app require a gigabyte of Javascript libraries? It's madness!
