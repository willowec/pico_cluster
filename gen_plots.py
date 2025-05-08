'''
gens some plots for the presentation/paper
'''

import matplotlib.pyplot as plt
import numpy as np

import json, argparse
from pathlib import Path




def plot_timeline(data: dict):
    '''
    plot an execution timeline
    '''
    n_procs = data['n_procs']
    trans = np.array(data['trans_times']) / 1e6
    convs = np.array(data['conv_times']) / 1e6
    recvs = np.array(data['recv_times']) / 1e6

    # we will use a broken barh to make a timeline
    fig, ax = plt.subplots(figsize=(10, 4))

    procs_transrange = []
    procs_convsrange = []
    procs_recvsrange = []

    # create start/stop tuples for each proc
    for i in range(n_procs):
        procs_transrange.append((sum(trans[0:i]), trans[i]))
        procs_convsrange.append((sum(trans[0:i])+trans[i], convs[i]))
        procs_recvsrange.append((sum(trans)+sum(recvs[0:i]), recvs[i]))

    # plot trans times
    for i in range(n_procs):
        ax.broken_barh([procs_transrange[i]], (i-.5, 1), color='orange')
        ax.broken_barh([procs_convsrange[i]], (i-.5, 1), color='blue')
        ax.broken_barh([procs_recvsrange[i]], (i-.5, 1), color='green')


    ax.set_yticks(range(n_procs), labels=[f'Pico {p}' for p in range(n_procs)])
    ax.set_xlabel('time (seconds)')
    ax.legend(['trans job', 'compute', 'trans results'], loc='upper left')

    plt.tight_layout()
    plt.show()



if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument('path', type=Path, help='path to the directory or individual j file to plot from')
    parser.add_argument('--timeline', action='store_true', help='plot the timeline (path must be one json file)')
    parser.add_argument('--procs', action='store_true', help='plot the n_procs variance')
    args = parser.parse_args()

    if args.timeline:
        assert args.path.is_file()
        plot_timeline(json.loads(args.path.read_text()))

    if args.procs:
        assert args.path.is_dir()
