'''
Created on Jun 9, 2015

@author: kaiwen
'''
from __future__ import division
import operator
import string
import glob
import os


os.chdir("./rawdata/")
for file in glob.glob("*-*-*.networkhash"):
    
    topology=file.split('-')[0]
    nNode=file.split('-')[1]
    f=open(file.split('.')[0]+'.percentage','w')
    f_read=open(file,'r')
    
    ID_HASH={}
    HASH_NUM={}
    old_percentage=-1
    old_time=-1
    old_old_percentage=-1
    old_old_time=-1
    
    for line in f_read.xreadlines():
        line=line.strip('\n')
        parts=line.split(" ")
        time=string.atof(parts[0])-1
        nodeID=parts[1]
        hashvalue=parts[2]
    
        if nodeID in ID_HASH:
            oldhash=ID_HASH[nodeID]
            HASH_NUM[oldhash]=HASH_NUM[oldhash]-1
            if HASH_NUM[oldhash]==0:
                del HASH_NUM[oldhash]
            if hashvalue in HASH_NUM: 
                HASH_NUM[hashvalue]=HASH_NUM[hashvalue]+1
            else: 
                HASH_NUM[hashvalue]=1;
        else:        
            if hashvalue in HASH_NUM: 
                HASH_NUM[hashvalue]=HASH_NUM[hashvalue]+1
            else: 
                HASH_NUM[hashvalue]=1;
            
        ID_HASH[nodeID]=hashvalue
        percentage_converge=max(HASH_NUM.iteritems(),key=operator.itemgetter(1))[1]/len(ID_HASH)
        
        if time!=old_time:
            if old_old_percentage!=old_percentage:
                f.write(str(old_time)+' '+str(old_percentage)+'\n')
            old_old_time= old_time
            old_old_percentage = old_percentage

        old_percentage=percentage_converge
        old_time=time
    
    #write convergence percentage into the file *.percentage#
    f.write(str(old_time)+' '+str(old_percentage)+'\n')
    f.close()
    f_read.close()
    
    
    if not os.path.exists('./../convergetime/'+topology+'.convergetime'):
        ff=open('./../convergetime/'+topology+'.convergetime','w')
        ff.close()
        
    
    found=False
    with open('./../convergetime/'+topology+".convergetime",'r') as afile:
        data=afile.readlines()
            
        for i in range (len(data)):
            if data[i].split(" ")[0]==nNode:
                found=True
                if old_percentage==1:
                    data[i]= data[i].strip('\n')+' '+str(old_time)+'\n'
                break
                    
    with open('./../convergetime/'+topology+".convergetime",'w') as afile:
        if found:
            afile.writelines(data)
        else:
            if old_percentage==1:
                data.append(nNode+' '+str(old_time)+'\n')
            else:
                data.append(nNode+' XX'+'\n')
            afile.writelines(data)
                    
