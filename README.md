# PayloadRepo2026

This is the Boomer Rocket Teams codebase for the Payload for the 2025-2026 NASA USLI Competition.🚀👨‍🚀

## Needed Hardware
- Single Board Computer (in our case AML-S905X (or Libreboard Le Potato))
- Soil Sensor from Temu
- RYLR998 LoRa Transceiver
- ESP-32 Dev Kit V1 (not strictly needed, but a form of micrcontroller with TXD, RXD, 3.3V, 5V and Ground pins of some sort)

## Needed Software
- Astral UV https://docs.astral.sh/uv/
- g++ (via sudo dnf install g++ OR sudo apt install g++)
- Linux for SBC (Preferably Raspbian, but other editions should be theoretically compatible, for libreboard, check here: https://libre.computer/products/aml-s905x-cc/ )

## Steps to Initiliaze

Please ensure prior to initiating program to compile the c++ programs as such:
- g++ -o modbus_reader modbus.cpp
- g++ -o recievermodule recievermodule.cpp
- g++ -o transmittermodule transmittermodule.cpp

before then initating the program using 

uv run main.py 

