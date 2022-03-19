# midi2cv
Arduino/ATMega MIDI to CV code for hardware module

I used [Larry McGovern's](https://github.com/elkayem/midi2cv) repo for the basis of this with a couple of differences:

-   MCP4922 rather than MCP4822 (essentially 10bit vs 12bit resolution).
-   6N137 rather than SFH618 opto-coupler which is faster.
-   Bare bones ATMega168 (with bootloader) rather than Arduino mini.
-   Omitted pitch bend and control change.
-   Added changeable clock pulse interval (24 ppq, 12 ppq and 6 ppq), hence 16th, 8th and quarter note pulses.
-   Added MIDI channel DIP switch (4bit) so channel 1-15 selectable or omni-mode.
-   Added tuning pot to the note CV output rather than using series resistors, allows finer tuning to the 1v / octave.
-   TL072 rather than LM324N op-amps as I had a load of TL072 and TL074's.
-   Added FTDI programmer header to allow updates to the software to be easily uploaded.
-   Added clock reset gate to handle stop / start MIDI events.
-   
![Panel view](modular-synth-project-part-2-1.jpg)

![Rear view](modular-synth-project-part-2-2.jpg)

## TO-DO:
- Add parts list
- Schematic
