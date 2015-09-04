#!/usr/bin/python3.4


import collections
import os
import sys
import threading
import subprocess
import json


DIR = "./dncp_output/"
SUBDIRS = ["rawdata", "traffic", "convergence"]
DNCP_EXEC = "env LD_LIBRARY_PATH=./build/ ./build/src/dncp/scripts/ns3.23-dncp_example-optimized"

TRAFFIC_INTERVAL = 0.1
START = 1
STOP = 100

THREADS = 15

def create_dir(dir):
    if not os.path.isdir(dir):
        try:
            os.mkdir(dir)
        except:
            print("Unable to create " + dir)
            os.exit(1)

def dicotomia_range(start, end):
    l = []
    interval = end - start
    while len(l) != end - start:
        for i in range(int(start), int(end), int(interval)):
            if i not in l:
                l.append(i)
        interval /= 2
        if interval == 0:
            interval = 1
    return l

class SimulationsSet():

    def __init__(self, directory = "./dncp_output/"):
        self.directory = directory
        self.topologies = ["tree", "doubletree", "star", "string", "link", "mesh"]
        self.sizes = dicotomia_range(5, 100)
        self.trials = dicotomia_range(1, 10)
        self.delays = [10, 100, 1000, 10000, 100000, 1000000]

        self.lock = threading.Semaphore(1)
        create_dir(self.directory)
        create_dir(self.directory+"/runs/")

        # Initial build
        ret = os.system("./waf --run 'dncp_example' --command-template='%s " +
                        "--topology=mesh --size=2 --stop_time=10 --delay=200us --seed=2'")
        if ret != 0:
            raise SystemError("Simulator build or testrun dfailed")

    def run_simulation(self, simulation):

        print("Doing "+str(simulation))
        exec = os.path.realpath(sys.argv[0])+" run "+str(simulation)
        os.system(exec)

        simulation.load()
        convergence_time = simulation.get_convergence_time()
        traffic = simulation.get_traffic_before_convergence()

        self.lock.acquire()

        # Update convergence time graph
        fname = self.directory+"/convergence-size-"+simulation.topology+"-"+str(simulation.delay)+".csv"
        f = open(fname, "a")
        f.write(str(simulation.size)+","+str(simulation.trial)+","+str(convergence_time)+"\n")
        f.close()
        plot = GnuPlot()
        plot.append_list([
            "set datafile separator ','",
            "set terminal png",
            "set xlabel 'Size (s)'",
            "set ylabel 'Convergence Time'",
            "set output '"+fname.replace(".csv", ".png")+"'",
            "plot '"+fname+"'"
            "using 1:3 title 'Convergence time'"
        ])
        plot.execute()

        # Update traffic graph
        fname = self.directory+"/traffic-size-"+simulation.topology+"-"+str(simulation.delay)+".csv"
        f = open(fname, "a")
        f.write(str(simulation.size)+","+str(simulation.trial)+","+str(traffic)+"\n")
        f.close()
        plot = GnuPlot()
        plot.append_list([
            "set datafile separator ','",
            "set terminal png",
            "set xlabel 'Size (s)'",
            "set ylabel 'Traffic (Bytes)'",
            "set output '"+fname.replace(".csv", ".png")+"'",
            "plot '"+fname+"'"
            "using 1:3 title 'Total amount of traffic'"
        ])
        plot.execute()

        print("Done "+str(simulation))
        self.lock.release()

    def run(self):

        thread_pool = ThreadPool(THREADS)
        try:
            for trial in self.trials:
                for size in self.sizes:
                    for topology in self.topologies:
                        for delay in self.delays:
                            sim = Simulation(self.directory+"runs/", topology, size, trial, delay)
                            thread_pool.add_thread(SimulationsSet.run_simulation, (self, sim, ))
        except KeyboardInterrupt as e:
            print("Interrupted")
        thread_pool.wait()

class Simulation:
    def __init__(self, directory, topology, size, trial, delay):
        self.topology = topology
        self.size = size
        self.trial = trial
        self.delay = delay
        self.directory = directory+"/"+str(self)+"/"
        create_dir(self.directory)
        self.json = {}
        self.load()

    @classmethod
    def serialize(cls, directory, s):
        p = s.split("-")
        return cls(directory, p[0], int(p[1]), int(p[2]), int(p[3]))

    def __str__(self):
        return self.topology + "-" + str(self.size) + "-" + str(self.trial) + "-" + str(self.delay)

    def json_file(self):
        return self.directory + "/obj.json"

    def load(self):
        try:
            with open(self.json_file(), "r") as f:
                self.json = json.load(f)
        except Exception as e:
            self.json = {}

    def store(self, key, value):
        self.json[key] = value
        with open(self.json_file(), "w") as f:
            json.dump(self.json, f)
        return value

    def ns3_log_path(self):
        return self.directory + "/ns3-log.csv"

    def traffic_log_path(self):
        return self.directory + "/traffic.csv"

    def convergence_log_path(self):
        return self.directory + "/convergence.csv"

    def traffic_plot_path(self):
        return self.traffic_log_path().replace(".csv", ".png")

    def convergence_plot_path(self):
        return self.convergence_log_path().replace(".csv", ".png")

    def open_ns3_logs(self):
        if not os.path.isfile(self.ns3_log_path()):
            ret = os.system(DNCP_EXEC +
                            " --topology=" + self.topology +
                            " --size=" + str(self.size) +
                            " --start_time=" + str(START) + "s" +
                            " --stop_time=" + str(STOP) + "s" +
                            " --delay=" + str(self.delay) + "us" +
                            " --seed=" + str(self.trial) +
                            " --output=" + self.ns3_log_path())

            if ret != 0:
                if os.path.exists(self.ns3_log_path()):
                    os.remove(self.ns3_log_path())
                raise Exception("NS3 simulation failed "+str(self))
        return open(self.ns3_log_path(), "r", encoding="utf-8")

    def open_convergence_logs(self):
        if not os.path.isfile(self.convergence_log_path()):
            with open(self.convergence_log_path(), "w", encoding="utf-8") as out, self.open_ns3_logs() as f:
                node_hashes = {}
                hash_count = collections.Counter()
                for line in f:
                    parts = line.split(",")
                    if len(parts) < 4 or parts[1] != "NetHash":
                        continue
                    time = str(parts[0])
                    node = parts[2]
                    hash = parts[3]

                    if node in node_hashes:
                        if node_hashes[node] == hash:
                            continue

                        hash_count[node_hashes[node]] -= 1
                        if hash_count[node_hashes[node]] == 0:
                            del hash_count[node_hashes[node]]

                    hash_count[hash] += 1
                    node_hashes[node] = hash

                    if len(node_hashes) != self.size:
                        continue

                    s = str(time)+",DncpConv,"
                    commons = hash_count.most_common(5)
                    for c in commons:
                        s += str(round((c[1]*1.0)/self.size, 5))+","
                    out.write(s+"\n")

        return open(self.convergence_log_path(), "r", encoding="utf-8")

    def get_convergence_time(self):
        if "convergence_time" in self.json:
            return self.json["convergence_time"]

        non_converged = 0
        with self.open_convergence_logs() as f:
            for line in f:
                parts = line.split(",")
                if parts[1] != "DncpConv":
                    continue

                time = float(parts[0])
                percent = float(parts[2])

                if percent == 1:
                    if non_converged:
                        return self.store("convergence_time", time)
                else:
                    non_converged = 1

        print("Could not find convergence time for "+str(self))
        return self.store("convergence_time", 1.5*STOP)

    def open_traffic_logs(self):
        if not os.path.isfile(self.traffic_log_path()):
            with open(self.traffic_log_path(), "w", encoding="utf-8") as out, self.open_ns3_logs() as f:

                meter_all = ThroughputMeter(TRAFFIC_INTERVAL, "DncpTraffic", out)
                meter_nodes = {}
                meter_devices = {}
                for line in f:
                   # 101.916,DncpTx,32,16,51847,M,fe80::200:ff:fe00:3e2,ff02::8808,24,NET_STATE,
                    parts = line.split(",")
                    if len(parts) < 4 or parts[1] != "DncpTx":
                        continue

                    time = float(parts[0])
                    node = parts[2]
                    device = node + "-" + parts[3]
                    size = int(parts[8])

                    if node not in meter_nodes:
                         meter_nodes[node] = ThroughputMeter(TRAFFIC_INTERVAL, "DncpTrafficNode," + node, out)

                    if device not in meter_devices:
                         meter_devices[device] = ThroughputMeter(TRAFFIC_INTERVAL, "DncpTrafficDev," + node + "," + parts[3], out)

                    meter_all.push_packet(time, size)
                    meter_nodes[node].push_packet(time, size)
                    meter_devices[device].push_packet(time, size)

        return open(self.traffic_log_path(), "r", encoding="utf-8")

    def get_traffic_before_convergence(self):
        if "traffic_before_convergence" not in self.json:
            convergence_time = self.get_convergence_time()
            with self.open_ns3_logs() as f:
                tot = 0
                for line in f:
                    parts = line.split(",")
                    if parts[1] == "DncpTx":
                        if float(parts[0]) > convergence_time:
                            break
                        tot += int(parts[8])
                self.store("traffic_before_convergence", tot)

        return self.json["traffic_before_convergence"]

    def plot_traffic(self):
        if os.path.isfile(self.traffic_plot_path()):
            return

        self.open_traffic_logs().close()  # Make sure it exists
        convergence = self.get_convergence_time()  # Get convergence time
        plot = GnuPlot()
        plot.append_list([
            "set datafile separator ','",
            "set terminal png",
            "set xlabel 'Time (s)'",
            "set ylabel 'Global Traffic (Byte/s)'",
            "set output '"+self.traffic_plot_path()+"'",
            "set xrange [0:"+str(convergence * 1.5)+"]",
            "plot '<(cat " + self.traffic_log_path() + " | grep ,DncpTraffic,)' " +
            "using 1:3 title 'Global Throughput for " + str(self) + "'"
        ])
        plot.execute()

    def plot_convergence(self):
        if os.path.isfile(self.convergence_plot_path()):
            return

        self.open_convergence_logs().close()  # Make sure it exists
        convergence = self.get_convergence_time()  # Get convergence time
        plot = GnuPlot()
        plot.append_list([
            "set datafile separator ','",
            "set terminal png",
            "set xlabel 'Time (s)'",
            "set ylabel 'Convergence (%)'",
            "set output '"+self.convergence_plot_path()+"'",
            "set xrange [0:"+str(convergence * 1.5)+"]",
            "plot '" + self.convergence_log_path() + "' " +
            "using 1:3 title 'Convergence percentage for " + str(self) + "'"
        ])
        plot.execute()


class GnuPlot:
    def __init__(self):
        self.args = []

    def append(self, arg):
        self.args.append(arg)

    def append_list(self, args):
        for s in args:
            self.append(s)

    def execute(self):
        s = ""
        for i in self.args:
            s += i+"\n"

        plot = subprocess.Popen("gnuplot", stdin=subprocess.PIPE)
        plot.communicate(input=bytearray(s, encoding="utf-8"))


class ThreadPool:
    @staticmethod
    def _run_inside(pool, function, args):
        try:
            function(*args)
        except Exception as e:
            pool.semaphore.release()
            raise e

        pool.semaphore.release()

    def __init__(self, size):
        self.size = size
        self.semaphore = threading.Semaphore(size)

    def add_thread(self, function, args):
        self.semaphore.acquire()
        t = threading.Thread(target=ThreadPool._run_inside, args=(self, function, args))
        t.start()

    def wait(self):
        for i in range(0, self.size):
            self.semaphore.acquire()
        for i in range(0, self.size):
            self.semaphore.release()


class ThroughputMeter:
    def __init__(self, interval, log, out):
        self.interval = interval
        self.log = log
        self.packets = []
        self.last_log = 0
        self.total = 0
        self.out = out

    def push_packet(self, time, size):
        self.packets.append((time, size))
        while 1:
            new_log = self.last_log + self.interval
            if new_log > time:
                return

            tot = 0
            while len(self.packets) != 0 and self.packets[0][0] < new_log:
                tot += self.packets[0][1]
                self.packets.pop(0)

            self.out.write(str(round(new_log, 5)) + "," + self.log + "," + str((tot * 1.0) / self.interval) + ",\n")
            self.last_log = new_log
            self.total += tot

    def get_total(self):
        return self.total


command = ""
if len(sys.argv) > 1:
    command = sys.argv[1]

if command == "run":
    sim = Simulation.serialize("./dncp_output/runs/", sys.argv[2])
    sim.plot_traffic()
    sim.plot_convergence()
    sim.get_convergence_time()
    sim.get_traffic_before_convergence()
else:
    set = SimulationsSet()
    set.run()



