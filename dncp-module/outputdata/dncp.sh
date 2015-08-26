#!/bin/sh

TOPOLOGY="0 1 2 3 4 5"
NNODE="5 10 15 20 25 30 35 40 45 50 55 60 65 70 75 80"
TRIALS="1 2 3 4 5 6 7 8 9 10"
TRIAL="9 10"


for topology in $TOPOLOGY
do
  for nnode in $NNODE
  do
    for trial in $TRIALS
    do
    echo Topology $topology, nNode $nnode, Trial $trial,
    ./waf --run "dncpex2 --topology=$topology --nNode=$nnode --trialID=$trial --stop_time=101"
    done
  done
done

#python parse_networkhash.py

mv  *.networkhash ./src/dncp2/outputdata/rawdata
mv  *.packets ./src/dncp2/outputdata/rawdata
