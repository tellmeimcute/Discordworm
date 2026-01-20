#pragma once
#include "framework.h"
#include <stdint.h>

bool parse_hex_str(const char* s, uint8_t* pbuf, size_t* size);
bool ParseConf(DwormConfig& config);
