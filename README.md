# Project-ES

Project-ES is a building automation project for a 4-floor rehabilitation facility.  
The goal is to improve comfort, safety, accessibility, and energy efficiency using embedded systems, sensors, and actuators.

This repository focuses on the automation of the entrance floor (entree) only.

The system is organized per floor and uses:
- Raspberry Pi nodes for control and socket communication
- STM32 microcontrollers for bus-connected sensor/actuator control
- Wemos D1 modules for Wi-Fi-connected components

In this first phase, the team focuses on:
- defining requirements and a project plan
- comparing bus systems (CAN, I2C, SPI)
- creating a Proof of Concept where a sensor is read and an actuator is controlled via Raspberry Pi

This repository contains the embedded firmware, socket communication code, and supporting example components used in the project.
