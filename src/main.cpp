#include <Arduino.h>
/*
 * Basic framework test
 *
 */
// #include <MQTT.h>
#ifdef ESP32
#include <WiFi.h>
#else
#ifdef ESP8266
#include <ESP8266WiFi.h>
#endif
#endif
#include <PubSubClient.h> // lmroy version! https://github.com/lmroy/pubsubclient
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
#define SKETCHVERS "Sketch development.ino v.20180113"

// Comment this out to strip all debugging messages
#define G_DEBUG true

#include "private-data.h"
#include "gdebug.h"
#include "SensorMQTT.hpp"

#ifdef G_DEBUG
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

/**
 * Sensor type definitions. DHT11 = 11, DHT21 = 21, DHT22 = 22 as per dht.h
 */
#define ONEWIRETYPE 1 // Not 11 or 21 or 22
#define BAROTYPE 2
#define SOIL 3

// pringlei hostap at 192.168.42.1
IPAddress mqttserver(192, 168, MQTT_OCTET3, 1);

/*****************************************************
 *
 * Utility macros
 *
 *****************************************************/
#define ONEMINUTE 60000000
#define FAILURE 0
#define SUCCESS (!FAILURE)
#define BUFFER_SIZE 100
#define OFF 0
#define ON 1

// "Valve" control
#define MAX_VALVES 10
#define MAX_SENSORS 10
#define MAX_PINSTRINGLENGTH 6
#define VALVE1A D6
// D6 = GPIO 12
#define VALVE1B D5
// D5 = GPIO 14
// ms to pulse the valve
#define VALVEPULSEWIDTH 20

#define DHTPIN D7
#define DHTTYPE DHT22
//#define DHTTYPE DHT11
//#define DHTTYPE DHT21

//DHT dht(DHTPIN, DHTTYPE);
DHT *dhtp;

/*****************************************************
 *
 * Initialisation
 *
 *****************************************************/
// 1-Wire setup
//OneWire oneWire(ONE_WIRE_BUS);
OneWire *oneWirep;

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
char sChipID[13];
#ifdef ESP32
void getChipId(char **sChipID);
getChipId(&sChipID);
#else
String cid(sChipID);
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

#ifdef ESP32
/*****************************************************
 *
 * getchipid() for ESP32
 *
 *****************************************************/
void getChipId(char **sChipID)
{
  uint64_t chipid;
  chipid = ESP.getEfuseMac();                                               //The chip ID is essentially its MAC address(length: 6 bytes).
  sprintf(sChipID, "%04X%08X", (uint16_t)(chipid >> 32), (uint32_t)chipid); //print High 2 then Low 4bytes.
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
  GDEBUG_PRINTLN("var: [" + var + "], value: [" + value + "]");
  if (var == "sleepdur")
  {
    sleepdur = value.toInt();
  }
  // else if ( var == "valve1") {
  // valve1 = value.toInt();
  // }
  else if (var == "valvecount")
  {
    valvecount = value.toInt();
  }
  else if (var.startsWith("valvepins"))
  {
    int index = var.substring(11, 1).toInt();
    valvepin1[index] = value.substring(0, value.indexOf(",")).toInt();
    GDEBUG_PRINT("valvpin1[");
    GDEBUG_PRINT(index);
    GDEBUG_PRINT("] = <");
    GDEBUG_PRINT(valvepin1[index]);
    GDEBUG_PRINTLN(">");
    pinMode(valvepin1[index], OUTPUT);
    digitalWrite(valvepin1[index], HIGH); // Default state (active LOW)
    valvepin2[index] = value.substring(1 + value.indexOf(",")).toInt();
    GDEBUG_PRINT("valvepin2[");
    GDEBUG_PRINT(index);
    GDEBUG_PRINT("] = <");
    GDEBUG_PRINT(valvepin2[index]);
    GDEBUG_PRINTLN(">");
    pinMode(valvepin2[index], OUTPUT);
    digitalWrite(valvepin2[index], HIGH); // Default state (active LOW)
  }
  else if (var.startsWith("valvestate"))
  {
    int index = var.substring(12, 1).toInt();
    valvestate[index] = value.toInt();
    GDEBUG_PRINT("valvpin1[");
    GDEBUG_PRINT(index);
    GDEBUG_PRINT("] = <");
    GDEBUG_PRINT(valvestate[index]);
    GDEBUG_PRINTLN(">");

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
    GDEBUG_PRINT("222:sensorpins[");
    GDEBUG_PRINT(index);
    GDEBUG_PRINT("] = '");
    GDEBUG_PRINT(sensorpins[index]);
    GDEBUG_PRINTLN("'");
  }
  else if (var.startsWith("sensortype"))
  {
    GDEBUG_PRINTLN("var.substring(11,12) = '" + var.substring(11, 12) + "'");
    int index = var.substring(11, 12).toInt();
    sensortype[index] = value.toInt();
    GDEBUG_PRINT("sensortype[");
    GDEBUG_PRINT(index);
    GDEBUG_PRINT("] = '");
    GDEBUG_PRINT(sensortype[index]);
    GDEBUG_PRINTLN("'");
  }
}

void mqttcallback(char *topic, byte *payload, unsigned int length)
{
  GDEBUG_PRINT(topic);
  GDEBUG_PRINT(" => ");
#ifdef NotBloodyLikely
  // Process bind response, setting device ID appropriately
  if (!strcmp(topic, strcat("bind/", cid.c_str())))
  {
    devid = String((char *)payload);
  }

  // Process persistent data
  if (topic == "persist/" + devid + "/set")
  {
    // payload will be a pair, varname and varvalue separated by : character
    setvar(pub.payload_string());
    // example code follows
    if (pub.has_stream())
    {
      uint8_t buf[BUFFER_SIZE];
      int read;
      while (read = pub.payload_stream()->read(buf, BUFFER_SIZE))
      {
        Serial.write(buf, read);
      }
      pub.payload_stream()->stop();
      GDEBUG_PRINTLN("");
    }
    else
    {
      GDEBUG_PRINTLN(pub.payload_string());
    }
  }
#endif
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

#ifdef G_DEBUG
  Serial.begin(115200);
#endif

  GDEBUG_PRINTLN(SKETCHVERS);
  GDEBUG_PRINTLN("============================================");
  GDEBUG_PRINTLN("cid: " + cid + ", devid: " + devid + "\nVALVE1A = " + String(VALVE1A) + " VALVE1B = " + String(VALVE1B));

  // setup 'barosensor'
  // SDA = 4 = D2; SCL = 5 = D1
  // Wire.begin(SDA, SCL);
  // BaroSensor.begin();
  //dht = new DHT;
  //dhtp->begin();

  // MQTT intialise
  //*TODO*mqttclient.set_callback(mqttcallback);

  //  Connect to AP
  if (connect_to_AP())
  {
    // request a device id (devid string)
    if (request_bind())
    {
      // Try allowing 2.5 seconds for subscriptions to arrive?
      GDEBUG_PRINT("Waiting for bind result");
      while (devid == "DEV" + cid && --retries)
      {
        delay(500);
        GDEBUG_PRINT("*");
        mqttclient.loop();
      }
      GDEBUG_PRINTLN("...done.");
#ifdef NotBloodyLikely
      // All other subscriptions (and publishes) use device id
      mqttclient.subscribe(MQTT::Subscribe()
                               .add_topic("control/" + devid + "/#", 1)
                               .add_topic("persist/" + devid + "/set", 1));
      mqttclient.publish(MQTT::Publish("persist/fetch", devid)
                             .set_qos(1));
#endif
      GDEBUG_PRINT("Waiting for persistent variables...");
      two_second_pause();
    }
    control_valves();
    read_sensors();
  }
  else
  {
    GDEBUG_PRINTLN("Uh-oh, couldn't find the AP");
  }

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
 * two_second_pause
 *
 * Do an mqtt loop 40 times at 50ms intervals
 *
 *****************************************************/
void two_second_pause()
{
  byte retries = 40;
  while (--retries)
  {
    delay(50);
    GDEBUG_PRINT("*");
    mqttclient.loop();
  }
  GDEBUG_PRINTLN("");
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
#ifdef NotBloodyLikely
  if (!mqttclient.connected())
  {
    if (!mqttclient.connect(MQTT::Connect("Client" + devid)
                                /*.set_clean_session() */
                                /*.set_will("status", "down") */
                                .set_auth("ESP8266", "I am your father")
                            /*.set_keepalive(30) */
                            ))
    {
      GDEBUG_PRINTLN("Failed to connect to MQTT broker");
      return FAILURE;
    }
  }
  // 'bind' uses CHIP id, not device ID.
  // should receive device id after publishing bind/request
  mqttclient.subscribe(MQTT::Subscribe()
                           .add_topic("bind/" + cid, 1));
  mqttclient.publish(MQTT::Publish("bind/request", cid)
                         .set_qos(0));
  mqttclient.loop();
  GDEBUG_PRINTLN("request_bind: request sent");
#endif
  return SUCCESS;
}

/*****************************************************
 *
 * read_sensors
 *
 ****************************************************/
void read_sensors()
{
  GDEBUG_PRINTLN("read_sensors");
  float temp = -127.0;
#ifdef NotBloodyLikely
  // Connect to mqtt if not already connected
  if (!mqttclient.connected())
  {
    if (!mqttclient.connect(MQTT::Connect("Client" + devid)
                                /*.set_clean_session() */
                                /*.set_will("status", "down") */
                                .set_auth("ESP8266", "I am your father")
                            /*.set_keepalive(30) */
                            ))
    {
      // If we can't connect to the mqtt broker, there's no point reading sensors
      GDEBUG_PRINTLN("Failed to connect to mqtt broker");
      return;
    }
  }
#endif

  /*** This assumes we're not using analogue input pin for an actual sensor ***/
  //  if (mqttclient.connected()) {
  //    GDEBUG_PRINTLN("publishing VCC..." + String(ESP.getVcc()));
  //    // Hierarchy is sensors/<deviceid>/<sensorid>/<parameter>
  //    mqttclient.publish(MQTT::Publish( "sensors/" + devid + "/esp-VCC/mV", String(ESP.getVcc()) )
  //                       .set_retain()
  //                      );
  //    mqttclient.loop();
  //  }

  for (int i = 0; i < sensorcount; i++)
  {
    GDEBUG_PRINTLN("sensortype[" + String(i) + "]=<" + String(sensortype[i]) + ">");
#ifdef NotBloodyLikely
    if (sensortype[i] == DHT11 or sensortype[i] == DHT21 || sensortype[i] == DHT22)
    {
      dhtp = new DHT(String(sensorpins[i]).toInt(), sensortype[i]);
      dhtp->begin();
      ///
      // DHT Humidity/Temperature sensor
      ///
      if (mqttclient.connected())
      {
        /* Unclear if this is blocking or non-blocking */
        delay(2500); // for DHT sensor
        // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
        float h = dhtp->readHumidity();
        // Read temperature as Celsius (the default)
        float t = dhtp->readTemperature();
        // Check if any reads failed and exit early (to try again).
        byte retries = 4;
        while ((isnan(h) || isnan(t)) && retries--)
        {
          GDEBUG_PRINTLN("Failed to read from DHT sensor!");
          delay(2100);
          h = dhtp->readHumidity();
          t = dhtp->readTemperature();
        }
        if (isnan(h) || isnan(t))
        {
          GDEBUG_PRINTLN("DHT is a pain in the backside");
        }
        else
        {
          GDEBUG_PRINT("Humidity: ");
          GDEBUG_PRINT(h);
          GDEBUG_PRINT(" %\t");
          GDEBUG_PRINT("Temperature: ");
          GDEBUG_PRINT(t);
          GDEBUG_PRINTLN(" *C ");
          // Hierarchy is sensors/<deviceid>/<sensorid>/<parameter>
          mqttclient.publish(MQTT::Publish("sensors/" + devid + "/DHT" + String(sensortype[i]) + "-T/degC", String(t))
                                 .set_retain());
          mqttclient.publish(MQTT::Publish("sensors/" + devid + "/DHT" + String(sensortype[i]) + "-RH/%", String(h))
                                 .set_retain());
        }
        mqttclient.loop();
      }
    }
    else if (sensortype[i] == ONEWIRETYPE)
    {
      GDEBUG_PRINT(" One wire type on pin ");
      GDEBUG_PRINTLN(sensorpins[i]);
      oneWirep = new OneWire(atoi(sensorpins[i]));
      // All 1-Wire sensors...
      byte oneWireCount = discoverOneWireDevices();
      DallasTemperature DSTemp(oneWirep);
      DSTemp.requestTemperatures();
      for (byte i = 0; i < oneWireCount; i++)
      {
        do
        {
          temp = DSTemp.getTempCByIndex(i);
          GDEBUG_PRINT("temp = ");
          GDEBUG_PRINTLN(temp);
        } while (temp == 85.0 || temp == (-127.0));
        GDEBUG_PRINT("  Temperature[");
        GDEBUG_PRINT(i);
        GDEBUG_PRINT("]: ");
        GDEBUG_PRINTLN(temp);
        if (mqttclient.connected())
        {
          mqttclient.publish(MQTT::Publish("sensors/" + devid + "/DS" + i + "-T/degC", String(temp))
                                 .set_retain());
          mqttclient.loop();
        }
      }
    }
    else if (sensortype[i] == BAROTYPE)
    {
      char *pintok, *strtokk;
      // Split sensorpins to get SDA and SCL separately
      GDEBUG_PRINTLN("sensorpins[" + String(i) + "] = '" + sensorpins[i] + "'");
      pintok = strtok_r(sensorpins[i], ",", &strtokk);
      uint8_t sdapin = atoi(pintok);
      pintok = strtok_r(NULL, ",", &strtokk);
      uint8_t sclpin = atoi(pintok);
      GDEBUG_PRINTLN("Wire.begin(" + String(sdapin) + "," + String(sclpin) + ");");
      Wire.begin(sdapin, sclpin);
      BaroSensor.begin();
      if (mqttclient.connected())
      {
        if (!BaroSensor.isOK())
        {
          GDEBUG_PRINT("BaroSensor not Found/OK. Error: ");
          GDEBUG_PRINTLN(BaroSensor.getError());
          BaroSensor.begin(); // Try to reinitialise the sensor if we can
        }
        else
        {
          GDEBUG_PRINT("BaroSensor Temperature:\t");
          GDEBUG_PRINTLN(BaroSensor.getTemperature());
          GDEBUG_PRINT("BaroSensor Pressure:\t");
          GDEBUG_PRINTLN(BaroSensor.getPressure());
          // Hierarchy is sensors/<deviceid>/<sensorid>/<parameter>
          mqttclient.publish(MQTT::Publish("sensors/" + devid + "/baro-T/degC", String(BaroSensor.getTemperature()))
                                 .set_retain());
          mqttclient.publish(MQTT::Publish("sensors/" + devid + "/baro-p/mbar", String(BaroSensor.getPressure()))
                                 .set_retain());
          mqttclient.loop();
        }
      }
    }
    else if (sensortype[i] == SOIL)
    {
      int output_value;
      output_value = analogRead(atoi(sensorpins[i]));
      //      output_value = map(output_value,550,0,0,100);
      GDEBUG_PRINT("Soil moisture sensor raw read: ");
      GDEBUG_PRINTLN(output_value);
      // Hierarchy is sensors/<deviceid>/<sensorid>/<parameter>
      //      mqttclient.publish(MQTT::Publish( "sensors/" + devid + "/soil/%", String(BaroSensor.getTemperature()) )
      //                         .set_retain()
      //                        );
      //      mqttclient.loop();
    }
#endif
  }
}
/*****************************************************
 *
 * enter deep sleep for microseconds
 *
 ****************************************************/
void deep_sleep(long microseconds)
{
  GDEBUG_PRINTF("About to sleep for %ld microseconds...\n\n", microseconds);
  ESP.deepSleep(microseconds, WAKE_RF_DEFAULT); // Sleep for required time
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
  byte i;
  byte devcount = 0;
  byte present = 0;
  byte data[12];
  byte addr[8];

  GDEBUG_PRINT("Looking for 1-Wire devices...\n\r");
  oneWirep->reset_search();
  while (oneWirep->search(addr))
  {
    devcount++;
    GDEBUG_PRINT("\n\rFound \'1-Wire\' device with address: ");
    for (i = 0; i < 8; i++)
    {
      if (addr[i] < 16)
      {
        GDEBUG_PRINT('0');
      }
      GDEBUG_PRINT(addr[i], HEX);
      if (i < 7)
      {
        GDEBUG_PRINT(":");
      }
      else
      {
        GDEBUG_PRINTLN("");
      }
    }
    if (OneWire::crc8(addr, 7) != addr[7])
    {
      GDEBUG_PRINT("CRC is not valid!\n");
      //==>>> return;
    }
    switch (addr[0])
    {
    case DS18S20MODEL:
      GDEBUG_PRINTLN("Temp sensor DS18S20 found");
      break;
    case DS18B20MODEL:
      GDEBUG_PRINTLN("Temp sensor DS18B20 found");
      break;
    case DS1822MODEL:
      GDEBUG_PRINTLN("Temp sensor DS1822 found");
      break;
    case DS1825MODEL:
      GDEBUG_PRINTLN("Temp sensor DS1825 found");
      break;
    default:
      GDEBUG_PRINTLN("Device does not appear to be a known type temperature sensor.");
      break;
    }
  }

  oneWirep->reset_search();
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
    GDEBUG_PRINT("Turning valve ");
    GDEBUG_PRINT(index);
    GDEBUG_PRINT(" ON by pin ");
    GDEBUG_PRINTLN(valvepin1[index]);
    digitalWrite(valvepin1[index], LOW);
    delay(VALVEPULSEWIDTH);
    digitalWrite(valvepin1[index], HIGH);
  }
  else
  {
    GDEBUG_PRINT("Turning valve ");
    GDEBUG_PRINT(index);
    GDEBUG_PRINT(" OFF by pin ");
    GDEBUG_PRINTLN(valvepin2[index]);
    digitalWrite(valvepin2[index], LOW);
    delay(VALVEPULSEWIDTH);
    digitalWrite(valvepin2[index], HIGH);
  }
}
/*****************************************************
 *
 * connect_to_AP
 *
 * What it says on the box, connects  to
 * predefined AP, returns SUCCESS or
 * FAILURE after maximum 15 seconds (30 x 500ms)
 *
 ****************************************************/
int connect_to_AP()
{
  /* */
  //#define ALTERNATIVE_METHOD
#ifdef ALTERNATIVE_METHOD
  GDEBUG_PRINT("Connecting by alternative method to ");
  GDEBUG_PRINT(AP_SSID);
  GDEBUG_PRINTLN("...");
  WiFi.begin(AP_SSID, AP_PASSWORD);
  if (WiFi.status() != WL_CONNECTED)
  {
    WiFi.begin(AP_SSID, AP_PASSWORD);

    if (WiFi.waitForConnectResult() != WL_CONNECTED)
    {
      GDEBUG_PRINTLN("Failed to connect to AP");
      return FAILURE;
    }
  }
#else
  GDEBUG_PRINT("Connecting to ");
  GDEBUG_PRINT(AP_SSID);

  WiFi.begin(AP_SSID, AP_PASSWORD);
  int counter = 0;
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    if (++counter > 30)
      return FAILURE;
    GDEBUG_PRINT(".");
  }
#endif

  GDEBUG_PRINT("\nWiFi connected, IP address: ");
  GDEBUG_PRINTLN(WiFi.localIP());

  return SUCCESS;
}
