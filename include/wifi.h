#ifndef __WIFI_H__
#define __WIFI_H__

#include "flags.h"

#if WIFI_ENABLED

bool connect_wifi();
bool connect_tcp_server();

#endif // WIFI_ENABLED

#endif // __WIFI_H__