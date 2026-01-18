// header.h: включаемый файл для стандартных системных включаемых файлов
// или включаемые файлы для конкретного проекта
//

#pragma once

#define WIN32_LEAN_AND_MEAN             // Исключите редко используемые компоненты из заголовков Windows
// Файлы заголовков Windows
#include <windows.h>
#include <string>
#include <WinSock2.h>
#include <WS2tcpip.h>

//extern IN_ADDR ProxyAddress;
//extern uint16_t ProxyPort;
//extern uint8_t FakeUDPpayload[16];
//extern int ReadWriteTimeout;
//extern bool ProxyMedia;

struct DwormConfig {
	IN_ADDR ProxyAddress;
	uint16_t ProxyPort;
	int ReadWriteTimeout;
	bool ProxyMedia;
	uint8_t FakeUDPpayload[256];
	size_t FakePayloadSize;
};

extern DwormConfig config;