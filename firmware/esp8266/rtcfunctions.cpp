#include "rtcfunctions.h"
 


//RTC functions
const int _ytab[2][12] = {
{31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},
{31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}
};

#define DAYSPERWEEK (7)
#define DAYSPERNORMYEAR (365U)
#define DAYSPERLEAPYEAR (366U)
#define SECSPERDAY (86400UL) /* == ( 24 * 60 * 60) */
#define SECSPERHOUR (3600UL) /* == ( 60 * 60) */
#define SECSPERMIN (60UL) /* == ( 60) */
#define LEAPYEAR(year)          (!((year) % 4) && (((year) % 100) || !((year) % 400)))


byte decToBcd(byte val){
  return ( (val/10*16) + (val%10) );
}

// Convert binary coded decimal to normal decimal numbers
byte bcdToDec(byte val){
  return ( (val/16*10) + (val%16) );
}


void syncRTCWithNTP() {
  timeClient.update();
  time_t rawTime = timeClient.getEpochTime();
  struct tm *timeInfo = gmtime(&rawTime);  // Convert to UTC struct
  byte second      = timeInfo->tm_sec;
  byte minute      = timeInfo->tm_min;
  byte hour        = timeInfo->tm_hour;
  byte dayOfWeek   = timeInfo->tm_wday == 0 ? 7 : timeInfo->tm_wday; // Convert Sunday (0) to 7
  byte dayOfMonth  = timeInfo->tm_mday;
  byte month       = timeInfo->tm_mon + 1; // tm_mon is 0-based
  byte year        = timeInfo->tm_year - 100; // Convert from years since 1900
  char buffer[120];
  sprintf(buffer, "Setting RTC: %02d:%02d:%02d %02d/%02d/%02d\n", hour, minute, second,  month,  dayOfMonth, year + 2000);
  textOut(String(buffer));
  setDateDs1307(second, minute, hour, dayOfWeek, dayOfMonth, month, year);
}

/****************************************************
* Class:Function    : getSecsSinceEpoch
* Input     : uint16_t epoch date (ie, 1970)
* Input     : uint8 ptr to returned month
* Input     : uint8 ptr to returned day
* Input     : uint8 ptr to returned years since Epoch
* Input     : uint8 ptr to returned hour
* Input     : uint8 ptr to returned minute
* Input     : uint8 ptr to returned seconds
* Output        : uint32_t Seconds between Epoch year and timestamp
* Behavior      :
*
* Converts MM/DD/YY HH:MM:SS to actual seconds since epoch.
* Epoch year is assumed at Jan 1, 00:00:01am.
* looks like epoch has to be a date like 1900 or 2000 with two zeros at the end.  1984 will not work!
****************************************************/
uint32_t getSecsSinceEpoch(uint16_t epoch, uint8_t month, uint8_t day, uint8_t years, uint8_t hour, uint8_t minute, uint8_t second){
  uint32_t secs = 0;
  int countleap = 0;
  int i;
  int dayspermonth;
  secs = years  * (SECSPERDAY * 365);
  if(epoch == 1970) {
    secs = secs + 946684800;
  }
  for (i = 0; i < (years - 1); i++){   
      if (LEAPYEAR((epoch + i)))
        countleap++;
  }
  secs += (countleap * SECSPERDAY);
  secs += second;
  secs += (hour * SECSPERHOUR);
  secs += (minute * SECSPERMIN);
  secs += ((day - 1) * SECSPERDAY);
  if (month > 1){
      dayspermonth = 0;
      if (LEAPYEAR((epoch + years))){ // Only counts when we're on leap day or past it
          if (month > 2){
              dayspermonth = 1;
          } else if (month == 2 && day >= 29) {
              dayspermonth = 1;
          }
      }
      for (i = 0; i < month - 1; i++){   
          secs += (_ytab[dayspermonth][i] * SECSPERDAY);
      }
  }
  return secs;
}

// 1) Sets the date and time on the ds1307
// 2) Starts the clock
// 3) Sets hour mode to 24 hour clock
// Assumes you're passing in valid numbers
void setDateDs1307(byte second,        // 0-59
                   byte minute,        // 0-59
                   byte hour,          // 1-23
                   byte dayOfWeek,     // 1-7
                   byte dayOfMonth,    // 1-28/29/30/31
                   byte month,         // 1-12
                   byte year)          // 0-99
{
  /*
  Serial.print(year);
  Serial.print("-");
  Serial.print(month);
  Serial.print("-");
  Serial.print(dayOfMonth);
  Serial.print(" ");
  
  Serial.print(hour);
  Serial.print(":");
  Serial.print(minute);
  Serial.print(":");
  Serial.print(second);
  Serial.print("/");
  Serial.print(dayOfWeek);
  Serial.println(" ");
   */
   Wire.beginTransmission(ci[RTC_ADDRESS]);
   Wire.write(0);
   Wire.write(decToBcd(second));    // 0 to bit 7 starts the clock
   Wire.write(decToBcd(minute));
   Wire.write(decToBcd(hour));      // If you want 12 hour am/pm you need to set
                                   // bit 6 (also need to change readDateDs1307)
   Wire.write(decToBcd(dayOfWeek));
   Wire.write(decToBcd(dayOfMonth));
   Wire.write(decToBcd(month));
   Wire.write(decToBcd(year));
   Wire.endTransmission();
}

// Gets the date and time from the ds1307
void getDateDs1307(byte *second,
          byte *minute,
          byte *hour,
          byte *dayOfWeek,
          byte *dayOfMonth,
          byte *month,
          byte *year){
  // Reset the register pointer
  Wire.beginTransmission(ci[RTC_ADDRESS]);
  Wire.write(0);
  Wire.endTransmission();
  Wire.requestFrom(ci[RTC_ADDRESS], 7);
  // A few of these need masks because certain bits are control bits
  *second     = bcdToDec(Wire.read() & 0x7f);
  *minute     = bcdToDec(Wire.read());
  *hour       = bcdToDec(Wire.read() & 0x3f);  // Need to change this if 12 hour am/pm
  *dayOfWeek  = bcdToDec(Wire.read());
  *dayOfMonth = bcdToDec(Wire.read());
  *month      = bcdToDec(Wire.read());
  *year       = bcdToDec(Wire.read());
}

void printRTCDate(){
  byte second, minute, hour, dayOfWeek, dayOfMonth, month, year;
  getDateDs1307(&second, &minute, &hour, &dayOfWeek, &dayOfMonth, &month, &year);
  textOut(String(year));
  textOut("-");
  textOut(String(month));
  textOut("-");
  textOut(String(dayOfMonth));
  textOut(" ");
  textOut(String(hour));
  textOut(":");
  textOut(String(minute));
  textOut(":");
  textOut(String(second));
  textOut("\n");
}

uint32_t currentRTCTimestamp(){
  byte second, minute, hour, dayOfWeek, dayOfMonth, month, year;
  getDateDs1307(&second, &minute, &hour, &dayOfWeek, &dayOfMonth, &month, &year);
  //Serial.println(dayOfMonth);
  //Serial.println(month);
  //Serial.println(year);
  return getSecsSinceEpoch((uint16_t)1970, (uint8_t)month,  (uint8_t)dayOfMonth,   (uint8_t)year,  (uint8_t)hour,  (uint8_t)minute,  (uint8_t)second);
}

