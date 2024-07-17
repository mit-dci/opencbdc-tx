import sys
import os.path
import glob
import argparse
import matplotlib.pyplot as plt
import numpy as np


def parse_args():
    '''
    Allow user to specify the directory containing the performance data
    example usage: python plot-samples.py <TEST DIRECTORY>
    generates plots for all tx_samples in the specified directory
    '''
    parser = argparse.ArgumentParser(description=
                                     'Plot performance data from tx_samples')
    # help message for the directory argument
    parser.add_argument('-d', '--dir', dest='tests_dir',
                        action='store', default='.', type=str,
                        help='Directory containing performance data')
    return parser.parse_args()


def plot_latency(fname, fig=None, ax1=None, ax2=None):
    '''
    Plot the throughput and latency data from a file in the
    tx_samples directory - called by scripts/native-system-benchmark.sh
    # list<int>, Axes -> void
    '''
    x, y, th_moving_avg, rates, tx_vals = [], [], [], [], []
    fresh = False  # is this the plot of all data, or just a single plot

    if not fig: # create new axes if necessary
        fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(12, 5))
        fig.suptitle(fname+" performance data")
        fresh = True

    local_file = fname.split('/')[-1].split('_')
    filename = f"loadgen_{local_file[-1].split('.')[0]}"

    # get data from file
    data = read_in_data(fname)
    # first sample in file is reference time 0
    time_start = int(data[0].split()[0])

    # Format data for plotting
    t_prev = 0
    for idx, line in enumerate(data):
        d = line.split()
        if len(d) < 2:
            break
        a = line.split()
        x.append((int(a[0]) - time_start)/10**9)
        y.append(int(a[1])/10**9)
        if x[idx] - x[t_prev] > 1:
            tx_vals.append(x[idx])
            rates.append(idx - t_prev)
            th_moving_avg.append(np.mean(rates))
            t_prev = idx

    # get line of best fit
    f1, f2 = np.polyfit(x, y, 1)
    f1 = round(f1, 3)
    f2 = round(f2, 3)

    # plot latency data
    ax2.set_title("Tx delay (s) vs time since start (s)")
    ax2.plot(x, y, label=f'{filename}: data')
    sign = '+ ' if f2 > 0 else ''
    label = f"{filename}: Line of best fit: {f1}(sec) {sign}{f2}"
    ax2.plot(np.array(x), f1*np.array(x)+f2, label=label)
    ax2.legend(loc="upper right")
    ax2.set(xlabel="Time (s)", ylabel="Latency (s)")

    # plot throughput data
    ax1.set_title("Throughput (TX/s) vs. time (s)")
    ax1.plot(tx_vals, rates, label="Throughput")
    ax1.plot(tx_vals,  th_moving_avg, label="(Moving) Average Throughput")
    ax1.legend(loc="upper right")
    ax1.set(xlabel="Time (s)", ylabel="Throughput (TX/s)")
    if fresh:
        fig.savefig(f"{filename}_performance.png")


def read_in_data(fname) -> list:
    '''
    get data from file and return as a list of lines
    '''
    if not os.path.isfile(fname):
        print(f'File {fname} does not exist')
        sys.exit(1)

    lines = []
    try:
        with open(fname, 'r') as f:
            lines = f.readlines()
    except IOError as e:
        print(f'Error reading from file {fname}\n{e}\n')
        sys.exit(1)

    return lines


if __name__ == '__main__':

    args = parse_args()
    tests_dir = args.tests_dir

    # Get all tx sample files in the test directory
    f_list = glob.glob(f'{tests_dir}/tx_samples_*.txt')
    if not f_list:
        print(f'No tx_samples files found in {tests_dir = }')
        sys.exit(1)

    global_fig, global_axs = plt.subplots(1, 2, figsize=(12, 5))

    for file in f_list:
        plot_latency(file)
        plot_latency(file, global_fig, global_axs[0], global_axs[1])

    global_fig.savefig(f'{tests_dir}/aggregate_performance.png')
