#include "framework.h"
#include "hooks.h"
#include <stdint.h>
#include <fstream>
#include <sstream>
#include <string>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <detours/detours.h>

#pragma comment(lib, "Ws2_32.lib")

HMODULE OriginalDLL;

extern "C" {
	uintptr_t OrignalDWriteCreateFactory{ 0 };
}

DwormConfig config{};

#define INVALID_HEX_DIGIT ((uint8_t)-1)
static inline uint8_t parse_hex_digit(char c)
{
    return (c >= '0' && c <= '9') ? c - '0' : (c >= 'a' && c <= 'f') ? c - 'a' + 0xA : (c >= 'A' && c <= 'F') ? c - 'A' + 0xA : INVALID_HEX_DIGIT;
}

static inline bool parse_hex_byte(const char* s, uint8_t* pbyte)
{
    uint8_t u, l;
    u = parse_hex_digit(s[0]);
    l = parse_hex_digit(s[1]);
    if (u == INVALID_HEX_DIGIT || l == INVALID_HEX_DIGIT)
    {
        *pbyte = 0;
        return false;
    }
    else
    {
        *pbyte = (u << 4) | l;
        return true;
    }
}

bool parse_hex_str(const char* s, uint8_t* pbuf, size_t* size)
{
    uint8_t* pe = pbuf + *size;
    *size = 0;
    while (pbuf < pe && *s)
    {
        if (!parse_hex_byte(s, pbuf))
            return false;
        pbuf++; s += 2; (*size)++;
    }
    return true;
}

void LoadOriginalLib() {
    char path[MAX_PATH]{};
    memcpy(path + GetSystemDirectoryA(path, MAX_PATH - 12), "\\DWrite.dll", 12);
    OriginalDLL = LoadLibraryA(path);
}

bool ParseConf() {
    std::ifstream file("dwormconf.txt");
    std::string line{};

    if (!file.is_open()) {
        return false;
    }

    config.ReadWriteTimeout = 5;
    config.FakePayloadSize = sizeof(config.FakeUDPpayload);

    while (getline(file, line)) {
        std::stringstream ss(line);
        std::string key, value;

        if (getline(ss, key, '=') && getline(ss, value)) {
            if (!key.compare("proxy_address")) {
                inet_pton(AF_INET, value.c_str(), &config.ProxyAddress);
            }

            if (!key.compare("proxy_port")) {
                config.ProxyPort = htons(static_cast<uint16_t>(std::stoi(value)));
            }

            if (!key.compare("proxy_udp")) {
                config.ProxyMedia = static_cast<bool>(std::stoi(value));
            }

            if (!key.compare("fake_udp_payload")) {
                if (value.c_str()[0] == '0' && value.c_str()[1] == 'x') {
                    if (!parse_hex_str(value.c_str() + 2, config.FakeUDPpayload, &config.FakePayloadSize)) {
                        return false;
                    }
                }
            }
        }
    }

    return true;
}

BOOL APIENTRY DllMain(
	HMODULE hModule,
    DWORD  ul_reason_for_call,
    LPVOID lpReserved
)
{
	if (DetourIsHelperProcess()) {
		return TRUE;
	}

    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);

        LoadOriginalLib();
        if (OriginalDLL == NULL)
        {
            MessageBoxA(0, "Cannot load original dll", "Dworm Proxy", MB_ICONERROR);
            return FALSE;
        }

        OrignalDWriteCreateFactory = (uintptr_t)GetProcAddress(OriginalDLL, "DWriteCreateFactory");

        if (!ParseConf()) {
            MessageBoxA(0, "Error with config file dwormconf.txt", "Dworm Proxy", MB_ICONERROR);
            return false;
        }

        DetourRestoreAfterWith();
        HooksAttach();
        break;

    case DLL_PROCESS_DETACH:
        FreeLibrary(OriginalDLL);
        HooksDetach();
        break;
    }
    return TRUE;
}

