import os
import glob

os.chdir("./convergetime")

for file in glob.glob('*.convergetime'):
    topology=file.split('.')[0]
    f=open(topology+'.pltconvergetime','w')
    data=["set terminal png\n",
          "set output 'convergetime-"+topology+".png'\n",
          'set title "the time that '+topology+' network takes to converge"\n',
              'set key right bottom\n',
              'set xlabel "number of nodes in the network"\n',
              "set ylabel 'time taken to converge (s)'\n",
              'set xrange [0:85]\n',
              "set grid\n",
              "set ytics 5\n",
              "set xtics 10\n",
              "set border lw 2\n"]
    f.writelines(data)
    line=""
    for i in range(2,12):
        line=line+'"'+file+'" u 1:'+str(i)+' title"" with p pt 3 ps 1.5,'
    
    f.write("plot "+line)
    f.close()
    
for file in glob.glob('*.pltconvergetime'):
    os.system("gnuplot " +file)

os.system("mv  *.png ../Graphs")
os.system("rm  ./*.pltconvergetime")

