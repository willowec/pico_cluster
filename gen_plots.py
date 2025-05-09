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
    fig, ax = plt.subplots(figsize=(12, 4))

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


def plot_procs(datas: list[dict]):
    '''
    produce a plot across the n_procs
    '''

    # get each proc count that exists in datas
    procs_set = set()
    for data in datas:
        procs_set.add(data['n_procs'])

        # and also np array it all
        for k in data.keys():
            if isinstance(data[k], list):
                data[k] = np.array(data[k])
    procs_set = list(procs_set)

    # 1. average data per point
    procs_dict = {}
    for procs in procs_set:
        for data in datas:
            if data['n_procs'] == procs:
                if procs not in procs_dict.keys():
                    # create entry if not exist
                    procs_dict[procs] = data
                    procs_dict[procs]['count'] = 1
                else:
                    # add to entry if exist
                    procs_dict[procs]['count'] += 1
                    procs_dict[procs]['head_time'] += data['head_time']
                    procs_dict[procs]['total_time'] += data['total_time']
                    procs_dict[procs]['trans_times'] += data['trans_times']
                    procs_dict[procs]['conv_times'] += data['conv_times']
                    procs_dict[procs]['recv_times'] += data['recv_times']

        # div to mean each
        procs_dict[procs]['head_time'] /= procs_dict[procs]['count']
        procs_dict[procs]['total_time'] /= procs_dict[procs]['count']
        procs_dict[procs]['trans_times'] = (procs_dict[procs]['trans_times'] / procs_dict[procs]['count'])
        procs_dict[procs]['conv_times'] = (procs_dict[procs]['conv_times'] / procs_dict[procs]['count'])
        procs_dict[procs]['recv_times'] = (procs_dict[procs]['recv_times'] / procs_dict[procs]['count'])

    # 1.5 print some results
    for proc in procs_dict.keys():
        print(f"proc: {proc} headtime={procs_dict[proc]['head_time']}, runtime={procs_dict[proc]['total_time']}")

    # 2. produce (labeled?) scatter
    f, ax = plt.subplots(1, 3, figsize=(12, 5))
    for i, procs in enumerate(procs_set):
        ax[0].scatter([i for x in range(procs)], procs_dict[procs]['trans_times'])
        ax[1].scatter([i for x in range(procs)], procs_dict[procs]['conv_times'])
        ax[2].scatter([i for x in range(procs)], procs_dict[procs]['recv_times'])
    
    ax[0].set_xticks(range(len(procs_set)), labels=[f'{p} Picos' for p in procs_set])
    ax[1].set_xticks(range(len(procs_set)), labels=[f'{p} Picos' for p in procs_set])
    ax[2].set_xticks(range(len(procs_set)), labels=[f'{p} Picos' for p in procs_set])
    ax[0].set_ylim(bottom=0)
    ax[1].set_ylim(bottom=0)
    ax[2].set_ylim(bottom=0)
    ax[0].set_title('(a) Time transmitting')
    ax[1].set_title('(b) Time computing')
    ax[2].set_title('(c) Time retrieving')

    f.supylabel('Time (microseconds)')

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

        datas = []
        for p in args.path.iterdir():
            datas.append(json.loads(p.read_text()))

        plot_procs(datas)
