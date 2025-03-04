# JetKVM compatible audio ADC
This project takes in mic/line level audio from the 3.5mm jack and digitizes it using a Raspberry Pi Pico 2.

The audio is sent through the serial interface in a modified 48kHz 16-bit 2-channel PCM format. The audio is sent in packets of 32768 samples each, with a `0xFFFF` stop character at the end to ensure left/right channel alignment.

The JetKVM reads its serial interface from `/dev/ttyS3` and sends the raw byte packets though the network to a client PC running a python script (`test_piping.py`) that decodes the packets into a standard PCM audio stream.

Example client command: `ssh root@jetkvm cat /dev/ttyS3 | python test_piping.py | ffplay -f s16le -ac 2 -ar 48k -`.
