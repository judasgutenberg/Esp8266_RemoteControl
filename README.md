# Esp8266 Remote, Automation, and Environmental Logging
## Overview
If you need complete control of all the code of a robust multi-user/multi
-tenant remote-control, automation and weather data logging system without infuriating compilation and dependency headaches (in Javascript or PHP), you might be interested in this system. I developed it to more efficiently use solar energy at my off-grid Adirondack cabin while also keeping the internet accessible and the pipes from freezing.  This started out as a simple multi-probe weather monitoring system (<a href=https://github.com/judasgutenberg/ESP8266-Micro-Weather target=esp>ESP8266 MicroWeather</a>) to which I first added reliability automation (<a href=https://github.com/judasgutenberg/Hotspot-Watchdog  target=hot>Hotspot-Watchdog</a>), then remote control, then solar inverter monitoring, then cabin automation, and finally a multi-tenant security model (so others could use the same server installation as a backend).  

For monitoring multiple weather sensors (placed, say, in various places inside and outside a building), there's a multi-plot graph.
![alt text](weathergraph.jpg?raw=true)

There's also a page to show data from your solar inverter if you happen to be using the one I know about (SolArk):
![alt text](invertergraph.jpg?raw=true)

But the main feature in this system is that it allows you to remotely control devices across the internet and also supports automation based on the values of sensors known to the central database.  It does this using a server running PHP/MySQL.  There is no server-side or Javascript compilation and relatively few dependencies:  PHP, MySQL, APCu, the Chart.js graphing library, and the Arduino sensor libraries, so this is not much of a headache to get working on just about any server where you have a reasonable amount of control.

A web-based tool (tool.php) allows you to edit the value column of a table called device_feature, where device_id is equal to device_id specified in config.c in the Arduino-compiled code (remote.ino, config.h, config.c, and index.h) uploaded to your ESP8266 device.  Once installed on the ESP8266, it regularly polls the server (make sure to set host_get in config.c before compiling) to look for information in those device_feature records, and it automatically copies data from the value column to the pins they're connected to (which is specified in device_type_feature).  These pins can then be used to turn on relays, which allow you to control potentially large loads in remote locations. I'm using this system to remotely turn on the boiler, hot water heater, a 240 volt EV outlet, and three different 120 volt outlets connected to electric space heaters in my off-grid cabin.  Obviously, this involves working with dangerous voltage, so if you don't know anything about electrical wiring and basic safety with such things, you should instead use this to control boring low-voltage gizmos like battery-powered radios, toy motors, or LEDs.

Device_features can also be sensors reachable by a GPIO pin or I2C, and if multiple sensors are attached to an ESP8266, the additional ones are all identified by device_feature_id.

One caveat: this system is one where the server tells the microcontroller what pins do what and can change assignments without a need to reflash or possibly even restart the microcontroller. For some pins (notably GPIO10 and GPIO9 on an ESP8266, which are used for accessing on-board flash storage), setting them to outputs and forcing them to take a value will cause the ESP8266 to crash and restart.  So be sure to test all your pin control arrangements while local to the ESP8266 before using it remotely.

To expand the number of pins usable for remote control, you can add one or more slave Arduinos running my slave software (be sure to give them all different I2C addresses, which is a small code tweak to the Arduino firmware):
https://github.com/judasgutenberg/Generic_Arduino_I2C_Slave and just add the I2C address of the slave Arduino to the device_type_feature record.

## Design Philosophy
This repository showcases my design philosphy, which is highly practical and uninterested in trends and styles unless they have concrete benefits that clearly outweigh their costs.  I wanted to minimize dependency on other projects to avoid the introduction of such distractions as compilation, dependency incompatibility, bewilderingly-enormous storage requirements, and the evolutionary drift of APIs.  Obviously, one can go too far with this attitude, thus the goal in this repo was to strike a balance between struggling to orchestrate numerous modules created by others and reinventing all the components myself (starting with the wheel!).  So this repo uses PHP, MySQL, Javascript (the particular versions don't much matter), Chart.js, the Arduino IDE, several non-hardware Arduino libraries, and the Arduino libraries for various common sensors. It does not, however, use any frontend or backend frameworks, as all of those introduce thick layers of dependencies, which can take days for someone new to the codebase to coax into life (I'm a professional software developer, and there are many hours I've lost to this). 

Given all that, in this repository, all of the login, database access, and web rendering code (with the exception of the Chart.js code) is self-contained within this repo.  I've used pieces of it elsewhere (for example, in my <a href=https://github.com/judasgutenberg/networked_spellingbee target=nsb>Networked Spelling Bee repository</a>), so technically it <em>is</em> a framework, though one I've felt no need to release as a separate project.  This means there is no compilation of Javascript or PHP. If you need to make changes, you just change the source files, since they are also the object files.  I've found this is my preferred way to work on client/server code, as compilation creates a distraction that negatively affects creativity and productivity.  Obviously, the code on the ESP8266s must be compiled in the Arduino environment, but I've become comfortable with that workflow, especially since other distractions aren't present (such as copying files to a server or clearing caches). 

This code uses a mix of server-side and client-side rendering of display elements, depending on which method is appropriate. Some components (such as the graphs output for weather and inverter data) use modern single-page techniques in vanilla Javascript, though in many places it makes more sense to render HTML in the backend and to provide conventional navigation between the pages.  Often the HTML in such pages is later modified by Javascript. When you aren't working in someone else's framework, you can make things behave in precisely nuanced ways.

Another important philosophy is that I want to support older hardware with my code, especially in the web client.  Not only do I have a fondness for old Espressif microcontrollers, but I love old Chromebooks, the kind that kids might've been using in school well before the pandemic. I have a half dozen of them in various places, and they typically cost $40.  They have all the advantages of a tablet, yet they also have a physical keyboard. And they're disposable enough for me to use in the tub (where I've never yet lost one). But I hate it when a website (fuck you, Reddit!) tells me that my Chromebook is no longer supported, especially given how easy it is to provide support.  (In my code, the only change I've had to make was to stop using the Javascript replaceChildren() method.)  If you're insisting on using all the latest Javascript methods and constantly throwing up alerts telling your users to "upgrade," what you're really saying is that landfills don't contain quite enough eWaste. Solark's dashboard was one of the offenders, and it's great to have completely replaced that with the energy tab in this application.

Finally, I prefer my code to be as straightforward as possible. I don't, for example, appreciate clever one-line solutions to programming challenges and prefer to use explicit loops where it is obvious to as many viewers as possible what precisely is going on.  Similarly, I prefer an imperative over a declarative coding style, and I've never been enamored with an object-oriented approach.  This is not to say this code contains none of those things, but I keep it to a minimum.

Obviously, if one is insisting on making all their own database access and login methods, there is a huge possibility of security vulnerabilities creeping into the design. I'm well-versed in most of these, though it's possible I've overlooked something.  My reporting system gives immense powers to super users, for example, but that's something I personally demand. And it's easy enough not to give access to report writing (or even execution) to your users under this system.

![alt text](esp8266-remote-schematic.jpg?raw=true)
## Hardware Setup
This system expects an ESP-8266-based device programmed in the Ardunio environment to be able to control the systems that need to be turned on or off.  I've had success connecting a NodeMCU to a ULN2003 and having that switch 12 volt power to 12 volt relays capable of switching enormous loads.   See the schematic for how I connected seven relays using both a NodeMCU and an Arduino Mini running at 3.3 volts that I flashed with <a href=https://github.com/judasgutenberg/Generic_Arduino_I2C_Slave target=arduino>my Arduino Slave firmware</a>.  To see other examples of the ULN2003 used, see (for example) https://microcontrollerslab.com/relay-driver-circuit-using-uln2003/.

## Arduino Setup
Some of the files (remote.ino, config.h, config.c, and index.h) are designed to be compiled in the Arduino environment and uploaded to an ESP8266. (I used a NodeMCU, which is cheap and has conveniently-gendered pins making it easy to work with.)  The Arduino code is designed to be able to handle a diverse collection of common sensors with the ability to dynamically change sensor types without requiring a recompilation or even a restart.  This requires, of course, that the libraries for these devices are all installed at compilation time.  As the code exists now, those libraries are as follows:
<ul>
<li>Adafruit DHT, a temperature/humidity sensor: (https://www.arduino.cc/reference/en/libraries/dht-sensor-library/)
<li>Adafruit AHT, a temperature/humidity sensor: (https://github.com/adafruit/Adafruit_AHTX0)
<li>Adafruit BMP085, a temperature/air-pressure sensor: (https://github.com/adafruit/Adafruit-BMP085-Library)
<li>BME680, a temperature/air-pressure/humidity sensor: (https://github.com/Zanduino/BME680/blob/master/src/Zanshin_BME680.h)
<li>BMP180, a temperature/air-pressure sensor: (https://github.com/LowPowerLab/SFE_BMP180)
<li>LM75, a temperature sensor: (https://github.com/jeremycole/Temperature_LM75_Derived)
<li>Adafruit BMP280, a temperature/air-pressure sensor: (https://github.com/adafruit/Adafruit_BMP280_Library/blob/master/Adafruit_BMP280.h)
</ul>
This system also requires a number of external libraries:
<ul>
<li>https://github.com/bblanchon/ArduinoJson
<li>https://github.com/arduino-libraries/NTPClient
<li>https://github.com/esp8266/Arduino/blob/master/libraries/ESP8266WiFi/src/WiFiUdp.h
<li>https://github.com/spacehuhn/SimpleMap
</ul> 


## Server Setup
The rest of the code needs to be placed on a server that can process PHP pages and communicate with a MySQL database. (It's not an elegant language, but I like PHP because it runs pretty well and doesn't require compilation or setup headaches on most servers or Raspberry Pis.) 

To get this installed, first create an initial database by running remote_control.sql on the server and then change config.php so the PHP will know how to connect to the database.  You then create a tenant and a user connected to that tenant via tenant_user and give that tenant a storage_password so that the ESP8266 will be able to authenticate communication with the backend (all this can be done under the tenant tab in the web UI if your user has 'super' status).  This storage password is then set as storage_password (along with other important config information like your WiFi credentials and the domain and path where your backend is hosted) in config.c before compiling the Arduino code and uploading it to an ESP8266.  Then you create a device and give it device_features, and set the device_id of the ESP8266 to the device_id of that device in config.c so that the backend will know which ESP8266 is polling to log sensor data and perhaps pick up remote control settings and additional sensors.

Now you need to create a new account with the web front end. The first user in the database is automatically given the role of super, which can edit the user table, create reports, and administer other tenants in the database, among other privileges.  That user also automatically gets a tenant, connected to the user via the tenant_user table.  Superusers can manage the connection of tenants to users via the user or tenant editors.  Users who need tenants can either ask the superuser to connect them to their tenant or new users can be invited to join their tenant by using a specially-crafted link (which can be produced by one of the utilities in the Utilities tab).  

This is another advancement from my Moxee Hotspot Watchdog system, which was itself an advancement from my ESP8266-Micro-Weather system.  This system still logs weather data using a variety of common weather sensors and can, if configured correctly, reboot a Moxee cellular hotspot when it craps out and needs a reboot (which is essential for reliably connecting to my off-grid cabin, which is a pre-requisite for all the features I am about to describe).

From there, here's an overview of how to set up control for a particular system controlled by a pin on your ESP8266:

1. Add a record to the device table to represent your particular ESP8266.  In the code, device_id and location_id refer to the same entity. Your ESP8266 will send that id as device_id to the server whenever it polls it to get updates on what values the device with its pins should have.  You will need to set device_id to this value in config.c before compiling the code for you ESP8266.
2. Add a record to the device_type table describing your particular type of device (in this case, ESP8266).  Details for these don't matter much.
3. Add a record to the device_type_feature table describing the pin (mostly the pin number and, if it is on an Arduino slave, the I2C address).
4. Add a record to the device_feature table describing the specific pin (here a human readable name would be useful).
5. You can change the state of a particular ESP8266's pin by changing the state of the value column in the device_feature record.  Make sure to set enabled to 1 as well.  Depending on the speed of your network and how frequently you set polling, the change should manifest within a minute or so.

Here is the user interface, which allows you to turn items on and off in the list by checking the "power on" column. (You do it all from the device_feature list view.)

## Data Logging
The ESP8266 transmits delimited data in a querystring to the backend at a regular polling interval set in config.c. This data includes temperature, air pressure, humidity, and many other possible weather sensor values, as well as the states of devices being controlled remotely.  It can also transmit positional data from a mobile ESP8266. All of this data is processed by data.php and logged in the device_log table.  If a FRAM or flash I2C memory is attached to the ESP8266 and properly configured in config.c, then logging will be stored locally when the internet cannot be reached and then gradually sent to the backend once the internet reappears.  Depending on the amount of data being stored locally per record, a $6 32K FRAM can hold about 1000 records.  An ESP8266 has no built-in real time clock, so proper timestamping requires access to the internet (at least initially) unless a real time clock is added via I2C.  (I'm still building the code to actually update the server after a period of local logging.)

![alt text](esp8266-remote.jpg?raw=true)
## Remote Control
The model of remote control this system uses is server-as-truth. Every time a device polls the server, it gets the server's idea of what the state of its features should be.  The device then dutifully changes the device features as necessary.  The only exceptions to this behavior are cases where either actions performed on a device's locally-served web page or a Local Remote make changes to a device_feature on the device directly, and this sets a flag telling the device (and the server) that data from the device should flow in the opposite direction, from device to server. In that case server-as-truth is overriden and the server updates its state accordingly using backwards-flowing data.

This system tracks whether or not data makes it to the device that it is sent to via the last_known_device_value and last_known_device_modified columns.  This is important when a remote control action needs to be verified as having happened. Otherwise you end up looking for a change of temperature or power consumption at your off-grid cabin for such confirmation, and that can take a fair amount of time.  I use a string hash table as a very simple database to store this information on the microcontroller, which might be overkill. But the ESP8266 has enough storage and memory to be a little wasteful of resources. Also, every change of state for a device_feature is logged in the device_feature_log table.

![alt text](reallifecircuit.jpg?raw=true)

In addition to supporting the changing of pin states using a server, this system also implements a local API with two ESP8266-served endpoints (first you have to know its local address, which is being saved in the device table as ip_address). These endpoints are /readLocalData and /writeLocalData. The former gives a JSON object with info about the pins and their current state.  The other accepts three querystring params:  ip_address, id, and on.  ip_address helps with logging what initiated a change. id is usually the pin number, unless it's a pin on a slave Arduino, in which case it is [I2C address of slave].[pin number].  On is just a 1 for on and 0 for off.  Using that second API, you can turn on circuits directly and then the ESP8266 will update the server with the new value information.  This is useful when building a local remote control panel, which was my next development effort.

(see https://github.com/judasgutenberg/Local_Remote)

index.h has the HTML for a locally-served front-end to take advantage of the local API, though the Local Remote is better for this than relying on the massive computational overhead of a modern web browser. It would also be easy to write a phone app, which would be great for someone who always keeps a phone nearby. But I am not such a person.  

## Graphical Data
Data from the weather sensors can be viewed at /index.php in your server's web installation.  Data plots from different locations can be selected in a dropdown and the graphs can show different time ranges and you can page back into their history.  The same is also true with solar inverter data (if you have any to log) at /energy.php. I will probably be adding support for paging data in this way to graph-generating reports as well.  On the weather display, it is possible to see all the data at one location in a set of plots on a single grid or to see multiple plots from different locations of a particular weather parameter, such as temperature.  There is also a button that will cause data from precisely a year before the timeframe of the graph being displayed to also be displayed in a fainter color on the same graph, thereby giving the viewer a sense of how this period compares to the period a year before.  This is particularly useful for seeing if some insulated place with a large thermal mass (such as a basement) is ahead of or behind the changes of temperatures from the year before.  As parameters are changed on the pages showing weather and graphical data, the url is changed accordingly, providing urls that can be saved for a return to a future version of the very same display.

![alt text](weathergraph.jpg?raw=true)

## Automation and Conditions
There is also an inverter-related endpoint in data.php to return live inverter information to the local remote (for now, this only works with SolArk inverters, as that is the kind I have, though they are notoriously hard to get data from).  This inverter data is also available to a conditions-processing system that automatically turns device_features on or off depending on inverter sensor values.  Such conditions are entered in the table management_rule in the conditions column.   Conditions include tokens that take the form <tablename[location_id].columnName>.  An example token would be <inverter_log[].battery_percentage>.  A condition made with that token would be something like


<inverter_log[].battery_percentage> > 80

which would set the connected device_feature's value to the value of management_rule.result if the condition is met and allow_automatic_management in the device_feature record is true.  Multiple management_rules can be added to device_features in the device_feature editor, which looks like this:

![alt text](devicefeature.jpg?raw=true)

Management_rules can be edited in the management_rule editor, which looks like this:


![alt text](managementrule.jpg?raw=true)

At the bottom is a tool you can use to automatically construct a value token to place in conditions.  Treat these as variables in an expression to be evaluated as true or false.  You can use multiple tokens, parentheses, arithmatic operators, and scalar numbers in such expressions.
Since manual changes to the status of device_features are usually at odds with automation, whenever a manual change to a device_feature is made, automation is automatically suspended for restore_automation_after hours, starting at the instant of the manual change.

## Reports
The great thing about storing the data from your IoT devices in your own database is that you then have lots of ways to examine this data. If you're handy with SQL, then you can find data in any form you want it matching any criteria you can come up with.  With is in mind, I built an elaborate reporting system that allows me to save parameterized SQL queries and run them with forms to make entering the parameters easy.  Not only that, but every report you run is remembered in the report_history table, allowing you to re-run the report with the same parameters (or tweak the parameters as needed).  Reports are reached via the Reports tab, where the ones you've created (or that are created with your tenancy)  are listed. From there you can run or edit them. The core of every report is a SQL statement that is run to produce a table of data, a graph, or, in some cases, locations on a map.  They are defined as a form (using JSON and using the form description system used throughout the admin website), possibly an output configuration, and SQL. Simple reports can just be SQL, though if you need to send parameters to a report, you will need to define a form.  Perhaps forms definitions are best shown by example.

This JSON defines a form with one parameter

<code>
  [ {
	"label": "Which changer",
	"name" :"changerX",
	"type" : "select",
	"values": [
		"changer1", "changer2", "changer3", "changer4", "changer5", "changer6","changer7"
	]
	}]
</code>
the value of which is substituted into the SQL component of the report:
<code>
SELECT  &lt;changerX/&gt;, battery_percentage, solar_power, load_power, battery_power FROM inverter_log  ORDER BY <changerX/> DESC
 
</code>

the "values" parameter in the form JSON can also be a SQL string to generate a list of options from the database.  If so, the SQL needs to return a 'text' column for a proper dropdown list of options to be displayed. Obviously, anyone allowed to write such reports could wreak a lot of havoc with malicious SQL. A log of each report that is run is kept, and each log item is accessible via the web UI with enough information to allow it to be re-run with the same set of parameters as the original run.

Here is the JSON for a report that can generate two different graphs ("good graph" and "better graph" will appear in a the output format dropdown as options, in addition to options for HTML and CSV):

<code>
{
  "output": [{
    "name": "good graph",
    "labelColumn": "recorded",
    "colors": "ff0000,00ff00,0000ff",
    "color": "ff0000,00ff00,0000ff",
    "plots": [
      {
	"column": "battery_percentage",
	"darkenBy": "changer7",
	"darkenByDivisor": 100,
	"color": "green",
	"label": "battery_percentage",
	"shape": {
		"radius": 1
        }
      },
      {
	"column": "battery_voltage",
	"darkenBy": "changer7",
	"darkenByDivisor": 100,
	"color": "red",
	"label": "battery_voltage",
	"shape": {
		"radius": 1
        }
      }
    ]
  },
	{
    "name": "better graph",
    "labelColumn": "recorded",
    "colors": "ff0000,00ff00,0000ff",
    "color": "ff0000,00ff00,0000ff",
    "plots": [
      {
	"column": "battery_percentage",
	"darkenBy": "changer7",
	"darkenByDivisor": 100,
	"color": "blue",
	"label": "battery_percentage",
	"shape": {
		"radius": 12
        }
      },
      {
	"column": "battery_voltage",
	"darkenBy": "changer7",
	"darkenByDivisor": 100,
	"color": "orange",
	"label": "battery_voltage",
	"shape": {
		"radius": 15
        }
      }
    ]
  }
	]
}
</code>

Note that for a parameter form to be produced for a report, there must be "form" node in the JSON in addition to the "output" node.

Obviously there is a lot of power in such a system, since, depending on MySQL user permissions, a report-writer might be given access to any data on the database server;  only fully trusted users should get access to report creation and some reports are too powerful for anyone but users with the role 'super' to run.  Currently only 'super' users can create and edit reports, though, depending on the role given to a report, less-powerful users may be able to run it.  It's also possible to write reports that give enormous power to users, and such reports should be restricted (via role) to 'super.'

## Security

ESP8266s have limited memory, so I don't bother with making their polling happen via https.  Some data is deeply personal, such as your email, bank records, or the particular web sites you visit.  I do not consider weather data personal, so I do not chew off my fingernails fretting about sending the values of sensors in plaintext over the open internet.  I do, however, worry about pranksters getting the information about my backend and then polluting my database with bogus data or sending state orders to my remote control relays to turn things on or off contrary to my wishes.  So each ESP8266 sends an authentication secret with every poll of the server, and this has to be correct for the server to log data or perform remote control tasks.  I did not feel comfortable sending the secret in plaintext, so I encrypt it using a simple algorithm (much simpler and using much fewer resources than https) that takes into account information about the plaintext data.  That way the secret is <em>also</em> encrypted. Since a timestamp is a factor in the encryption, the URLs will expire after ten seconds or so.  Crypotographers may augh at my scheme, but it protects my data and my remote control against casual prankster attacks.  (If a government wants to get its LOLs filling my database with billion-degree temperatures, they could probably do this even if I was using https.)

The actual website with all the tools to view data and control ESP8266s should be hosted on https, since otherwise password and cookie data is sent to the server in plaintext, allowing someone with access to that data (very few people) to view or alter it (unlikely).  Personally, I am not particularly concerned about such issues.  But if you don't understand the risks, you should probably host this site in https (while also allowing the ESP8266 to communicate over http).


