'''
Python script that allows for sending jobs to the cluster
'''

from pathlib import Path
from PIL import Image, ImageChops
import argparse
import numpy as np

import time
import serial
import serial.tools.list_ports

import matplotlib.pyplot as plt

COMMAND_TRANS_IMSTART='TRANS_IMAGE'
COMMAND_TRANS_KSTART='TRANS_KERNEL'
COMMAND_TRANS_ACK='ACK'
COMMAND_TRANS_REPEAT='REPEAT'

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument('imfile', type=Path, help='the image file to send to the cluster')
    parser.add_argument('-p', '--port', type=str, help='com port to go to')

    args = parser.parse_args()


    # open image and ensure always RGB
    im = Image.open(args.imfile)
    im = im.convert(mode='RGB')
    ima = np.asarray(im)

    # hardcode sharpen kernel for testing
    kernel = np.array([[0, -1, 0], [-1, 5, -1], [0, -1, 0]], dtype=np.int8)

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
    with serial.serial_for_url(port, baudrate=115200, timeout=1, write_timeout=5) as ser:

        # tell cluster we are about to send an image
        ser.write((COMMAND_TRANS_IMSTART + '\n').encode('utf-8'))

        # send image width and heights as strings (the head node knows that the color channel is always 3)
        ser.write(f'{ima.shape[1]}\n'.encode('utf-8')) # width
        ser.write(f'{ima.shape[0]}\n'.encode('utf-8')) # height

        # send image in rgb rgb rgb rgb pixel by pixel
        ser.write(ima.flatten().tobytes())

        print('BOARD', ser.readline().decode(), end='')

        # tell the cluster we are about to send a kernel
        ser.write((COMMAND_TRANS_KSTART + '\n').encode('utf-8'))

        # send kernel w, h
        ser.write(f'{kernel.shape[1]}\n'.encode('utf-8')) # width
        ser.write(f'{kernel.shape[0]}\n'.encode('utf-8')) # height

        # send kernel data
        ser.write(kernel.flatten().astype(dtype=np.int8).tobytes())

        print('BOARD', ser.readline().decode(), end='')
        print('BOARD', ser.readline().decode(), end='')
        print('BOARD', ser.readline().decode(), end='')
        print('BOARD', ser.readline().decode(), end='')

        received = False
        while not received:
            out = ser.read(im.width*im.height*3)
            print(f'Read {len(out)} image bytes from head node')
            if len(out) < im.width * im.height * 3:
                ser.write((COMMAND_TRANS_REPEAT + '\n').encode('utf-8'))
            else:
                received = True
        
        print("Acknowledging complete")
        ser.write((COMMAND_TRANS_ACK + '\n').encode('utf-8'))
        out = np.frombuffer(out, dtype=np.int8)

        f, ax = plt.subplots(2, 2)

        im_out = Image.fromarray(out.reshape((im.height, im.width, 3)), 'RGB')

        ax[0][0].imshow(im)
        ax[0][1].imshow(im_out)
        ax[1][0].imshow(ImageChops.difference(im, im_out))
        plt.show()

    print('closed')
