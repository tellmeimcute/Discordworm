// header.h: включаемый файл для стандартных системных включаемых файлов
// или включаемые файлы для конкретного проекта
//

#pragma once

#define WIN32_LEAN_AND_MEAN             // Исключите редко используемые компоненты из заголовков Windows
// Файлы заголовков Windows
#include <windows.h>
#include <string>
#include <WS2tcpip.h>
#include <WinSock2.h>

extern IN_ADDR ProxyAddress;
extern uint16_t ProxyPort;
extern uint8_t FakeUDPpayload[16];
extern int ReadWriteTimeout;
extern bool ProxyMedia;