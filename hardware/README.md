# coffee_assistant/hardware

## notes
*   Voltage supervisor

## nrf52

A bluetooth module based around an nrf52840 designed to be connected to the other functional
modules in this project. Includes USB for power, updating and communication.

## load_cell

A load cell module based around the hx711 adc with integrated amp designed to read weight
from up to two load cells. Also provides the user interface for the whole system and performs
control functions for the other node(s).

Requires nrf52 module.

## coffee_machine_interface

A module designed to attach to a coffee machine to:

*   Trigger the functions of the machine via mosfet or ssr output
*   Monitor the temperature of the machine via thermocouple

Can be powered from mains or up to 24v dc.

Requires nrf52 module.
