# PayloadRepo2026

This is the Boomer Rocket Teams codebase for the Payload for the 2025-2026 NASA USLI Competition.🚀👨‍🚀

## Needed Hardware
- Single Board Computer (in our case AML-s905x (or libreboard le potato))
- Soil Sensor from Temu
- RYLR998 LoRa Transceiver

## Needed Software
- Astral UV https://docs.astral.sh/uv/
- g++ (via sudo dnf install g++ OR sudo apt install g++)
- Linux for SBC (Preferably Raspbian, but other editions should be theoretically compatible, for libreboard, check here: https://libre.computer/products/aml-s905x-cc/ )

## Steps to Initiliaze

Please ensure prior to initiating program to compile the c++ programs as such:
g++ -o modbus_reader modbus.cpp
g++ -o lora_reader recievermodule.cpp

before then initating the program using 

uv run main.py 

