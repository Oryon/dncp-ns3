#for each topology create the graph of the overall traffic and traffic per node
import os 
import glob

os.chdir("./")
for file in glob.glob("./traffic/*.bytecount"):
    topology=file.split("/")[-1].split(".")[0]

    f=open('./traffic/'+topology+'.pltbytecount','w')
    f1=open('./traffic/'+topology+'.pltbytecountpn','w')
    data=["set terminal png\n",
          "set output 'traffic-"+topology+"-overall.png'\n",
          'set title "the overall traffic sent in '+topology+' network"\n',
          'set key right bottom\n',
          'set xlabel "number of nodes in the network"\n',
          "set ylabel 'kilo bytes'\n",
          'set xrange [0:85]\n',
          "set grid\n",
          "set xtics 10\n",
          "set border lw 2\n"]
    data1=["set terminal png\n",
          "set output 'traffic-"+topology+"-pernode.png'\n",
          'set title "the traffic sent per node in '+topology+' network"\n',
          'set key right bottom\n',
          'set xlabel "number of nodes in the network"\n',
          "set ylabel 'kilo bytes'\n",
          'set xrange [0:85]\n',
          "set grid\n",
          "set xtics 10\n",
          "set border lw 2\n"]
    f.writelines(data)
    f1.writelines(data1)
    line=""
    line1=""
    line=line+'"'+file.split("/")[-1]+'" u 1:($2/1000) title "overall traffic" with p pt 3 ps 2 lt 3,'
    line1=line1+' "'+file.split("/")[-1]+'" u 1:(($2/1000)/$1) title "average traffic per node" with p pt 3 ps 2 lt 4,'
    for i in range(3,12):
        line=line+'"'+file.split("/")[-1]+'" u 1:($'+str(i)+'/1000) title"" with p pt 3 ps 2 lt 3,'
        line1=line1+' "'+file.split("/")[-1]+'" u 1:(($'+str(i)+'/1000)/$1) title"" with p pt 3 ps 2 lt 4,'
	

    for file in glob.glob('./traffic/'+topology+'-*.bytecountin'):
	nodeid=file.split("-")[-1].split(".")[0]
	line=line+'"'+file.split("/")[-1]+'" u 1:($2/1000) title "traffic of node '+nodeid+'" with p pt 3 ps 2 lt '+str(int(nodeid)+1)+','
	line1=line1+'"'+file.split("/")[-1]+'" u 1:($2/1000) title "traffic of node '+nodeid+'" with p pt 3 ps 2 lt '+str(int(nodeid)+1)+','
	for i in range(3,12):
		line=line+'"'+file.split("/")[-1]+'" u 1:($'+str(i)+'/1000) title"" with p pt 3 ps 2 lt 1,'
		line1=line1+'"'+file.split("/")[-1]+'" u 1:($'+str(i)+'/1000) title"" with p pt 3 ps 2 lt 1,'

    f.write("plot "+line)
    f1.write("plot "+line1)
    f.close()
    f1.close()

os.chdir("./traffic")
for file in glob.glob('*.pltbytecount'):
    os.system("gnuplot " +file)
for file in glob.glob('*.pltbytecountpn'):
    os.system("gnuplot " +file)

os.system("mv ./*.png ./../Graphs/")
os.system("rm *.pltbytecount")
os.system("rm *.pltbytecountpn")
