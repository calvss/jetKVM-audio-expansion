import sys
import struct

if __name__ == "__main__":
    # discard bytes until we hit 0xffff
    # we use 0xffff as the stop character between packets
    while True:
        while(sys.stdin.buffer.read(1) != b'\xff'):
            pass

        if(sys.stdin.buffer.read(1) == b'\xff'):
            # 0xffff was reached, stop discarding bytes
            break;
        else:
            # just a random isolated 0xff, keep discarding bytes
            pass

    # keep track of which channel we're working on, since samples are interleaved L-R-L-R...
    leftChannel = True
    fourBytes = [None] * 4
    while(twoBytes := sys.stdin.buffer.read(2)):
        if(len(twoBytes) < 2):
            # if we hit the end of the file
            break
        elif(twoBytes == b'\xff\xff'):
            # if we hit the end of a packet, skip
            continue
        else:
            # alternate reading left and right channel (interleaved)
            if(leftChannel):
                leftChannel = False
                fourBytes[0:2] = twoBytes
            else:
                leftChannel = True
                fourBytes[2:4] = twoBytes

                # once we have the left and right channel, assemble the frame
                left_int = int.from_bytes(fourBytes[0:2], byteorder='little')
                right_int = int.from_bytes(fourBytes[2:4], byteorder='little')

                sys.stdout.buffer.write(struct.pack('<HH',left_int, right_int))
            
    # # read 4 bytes at a time, since each frame is two 16-bit integers
    # # stereo 16-bit PCM encoding
    # while(fourBytes := rawFile.read(4)):
    #     if(len(fourBytes) < 4):
    #         # if we hit the end of the file
    #         break
    #     left_int = int.from_bytes(fourBytes[0:2], byteorder='big')
    #     right_int = int.from_bytes(fourBytes[2:4], byteorder='big')

    #     waveFile.writeframesraw(struct.pack('<HH',left_int, right_int))
