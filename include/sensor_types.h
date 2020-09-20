#ifndef _SENSOR_TYPES_H_
#define _SENSOR_TYPES_H_

/**
 * Sensor type definitions. DHT11 = 11, DHT21 = 21, DHT22 = 22 as per DHT.h
 */

#ifndef DHT22
#  include <DHT.h>
#endif

#define SENSORONEWIRE 1 // Not 11 or 21 or 22
#define SENSORBARO 2
#define SENSORSOIL 3
#define SENSORVCC 4 // NEVER USE THIS!
#define SENSORADC 5
#define SENSORSWITCHEDADC 6
#define SENSORDHT11 DHT11 // 11
#define SENSORDHT21 DHT21 // 21
#define SENSORDHT22 DHT22 // 22

#endif // _SENSOR_TYPES_H_