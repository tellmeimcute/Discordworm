
# Discordworm 
Программа заставляющая приложение discord под windows использовать SOCKS5-прокси. Умеет проксировать TCP и UDP трафик.
Можно выбрать желаемое поведение с UDP трафиком из трех вариантов donttouch, proxy и sendfake.

## Зачем это существует?
Не хочется включать TUN, только ради дискорда.
Да и не нашел я решения, которое позволяет проксировать TCP и предлагает выбор действий с UDP трафиком.
Находил только либо полное проксирование, либо проксирование TCP + фейк на udp. Поэтому вот.

---

## Использование
1. Закинуть DWrite.dll в папку %localappdata%\Discord\\{ПОСЛЕДНЯЯ_ВЕРСИЯ}\
2. Создать в этой же папке dwormconf.txt.
3. Настроить конфиг:
```
proxy_address=PROXY_IPV4
proxy_port=PROXY_PORT
udp_mode=proxy|sendfake|donttouch
fake_udp_payload=0xHEX
```

**udp_mode** - Проксирование (proxy), отправка фейка (sendfake), ничего не делать (donttouch).

При выборе sendfake: на discord_ip_discovery будет отсылаться фейк с содержимым указанным в fake_udp_payload, по умолчанию 256 нулей.

**fake_udp_payload** - HEX строка максимум 256 байт. Если не указано будет 256 нулей.
