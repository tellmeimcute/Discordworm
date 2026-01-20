#include "framework.h"
#include "hooks.h"
#include <unordered_map>
#include <functional>
#include <charconv>
#include <stdint.h>
#include <fstream>
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
    static const std::unordered_map<std::string, std::function<bool(std::string_view valuev)>> handlers = {
        {"proxy_address", [](std::string_view valuev) -> bool {
            return inet_pton(AF_INET, valuev.data(), &config.ProxyAddress) > 0;
        }},
        {"proxy_port", [](std::string_view valuev) -> bool {
            int port{0};
            auto result = std::from_chars(valuev.data(), valuev.data() + valuev.size(), port);

            if (result.ec != std::errc() || port < 0 || port > 65535) return false;
            config.ProxyPort = htons(static_cast<uint16_t>(port));
            return true;
        }},
        {"proxy_udp", [](std::string_view valuev) -> bool {
            if (valuev == "true" || valuev == "1") {
                config.ProxyMedia = true;
            }
            if (valuev == "false" || valuev == "0") {
                config.ProxyMedia = false;
            }
            return true;
        }},
        {"fake_udp_payload", [](std::string_view valuev) -> bool {
            if (valuev.size() < 2) return false;
            if (valuev.data()[0] != '0' || valuev.data()[1] != 'x') {
                return false;
            }
            return parse_hex_str(valuev.data() + 2, config.FakeUDPpayload, &config.FakePayloadSize);
        }}
    };

    std::ifstream file("dwormconf.txt");
    std::string line{};

    if (!file.is_open()) {
        return false;
    }

    config.ReadWriteTimeout = 5;
    config.ProxyMedia = false;
    config.FakePayloadSize = sizeof(config.FakeUDPpayload);

    while (getline(file, line)) {
        std::string_view view = line;
        size_t delimiter_pos = view.find('=');

        if (delimiter_pos == std::string_view::npos) continue;

        auto key = view.substr(0, delimiter_pos);
        auto value = view.substr(delimiter_pos + 1);

        std::string key_str = static_cast<std::string>(key);
        if (handlers.contains(key_str)) {
            if (!handlers.at(key_str)(value)) {
                return false;
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
            return false;
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

