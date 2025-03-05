import wave
import sys
import struct

if __name__ == "__main__":
    with open("test_crab.bin","rb") as rawFile:
        with wave.open("test_output.wav", 'wb') as waveFile:

            waveFile.setnchannels(2) # stereo
            waveFile.setsampwidth(2) # 16-bit PCM
            waveFile.setframerate(48000)
            

            # discard bytes until we hit 0xffff
            # we use 0xffff as the stop character between packets
            while True:
                while(rawFile.read(1) != b'\xff'):
                    pass

                if(rawFile.read(1) == b'\xff'):
                    # 0xffff was reached, stop eating bytes
                    break;
                else:
                    # just a random isolated 0xff, keep eating bytes
                    pass

            leftChannel = True
            fourBytes = [None] * 4
            while(twoBytes := rawFile.read(2)):
                if(len(twoBytes) < 2):
                    # if we hit the end of the file
                    break
                elif(twoBytes == b'\xff\xff'):
                    # if we hit the end of a packet, skip it
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
                        
                        if(left_int >= 4095 or right_int >= 4095):
                            print(left_int, right_int, file=sys.stderr)
                        

                        try:
                            waveFile.writeframesraw(struct.pack('<hh',((left_int-2048)*16), ((right_int-2048)*16)))
                            print("w", file=sys.stderr)
                        except Exception as e:
                            print(e, file=sys.stderr)
                            print(left_int, right_int, file=sys.stderr)

                        # waveFile.writeframesraw(struct.pack('<HH',left_int, right_int))
                    
            # # read 4 bytes at a time, since each frame is two 16-bit integers
            # # stereo 16-bit PCM encoding
            # while(fourBytes := rawFile.read(4)):
            #     if(len(fourBytes) < 4):
            #         # if we hit the end of the file
            #         break
            #     left_int = int.from_bytes(fourBytes[0:2], byteorder='big')
            #     right_int = int.from_bytes(fourBytes[2:4], byteorder='big')

            #     waveFile.writeframesraw(struct.pack('<HH',left_int, right_int))
