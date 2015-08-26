import os
import glob

os.chdir("./throughput")
topology=["link","doubletree","tree","string","mesh","star"]

for t in topology:
    for nnode in range(10,81,10):
        data1=["set terminal png\n",
               "set output 'throughput-"+t+"-"+str(nnode)+".png'\n",
               'set title "Throughput in '+t+' network of '+str(nnode)+' nodes"\n',
	       'set key right top\n',
               'set xlabel "time(s)"\n',
               "set ylabel 'KB/s'\n",
	       "set xrange [1:100]\n",
               "set grid\n",
	       "set border lw 2\n"]
        pltfile=open(t+"-"+str(nnode)+".pltthroughput","w")
        pltfile.writelines(data1)
        line=""
        for id in range(1,6):
            line=line+"'"+t+"-"+str(nnode)+"-"+str(id)+".throughput' u 1:($2/1000)  title 'trial "+str(id)+"' with steps,"
        pltfile.write("plot "+line)

for file in glob.glob("*.pltthroughput"):
    os.system("gnuplot "+file)
for file in glob.glob("*.pltthroughput"):
    os.system("rm "+file)
for file in glob.glob("*.png"):
    os.system("mv "+file+" ../Graphs/")
