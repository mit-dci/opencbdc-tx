import matplotlib.pyplot as plt
import numpy as np
import os.path
import glob
import sys

# Usage: python plot.py <TEST DIRECTORY>
# generate plots from tx_samples
# list<int>, Axes -> void
def plot_latency(fname, fig=None, ax1=None, ax2=None):
    x = []
    y = []
    th_moving_avg = []
    rates = []
    tx_vals = []
    fresh = False  # is this the plot of all data, or just a single plot

    if (fig == None):  # create new axes if necessary
        fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(12, 5))
        fig.suptitle(fname+" performance data")
        fresh = True
    local_file = fname.split('/')
    local_file = local_file[-1].split('_')
    id = local_file[-1].split('.')[0]
    nm = 'loadgen_' + id

    # get data from file
    data = read_in_data(fname)
    # first sample in file is reference time 0
    time_start = int(data[0].split()[0])

    queue = []
    queue_max = 15

    t_prev = 0

    # Format data for plotting
    for i in range(len(data)):
        d = data[i].split()
        if (len(d) < 2):
            break
        a = data[i].split()
        x.append((int(a[0]) - time_start)/10**9)
        y.append(int(a[1])/10**9)
        if (x[i] - x[t_prev] > 1):
            tx_vals.append(x[i])
            rates.append(i-t_prev)
            th_moving_avg.append(np.mean(rates))
            t_prev = i

    # get line of best fit
    f1, f2 = np.polyfit(x, y, 1)
    f1 = round(f1, 3)
    f2 = round(f2, 3)

    # plot latency data
    ax2.set_title("Tx delay (s) vs time since start (s)")
    string = nm + ': data'
    ax2.plot(x, y, label=string)
    sign = '+ ' if f2 > 0 else ''
    string = "Line of best fit: " + str(f1) + "(sec) " + sign + str(f2)
    string = nm + ': ' + string
    ax2.plot(np.array(x), f1*np.array(x)+f2, label=string)
    ax2.legend(loc="upper right")
    ax2.set(xlabel="Time (s)", ylabel="Latency (s)")

    # plot throughput data
    ax1.set_title("Throughput (TX/s) vs. time (s)")
    ax1.plot(tx_vals, rates, label="Throughput")
    ax1.plot(tx_vals,  th_moving_avg, label="(Moving) Average Throughput")
    ax1.legend(loc="upper right")
    ax1.set(xlabel="Time (s)", ylabel="Throughput (TX/s)")
    if (fresh):
        fig.savefig(nm + "_performance.png")

# get data from file
def read_in_data(fname):
    if (not os.path.isfile(fname)):
        raise Exception("Cannot find file " + fname)
    fin = open(fname, "r")
    data = fin.readlines()
    fin.close()
    return data


if __name__ == '__main__':
    path = "."
    # Get path to test data
    if (len(sys.argv) > 1):
        path = str(sys.argv[1])
    f_list = glob.glob(path + '/tx_samples_*.txt')
    global_fig, global_axs = plt.subplots(1, 2, figsize=(12, 5))
    for fin in f_list:
        plot_latency(fin)
        plot_latency(fin, global_fig, global_axs[0], global_axs[1])
    global_fig.savefig(path + "/aggregate_performance.png")
