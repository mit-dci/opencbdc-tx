import matplotlib
import struct
import itertools
matplotlib.use('Agg')
from genericpath import exists
from os import listdir, environ, remove
from os.path import isdir, isfile, join
import random
import sys
import json
import matplotlib.pyplot as plt
from matplotlib.ticker import PercentFormatter
import numpy as np
import vaex
from vaex import BinnerTime
import pandas
import datetime
import matplotlib.dates as mdates

# Ensure this matches the variable TestResultVersion at the
# top of coordinator/testruns/testruns.go
version = 2
colors_hex = ['000000','DDCC77', '332288','88CCEE','44AA99', '117733', '999933', 'CC6677', '882255', 'AA4499']
colors = [(tuple(float(int(c[idx:idx+2], 16)/255) for idx in (0, 2, 4))) for c in colors_hex]
event_names = [
    'unknown',
    'controller.server_handler.tx_notify',
    'raft_node.tx_notify',
    'raft_node.send_complete_txs',
    'state_machine.tx_notify',
    'atomizer.insert_complete.success',
    'atomizer.insert_complete.discard.expired',
    'atomizer.insert_complete.discard.spent',
    'atomizer.make_block',
]



class MyBinnerTime(BinnerTime):
    def __init__(self, expression, resolution='W', df=None, every=1):
        self._promise = vaex.promise.Promise.fulfilled(None)
        self.every = every
        self.resolution = resolution
        self.expression = expression
        self.df = df or expression.ds
        self.sort_indices = None
        # make sure it's an expression
        self.expression = self.df[str(self.expression)]
        self.tmin, self.tmax = self.df[str(self.expression)].min(), self.df[str(self.expression)].max()

        self.label = ''

        self.resolution_type = 'M8[%s]' % self.resolution
        dt = (self.tmax.astype(self.resolution_type) - self.tmin.astype(self.resolution_type))
        self.N = (dt.astype(int).item() + 1)
        # divide by every, and round up
        self.N = (self.N + every - 1) // every
        self.bin_values = np.arange(self.tmin.astype(self.resolution_type), self.tmax.astype(self.resolution_type)+1, every)
        # TODO: we modify the dataframe in place, this is not nice
        self.begin_name = self.df.add_variable('t_begin', self.tmin.astype(self.resolution_type), unique=True)
        # TODO: import integer from future?
        self.binby_expression = str(self.df['%s - %s' % (self.expression.astype(self.resolution_type), self.begin_name)].astype('int') // every)
        self.binner = self.df._binner_ordinal(self.binby_expression, self.N)

one_sec = 10**9

def read_throughput_sample_file(file):
    samples = []
    failed_values = 0
    with open(file, encoding="ISO-8859-1") as f:
        for line in f:
            try:
                val = float(line[:-1])
                samples.append(val)
            except:
                failed_values = failed_values + 1
    if failed_values > 10:
        raise "Too many failed values in throughput file {}".format(file)
    return samples

def read_latency_sample_file(file):
    samples = []
    failed_values = 0
    with open(file, encoding="ISO-8859-1") as f:
        for line in f:
            try:
                val = float(line[:-1])
                samples.append(val/one_sec)
            except:
                failed_values = failed_values + 1
    if failed_values > 10:
        raise "Too many failed values in throughput file {}".format(file)
    return samples

lats = []

def make_series_line(its, min_time, time_f, val_f):
    current = min_time #tps_its[0][1][0].astype(datetime.datetime)
    end = time_f(its, -1)
    tps = []
    idx = 0

    while current < end:
        dt = time_f(its, idx)
        if dt != current:
            tps.append(0)
        else:
            tps.append(val_f(its, idx))
            idx += 1
        current += datetime.timedelta(seconds=1)

    return tps

def make_time_series(times_dt, client_cnt, lines, yaxis_title, plot_title, ma, out):
    fig, (ax) = plt.subplots(nrows=1, figsize=(10,12))
    totals = []
    max = 0
    lns = []
    tps_ma_ms = 0
    for i, line in enumerate(lines):
        if len(line["values"]) == 0: continue
        #totals.append({"counter":line["title"], "amount":np.sum(line["values"])})
        tps_time = []
        tps_ma = []
        time = 0
        tps_ma_tmp = []
        tps_ma_ms = line["freq"] * 5000

        for t in line["values"]:
            if ma:
                tps_ma_tmp.append(t)
                while len(tps_ma_tmp) > 5:
                    tps_ma_tmp = tps_ma_tmp[1:]

                val = np.mean(tps_ma_tmp)
                if max < val:
                    max = val
                tps_ma.append(np.mean(tps_ma_tmp))
            else:
                if max < t:
                    max = t
                tps_ma.append(t)


            time = time + line["freq"]
            tps_time.append(datetime.datetime.fromtimestamp(time))

        color = (random.random(), random.random(), random.random())
        if len(colors) > i+1:
            color = colors[i+1]
        lns.append(ax.plot(tps_time, tps_ma, label=line["title"], color=color))

    if len(times_dt) > 0:
        ax2 = ax.twinx()
        lns.append(ax2.plot(times_dt, client_cnt, label="Number of clients", color=colors[0]))
        ax2.set_ylim(ymin=0)
        ax2.set_ylabel('Number of clients')

    timeFmt = mdates.DateFormatter('%M:%S')
    ma_title = ''
    if ma:
         ma_title = '({}ms moving average)'.format(tps_ma_ms)
    ax.set_ylabel('{} {}'.format(yaxis_title, ma_title))
    ax.set_xlabel('Time (mm:ss)')
    ax.set_title(plot_title)
    ax.set_ylim(ymin=0, ymax=max)
    ax.grid()

    ax.xaxis.set_major_formatter(timeFmt)
    ax.set_xlim(xmin=datetime.datetime.fromtimestamp(0))

    # Shrink current axis by 20%
    box = ax.get_position()
    ax.set_position([box.x0 + box.width * .05, box.y0 + box.height * 0.3, box.width * 0.9, box.height * 0.7])

    lns = [l[0] for l in lns]
    labs = [l.get_label() for l in lns]
    ax.legend(lns, labs, loc='upper center', bbox_to_anchor=(0.5, -0.1),
          fancybox=True, shadow=True)

    plt.savefig(out)
    plt.close('all')

def calculate_results(folder):
    (timestamp, commit) = folder.split('-')
    test_date = datetime.datetime.fromtimestamp(int(timestamp))
    test_commit = commit

    print('Test date: {} - Commit: {}'.format(test_date, test_commit))
    output_files = [f for f in listdir(folder) if isfile(join(folder, f))]

    global_min_time = 0
    if exists(join(folder, 'client-count.log')):
        p = pandas.read_csv(join(folder, 'client-count.log'), sep=' ', error_bad_lines=True, warn_bad_lines=True, names=['time', 'clients'], encoding="ISO-8859-1")
        global_min_time = p['time'][0]

    min_time = datetime.datetime.utcfromtimestamp(global_min_time/1000000000).replace(microsecond=0)

    v = vaex.from_dict({'e':[-1], 't':[1000000000000000], 'l':[2000000], 'c':[1]})
    # Read event-samples
    event_sample_struct = struct.Struct('<bqqQ')
    for es_file in [join(folder,f) for f in output_files if 'event_sampler_' in f and '.bin' in f]:
        print("Reading {}".format(es_file))
        esf = open(es_file, "rb")
        while True:
            chunk = esf.read(50000000)
            if not chunk or len(chunk) < 25:
                break
            events = itertools.islice(event_sample_struct.iter_unpack(chunk), int(len(chunk)/25))
            df = pandas.DataFrame(events, columns=['e','t','l','c'])
            vchunk = vaex.from_pandas(df)
            v = v.concat(vchunk)
        esf.close()

    v['pDate'] = v.t.values.astype('datetime64[ns]')

    tps_lines = []
    lat_lines = []
    tps_lines_allow = []
    should_include_event_in_tps = lambda i : True
    if 'FILTERED_TIME_SERIES_EVENTS' in environ:
        tps_lines_allow = eval(environ['FILTERED_TIME_SERIES_EVENTS'])
        should_include_event_in_tps = lambda i : (i in tps_lines_allow)

    event_types = v.e.unique()
    event_types.sort()
    for t in event_types:
        filtered_v = v[v.e == t]
        dat = filtered_v.groupby(by=MyBinnerTime(expression=filtered_v.pDate, resolution='s', df=filtered_v), agg={'c': 'sum', 'l': 'mean'})
        its = dat.to_items()
        tps = make_series_line(its, min_time, (lambda its,idx: its[0][1][idx].astype(datetime.datetime)), (lambda its,idx: its[1][1][idx]))
        lat = make_series_line(its, min_time, (lambda its,idx: its[0][1][idx].astype(datetime.datetime)), (lambda its,idx: its[2][1][idx]))
        if should_include_event_in_tps(t):
            tps_lines.append({"values":tps, "title":"Event {}".format(event_names[t]), "freq": 1})
        lat_lines.append({"values":lat, "title":"Event {}".format(event_names[t]), "freq": 1})

    two_phase = False
    for output_file in output_files:
        if 'tx_samples' in output_file:
            two_phase = True

    if two_phase:
        hdf5_files = [join(folder, x) for x in listdir(folder) \
                if 'tx_samples' in x and 'hdf5' in x]

        for hdf in hdf5_files:
            remove(hdf)

        files = [join(folder,x) for x in listdir(folder) \
                if 'tx_samples' in x and 'hdf5' not in x]

        for f in files:
            print('Reading {}'.format(f))
            p = pandas.read_csv(f, sep=' ', error_bad_lines=True, warn_bad_lines=True, names=['time', 'latency'], encoding="ISO-8859-1")
            if p.dtypes['time'] != np.int64:
                p.time = pandas.to_numeric(p.time, errors='coerce', downcast='integer')
                p = p[pandas.notnull(p.time)]

            if p.size > 0:
                v = vaex.from_pandas(p, copy_index=False)
                v.export_hdf5(f + '.hdf5')
            else:
                print('{} has no rows', f)

        df = vaex.open(join(folder,'tx_samples*.txt.hdf5'))
        df['lats'] = df.latency // 10**6
        df['latsS'] = df.lats / 10**3
        df['pDate'] = df.time.values.astype('datetime64[ns]')
        dat = df.groupby(by=MyBinnerTime(expression=df.pDate, resolution='s', df=df), agg={'count': 'count'})
        its = dat.to_items()
        tps = make_series_line(its, min_time, (lambda its,idx: its[0][1][idx].astype(datetime.datetime)), (lambda its,idx: its[1][1][idx]))
        tps_lines.insert(0,{"values":tps, "title":"Loadgens", "freq": 1})

    times_dt = []
    client_cnt = []
    # Read clients count (if it exists)
    if exists(join(folder, 'client-count.log')):
        p = pandas.read_csv(join(folder, 'client-count.log'), sep=' ', error_bad_lines=True, warn_bad_lines=True, names=['time', 'clients'], encoding="ISO-8859-1")
        v = vaex.from_pandas(p, copy_index=False)
        v['timeOffset'] = v['time'] - global_min_time
        times = (v['timeOffset'] / 1000000000).to_numpy()
        clt_count = v['clients'].to_numpy()
        current = times[0]
        idx = 0
        end = times[-1] + 10 # + 10 seconds trailing
        times_dt = []
        client_cnt = []
        while current < end:
            times_dt.append(datetime.datetime.fromtimestamp(current))
            client_cnt.append(clt_count[idx])

            current = current + 1
            new_idx = -1
            while new_idx+1 < len(times) and times[new_idx+1] < current:
                new_idx = new_idx + 1
            idx = new_idx

    ## Create time series plots
    make_time_series(times_dt, client_cnt, tps_lines, 'Throughput in TX/s','Time series (Commit {} run at {})'.format(test_commit, test_date),True,join(folder,'throughput_time_series.pdf'))
    for lat_line in lat_lines:
        print("Mean latency for {}: {}ns".format(lat_line["title"], np.mean(lat_line["values"])))
        make_time_series(
            times_dt,
            client_cnt,
            [lat_line],
            'Latency in ns',
            'Latency Time series - {} (Commit {} run at {})'.format(
                lat_line["title"],
                test_commit,
                test_date
            ),
            False,
            join(folder,'latency_time_series_{}.pdf'.format(lat_line["title"]))
        )

    # Write marker file
    with open(join(folder,'results.json'), 'w') as outfile:
        json.dump({}, outfile, allow_nan=False)


for f in [f for f in listdir(".") if isdir(f) and not "debug" in f]:
    if not exists(join(f, "results.json")):
        calculate_results(f)
