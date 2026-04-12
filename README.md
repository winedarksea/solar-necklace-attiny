# solar-necklace-attiny
Solar Necklace ATTiny code

The purpose of this project is quite simple. Pulse a set of LEDs, driven by mosfets via an ATTINY 416. The schematic for this circuit can be accessed in @lightsaber_necklace.kicad_sch 
The LEDs are behind a moissanite diamond, so the goal is to pulse them in an alternating pattern to create scintillation. The system runs on battery power, so a primary goal is to keep the entire system as power efficient as possible (for example, MCU going to sleep between LED activations).
The LEDs are linked to pins PB3, PA2, PC1, and PC2 with these pins linked to the gates of NMOS FETs on the low side of the LED (gate should be set to high to turn a given LED on). Only one LED should be on at any time. In order to reduce stress on the boost regulator, the next led should be turned on, then the currently active LED immediately turned off (one LED should always be on, with a second led only briefly overlapping as the gates switch).
PA4 is connected to the gate of an NMOS resistor on the low side of other resistor, called Turbo because when on this will change the current flow setting of the boost regulator to make the LED brighter (PA4 high sets system to brighter). The idea is that brief pulses of 'turbo' may enhance the scintillation, but this is only done when the system has more power (PB4 detects above 1.8V).
PB4 is an analog sensing input directly connected to the unregulated voltage of the system (the LEDs and microcontroller are driven by a buck boost MCP1643 constant current LED driver that can run on input power as low as 0.5V). This sensing is currently unused.
The system is only activate when no light is present (ie in a dark room). This is handled by an analog circuit, the microcontroller won't control it, but note that the microcontroller may lose power at any time due to this.
The Attiny will be programmed over UPDI.
An example scintillation pattern is alternating to a new LED 3 times per second, with the next led chosen randomly (it doesn't need to be true randomness, a simple pseudo randomness is likely fine).
