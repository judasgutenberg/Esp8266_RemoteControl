# Feature Notes

### 29 June 2026
Added API support for OpenMeteo, the free weather forecast service.  Also altered management rule token parsing to be able to accept MySQL aggregator functions with respect to timespans (right now supporting "yesterday" and "today").  This allows you to set up 
management rules that look at the weather forecast, allowing you to consume more electricity earlier on days that are forecast to be sunny, for example.
