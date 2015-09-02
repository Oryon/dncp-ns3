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

#define SEPARATOR ","

NS_LOG_COMPONENT_DEFINE("DncpExample");

using namespace ns3;
using namespace std;

static void Trace_Hash(Ptr<OutputStreamWrapper> file, Ptr<DncpApplication> app,
		const uint64_t oldvalue, const uint64_t newvalue)
{
	*file->GetStream()
			<< Simulator::Now().GetSeconds() << SEPARATOR
			<< "NetHash" << SEPARATOR
			<< app->GetNode()->GetId() << SEPARATOR
			<< newvalue << SEPARATOR
			<< std::endl;
}

static void TraceDncpPkt(Ptr<OutputStreamWrapper> out, Ptr<DncpApplication> app,
		Ptr<NetDevice> dev, Ptr<Packet> packet, const Ipv6Address &saddr, const Ipv6Address &daddr,
		bool isReceive)
{
	*out->GetStream()
			<< Simulator::Now().GetSeconds () << SEPARATOR
			<< (isReceive?"DncpRx":"DncpTx") << SEPARATOR
			<< app->GetNode()->GetId() << SEPARATOR
			<< dev->GetIfIndex() << SEPARATOR
			<< packet->GetUid() << SEPARATOR
			<< (daddr.IsMulticast()?"M":"U") << SEPARATOR
			<< saddr << SEPARATOR
			<< daddr << SEPARATOR
			<< packet->GetSize() << SEPARATOR;

	Ptr<Ipv6> ipv6 = app->GetNode()->GetObject<Ipv6>();
	std::vector<char> buff(packet->GetSize() + sizeof(tlv_attr));
	struct tlv_attr *msg = (struct tlv_attr *)&buff[0];
	tlv_init(msg, 0, packet->GetSize() + sizeof(struct tlv_attr));
	packet->CopyData((uint8_t *)msg->data, packet->GetSize());

	int counter[11]={0};
	struct tlv_attr *a;

	tlv_for_each_attr(a, msg)
		counter[tlv_id(a)]++;

	if(!counter[DNCP_T_NODE_ENDPOINT])
		*out->GetStream() << "MALFORMED,NoEndPoint";
	else if (counter[DNCP_T_REQ_NET_STATE]==1)
		*out->GetStream() << "REQ_NET";
	else if (counter[DNCP_T_REQ_NODE_STATE]==1)
		*out->GetStream() << "REQ_NODE";
	else if (counter[DNCP_T_NET_STATE]==1)
		*out->GetStream()<< "NET_STATE";
	else if ((counter[DNCP_T_NET_STATE]==0) && (counter[DNCP_T_NODE_STATE]==1))
		*out->GetStream()<< "NODE_STATE";
	else
		*out->GetStream() <<"MALFORMED,"<<counter[DNCP_T_NET_STATE]<<","<<counter[DNCP_T_NODE_STATE];

	*out->GetStream() << SEPARATOR << std::endl;

	/*const char *buffer = "0123456789ABCDEF";
	for(int i=0; i<(int)packet->GetSize();i++) {
		*out->GetStream() << buffer[msg->data[i] >> 4] << buffer[msg->data[i] & 0x0f] << ":";
	}
	//packet->Print(*out->GetStream());
	*out->GetStream() << std::endl;*/
}

static void TraceDncpTxPkt(Ptr<OutputStreamWrapper> out, Ptr<DncpApplication> app,
		Ptr<NetDevice> dev, Ptr<Packet> packet, const Ipv6Address &saddr, const Ipv6Address &daddr)
{
	TraceDncpPkt(out, app, dev, packet, saddr, daddr, 0);
}

static void TraceDncpRxPkt(Ptr<OutputStreamWrapper> out, Ptr<DncpApplication> app,
		Ptr<NetDevice> dev, Ptr<Packet> packet, const Ipv6Address &saddr, const Ipv6Address &daddr)
{
	TraceDncpPkt(out, app, dev, packet, saddr, daddr, 1);
}

static void
Trace_CsmaPkt(Ptr<OutputStreamWrapper> file, Ptr<NetDevice> dev, const char *op,
		const Ptr<const Packet> packet)
{
	*file->GetStream()
			<< Simulator::Now().GetSeconds()<< SEPARATOR
			<< op << SEPARATOR
			<< dev->GetNode()->GetId() << SEPARATOR
			<< dev->GetIfIndex() << SEPARATOR
			<< packet->GetUid() << SEPARATOR
			<< packet->GetSize() << SEPARATOR
			<< std::endl;
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

int
main (int argc, char *argv[]){

	Time::SetResolution (Time::US);

	log_level=0;
	bool verbose=false;
	int N=3;
	std::string output = "dncp_example";
	std::string topology = "string";
	unsigned int seed = 0;
	Time start = Seconds(1);
	Time stop = Seconds(100);
	Time delay = MicroSeconds(20);

	CommandLine cmd;
	cmd.AddValue("log_level", "Log level in dncp code", log_level);
	cmd.AddValue("verbose", "Set it to true to enable Dncp2Application logging component", verbose);
	cmd.AddValue("start_time", "The time that dncp applications begin to run", start);
	cmd.AddValue("stop_time", "The stop time of dncp application", stop);
	cmd.AddValue("topology", "The topology that the simulation is going to generate", topology);
	cmd.AddValue("size", "Number of nodes in the network", N);
	cmd.AddValue("delay", "Additional processing time (us) within DNCP application", delay);
	cmd.AddValue("seed", "Pseudo random seed", seed);
	cmd.AddValue("output", "Output file", output);
	cmd.Parse (argc, argv);

	if(verbose){
		LogComponentEnable ("DncpApplication", LOG_LEVEL_INFO);
		LogComponentEnable ("DncpExample", LOG_LEVEL_INFO);
	}

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
	NodeList::Iterator listEnd;
	if(!topology.compare("link")) {
			NS_LOG_INFO("Creating LINK topology");
			csmaDevices = csma.Install(nodes);
			interfaces =ipv6addr.AssignWithoutAddress(csmaDevices);
	} else if (!topology.compare("mesh")) {
			NS_LOG_INFO("Creating MESH topology");
			listEnd = NodeList::End();
			for(NodeList::Iterator i = NodeList::Begin(); i != listEnd; i++) {
				Ptr<Node> node= *i;
				for(NodeList::Iterator j=i+1;j != listEnd; j++) {
					csmaDevices = JoinTwoNodes(node,*j,csma);
					ipv6addr.AssignWithoutAddress(csmaDevices);
				}
			}

	} else if (!topology.compare("string")) {
			NS_LOG_INFO("Creating STRING topology");
			listEnd = NodeList::End()-1;
			for(NodeList::Iterator i = NodeList::Begin(); i!=listEnd; i++){
				csmaDevices = JoinTwoNodes(*i,*(i+1),csma);
				ipv6addr.AssignWithoutAddress(csmaDevices);
			}

	} else if (!topology.compare("tree")) {
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
	} else if (!topology.compare("doubletree")) {
			NS_LOG_INFO("Creating DOUBLETREE topology");
			NodeContainer nodePair;
			nodePair.Add(nodes.Get(0));
			nodePair.Add(nodes.Get(1));
			csmaDevices = csma.Install (nodePair);
			ipv6addr.AssignWithoutAddress(csmaDevices);

			int max = (N > 4)?2*((N-2)/4)-1:0; //FIXME
			for(int i=0;i<=max;i++){
				if(i%2) {
					for (int j=2*i;j<=2*i+3;j++){
						csmaDevices=JoinTwoNodes(nodes.Get(i),nodes.Get(j),csma);
						ipv6addr.AssignWithoutAddress(csmaDevices);
					}
				} else {
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
	} else if (!topology.compare("star")) {
			NS_LOG_INFO("Creating STAR topology");
			for(int i=1;i<N;i++){
				csmaDevices=JoinTwoNodes(nodes.Get(0),nodes.Get(i),csma);
				ipv6addr.AssignWithoutAddress(csmaDevices);
			}
	} else {
		NS_LOG_ERROR("Unknown topology");
		return 1;
	}

	AsciiTraceHelper asciiTraceHelper;
	Ptr<OutputStreamWrapper> stream_all  = asciiTraceHelper.CreateFileStream(output);

	ObjectFactory factory;
	factory.SetTypeId(DncpApplication::GetTypeId ());
	for(int i=0; i<N; i++) {
		Ptr<Node> n = nodes.Get(i);
		Ptr<DncpApplication> app = factory.Create<DncpApplication>();
		nodes.Get(i)->AddApplication(app);
		app->SetQueueProperties(MicroSeconds(delay));
		app->TraceConnectWithoutContext("NetworkHash", MakeBoundCallback(&Trace_Hash, stream_all, app));
		app->TraceConnectWithoutContext("PktRx", MakeBoundCallback (&TraceDncpRxPkt, stream_all, app));
		app->TraceConnectWithoutContext("PktTx", MakeBoundCallback (&TraceDncpTxPkt, stream_all, app));
		for(int j=0; j < (int)n->GetNDevices(); j++) {
			Ptr<NetDevice> dev = n->GetDevice(j);
			char path[128];
			sprintf(path, "/NodeList/%d/DeviceList/%d/$ns3::CsmaNetDevice/*", i, j);
			char *t = strchr(path, '*');
			strcpy(t, "MacTx");
			Config::ConnectWithoutContext(path, MakeBoundCallback (&Trace_CsmaPkt, stream_all, dev, "MacTx"));
			strcpy(t, "MacTxDrop");
			Config::ConnectWithoutContext(path, MakeBoundCallback (&Trace_CsmaPkt, stream_all, dev, "MacTxDrop"));
			strcpy(t, "PhyTxDrop");
			Config::ConnectWithoutContext(path, MakeBoundCallback (&Trace_CsmaPkt, stream_all, dev, "PhyTxDrop"));
		}
		app->SetStartTime(start);
	}

	Simulator::Stop(stop);
	Simulator::Run();
	Simulator::Destroy();
	return 0;
}
