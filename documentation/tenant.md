# All About Tenants #

A set of devices and a set of users belongs to a tenant, which specifies a specific user-access group.  Typically all devices are accessible in some way to all users, and a device set up for a tenant will be able to log data for whatever device it claims to be.  Some preferences are tenant-wide, such as default device_id (the device whose weather data is displayed by default on the weather tab) and specified_entities (a configuration helping the report history display to show what entities were referred to by each report in the history), though these can also be set differently for different users.

The APIs used to get weather information, communicate with an inverter cloud, or do geotracking are tenant-specific and can be set from the API page under the ETC tab.
