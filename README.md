# Programowanie sieciowe 26Z (Z35)

Zadanie 1B - Symulacja komunikacji klient–serwer TCP w sytuacji, gdy odbiorca nie nadąża za szybkim nadawcą.

## Zawartość repozytorium do zadania 1B

* `sock_client1.c` – kod źródłowy klienta (wysyłanie strumienia danych, pomiar czasów `MIN`/`AVG`/`MAX` wywołań `write()`).
* `sock_server_gi1.c` – kod źródłowy serwera (odbiór danych ze stałym opóźnieniem).
* `Dockerfile.client`, `Dockerfile.server` – obrazy kontenerów budujące programy.
* `run.sh` – skrypt automatyzujący budowanie obrazów, uruchomienie serwera w wydzielonej sieci Dockera oraz wykonanie serii pomiarów dla trzech rozmiarów bufora.

## Uruchomienie Zadania 1

```bash
cd zad1
./run.sh