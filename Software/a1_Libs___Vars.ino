// *** libraries***

#include <Wire.h>          // Libs for I2C
#include <INA.h>           // Zanshin INA Library https://github.com/Zanduino/INA
#include <ArduinoJson.h>   // Libs for Webscraping
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <ESP8266WiFi.h>   // default from Espressif
#include <ESP8266HTTPClient.h>
#include <TZ.h>            // default from Espressif
#include <FS.h>
#if defined(THINGER)
#include <ThingerESP8266.h>
//#include <ThingerConsole.h>
#endif
# if defined(INFLUX)
#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>
#endif

#include <EEPROM.h>

#ifndef DISPLAY_IS_NONE
#include "SSD1306Wire.h"  // from https://github.com/ThingPulse/esp8266-oled-ssd1306/
#endif
#ifdef DISPLAY_IS_OLED64x48
SSD1306Wire display(0x3c, SDA, SCL, GEOMETRY_64_48 ); // WEMOS OLED 64*48 shield
#endif
#ifdef DISPLAY_IS_OLED128x64
SSD1306Wire display(0x3c, SDA, SCL);                  //OLED 128*64 soldered
#endif

// ESP8266 pin definitions Lolin / D1 /Witty (see definitions for other boards in Parked code)
#define SCL 5           // D1 GPIO05 for I2C (Wire) System Clock
#define SDA 4           // D2 GPIO04 for I2C (Wire) System Data
#define RST 0           // GPIO0

#define RELAY1     D5   // GPIO14 Relay or FET control 1
#define RELAY2     D6   // GPIO16 Relay or FET control 2
#define LP_BUCK    D0   // GPIO12 Digital out control low Power buck
#define HP_BUCK    D7   // GPIO12 Digital out control high Power buck
#define PWM_BAT    D3   // GPIO0  PWM output to control scc buck
#define PWM_AUX    D4   // GPIO2  PWM output to control aux buck (lights also the built-in LED)
#define AUX_BUCK   D8   // GPIO15 Digital out control aux buck


// Battery parameters
#ifdef BATTERY_IS_12V_FLA
float phase_ratio_C[] = {0, 0.05, 0.2, 0.2, 0.1, 0.1, 0.1, 0, 0, 0, 0, 0, 0};
float phase_voltage[] = {0, 12.4, 13.8, 13.8, 14.6, 13.7, 15.2, 15.6, 11.0, 0, 0, 0, 0};
#define MIN_VOLT 10.8
#define MAX_VOLT 15.6
#endif
#ifdef BATTERY_IS_12V_AGM
float phase_ratio_C[] = {0, 0.05, 0.2, 0.2, 0.1, 0.1, 0.1, 0, 0, 0, 0, 0, 0};
float phase_voltage[] = {0, 12.4, 13.8, 13.8, 14.4, 13.7, 14.4, 15.6, 11.0, 0, 0, 0, 0};
#define MIN_VOLT 10.8
#define MAX_VOLT 15.6
#endif
#ifdef BATTERY_IS_12V_GEL
float phase_ratio_C[] = {0, 0.05, 0.2, 0.2, 0.1, 0.1, 0.1, 0, 0, 0, 0, 0, 0};
float phase_voltage[] = {0, 12.4, 13.8, 13.8, 14.4, 13.7, 15.2, 15.6, 11.0, 0, 0, 0, 0};
#define MIN_VOLT 10.8
#define MAX_VOLT 15.6
#endif
#ifdef BATTERY_IS_12V_LIFEPO
float phase_ratio_C[] = {0, 0.3, 1, 1, 0.5, 0.1, 0.3, 0, 0, 0, 0, 0, 0};
float phase_voltage[] = {0, 11.4, 13.2, 13.7, 13.7, 12.0, 12.0, 12.0, 10.5, 0, 0, 0, 0};
#define MIN_VOLT 10.36
#define MAX_VOLT 14.97
#endif
#ifdef BATTERY_IS_11V_LIPO
float phase_ratio_C[] = {0, 0.3, 1, 1, 0.5, 0.1, 0.3, 0, 0, 0, 0, 0, 0};
float phase_voltage[] = {0, 10, 12.8, 12.8, 12.8, 12.8, 12.8, 12.8, 10.5, 0, 0, 0, 0};
#define MIN_VOLT 10.05
#define MAX_VOLT 14.45
#endif
boolean expired;

#define BAT_TEMP_COMP   30 //mV per°C under 25°C
#define PANEL_TEMP_COMP 64 //mV per °C under 25°C


// Concatenate URLs
#define OPEN_WEATHER_MAP_URL  "http://api.openweathermap.org/data/2.5/weather?id=" OPEN_WEATHER_MAP_LOCATION_ID "&appid=" OPEN_WEATHER_MAP_APP_ID "&units=" OPEN_WEATHER_MAP_UNITS "&lang="  OPEN_WEATHER_MAP_LANGUAGE
//#define OPEN_WEATHER_MAP_URL   "http://api.openweathermap.org/data/2.5/weather?id=2928810&appid=208085abb5a3859d1e32341d6e1f9079&lang=de&units=metric"
#define DFLD_URL "http://api.dfld.de/noise/dfld.de/" DFLD_REGION "/" DFLD_STATION
//#define DFLD_URL "http://api.dfld.de/noise/dfld.de/004/020"

//***Variables for Time***
tm*        timeinfo;                 //localtime returns a pointer to a tm struct static int Second;
time_t     Epoch;
time_t     now;
byte Second;
long SecondOfDay;
byte GracePause;
long MillisMem;
byte Minute;
byte Hour;
byte Day;
byte Month;
unsigned int Year;
byte Weekday;
char DayName[12];
char MonthName[12];
char Time[10];
char Date[12];
byte slice;
boolean Each6S;
boolean NewMinute;
boolean MinuteExpiring;
boolean NewHour;
boolean HourExpiring;
boolean NewDay;
boolean DayExpiring;
byte    SunriseHour;
byte    SunriseMin;
byte    SunsetHour;
byte    SunsetMin;

// ***Variables for Menu***
byte    inbyte;
byte    displayPage;
byte    displaySubPage;
byte    serialPage;
byte    serialPeriodicity;
boolean serialEvent;
boolean triglEvent;

static IPAddress ip;
static IPAddress remip;

//***Payload Variables***
// INA226
byte devicesFound =      0; ///< Number of INAs found
float ina1_current;
float ina1_voltage;
float ina1_shunt;
float ina1_power;
float ina2_current;
float ina2_voltage;
float ina2_shunt;
float ina2_power;
float ina3_current;
float ina3_voltage;
float ina3_shunt;
float ina3_power;
float delta_current;
float delta_voltage;
float voltageAt0H ;
float voltageDelta ;
float currentInt = 0;
int   nCurrent;
float AhBat[31];
float Vbat[31];
float last_power;

struct dashboard {
// Measures
  float Vbat ;
  float Ibat ;
  float Wbat ;
  float Vpan ;
  float Ipan ;
  float Wpan ;
  float Vaux ;
  float Iaux ;
  float Waux ;
  
// User set-points converted to float.
  float CVbat;
  float CCbat;
  float CVpan;
  float DVinj;
  float CVaux;
  
// Set points changed from regulation
  float CVinj;                //Battery Voltage control modified
  float CCinj;                //Battery Current control modified
  float MPPTinj;                 //Voltage control modified

// 
  byte  phase ;                // charger phases
  byte  modus ;                // mode of operation
  float efficiency;            // converter efficiency
  float internal_resistance;   // battery internal resistance (as low as possible, >0,3 is bad)
  float percent_charged;       // not much precise though..
} dashboard;

// User set-points Set points from sliders
  int   CVbat;
  int   CCbat;
  int   CVpan;
  int   CVaux;

// Injection PWM output values
unsigned int bat_injection;
unsigned int aux_injection;

// MPPT
float MPPT_last_power;
float MPPT_last_voltage;
float dP; // power difference;
float dV; // voltage difference;

// Charger
// Solar charger dashboard.phases
#define NIGH         0  // panel voltage < battery voltage Low-Power mode
#define RECO         1  // battery voltage < LOWLIM, panel current > low limit, cut off load 
#define BULK         2  // battery voltage < FLOAT, current limited by battery
#define PANL         3  // battery voltage < FLOAT, current limited by panel
#define ABSO         4  // battery voltage > FLOAT < ABSORB, current limited by battery and time
#define FLOA         5  // battery voltage = FLOAT
#define EQUA         6  // battery voltage = EQUALIZE, current limited by battery and time
#define OVER         7  // battery voltage > ABSORB and not EQUALIZATION
#define DISC         8  // battery voltage < LOWLIM, panel current < low limit , cut off load
#define PAUS         9  // no charge, wait for a defined time
#define NOBA         10 // no battery current possible at Vbat = ABSORB
#define NOPA         11 // no panel current for more than 20h
#define EXAM         12 // evaluate battery condition

String phase_description[] = {"NIGH","RECO","BULK","PANL","ABSO","FLOA","EQUA","OVER","DISC","PAUS","NOBA","NOPA","EXAM"}; // for dashboard.phase
unsigned int phase_timer;
unsigned int phase_duration[13];

// phase_limit is the maximal duration (in minutes) for each dashboard.phase, 0= no limit
// dashboard.phase_ratio_C is the ratio charging current to BATT_CAPACITY for each dashboard.phase
// dashboard.phase_voltage is the battery voltage for each dashboard.phase
int phase_limit[13] = {1000, 300, 900, 900, 480, 900, 240, 0, 0, 30, 0, 0, 10};

// Solar charger Modus modes
#define CVFX         0  // fix voltage
#define CVTR         1  // voltage ahead tracking 
#define CCFX         2  // fix current
#define PVFX         3  // fix panel voltage
#define MPPT         4  // maximum power point tracking
#define AUTO         5  // automatic

String modus_description[] = {"CVFX","CVTR","CCFX","PVFX","MPPT","AUTO"}; // for dashboard.Modus

// Digital outputs
boolean relay1_value;
boolean relay2_value;
boolean high_power_enable;
boolean aux_enable;

char dashboardPayload[sizeof(dashboard)];  //  Array of characters as image of the structure for UDP xmit/rcv

//Weather
float outdoor_temperature;
float outdoor_humidity;
float outdoor_pressure;
float wind_speed;
int   wind_direction;
int   cloudiness;
String weather_summary;

//UDP Trasnmission
String  JSONpayload;
byte    wifiConnectCounter;

//*** Buffers ***
static char charbuff[80];    //Char buffer for many functions

int A0Raw;
boolean trigEvent;
