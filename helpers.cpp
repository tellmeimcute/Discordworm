#include "helpers.h"
#include "framework.h"
#include <unordered_map>
#include <functional>
#include <charconv>
#include <stdint.h>
#include <fstream>
#include <string>

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
    while (pbuf < pe&&* s)
    {
        if (!parse_hex_byte(s, pbuf))
            return false;
        pbuf++; s += 2; (*size)++;
    }
    return true;
}

bool ParseConf(DwormConfig& config) {
    static const std::unordered_map<std::string, std::function<bool(std::string_view valuev)>> handlers = {
        {"proxy_address", [&config](std::string_view valuev) -> bool {
            return inet_pton(AF_INET, valuev.data(), &config.ProxyAddress) > 0;
        }},
        {"proxy_port", [&config](std::string_view valuev) -> bool {
            int port{0};
            auto result = std::from_chars(valuev.data(), valuev.data() + valuev.size(), port);

            if (result.ec != std::errc() || port < 0 || port > 65535) return false;
            config.ProxyPort = htons(static_cast<uint16_t>(port));
            return true;
        }},
        {"proxy_udp", [&config](std::string_view valuev) -> bool {
            if (valuev == "true" || valuev == "1") {
                config.ProxyMedia = true;
            }
            if (valuev == "false" || valuev == "0") {
                config.ProxyMedia = false;
            }
            return true;
        }},
        {"fake_udp_payload", [&config](std::string_view valuev) -> bool {
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

        std::string key_str(key);
        if (handlers.contains(key_str)) {
            if (!handlers.at(key_str)(value)) {
                return false;
            }
        }
    }

    return true;
}