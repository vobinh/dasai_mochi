#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiAP.h>
#include <WebServer.h>
#include "globals.h"

void wifi_manager_init(AppSettings* settings);
bool wifi_manager_connect();
void wifi_manager_disconnect();
void wifi_manager_loop();
String wifi_manager_get_ip();
void wifi_manager_save_settings();

#endif
