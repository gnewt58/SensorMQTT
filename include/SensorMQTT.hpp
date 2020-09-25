#ifndef _SENSORMQTT_HPP_
#define _SENSORMQTT_HPP_

void brief_pause(void);
int request_bind(void);
void control_valves(void);
void read_sensors(void);
void deep_sleep(long);
void setvalve(int index, int state);
int connect_to_AP(void);
byte discoverOneWireDevices(void);
void send_error(String err);
const char* buildENV = BUILD_ENV_NAME;
#endif // _SENSORMQTT_H_