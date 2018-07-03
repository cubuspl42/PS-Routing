# Protokół routingu dynamicznego

Projekt realizuje algorytm routingu dynamicznego zainspirowany protokołem RIPv1. Każdy węzeł co 30s rozsyła całą tablicę routingu za pomocą protokołu UDP na porcie 1234 na adres broadcast 255.255.255.255. Interakcja z jądrem odbywa się za pomocą gniazd typu netlink.

## Pliki

* **NetlinkRouteSocket.{h,cpp}** - klasa realizujca komunikację z jądrem za pomocą gniazda netlink route
* **Service.{h,cpp}** - klasa implementujca serwis (demona) realizujacy podstawową funkcjonalność projektu
* **main.cpp** - punkt wejściowy progrmau
* **config{1,2}.json** - przykładowe pliki konfiguracyjne

## Kompilacja

    make

## Uruchomienie

    ./a.out config.json
