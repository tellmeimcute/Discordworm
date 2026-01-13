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

//IN_ADDR ProxyAddress{};
//uint16_t ProxyPort{};
//uint8_t FakeUDPpayload[16]{};
//int ReadWriteTimeout{ 15 };
//bool ProxyMedia{ false };

DwormConfig config{};

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

    while (getline(file, line)) {
        std::stringstream ss(line);
        std::string key, value;

        if (getline(ss, key, '=') && getline(ss, value)) {
            if (!key.compare("proxy_address")) {
                //inet_pton(AF_INET, value.c_str(), &ProxyAddress);
                inet_pton(AF_INET, value.c_str(), &config.ProxyAddress);
            }

            if (!key.compare("proxy_port")) {
                //ProxyPort = htons(static_cast<uint16_t>(std::stoi(value)));
                config.ProxyPort = htons(static_cast<uint16_t>(std::stoi(value)));
            }

            if (!key.compare("proxy_udp")) {
                //ProxyMedia = static_cast<bool>(std::stoi(value));
                config.ProxyMedia = static_cast<bool>(std::stoi(value));
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
            MessageBoxA(0, "failed to read config file dwormconf.txt", "Dworm Proxy", MB_ICONERROR);
            ExitProcess(0);
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

