/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#include <fstream>
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <cstdlib>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/dncp-module.h"
#include "ns3/csma-module.h"
#include <iostream>

NS_LOG_COMPONENT_DEFINE("DncpExample");

using namespace ns3;
using namespace std;

static void
Trace_Hash(Ptr<OutputStreamWrapper> file, std::string context,
		const uint64_t oldvalue, const uint64_t newvalue)
{
	*file->GetStream() << Simulator::Now().GetSeconds()
			<< " "<< context.substr(10,(context.find("/ApplicationList")-10))<< " "<< newvalue<<std::endl;
}

static void TraceDncpPkt(Ptr<OutputStreamWrapper> out,
		Ptr<Node> n, Ptr<NetDevice> dev,
		Ptr<Packet> packet, const Ipv6Address &saddr, const Ipv6Address &daddr,
		bool isReceive)
{
	*out->GetStream()
			<< Simulator::Now().GetSeconds () << " "
			<< (isReceive?"R ":"S ")
			<< n->GetId() << " "
			<< packet->GetUid() << " "
			<< (daddr.IsMulticast()?"M ":"U ")
			<< saddr << " "
			<< daddr << " "
			<< dev->GetIfIndex() << " "
			<< packet->GetSize() <<" ";

	Ptr<Ipv6> ipv6 = n->GetObject<Ipv6>();
	std::vector<char> buff(packet->GetSize() + sizeof(tlv_attr));
	struct tlv_attr *msg = (struct tlv_attr *)&buff[0];
	packet->CopyData((uint8_t *)msg->data, packet->GetSize());
	tlv_init(msg, 0, packet->GetSize() + sizeof(struct tlv_attr));

	int counter[11]={0};
	struct tlv_attr *a;

	tlv_for_each_attr(a, msg)
		counter[tlv_id(a)]++;

	if(!counter[DNCP_T_ENDPOINT_ID])
		*out->GetStream() << "MALFORMED";
	else if (counter[DNCP_T_REQ_NET_STATE]==1)
		*out->GetStream() << "REQ_NET";
	else if (counter[DNCP_T_REQ_NODE_STATE]==1)
		*out->GetStream() << "REQ_NODE";
	else if (counter[DNCP_T_NET_STATE]==1)
		*out->GetStream()<< "NET_STATE";
	else if ((counter[DNCP_T_NET_STATE]==0) && (counter[DNCP_T_NODE_STATE]==1))
		*out->GetStream()<< "NODE_STATE";
	else
		*out->GetStream() <<"MALFORMED";

	*out->GetStream() << std::endl;
}

static void TraceDncpTxPkt(Ptr<OutputStreamWrapper> out,
		Ptr<Node> n, Ptr<NetDevice> dev,
		Ptr<Packet> packet, const Ipv6Address &saddr, const Ipv6Address &daddr)
{
	TraceDncpPkt(out, n, dev, packet, saddr, daddr, 0);
}

static void TraceDncpRxPkt(Ptr<OutputStreamWrapper> out,
		Ptr<Node> n, Ptr<NetDevice> dev,
		Ptr<Packet> packet, const Ipv6Address &saddr, const Ipv6Address &daddr)
{
	TraceDncpPkt(out, n, dev, packet, saddr, daddr, 1);
}


static void
Trace_pkt2(Ptr<OutputStreamWrapper> file,std::string context,const Ptr<const Packet> packet){
	string s=context.substr(0,context.find("/DeviceList"));
	*file->GetStream()<<Simulator::Now ().GetSeconds ()<<" "<<context.substr(context.find_last_of("/")+1)<<
			" "<<context.substr(10,context.find("/",11)-10)<<" "<<packet->GetUid()<<" "
			<<s.substr(s.find_last_of ("/")+1)<<" "<<packet->GetSize()<<endl;
}

/*function that links two given nodes with csma channel*/
NetDeviceContainer
JoinTwoNodes(Ptr<Node> node1, Ptr<Node> node2, CsmaHelper csma){
	NodeContainer nodePair;
	nodePair.Add(node1);
	nodePair.Add(node2);

	NetDeviceContainer csmaDevices;
	csmaDevices = csma.Install(nodePair);
	return csmaDevices;
}

enum Topology{
	LINK,
	MESH,
	STRING,
	TREE,
	DOUBLETREE,
	STAR
};

int
main (int argc, char *argv[]){

	Time::SetResolution (Time::US);

	log_level=0;
	bool verbose=false;
	double startTime=1;
	double stopTime=50;
	int N=3;
	uint16_t topology=STRING;
	string trialID;
	int64_t delay = -1;

    stringstream sstr;
    sstr << time (NULL);
    trialID = sstr.str();

	CommandLine cmd;
	cmd.AddValue ("log_level", "log level in dncp code", log_level);
	cmd.AddValue ("verbose", "set it to true to enable Dncp2Application logging component", verbose);
	cmd.AddValue ("start_time", "the time that dncp applications begin to run", startTime);
	cmd.AddValue ("stop_time", "the stop time of dncp application", stopTime);
	cmd.AddValue ("topology", "the topology that the simulation is going to generate", topology);
	cmd.AddValue ("nNode", "number of nodes in the network", N);
	cmd.AddValue ("trialID", "the ID of this trial", trialID);
	cmd.AddValue ("delay", "Additional processing time (us) within DNCP application", delay);
	cmd.Parse (argc, argv);

	if(verbose){
		LogComponentEnable ("DncpApplication", LOG_LEVEL_INFO);
		LogComponentEnable ("DncpExample", LOG_LEVEL_INFO);
	}

	stringstream ss(trialID);
	unsigned int seed;
	ss>>seed;
	srandom(seed);

	Packet::EnablePrinting();

	NodeContainer nodes;
	nodes.Create(N);

	CsmaHelper csma;
	csma.SetChannelAttribute("DataRate", StringValue("1000Mbps"));
	csma.SetChannelAttribute("Delay", TimeValue(MicroSeconds(1)));

	InternetStackHelper stack;
	stack.Install(nodes);

	Ipv6AddressHelper ipv6addr;
	NetDeviceContainer csmaDevices;
	Ipv6InterfaceContainer interfaces;
	stringstream runID;
	NodeList::Iterator listEnd;
	if(topology == LINK) {
		    runID << "link-" << N<<"-"<<trialID;
			NS_LOG_INFO("Creating LINK topology");

			csmaDevices = csma.Install(nodes);
			interfaces =ipv6addr.AssignWithoutAddress(csmaDevices);
	} else if (topology == MESH) {
		    runID << "mesh-" << N<<"-"<<trialID;
			NS_LOG_INFO("Creating MESH topology");

			listEnd = NodeList::End();
			for(NodeList::Iterator i = NodeList::Begin(); i!=listEnd; i++){
				Ptr<Node> node= *i;
				for(NodeList::Iterator j=i+1;j != listEnd; j++) {
					csmaDevices = JoinTwoNodes(node,*j,csma);
					ipv6addr.AssignWithoutAddress(csmaDevices);
				}
			}

	} else if (topology == STRING) {
		    runID << "string-" << N<<"-"<<trialID;
			NS_LOG_INFO("Creating STRING topology");

			listEnd = NodeList::End()-1;
			for(NodeList::Iterator i = NodeList::Begin(); i!=listEnd; i++){
				csmaDevices = JoinTwoNodes(*i,*(i+1),csma);
				ipv6addr.AssignWithoutAddress(csmaDevices);
			}

	} else if (topology == TREE) {
		    runID << "tree-" << N<<"-"<<trialID;
			NS_LOG_INFO("Creating TREE topology");

			if (N%2){
				for(int i=0;i<=(N-3)/2;i++){
					csmaDevices=JoinTwoNodes(nodes.Get(i),nodes.Get(2*i+1),csma);
					ipv6addr.AssignWithoutAddress(csmaDevices);
					csmaDevices=JoinTwoNodes(nodes.Get(i),nodes.Get(2*i+2),csma);
					ipv6addr.AssignWithoutAddress(csmaDevices);
				}
			} else {
				for(int i=0;i<(N-2)/2;i++){
					csmaDevices=JoinTwoNodes(nodes.Get(i),nodes.Get(2*i+1),csma);
					ipv6addr.AssignWithoutAddress(csmaDevices);
					csmaDevices=JoinTwoNodes(nodes.Get(i),nodes.Get(2*i+2),csma);
					ipv6addr.AssignWithoutAddress(csmaDevices);
				}

				int i=(N-2)/2;
				csmaDevices=JoinTwoNodes(nodes.Get(i),nodes.Get(2*i+1),csma);
				ipv6addr.AssignWithoutAddress(csmaDevices);
			}
	} else if (topology == DOUBLETREE) {
		    runID << "doubletree-" << N <<"-"<<trialID;
			NS_LOG_INFO("Creating DOUBLETREE topology");

			NodeContainer nodePair;
			nodePair.Add(nodes.Get(0));
			nodePair.Add(nodes.Get(1));
			csmaDevices = csma.Install (nodePair);
			ipv6addr.AssignWithoutAddress(csmaDevices);

			for(int i=0;i<=2*((N-2)/4)-1;i++){
				if(i%2){
					for (int j=2*i;j<=2*i+3;j++){
						csmaDevices=JoinTwoNodes(nodes.Get(i),nodes.Get(j),csma);
						ipv6addr.AssignWithoutAddress(csmaDevices);
					}
				}
				else{
					for (int j=2*i+2;j<=2*i+5;j++){
						csmaDevices=JoinTwoNodes(nodes.Get(i),nodes.Get(j),csma);
						ipv6addr.AssignWithoutAddress(csmaDevices);
					}
				}
			}

			for(int i=1;i<=(N-2)%4;i++){
				int index=2*((N-2)/4);
				csmaDevices=JoinTwoNodes(nodes.Get(index),nodes.Get(2*index+1+i),csma);
				ipv6addr.AssignWithoutAddress(csmaDevices);

				index=index+1;
				csmaDevices=JoinTwoNodes(nodes.Get(index),nodes.Get(2*index-1+i),csma);
				ipv6addr.AssignWithoutAddress(csmaDevices);
			}
	} else if (topology == STAR) {
			runID << "star-" << N<<"-"<<trialID;
			NS_LOG_INFO("Creating STAR topology");

			for(int i=1;i<N;i++){
				csmaDevices=JoinTwoNodes(nodes.Get(0),nodes.Get(i),csma);
				ipv6addr.AssignWithoutAddress(csmaDevices);
			}
	} else {
		NS_LOG_ERROR("Unknown topology");
	}

	ObjectFactory factory;
	factory.SetTypeId(DncpApplication::GetTypeId ());
	for(int i=1; i<N; i++) {
		Ptr<DncpApplication> app = factory.Create<DncpApplication> ();
		nodes.Get(i)->AddApplication(app);
		app->SetQueueProperties(MicroSeconds(1000));
	}

	AsciiTraceHelper asciiTraceHelper;
	Ptr<OutputStreamWrapper> stream_hash = asciiTraceHelper.CreateFileStream (runID.str()+".networkhash");
	Ptr<OutputStreamWrapper> stream_pkt = asciiTraceHelper.CreateFileStream (runID.str()+".packets");

	Config::Connect("/NodeList/*/ApplicationList/*/$ns3::DncpApplication/NetworkHash", MakeBoundCallback (&Trace_Hash, stream_hash));
	Config::ConnectWithoutContext("/NodeList/*/ApplicationList/*/$ns3::DncpApplication/PktRx", MakeBoundCallback (&TraceDncpRxPkt, stream_pkt));
	Config::ConnectWithoutContext("/NodeList/*/ApplicationList/*/$ns3::DncpApplication/PktTx", MakeBoundCallback (&TraceDncpTxPkt, stream_pkt));

	Config::Connect("/NodeList/*/DeviceList/*/$ns3::CsmaNetDevice/MacTx", MakeBoundCallback (&Trace_pkt2, stream_pkt));
	Config::Connect("/NodeList/*/DeviceList/*/$ns3::CsmaNetDevice/MacTxDrop", MakeBoundCallback (&Trace_pkt2, stream_pkt));
	Config::Connect("/NodeList/*/DeviceList/*/$ns3::CsmaNetDevice/PhyTxDrop", MakeBoundCallback (&Trace_pkt2, stream_pkt));

	Config::Set("/NodeList/*/ApplicationList/*/$ns3::DncpApplication/StartTime",TimeValue (Seconds (startTime)));
	Config::Set("/NodeList/*/ApplicationList/*/$ns3::DncpApplication/StopTime",TimeValue (Seconds (stopTime)));

	Simulator::Stop(Seconds(stopTime+1));
	Simulator::Run();
	Simulator::Destroy();
	return 0;
}
