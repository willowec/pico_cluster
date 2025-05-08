'''
Python script that allows for sending jobs to the cluster
'''

from pathlib import Path
from PIL import Image, ImageChops, ImageFilter
import argparse
import numpy as np

import time
import serial
import serial.tools.list_ports

import json, datetime

import matplotlib.pyplot as plt


COMMAND_TRANS_IMSTART='TRANS_IMAGE'
COMMAND_TRANS_KSTART='TRANS_KERNEL'
COMMAND_TRANS_ACK='ACK'
COMMAND_TRANS_REPEAT='REPEAT'


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument('imfile', type=Path, help='the image file to send to the cluster')
    parser.add_argument('-n', '--n_procs', type=int, default=4, choices=[1, 2, 4], help='number of picos to use')
    parser.add_argument('-p', '--port', type=str, help='com port to go to')
    parser.add_argument('-s', '--no_plot', action='store_true', help='dont plot')
    parser.add_argument('-v', '--verbose', action='store_true', help='verbose?')
    parser.add_argument('-j', '--json', action='store_true', help='save results to json')
    parser.add_argument('--filter_size', type=int, help='if set, generates a kernel of ones of that size')

    args = parser.parse_args()

    # open image and ensure always RGB
    im = Image.open(args.imfile)
    im = im.convert(mode='RGB')
    ima = np.asarray(im)

    # hardcode sharpen kernel for testing
    kernel = np.array([[0, -1, 0], [-1, 5, -1], [0, -1, 0]], dtype=np.int8)
    if args.filter_size:
        kernel = np.ones((args.filter_size, args.filter_size))
        print(f'Using generated filter!')

    # generate the ground truth
    im_gt = im.filter(ImageFilter.Kernel(kernel.shape, kernel.flatten()))

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
    with serial.serial_for_url(port, baudrate=115200, timeout=10, write_timeout=10) as ser:

        trans_times = []
        conv_times = []
        recv_times = []
        start_time = time.time_ns() / 1000 # get time in us for consistency with pico

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
        
        ser.write(f'{args.n_procs}\n'.encode('utf-8'))
        print('BOARD', ser.readline().decode(), end='')

        line = ser.readline().decode()
        while "ENDDBG" not in line:
            if args.verbose:
                print('BOARD', line, end='')
            line = ser.readline().decode()

        # right after debug we grab the times
        for i in range(args.n_procs):
            trans_times.append(int(ser.readline().rstrip()))
        for i in range(args.n_procs):
            conv_times.append(int(ser.readline().rstrip()))
        for i in range(args.n_procs):
            recv_times.append(int(ser.readline().rstrip()))
        total_head_time = int(ser.readline().rstrip())

        received = False
        while not received:
            out = ser.read(im.width*im.height*3)

            if args.verbose:
                print(f'Read {len(out)} image bytes from head node')
            
            if len(out) < im.width * im.height * 3:
                ser.write((COMMAND_TRANS_REPEAT + '\n').encode('utf-8'))
            else:
                received = True
        
        if args.verbose:
            print("Acknowledging complete")
        
        ser.write((COMMAND_TRANS_ACK + '\n').encode('utf-8'))
        out = np.frombuffer(out, dtype=np.int8).copy()

        total_time = int((time.time_ns() / 1000) - start_time)

        # clear last line to match padding
        out[-im.width*3:] = 0

        if not args.no_plot:
            f, ax = plt.subplots(2, 2)

            im_out = Image.fromarray(out.reshape((im.height, im.width, 3)), 'RGB')

            ax[0][0].imshow(im)
            ax[0][0].set_title('input')

            ax[0][1].imshow(im_out)
            ax[0][1].set_title('output')

            ax[1][0].imshow(ImageChops.difference(im_gt, im_out))
            ax[1][0].set_title('out - gt')

            ax[1][1].imshow(im_gt)
            ax[1][1].set_title('ground truth')

            # disable ticks
            for a in ax.flatten():
                a.get_yaxis().set_visible(False)
                a.get_xaxis().set_visible(False)

            plt.tight_layout()
            plt.show()


        print('Trans times')
        for i in range(args.n_procs):
            print(trans_times[i])
        print('Conv times')
        for i in range(args.n_procs):
            print(conv_times[i])
        print('Recv times')
        for i in range(args.n_procs):
            print(recv_times[i])

        print('Total head time')
        print(total_head_time)
        print('Total time')
        print(total_time)

        if args.json:   
            data = {}
            data['trans_times'] = trans_times
            data['conv_times'] = conv_times
            data['recv_times'] = recv_times
            data['head_time'] = total_head_time
            data['total_time'] = total_time

            data['kernel_dims'] = list(kernel.shape)
            data['image_dims'] = [im.width, im.height]

            data['n_procs'] = args.n_procs

            out_dir = Path('out')
            out_dir.mkdir(exist_ok=True, parents=True)

            out_dir.joinpath(datetime.datetime.now().isoformat().split('.')[0].replace(':', '-')).write_text(json.dumps(data))
