# coffee_assistant/hardware

## mcu

Base mcu module for other assemblies

### features
*   ESP32 module
*   USB for power, flashing and communication
*   Battery operation & charging
*   Castellation + breadboard friendly pitch

### todo
*   Power path & voltage supervisor
*   Fuel guage
*   Battery charging
*   Battery protection

## load_cell

Requires mcu module

### features
*   Control node
*   HX711 ADC to read load cell(s)
*   User interface
    *   Display
        *   Weight
        *   Time
        *   State
        *   Grind setting
        *   Advice
    *   Web
        *   Everything from `display`
        *   Manage bean inventory
        *   Store and read extraction history (browser local storage?)
    *   Cap sense buttons
        *   Tare
        *   Calibrate
        *   Select beans
        *   Start/stop
    *   Sound
        *   Buzzer to assist with bean weighing?

### todo
*   Load cell physical mounting
*   Waterproofing?
    *   Would a 2 board sandwitch with the electronics on the underside
        of the top PCB be sufficiently waterproof?

## coffee_machine_interface

Requires mcu module

### features
*   Puppet node
*   Trigger coffee machine buttons via SSR/mosfet
*   Monitor temperature via NTC (on board) and thermocouple
*   Mains or 24v operation
*   Can shut off at approximately the right time based off the previous best
    guess given by the control node in case of a loss of communication

### todo
*   Default cutoff time in case of loss of signal
