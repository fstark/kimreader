# kimreader
Recovers content of KIM-1 save tapes from audio recordings.

Current features:

Reads 44KHz unisgned 8 bits .wav files containing a single program for KIM-1.
Diplays timestamps for damaged parts.
Have  a simple re-sample/normalize/smooth function that can help reading damaged parts.
Reads the content replacing the unreadable bits by optional user-specified values.
Displays the text content of the KIM file.

Limitations:

Does not work on non 44KHz unsigned 8 bits wav files
If the SYN header is damaged, the text content will not be recovered. Using ``silent false`` may help to see the bitstream.
When using the code try to apply correction past the end of the tape

Potential future work:

Computing and checking checksums
Enumerating bit combination to find missing data
Working on multiple files
More robust on defect in header
Generating a correct wav file.

Usage:

Use ``kimreader --help`` for command-line help.

About the smooth argument: using ``--smooth 30`` will rescale every sample into a 0-255 range according to the min/max and average in a surrounding window of (for instance) 71 samples. This enables signals that are low and uncentered to be recognised as crossing the 128 line. This is badly coded and fundamentally makes the software 30 times slower.

