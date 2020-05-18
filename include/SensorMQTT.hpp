#ifndef _SENSORMQTT_HPP_
#define _SENSORMQTT_HPP_

void two_second_pause(void);
int request_bind(void);
void control_valves(void);
void read_sensors(void);
void deep_sleep(long);
void setvalve(int index, int state);
int connect_to_AP(void);
byte discoverOneWireDevices(void);

#endif // _SENSORMQTT_H_