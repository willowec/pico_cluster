'''
gens some plots for the presentation/paper
'''

import matplotlib.pyplot as plt
import numpy as np

import json, argparse
from pathlib import Path


def plot_nprocs(runs):
    head_times = []
    total_times = []
    conv_times_times = []
    trans_recv_times_times = []


    n_procs = []

    for i, run in enumerate(runs):
        n_procs.append(run['n_procs'])
        head_times.append(run['head_time'] / 1e6)
        total_times.append(run['total_time'] / 1e6)

        conv_times_times.append(run['conv_times'])
        conv_times_times[i] = [x / 1e6 for x in conv_times_times[i]]

        trans_recv_times_times.append([(x + y) / 1e6 for x, y in zip(run['trans_times'], run['recv_times'])])


    f, ax = plt.subplots(2, 2)
    ax[0][0].plot(n_procs, head_times)
    ax[0][0].set_ylim([0, max(head_times) + 1])
    ax[0][0].set_title('total head node time')

    for i, procs in enumerate(n_procs):
        print(procs, conv_times_times)
        ax[0][1].scatter([procs for p in range(procs)], conv_times_times[i])
    ax[0][1].set_title('convolution times')

    for i, procs in enumerate(n_procs):
        print(procs, trans_recv_times_times)
        ax[1][1].scatter([procs for p in range(procs)], trans_recv_times_times[i])
    ax[1][1].set_title('i2c communication times')



    ax[1][0].plot(n_procs, total_times)
    ax[1][0].set_ylim([0, max(total_times) + 1])
    ax[1][0].set_title('total run time')

    f.supxlabel('n picos')
    f.suptitle('3x3 sharpen filter over a 160x200 image, varying n picos')
    f.tight_layout()
    plt.show()




if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument('--data_dir', type=Path, default=Path('out'), help='path of the data directory')
    parser.add_argument('--explore', action='store_true', help='print info')
    parser.add_argument('--plot_nprocs', action='store_true')
    parser.add_argument('--k_size', type=int, help='if specified, only consider runs with this kernel size')
    parser.add_argument('--im_width', type=int, help='if specified, only consider runs with this image width')

    args = parser.parse_args()

    # load runs from the data dir
    runs = []
    for jfile in args.data_dir.iterdir():
        run = json.loads(jfile.read_text())

        if args.k_size:
            if run['kernel_dims'][0] != args.k_size:
                continue

        if args.im_width:
            if run['image_dims'][0] != args.im_width:
                continue

        runs.append(run)

    if args.explore:
        for run in runs:
            print(f"{run['kernel_dims']}, {run['image_dims']}", end='')
            if 'n_procs' in run.keys():
                print(f" n_procs={run['n_procs']}", end='')
            print('')


    if args.plot_nprocs:
        plot_nprocs(runs)