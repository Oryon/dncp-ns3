import os
import glob
import math
import collections

os.chdir('./rawdata')
interval=0.1
for file in glob.glob("*-*-*.packets"):
    counter={}
    topology=file.split('-')[0]
    nnode=int(file.split('-')[1])
    for line in open(file,"r").xreadlines():
        line=line.strip('\n')
        parts=line.split(" ")
        time=float(parts[0])-1
        if parts[1]=="MacTx":
            if math.floor(time)>=0:
                if int(time/interval)*interval not in counter:
                    counter[int(time/interval)*interval]=0
                counter[int(time/interval)*interval]=counter[int(time/interval)*interval]+int(parts[-1])
                
    od = collections.OrderedDict(sorted(counter.items()))
    with open('./../throughput/'+file.split('.')[0]+'.throughput',"w") as f:
        for key in od:
            f.write(str(key)+" "+str(od[key]/interval)+'\n')
