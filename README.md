# stoermelder PackOne

<!-- Version and License Badges -->
![Version](https://img.shields.io/badge/version-1.9.0-green.svg?style=flat-square)
![Rack SDK](https://img.shields.io/badge/Rack--SDK-1.1.6-red.svg?style=flat-square)
![License](https://img.shields.io/badge/license-GPLv3-blue.svg?style=flat-square)
![Language](https://img.shields.io/badge/language-C++-yellow.svg?style=flat-square)

The PackOne plugin gives you some modules for [VCV Rack](https://www.vcvrack.com).

**Stable versions are released in the [VCV Library](https://library.vcvrack.com/?brand=stoermelder).  
[Nightly builds](https://github.com/stoermelder/vcvrack-packone/releases/tag/Nightly) of the latest commit are also available for all platforms. Please review the [changelog](./CHANGELOG.md) for this plugin.**

If you like my modules consider donating to https://paypal.me/stoermelder. Thank you for your support!

## Latest additions to PackOne - new in v1.9

- [8FACE mk2, +8](./docs/EightFaceMk2.md): evolution and replacement for 8FACE and 8FACEx2
- [CV-MAP CTX](./docs/CVMap.md#ctx-expander): expander-module for CV-MAP, helper for mapping parameters by context menu
- [MIDI-CAT CTX](./docs/Oscelot.md#ctx-expander): expander-module for MIDI-CAT, helper for mapping parameters by context menu
- [MIDI-PLUG](./docs/MidiPlug.md): a virtual MIDI merger and splitter
- [ORBIT](./docs/Orbit.md): a polyphonic stereo field spreader
- [STRIP-BAY](./docs/Strip.md#stoermelder-strip-bay): a companion module for STRIP for keeping input/output connections while replacing strips
- [ME](./docs/Me.md): experimental module for "mouse enhacements", provides a screen overlay for parameters changes
- Also, [MIDI-CAT](./docs/Oscelot.md) has been hugely extended with support for MIDI 14-bit CC
- Many new features for existing modules, please refer to the [changelog](./CHANGELOG.md#190)

![Intro image](./docs/intro-latest.png)

## The modules of PackOne

- [AFFIX](./docs/Affix.md), [µAFFIX](./docs/Affix.md): inserts for polyphonic cables for adding offsets in volt, semitones or octaves
- [4ROUNDS](./docs/FourRounds.md): randomizer for up to 16 input signals to create 15 output signals
- [8FACE, 8FACEx2](./docs/EightFace.md): preset sequencer for eight or sixteen presets of any module working as an universal expander
- [ARENA](./docs/Arena.md): 2-dimensional XY-Mixer for 8 sources with various modulation targets and fun graphical interface
- [BOLT](./docs/Bolt.md): polyphonic CV-modulateable boolean functions
- [CV-MAP](./docs/CVMap.md): control 32 knobs/sliders/switches of any module by CV even when the module has no CV input
- [CV-PAM](./docs/CVPam.md): generate CV voltage by observing 32 knobs/sliders/switches of any module
- [GLUE](./docs/Glue.md): label maker for your modules!
- [GOTO](./docs/Goto.md): utility for jumping directly to 10 locations in your patch by hotkey or using MIDI
- [GRIP](./docs/Grip.md): lock for module parameters
- [HIVE](./docs/Hive.md): 4 channel sequencer running on a 2-dimensional hexagonal grid, similar to [MAZE](./docs/Maze.md)
- [INFIX](./docs/Infix.md), [µINFIX](./docs/Infix.md): insert for polyphonic cables
- [INTERMIX](./docs/Intermix.md): precision adder 8x8 advanced switch matrix with support for 8 scenes
- [MACRO](./docs/Macro.md): marco-knob for four parameter-mappings and two CV outputs
- [MAZE](./docs/Maze.md): 4 channel sequencer running on a 2-dimensional grid
- [MB](./docs/Mb.md): experimental replacement for Rack's module browser, formerly available in [stoermelder's PackTau](https://github.com/stoermelder/vcvrack-packtau)
- [MEM](./docs/Oscelot.md#mem-expander): storage-expander for MIDI mapping-presets with MIDI-CAT
- [µMAP](./docs/CVMapMicro.md): a single instance of CV-MAP's slots with attenuverters
- [MIDI-CAT](./docs/Oscelot.md): map parameters to midi controllers similar to MIDI-MAP with midi feedback and note mapping
- [MIDI-MON](./docs/MidiMon.md): input-monitor for MIDI messages
- [MIDI-STEP](./docs/MidiStep.md): utility for relative modes of endless knobs on your MIDI controller such as Arturia Beatstep
- [MIRROR](./docs/Mirror.md): utility for synchronizing module parameters
- [PILE, POLY-PILE](./docs/Pile.md): utility which translates increment triggers or decrement triggers in an absolute voltage, especially useful with MIDI-STEP
- [RAW](./docs/Raw.md): a digital effect based on the dynamics of bistable systems
- [ReMOVE Lite](./docs/ReMove.md): a recorder for knob/slider/switch-automation
- [ROTOR Model A](./docs/RotorA.md): spread a carrier signal across 2-16 output channels using CV
- [SAIL](./docs/Sail.md): control any parameter currently hovered by mouse with CV, especially useful with MIDI-CC
- [SIPO](./docs/Sipo.md): serial-in parallel-out shift register with polyphonic output and CV controls
- [SPIN](./docs/Spin.md): utility for converting mouse-wheel or middle mouse-button events into triggers
- [STROKE](./docs/Stroke.md): utility which converts used-defined hotkeys into triggers or gates, also provides some special commands for Rack's enviroment
- [STRIP](./docs/Strip.md): manage a group of modules in a patch, providing load, save as, disable and randomize
- [TRANSIT](./docs/Transit.md): parameter-morpher and sequencer for up to 96 snapshots
- [X4](./docs/X4.md): dual multiple for parameter-mapping

![Intro image](./docs/intro.png)

Feel free to contact me or create a GitHub issue if you have any problems or questions!  

## Special thanks

- [Artem Leonov](https://artemleonov.bandcamp.com/) of [VCV Rack Ideas](https://www.youtube.com/channel/UCc0cXlzQdOwQSiyW30NQ7Sg) for his endless ideas, feature requests and testing, also for his great video tutorials
- [Omri Cohen](https://omricohencomposer.bandcamp.com/) for his fabulous video tutorials and fun live streams
- Andrew Belt for creating and developing VCV Rack
- All others who contributed ideas or made donations for the development of PackOne

## Building

Follow the [build instructions](https://vcvrack.com/manual/Building.html#building-rack-plugins) for VCV Rack.

## License

All **source code** is copyright © 2021 Benjamin Dill and is licensed under the [GNU General Public License, version v3.0](./LICENSE.txt).

All **files** and **graphics** in the `res` and `res-src` directories are licensed under [CC BY-NC-ND 4.0](https://creativecommons.org/licenses/by-nc-nd/4.0/). You may not distribute modified adaptations of these graphics.