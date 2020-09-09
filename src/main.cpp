/*
 * Basic framework test
 *
 */
// Comment this out to strip all debugging messages
//#define SERIAL_DEBUG true
#include "serial-debug.h"
#include <Arduino.h>

// #include <MQTT.h>
#if defined(ESP32)
#include <WiFi.h>
#include <WiFiMulti.h>
class WiFiMulti WiFiMulti;
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#endif
#include <SPI.h>
#include <PubSubClient.h>
#include <Base64.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Wire.h>
#include <BaroSensor.h>

/*****************************************************
 *
 * Customisation section
 *
 *****************************************************/

#include "private-data.h"
#include "SensorMQTT.hpp"
#include "gitversion.h"
#define SKETCHVERS ("v." __DATE__)

#ifdef SERIAL_DEBUG
// Get DHT debugging messages, except this didn't work
// three years ago. Edit DHT.h directly mebbe? Ah, maybe
// a prior include includes DHT.h and the guard #ifdef guards!!!
#define DHT_DEBUG
#endif
#include <DHT.h>

// Pins to avoid! ESP8266
/****************************************************
 * GPIO  0 -> D3  used for program mode
 * GPIO  1 -> D10 UART0 TxD
 * GPIO  2 -> D4  Pulled up, for boot/reset
 * GPIO  3 -> D9  UART0 RxD
 * GPIO 15 -> D8  Pulled low, for boot/reset
 * GPIO 16 -> D0 Internal LED, but also deep-sleep to RST
 */
// OK pins! ESP8266
/*********************************************
static const uint8_t SDA = 4; GPIO4 = D2
static const uint8_t SCL = 5; GPIO5 = D1
*/

// Pin map
/*

static const uint8_t SS    = 15;
static const uint8_t MOSI  = 13;
static const uint8_t MISO  = 12;
static const uint8_t SCK   = 14;


static const uint8_t BUILTIN_LED = 16;
static const uint8_t A0 = 17;
 */
/*
static const uint8_t D0   = 16;
static const uint8_t D1   =  5;
static const uint8_t D2   =  4;
static const uint8_t D3   =  0;
static const uint8_t D4   =  2;
static const uint8_t D5   = 14;
static const uint8_t D6   = 12;
static const uint8_t D7   = 13;
static const uint8_t D8   = 15;
static const uint8_t D9   =  3;
static const uint8_t D10  =  1;
*/
/**
 * Add my own pin ID for SD1 (GPIO 10) (GPIO 9 also?)
 */
static const uint8_t SD2 = 9;
static const uint8_t SD3 = 10;
#define ONE_WIRE_BUS SD3 // DS18B20 pin (SD3 on NodeMCU Devkit = GPIO 10)
//#define ONE_WIRE_BUS D4 // DS18B20 pin (D4 on NodeMCU Devkit = GPIO 2)
#define MAX_ONEWIRE_DEVICES 5

/**
 * Sensor type definitions. DHT11 = 11, DHT21 = 21, DHT22 = 22 as per dht.h
 */
#include "sensor_types.h"

#if defined(ESP8266)
ADC_MODE(ADC_TOUT);
#endif
//#define DHT11 11
//#define DHT21 21
//#define DHT22 22

// pringlei hostap at 192.168.42.1
IPAddress mqttserver(192, 168, MQTT_OCTET3, 1);

/*****************************************************
 *
 * Utility macros
 *
 *****************************************************/
#define ONEMINUTE 60
#define FAILURE 0
#define SUCCESS (!FAILURE)
#define BUFFER_SIZE 100
#define OFF 0
#define ON 1

// "Valve" control
#define MAX_VALVES 10
#define MAX_SENSORS 10
#define MAX_PINSTRINGLENGTH 6
// #define VALVE1A D6
// D6 = GPIO 12
// #define VALVE1B D5
// D5 = GPIO 14
// ms to pulse the valve
#define VALVEPULSEWIDTH 40

DHT *dhtp;

/*****************************************************
 *
 * Initialisation
 *
 *****************************************************/
// 1-Wire setup
//OneWire oneWire(ONE_WIRE_BUS); now connect to bus when we know which pin
OneWire oneWire;

//DallasTemperature DS18B20(&oneWire);

// Make ADC read VCC instead of TOUT pin
//ADC_MODE(ADC_VCC);

// defined in Esp.h:
//enum ADCMode {
//    ADC_TOUT = 33,
//    ADC_TOUT_3V3 = 33,
//    ADC_VCC = 255,
//    ADC_VDD = 255
//};
//
//#define ADC_MODE(mode) extern "C" int __get_adc_mode() { return (int) (mode); }
#if defined(ESP8266)
String cid(ESP.getChipId());
#elif defined(ESP32)
char sChipID[13];
static char *getChipId(char *);
String cid(getChipId(sChipID));

/*****************************************************
 *
 * getchipid() for ESP32
 *
 *****************************************************/
static char *getChipId(char *sChipID)
{
  uint64_t chipid;
  chipid = ESP.getEfuseMac();                                               //The chip ID is essentially its MAC address(length: 6 bytes).
  sprintf(sChipID, "%04X%08X", (uint16_t)(chipid >> 32), (uint32_t)chipid); //print High 2 then Low 4 bytes.
  return sChipID;
}
#endif

// devid should be provided by mqtt quasi-bind client
// default value is set here
String devid("DEV" + cid);

// WiFi and MQTT client objects
WiFiClient wclient;
PubSubClient mqttclient(wclient);

// Default sleep of one minute
long sleepdur = ONEMINUTE;

// "Valve 1" output default to "off"
// int valve1 = OFF;
char valvepins[MAX_VALVES][MAX_PINSTRINGLENGTH];
int valvestate[MAX_VALVES];
int valvepin1[MAX_VALVES];
int valvepin2[MAX_VALVES];
int valvecount = 0;
int sensorcount = 0;
char sensorpins[MAX_SENSORS][MAX_PINSTRINGLENGTH];
uint8_t sensortype[MAX_SENSORS];
int adc_divisor = 1;
int adc_offset = 0;

DallasTemperature DSTemp;
/*****************************************************
 *
 * Connect to WiFi 
 *
 *****************************************************/
#if defined(ESP32)
bool connect_wifi(void)
{
  SDEBUG_PRINTF("Connecting to %s", IOT_SSID);
  WiFi.begin(IOT_SSID, IOT_PASSWORD);

  int counter = 0;
  while (WiFi.status() != WL_CONNECTED)
  {
    // blinky(1);
    delay(1000);
    if (++counter > 15)
    {
      SDEBUG_PRINTF("\n====>>> Failed to connect to wifi :'(\nWiFi.status()=%d\n", WiFi.status());
      return false;
    }
    SDEBUG_PRINT(".");
  }

  SDEBUG_PRINT("\nWiFi connected, IP address: ");
  SDEBUG_PRINTLN(WiFi.localIP());
#if defined(SERIAL_DEBUG)
  SDEBUG_PRINTLN("===================================\nDiagnostics after connect:");
  WiFi.printDiag(Serial);
#endif

  return true;
}
#elif defined(ESP8266)
bool connect_wifi(void)
{
  SDEBUG_PRINTF("Connecting to %s", IOT_SSID);
  // WiFi.disconnect();
  WiFi.begin(IOT_SSID, IOT_PASSWORD);

  int counter = 0;
  while (WiFi.status() != WL_CONNECTED)
  {
    // blinky(1);
    delay(1000);
    if (++counter > 15)
    {
      SDEBUG_PRINTF("\n====>>> Failed to connect to wifi :'(\nWiFi.status()=%d\n", WiFi.status());
      return false;
    }
    SDEBUG_PRINT(".");
  }

  SDEBUG_PRINT("\nWiFi connected, IP address: ");
  SDEBUG_PRINTLN(WiFi.localIP());
  return true;
}
#endif

/*****************************************************
 *
 * MQTT subscription callback function
 *
 *****************************************************/
void setvar(String var)
{
  int separator = var.indexOf(':');
  String value = var.substring(separator + 1);
  var = var.substring(0, separator);
  SDEBUG_PRINTLN("var: [" + var + "], value: [" + value + "]");
  if (var == "sleepdur")
  {
    sleepdur = value.toInt();
  }
  else if (var == "valvecount")
  {
    valvecount = value.toInt();
  }
  else if (var.startsWith("valvepins"))
  {
    int index = var.substring(11, 1).toInt();
    valvepin1[index] = value.substring(0, value.indexOf(",")).toInt();
    SDEBUG_PRINTF("valvepin1[%d] = <%d>\n", index, valvepin1[index]);
    pinMode(valvepin1[index], OUTPUT);
    digitalWrite(valvepin1[index], HIGH); // Default state (active LOW)
    valvepin2[index] = value.substring(1 + value.indexOf(",")).toInt();
    SDEBUG_PRINTF("valvepin2[%d] = <%d>\n", index, valvepin2[index]);
    pinMode(valvepin2[index], OUTPUT);
    digitalWrite(valvepin2[index], HIGH); // Default state (active LOW)
  }
  else if (var.startsWith("valvestate"))
  {
    int index = var.substring(strlen("valvestate") + 2, 1).toInt();
    valvestate[index] = value.toInt();
    SDEBUG_PRINTF("valvestate[%d] = <%d>\n", index, valvestate[index]);
    // Can't do this - might have valvestate before valvepins
    // setvalve(index, OFF);
  }
  else if (var == "sensorcount")
  {
    sensorcount = value.toInt();
  }
  else if (var.startsWith("sensorpins"))
  {
    // Index MUST only be a single gidget digit
    int index = var.substring(11, 12).toInt();
    strncpy(sensorpins[index], value.c_str(), MAX_PINSTRINGLENGTH);
    sensorpins[index][MAX_PINSTRINGLENGTH - 1] = '\0'; // ensure a terminated c_string
    SDEBUG_PRINTF("%d:sensorpins[%d] = '%s'\n", __LINE__, index, sensorpins[index]);
  }
  else if (var.startsWith("sensortype"))
  {
    SDEBUG_PRINTLN("var.substring(11,12) = '" + var.substring(11, 12) + "'");
    int index = var.substring(11, 12).toInt();
    sensortype[index] = value.toInt();
    SDEBUG_PRINTF("sensortype[%d] = '%d'\n", index, sensortype[index]);
  }
  else if (var == "adc_divisor")
  {
    adc_divisor = value.toInt();
  }
  else if (var == "adc_offset")
  {
    adc_offset = value.toInt();
  }
}

void mqttcallback(char *topic, byte *payload, unsigned int length)
{
  // Convert possibly unterminated payload to string
  String pstring;
  for (unsigned int i = 0; i < length; i++)
  {
    pstring = pstring + (char)payload[i];
  }
  SDEBUG_PRINTF("%s:%d Topic: [%s], bind/<cid>: [%s], payload: [%s]\n", __FILE__, __LINE__, topic, ("bind/" + cid).c_str(), pstring.c_str());
  // Process bind response, setting device ID appropriately
  if (!strcmp(topic, ("bind/" + cid).c_str()))
  {
    devid = pstring;
    SDEBUG_PRINTF("devid=%s\n", devid.c_str());
  }

  // Process persistent data
  if (!strcmp(topic, ("persist/" + devid + "/set").c_str()))
  {
    // payload will be a pair, varname and varvalue separated by : character
    setvar(pstring);
  }
}
/*****************************************************
 #####################################################
 *
 * SETUP
 *
 #####################################################
 *****************************************************/
void setup()
{
  byte retries = 10; // 5 seconds maximum wait

  // pinMode(VALVE1A, OUTPUT);     // Initialize the VALVE1A pin as an output
  // digitalWrite(VALVE1A, HIGH);  // Turn the LED off by making the voltage HIGH
  // pinMode(VALVE1B, OUTPUT);     // Initialize the VALVE1B pin as an output
  // digitalWrite(VALVE1B, HIGH);  // Turn the LED off by making the voltage HIGH

  Serial.begin(115200);

  SDEBUG_PRINTLN("============================================");
  SDEBUG_PRINTLN(SKETCHVERS);
  SDEBUG_PRINTLN("cid: " + cid + ", devid: " + devid + "\n");
  SDEBUG_PRINTLN("============================================");

  // MQTT initialise
  mqttclient.setCallback(mqttcallback);

  //  Connect to AP
  if (connect_wifi())
  {
    // request a device id (devid string)
    if (request_bind())
    {
      // Try allowing 2.5 seconds for subscriptions to arrive?
      SDEBUG_PRINT("Waiting for bind result");
      while (devid == "DEV" + cid && --retries)
      {
        delay(500);
        SDEBUG_PRINT("*");
        mqttclient.loop();
      }
      SDEBUG_PRINTLN("...done.");
      // All other subscriptions (and publishes) use device id
      mqttclient.subscribe(("control/" + devid + "/#").c_str());
      mqttclient.subscribe(("persist/" + devid + "/set").c_str());
      // Save current status (firmware, buildtime, ip)
      mqttclient.publish(("status/" + devid).c_str(), (String("firmware=") + String(gitHEAD)).c_str());
      mqttclient.publish(("status/" + devid).c_str(), (String("buildtime=") + __DATE__ + " " + __TIME__).c_str());
      mqttclient.publish(("status/" + devid).c_str(), (String("ip=") + WiFi.localIP().toString()).c_str());
      // And then request all persistent variables back again
      mqttclient.publish("persist/fetch", devid.c_str());
      SDEBUG_PRINT("Waiting for persistent variables...");
      brief_pause();
    }
    control_valves();
    read_sensors();
  }
  else
  {
    SDEBUG_PRINTLN("Uh-oh, couldn't find the AP");
  }
  mqttclient.disconnect();
  deep_sleep(sleepdur);
}

/*****************************************************
 *
 * control_valves
 *
 * Send an on (or off) pulse to valve based on value
 * in valve{1..n} variable(s)
 *
 *****************************************************/
void control_valves()
{
  for (int i = 0; i < valvecount; i++)
  {
    setvalve(i, valvestate[i]);
  }
}

/*****************************************************
 *
 * brief_pause
 *
 * Do an mqtt loop 10 times at 50ms intervals 
 * to give 1/2 second pause
 *
 *****************************************************/
void brief_pause()
{
  byte retries = 10;
  SDEBUG_PRINT(" BP:")
  while (--retries)
  {
    delay(50);
    SDEBUG_PRINT("*");
    mqttclient.loop();
  }
  SDEBUG_PRINTLN("");
}

/*****************************************************
 *
 * print_onewire_address
 *
 * Print the bytes of a OneWire address
 *
 *****************************************************/
void print_onewire_address(byte *addr)
{
  SDEBUG_PRINTF("%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X",
                addr[0], addr[1], addr[2], addr[3],
                addr[4], addr[5], addr[6], addr[7]);
}

/*****************************************************
 *
 * loop
 *
 *****************************************************/
void loop()
{
  // Empty loop - the loop is created by deep sleep...
}

/*****************************************************
 * request_bind
 *
 * subscribe to bind/<chipid>
 * publish bind/request => <chipid>
 *
 * Initiate exchange with 'bind' server to obtain a device ID
 *
 ****************************************************/
int request_bind()
{
  SDEBUG_PRINTF("wclient.connected(): %d\n", wclient.connected());
  SDEBUG_PRINTF("mqttclient.state()=%d\n", mqttclient.state());
  if (!mqttclient.connected())
  {
    mqttclient.setServer(IPAddress(192, 168, MQTT_OCTET3, 1), 1883);
    if (!mqttclient.connect(("Client-" + devid).c_str(), MQTT_USER, MQTT_PASSWORD))
    {
      SDEBUG_PRINTLN("Failed to connect to MQTT broker");
      return FAILURE;
    }
  }

  // 'bind' uses CHIP id, not device ID.
  // should receive device id after publishing bind/request
  mqttclient.subscribe(("bind/" + cid).c_str());
  mqttclient.publish("bind/request", cid.c_str());
  mqttclient.loop();
  SDEBUG_PRINTLN("request_bind: request sent");
  return SUCCESS;
}

/*****************************************************
 *
 * send_error
 *
 ****************************************************/
void send_error( String err )
{
  SDEBUG_PRINTLN(err);
  if (mqttclient.connected())
  {
      mqttclient.publish(("status/" + devid).c_str(), String("error="+err).c_str() );
  }
}
/*****************************************************
 *
 * read_sensors
 *
 ****************************************************/
void read_sensors()
{
  SDEBUG_PRINTLN("read_sensors");
  float temp = -127.0;

  /*** This assumes we're not using analogue input pin for an actual sensor ***/
  //  if (mqttclient.connected()) {
  //    SDEBUG_PRINTLN("publishing VCC..." + String(ESP.getVcc()));
  //    // Hierarchy is sensors/<deviceid>/<sensorid>/<parameter>
  //    mqttclient.publish(MQTT::Publish( "sensors/" + devid + "/esp-VCC/mV", String(ESP.getVcc()) )
  //                       .set_retain()
  //                      );
  //    mqttclient.loop();
  //  }

  /***************************************************************
   * send RSSI always
   ***************************************************************/
  if (mqttclient.connected())
  {
    brief_pause(); // for DHT sensor
    int32_t rssi = WiFi.RSSI();
    SDEBUG_PRINTF("Publishing RSSI: %d\n", rssi);
    mqttclient.publish(("sensors/" + devid + "/RSSI/dBm").c_str(), String(rssi).c_str());
  }
  for (int i = 0; i < sensorcount; i++)
  {
    SDEBUG_PRINTLN("sensortype[" + String(i) + "]=<" + String(sensortype[i]) + ">");
    if (sensortype[i] == DHT11 or sensortype[i] == DHT21 || sensortype[i] == DHT22)
    {
      dhtp = new DHT(String(sensorpins[i]).toInt(), sensortype[i]);
      dhtp->begin();
      ///
      // DHT Humidity/Temperature sensor
      ///
      if (mqttclient.connected())
      {
        brief_pause(); // for DHT sensor
        // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
        float h = dhtp->readHumidity();
        // Read temperature as Celsius (the default)
        float t = dhtp->readTemperature();
// Check if any reads failed and exit early (to try again).
#define MAX_DHT_RETRIES 4
        byte retries = MAX_DHT_RETRIES;
        while ((isnan(h) || isnan(t)) && retries--)
        {
          send_error( String("Failed to read from DHT sensor ")+String(MAX_DHT_RETRIES - retries)+String(" time(s)!") );
          brief_pause();
          // delay(2100);
          h = dhtp->readHumidity();
          t = dhtp->readTemperature();
        }
        if (isnan(h) || isnan(t))
        {
          send_error(String("DHT returned NAN"));
        }
        else
        {
          SDEBUG_PRINTF("Humidity: %f%%  Temperature: %f°C\n", h, t);
          // Hierarchy is sensors/<deviceid>/<sensorid>/<parameter>
          brief_pause();
          SDEBUG_PRINTLN("Publishing temperature");
          mqttclient.publish(("sensors/" + devid + "/DHT" + String(sensortype[i]) + "-T/degC").c_str(), String(t).c_str());
          brief_pause();
          SDEBUG_PRINTLN("Publishing humidity");
          mqttclient.publish(("sensors/" + devid + "/DHT" + String(sensortype[i]) + "-RH/%").c_str(), String(h).c_str());
          brief_pause();
        }
        mqttclient.loop();
      }
    }
    else if (sensortype[i] == SENSORADC)
    {
      float avgread = 0.0;
      // Bang out 100 quick reads, and average them
      for(int i = 0; i < 100; i++)
        avgread = avgread+analogRead( String( sensorpins[i] ).toInt() );
      avgread = avgread/100.0;
      // Now offset and scale...
      SDEBUG_PRINTF("Raw average ADC: %f", avgread);
      avgread = avgread - adc_offset;
      SDEBUG_PRINTF("Offset average ADC: %f", avgread);
      avgread = avgread / adc_divisor;
      SDEBUG_PRINTF("Scaled average ADC: %f", avgread);
      if (mqttclient.connected())
      {
          SDEBUG_PRINTLN("Publishing ADC");
          mqttclient.publish(("sensors/" + devid + "/ADC/V").c_str(), String(avgread,2).c_str());
          brief_pause();
      }
      // send_error("SENSORADC part isn't written yet");
    }
    else if (sensortype[i] == SENSORONEWIRE)
    {
      SDEBUG_PRINT(" One wire type on pin ");
      SDEBUG_PRINTLN(atoi(sensorpins[i]));
      oneWire.begin(atoi(sensorpins[i]));

      // All 1-Wire sensors...
      byte oneWireCount = discoverOneWireDevices();
      DSTemp.requestTemperatures();
      for (byte i = 0; i < oneWireCount; i++)
      {
        do
        {
          temp = DSTemp.getTempCByIndex(i);
          SDEBUG_PRINT("temp = ");
          SDEBUG_PRINTLN(temp);
        } while (temp == 85.0 || temp == (-127.0));
        SDEBUG_PRINT("  Temperature[");
        SDEBUG_PRINT(i);
        SDEBUG_PRINT("]: ");
        SDEBUG_PRINTLN(temp);
        if (mqttclient.connected())
        {
          mqttclient.publish(("sensors/" + devid + "/DS" + i + "-T/degC").c_str(), String(temp).c_str());
          brief_pause();
        }
      }
    }
    else if (sensortype[i] == SENSORBARO)
    {
      char *pintok, *strtokk;
      // Split sensorpins to get SDA and SCL separately
      SDEBUG_PRINTLN("sensorpins[" + String(i) + "] = '" + sensorpins[i] + "'");
      pintok = strtok_r(sensorpins[i], ",", &strtokk);
      uint8_t sdapin = atoi(pintok);
      pintok = strtok_r(NULL, ",", &strtokk);
      uint8_t sclpin = atoi(pintok);
      SDEBUG_PRINTLN("Wire.begin(" + String(sdapin) + "," + String(sclpin) + ");");
      Wire.begin(sdapin, sclpin);
      BaroSensor.begin();
      brief_pause();
      if (mqttclient.connected())
      {
        if (!BaroSensor.isOK())
        {
          SDEBUG_PRINT("BaroSensor not Found/OK. Error: ");
          SDEBUG_PRINTLN(BaroSensor.getError());
          BaroSensor.begin(); // Try to reinitialise the sensor if we can
        }
        else
        {
          SDEBUG_PRINT("BaroSensor Temperature:\t");
          SDEBUG_PRINTLN(BaroSensor.getTemperature());
          SDEBUG_PRINT("BaroSensor Pressure:\t");
          SDEBUG_PRINTLN(BaroSensor.getPressure());
          // Hierarchy is sensors/<deviceid>/<sensorid>/<parameter>
          mqttclient.publish(("sensors/" + devid + "/baro-T/degC").c_str(), String(BaroSensor.getTemperature()).c_str());
          brief_pause();
          mqttclient.publish(("sensors/" + devid + "/baro-p/mbar").c_str(), String(BaroSensor.getPressure()).c_str());
          brief_pause();
        }
      }
    }
#if defined(ESP8266)
    // unfortunately incompatible with any analogue read of the ADC.
    // If built in firmware cannot use analogue sensors.
    else if (sensortype[i] == SENSORVCC)
    {
      SDEBUG_PRINTLN("publishing VCC..." + String(ESP.getVcc()));
      if (mqttclient.connected())
      {
        brief_pause();
        mqttclient.publish(("sensors/" + devid + "/ESPVCC/mV").c_str(), String(ESP.getVcc()).c_str());
        brief_pause();
      }
    }
#endif
    else if (sensortype[i] == SENSORSOIL)
    {
      int output_value;
      output_value = analogRead(atoi(sensorpins[i]));
      //      output_value = map(output_value,550,0,0,100);
      SDEBUG_PRINTF("SENSORSOIL moisture sensor raw read: %d\n", output_value);
      if (mqttclient.connected())
      {
        brief_pause();
        mqttclient.publish(("sensors/" + devid + "/Soil/moistnesses").c_str(), String(output_value).c_str());
        brief_pause();
      }
    }
  }
}

/*****************************************************
 *
 * enter deep sleep for microseconds
 *
 ****************************************************/
void deep_sleep(long seconds)
{
  SDEBUG_PRINTF("About to sleep for %ld seconds...\n\n", seconds);
#if defined(ESP8266)
  ESP.deepSleep(1000000L * seconds, WAKE_RF_DEFAULT); // Sleep for required time
#elif defined(ESP32)
  ESP.deepSleep(1000000L * seconds);
#endif
}

/*****************************************************
 *
 * discoverOneWireDevices
 *
 * Return number of discovered devices
 *
 ****************************************************/
byte discoverOneWireDevices(void)
{
  uint8_t devcount = 0;
  DeviceAddress addr[MAX_ONEWIRE_DEVICES];

  DSTemp.setOneWire(&oneWire);
  DSTemp.begin();

  SDEBUG_PRINT("Looking for 1-Wire devices...\n\r");
  oneWire.reset_search();
  while (oneWire.search(addr[devcount]))
  {
    SDEBUG_PRINT("\n\rFound \'1-Wire\' device with address: ");
    print_onewire_address(addr[devcount]);
    if (!DSTemp.validAddress(addr[devcount]))
    {
      SDEBUG_PRINT("Address ");
      print_onewire_address(addr[devcount]);
      SDEBUG_PRINTLN(" is not valid!");
    }
    switch (addr[devcount][0])
    {
    case DS18S20MODEL:
      SDEBUG_PRINTLN("Temp sensor DS18S20 found");
      break;
    case DS18B20MODEL:
      SDEBUG_PRINTLN("Temp sensor DS18B20 found");
      break;
    case DS1822MODEL:
      SDEBUG_PRINTLN("Temp sensor DS1822 found");
      break;
    case DS1825MODEL:
      SDEBUG_PRINTLN("Temp sensor DS1825 found");
      break;
    default:
      SDEBUG_PRINTLN("Device does not appear to be a known type temperature sensor.");
      break;
    }
    if (++devcount > MAX_ONEWIRE_DEVICES)
    {
      return devcount;
    }
  }

  oneWire.reset_search();
  return devcount;
}

/*****************************************************
 *
 * setvalve
 *
 * Send an on (or off) pulse to valve based on value
 * in valve{1..n} variable(s)
 *
 *****************************************************/
void setvalve(int index, int state)
{
  if (state > 0)
  {
    SDEBUG_PRINTF("Turning valve %d ON by pin %d", index, valvepin1[index]);
    digitalWrite(valvepin1[index], LOW);
    delay(VALVEPULSEWIDTH);
    digitalWrite(valvepin1[index], HIGH);
  }
  else
  {
    SDEBUG_PRINTF("Turning valve %d OFF by pin %d", index, valvepin1[index]);
    digitalWrite(valvepin2[index], LOW);
    delay(VALVEPULSEWIDTH);
    digitalWrite(valvepin2[index], HIGH);
  }
}
//////////////////////// EOF /////////////////////////////