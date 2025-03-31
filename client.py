'''
Python script that allows for sending jobs to the cluster
'''

from pathlib import Path
from PIL import Image
import argparse
import numpy as np

import serial
import serial.tools.list_ports

COMMAND_TRANS_IMSTART='TRANS_IMAGE'

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument('imfile', type=Path, help='the image file to send to the cluster')
    parser.add_argument('-p', '--port', type=str, help='com port to go to')

    args = parser.parse_args()


    # open image and ensure always RGB
    im = Image.open(args.imfile)
    im = im.convert(mode='RGB')
    ima = np.asarray(im)

    # look for ports
    ports = [tuple(p)[0] for p in list(serial.tools.list_ports.comports())]
    if len(ports) == 0:
        print("no COM ports detected")
        exit(1)
    
    # set port
    port = ports[0]
    if args.port:
        port = args.port

    print(f'Sending {args.imfile.name} to cluster at {port}')


    # connect to the PMI
    with serial.Serial(port, baudrate=115200, timeout=10) as ser:
        # tell cluster we are about to send an image
        ser.write((COMMAND_TRANS_IMSTART + '\n').encode('utf-8'))

        # send image size as a string for convenience (its variable length)
        ser.write(f'{np.prod(ima.shape)}\n'.encode('utf-8'))

        # send image in rgb rgb rgb rgb pixel by pixel
        ser.write(ima.flatten().tobytes())
        
        # now read responses line by line
        while 1:
            print(ser.read_until())
