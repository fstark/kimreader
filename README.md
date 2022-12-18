# kimreader
Recovers content of KIM-1 save tapes from audio recordings.


## What is this thing?

This is a tool developed by the [MO5.com association](https://mo5.com/site/) to preserve some old KIM-1 software stored on damaged tapes.

If you have an unreadable KIM-1 tape, you may be able to recover it, by using the following steps:

* Compile ``kimreader`` using any C++17 compiler (no dependencies, just type ``make``).

* Extract the program you want to recover in a 8bits mono unsigned wav file. This file needs to contain from the header 'till the end of the recording.

* Use the command ``kimreader TAPE.WAV`` to try to recover the file. If it succeeds, the (hexdecimal) content will be printed on screen.

* If the preceding step is unsucessful, you can try to use the (slow) ``--smooth `` option, using something like ``--smooth 30`` or ``--smooth 50``. This will rescale the input and help recevoering in some cases.

If you have a tape that you cannot recover, enter an issue in ``kimreader``, I'll try to help you recover it.

## Current features:

Reads 44KHz unisgned 8 bits .wav files containing a single program for KIM-1.
Diplays timestamps for damaged parts.
Have  a simple re-sample/normalize/smooth function that can help reading damaged parts.
Reads the content replacing the unreadable bits by optional user-specified values.
Displays the text content of the KIM file.

## Limitations:

Does not work on non 44KHz unsigned 8 bits wav files
If the SYN header is damaged, the text content may not be recovered. Using ``silent false`` may help to see the bitstream.

## Potential future work:

Working on multiple files
Generating a correct wav file.

## Some note of usage

Use ``kimreader --help`` for command-line help.

About the smooth argument: using ``--smooth 30`` will rescale every sample into a 0-255 range according to the min/max and average in a surrounding window of (for instance) 71 samples. This enables signals that are low and uncentered to be recognised as crossing the 128 line. This is badly coded and fundamentally makes the software 30 times slower.

Using ``--silent false`` option you can see the bitstream ``kimreader`` recovered (sometimes kimdreader can recover the bitstream but not turn it into a working kim tape)

## Notes on kim-1 tapes

bits are stored little endian
3 cycles per bits, cycles are 996 (0) or 966 (1)
-> each bits ends up in a 6->9 transition

recording starts with 100 times 01101000
00010110

0x16


