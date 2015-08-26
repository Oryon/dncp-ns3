#creating a file for each trial recording the number of packet before convergence and for each topology, create a file recording the traffic sent.
import argparse
import glob
import os

parser=argparse.ArgumentParser()
parser.add_argument("-st","--starttime",type=float,default=1,
					help="the lower bound of the time range")
parser.add_argument("-et","--endtime",type=float,default=float("inf"),
					help="the upper bound of the time range")
args = parser.parse_args()

A=["MacTx","MacTxDrop","PhyTxDrop"]

os.chdir("./")
	
for file in glob.glob("./rawdata/*-*-*.percentage"):
	topology=file.split('/')[-1].split("-")[0]
	nNode=file.split('/')[-1].split('-')[1]
	rCounter={'NET_STATE':0,'NODE_STATE':0,'REQ_NET':0,'REQ_NODE':0}
	sCounter={'NET_STATE':0,'NODE_STATE':0,'REQ_NET':0,'REQ_NODE':0}
	sCounter_d=0
	byteCounter=0
	dCounter={'MacTxDrop':0,'PhyTxDrop':0}

	with open(file,'r') as f:
		data=f.readlines()
		stoptime=float(data[-1].split(' ')[0])+1
	file=file.split	('/')[-1]
	for line in open("./rawdata/"+file.split(".")[0]+".packets").xreadlines():
		line=line.strip('\n')
		parts=line.split(" ")
		if float(parts[0]) >= 1 and float(parts[0]) <= stoptime:
			if parts[1]=="R":
				rCounter[parts[-1]]=rCounter[parts[-1]]+1
			elif parts[1]=="S":
				sCounter[parts[-1]]=sCounter[parts[-1]]+1
			elif parts[1]=="MacTx":
				sCounter_d=sCounter_d+1
				byteCounter=byteCounter+int(parts[-1])
			elif parts[1]=="MacTxDrop":
				dCounter["MacTxDrop"]=dCounter["MacTxDrop"]+1
			elif parts[1]=="PhyTxDrop":
				dCounter["PhyTxDrop"]=dCounter["PhyTxDrop"]+1

	with open("./packetCount/"+file.split(".")[0]+".pcount",'w') as o:
		o.write(str(sCounter['NET_STATE'])+" "+str(sCounter['NODE_STATE'])+" "
			+str(sCounter['REQ_NET'])+" "+str(sCounter['REQ_NODE'])+'\n')
		o.write(str(rCounter['NET_STATE'])+" "+str(rCounter['NODE_STATE'])+" "
			+str(rCounter['REQ_NET'])+" "+str(rCounter['REQ_NODE'])+'\n')
		o.write(str(sCounter_d)+" "+str(dCounter['MacTxDrop'])+" "+str(dCounter['PhyTxDrop'])+"\n")
		o.write(str(byteCounter)+"\n") 
				
	if not os.path.exists('./traffic/'+topology+'.bytecount'):
        	ff=open('./traffic/'+topology+'.bytecount','w')
        	ff.close()
			
    	found=False
    	with open('./traffic/'+topology+".bytecount",'r') as afile:
        	data=afile.readlines()
        	for i in range (len(data)):
            		if data[i].split(" ")[0]==nNode:
                		found=True
                    		data[i]= data[i].strip('\n')+' '+str(byteCounter)+'\n'
                		break
                    
    	with open('./traffic/'+topology+".bytecount",'w') as afile:
        	if not found:
			data.append(nNode+' '+str(byteCounter)+'\n')
            	afile.writelines(data)

	
