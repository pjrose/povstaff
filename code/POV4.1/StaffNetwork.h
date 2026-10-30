#pragma once
#include <Arduino.h>
namespace StaffNetwork {
void begin();
void update();
String statusJSON();
bool hasClient();
}
