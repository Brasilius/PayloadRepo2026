#!/bin/bash

# Compile and run all tests
echo "Running Modbus tests..."
g++ tests/test_modbus.cpp -o tests/test_modbus && ./tests/test_modbus

echo -e "\nRunning Receiver tests..."
g++ tests/test_receiver.cpp -o tests/test_receiver && ./tests/test_receiver

echo -e "\nRunning BaseStation tests..."
g++ tests/test_basestation.cpp -o tests/test_basestation && ./tests/test_basestation

echo -e "\nRunning Stepper tests..."
g++ tests/test_stepper.cpp -o tests/test_stepper && ./tests/test_stepper

echo -e "\nRunning Transmitter tests..."
g++ tests/test_transmitter.cpp -o tests/test_transmitter && ./tests/test_transmitter

echo -e "\nRunning PayloadSeparator tests..."
g++ tests/test_payload_separator.cpp -o tests/test_payload_separator && ./tests/test_payload_separator

echo -e "\nAll test suites executed."
