/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "dncp-application.h"

extern "C" {
#include <libubox/md5.h>
#include <hncp.h>
#include <dncp_i.h>
}

#include <string.h>
#include <stdarg.h>
#include <vector>
#include <ns3/log.h>

#define HNCP_PORT 8808
#define HNCP_DTLS_SERVER_PORT 8809
#define HNCP_MCAST_GROUP "ff02::8808"

void _hnetd_log(int priority, const char *format, ...)
{
	va_list argptr;
	va_start(argptr, format);
	vprintf(format, argptr);
	printf("\n");
	va_end(argptr);
}

int log_level = 9;
void (*hnetd_log)(int priority, const char *format, ...) = _hnetd_log;

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("DncpApplication");

void DncpApplication::DncpTlvCb(dncp_subscriber s,
		dncp_node n, struct tlv_attr *tlv, bool add)
{
	ns3::DncpApplication* app = container_of(s, typeof(app->m_ext_s), subscriber)->app;
	app->m_dncpTlvTrace(tlv_id(tlv), tlv_len(tlv), (char *)tlv_data(tlv), add);
}

void DncpApplication::DncpHash(const void *buf, size_t len, void *dest)
{
	md5_ctx_t ctx;
	md5_begin(&ctx);
	md5_hash(buf, len, &ctx);
	md5_end(dest, &ctx);
}

struct tlv_attr *DncpApplication::DncpValidateNodeData(dncp_node n, struct tlv_attr *a)
{
	return a;
}

bool DncpApplication::DncpHandleCollision(dncp_ext ext)
{
	ns3::DncpApplication* app = container_of(ext, typeof(app->m_ext_s), ext)->app;
	std::vector<char> nodeId(ext->conf.node_id_length,0);
	for (int i = 0; i < ext->conf.node_id_length; i++)
		nodeId[i] = random();

	return dncp_set_own_node_id(app->m_dncp, &nodeId[0]);
}

ssize_t DncpApplication::DncpRecv(dncp_ext ext, dncp_ep *ep,
	      struct sockaddr_in6 **src_store, struct sockaddr_in6 **dst_store,
	      int *flags, void *buf, size_t len)
{
	ns3::DncpApplication* app = container_of(ext, typeof(app->m_ext_s), ext)->app;
	static struct sockaddr_in6 src, dst;
	*src_store = &src;
	*dst_store = &dst;

	while(1) {
		ssize_t r;
		int f = 0;
		if((r = app->DncpRecvFrom(buf,len,ep,&src,&dst)) < 0)
			return r;

		if (IN6_IS_ADDR_LINKLOCAL(&src.sin6_addr))
			f |= DNCP_RECV_FLAG_SRC_LINKLOCAL;

		if (IN6_IS_ADDR_LINKLOCAL(&dst.sin6_addr))
			f |= DNCP_RECV_FLAG_DST_LINKLOCAL;

		if (IN6_IS_ADDR_MULTICAST(&dst.sin6_addr)) {
			if (memcmp(&dst.sin6_addr, &app->m_multicast_address, sizeof(app->m_multicast_address))) {
				NS_LOG_DEBUG("hncp_io_recv: got wrong multicast address traffic?");
				continue;
			}
			*dst_store = NULL;
		}
		*flags = f;
		return r;
	}
	return -1; //Never comes here
}

void DncpApplication::DncpSend(dncp_ext ext, dncp_ep ep,
      struct sockaddr_in6 *src,
      struct sockaddr_in6 *dst,
      void *buf, size_t len)
{
	ns3::DncpApplication* app = container_of(ext, typeof(app->m_ext_s), ext)->app;
	app->DncpSendTo(std::atoi(ep->ifname), src, dst, buf, len);
}


hnetd_time_t DncpApplication::DncpGetTime(dncp_ext ext __unused)
{
	return (hnetd_time_t) ns3::Simulator::Now().GetMilliSeconds();
}

int DncpApplication::DncpGetHwAddrs(dncp_ext ext, unsigned char *buf, int buf_left)
{
	ns3::DncpApplication* app = container_of(ext, typeof(app->m_ext_s), ext)->app;
	if (buf_left < 12)
		return 0;

	uint32_t id = app->GetNode()->GetId();
	uint8_t h[16];
	DncpApplication::DncpHash(&id, 4, h);
	memcpy(buf, h, 12);
	return 12;
}

void DncpApplication::DncpScheduleTimeout(dncp_ext ext, int msecs)
{
	ns3::DncpApplication* app = container_of(ext, typeof(app->m_ext_s), ext)->app;
	app->DncpScheduleTimeout(msecs);
}

/***********************************************************************************************************************/

NS_OBJECT_ENSURE_REGISTERED (DncpApplication);

int DncpApplication::AddTlv(uint16_t type, uint16_t len, char *data)
{
	return dncp_add_tlv(m_dncp, type, data, len, 0) == NULL;
}

void DncpApplication::RemoveTlv(uint16_t type, uint16_t len, char *data)
{
	dncp_remove_tlv_matching(m_dncp, type, data, len);
}

void DncpApplication::DncpScheduleTimeout(int msecs)
{
	NS_LOG_FUNCTION (this<<msecs);
	if(!m_timeoutEvent.IsRunning())
		m_timeoutEvent=Simulator::Schedule(MilliSeconds(msecs), &DncpApplication::DncpTimeout, this);
}

void DncpApplication::DncpTimeout()
{
	NS_LOG_FUNCTION (this);
	if (m_running) {
		 dncp_ext_timeout(m_dncp);
		 m_net_hash = *((uint64_t *)&m_dncp->network_hash);
	}
}

DncpPacket::DncpPacket(Ptr<Packet> p, Time t, Address f):
m_packet(p),
m_delivery_time(t),
m_from(f)
{

}

void DncpApplication::RecvFromDevice(Ptr<Socket> socket)
{
	NS_LOG_FUNCTION(this);

	Address from;
	Ptr<Packet> p;
	while ((p = m_socket6->RecvFrom(from))) {
		DncpPacket *packet = new DncpPacket(p, ns3::Simulator::Now() + m_processing_delay, from);
		m_packets.push_back(packet);
		NS_LOG_DEBUG("Pushing packet to queue at " << ns3::Simulator::Now() <<
				" for retrieval at " << packet->m_delivery_time);
		if(!m_readEvent.IsRunning())
			m_readEvent = Simulator::Schedule(m_processing_delay, &DncpApplication::HandleRead,this);
	}
}

void DncpApplication::HandleRead ()
{
	NS_LOG_FUNCTION(this);
	dncp_ext_readable(m_dncp);
}

void DncpApplication::DncpSendTo(int Ifindex,struct sockaddr_in6 *src,
		struct sockaddr_in6 *dst, void *buf, size_t len)
{
	NS_LOG_FUNCTION (this<<m_dncp);
	if(!m_running)
		return;

	Ptr<Packet> packet = Create<Packet> (static_cast<uint8_t *>(buf), len);
	Ipv6Address dstAddr = Ipv6Address(reinterpret_cast<uint8_t *>(dst?(&dst->sin6_addr):(&this->m_multicast_address)));
	m_socket6->SendTo (packet,0,Inet6SocketAddress(dstAddr, HNCP_PORT),GetNode()->GetDevice (Ifindex));

	NS_LOG_INFO ("At time " <<Simulator::Now ().GetSeconds () << "s Node "<<GetNode()->GetId()<<" sent " << len << " bytes to "
			<<dstAddr <<" through interface "<< Ifindex);

	Ptr<Ipv6> ipv6 = GetNode()->GetObject<Ipv6>();
	m_packetTxTrace(GetNode()->GetDevice(Ifindex), packet,
			ipv6->GetAddress(ipv6->GetInterfaceForDevice(GetNode()->GetDevice(Ifindex)),0).GetAddress(),
			Ipv6Address::ConvertFrom(dstAddr));
}

ssize_t DncpApplication::DncpRecvFrom(void *buf, size_t len,  dncp_ep *ep,
						struct sockaddr_in6 *src, struct sockaddr_in6 *dst)
{
	NS_LOG_FUNCTION (this<<m_dncp);

	DncpPacket *packet;
	Time now = Simulator::Now();
	while (!m_packets.empty() && (packet = m_packets.front())->m_delivery_time <= now ) {
		NS_LOG_ERROR("Delivering packet at "<< now << " scheduled at "<< packet->m_delivery_time);

		m_packets.pop_front();

		if(m_readEvent.IsRunning())
			m_readEvent.Cancel();

		if(!m_packets.empty()) {
			if(now >= m_packets.front()->m_delivery_time) {
				m_readEvent = Simulator::Schedule(MicroSeconds(1), & DncpApplication::HandleRead,this);
				NS_LOG_DEBUG("Next read event in 1 us");
			} else {
				m_readEvent = Simulator::Schedule(m_packets.front()->m_delivery_time - now, & DncpApplication::HandleRead,this);
				NS_LOG_DEBUG("Next read event in " << m_packets.front()->m_delivery_time - now);
			}
		}

		if (!Inet6SocketAddress::IsMatchingType(packet->m_from)) {
			NS_LOG_WARN("IPv4 packet received in DNCP");
			continue;
		}
		Inet6SocketAddress from6 = Inet6SocketAddress::ConvertFrom(packet->m_from);

		uint32_t incomingIf;
		uint32_t pktSize;

		/*Get incoming interface index*/
		Ipv6PacketInfoTag pktInfo;
		if (!packet->m_packet->RemovePacketTag (pktInfo)) {
			NS_LOG_ERROR("No incoming interface on RIPng message, aborting.");
			continue;
		}

		//Get interface
		incomingIf = pktInfo.GetRecvIf();
		char ifname[IFNAMSIZ];
		std::sprintf(ifname,"%d",incomingIf);
		*ep = dncp_find_ep_by_name(m_dncp, ifname);
		if (!*ep)
			continue;

		/*Get packet size*/
		pktSize=packet->m_packet->GetSize();

		//Source address
		from6.GetIpv6().GetBytes(reinterpret_cast<uint8_t *>(&src->sin6_addr));
		src->sin6_family = AF_INET6;
		src->sin6_port = htons(from6.GetPort());
		src->sin6_scope_id = 0;

		//Destination address
		pktInfo.GetAddress().GetBytes(reinterpret_cast<uint8_t *>(&dst->sin6_addr));
		dst->sin6_family = AF_INET6;
		dst->sin6_port = htons(HNCP_PORT);
		dst->sin6_scope_id = incomingIf;

		//dump packet to the buffer
		packet->m_packet->CopyData(static_cast<uint8_t*>(buf), pktSize);

		//struct tlv_attr *msg =container_of(static_cast<char(*)[0]>(buf), tlv_attr, data);
		NS_LOG_INFO("At time " << Simulator::Now ().GetSeconds () << "s Node "<<GetNode()->GetId()<<
				" received " << pktSize<< " bytes ["<<from6.GetIpv6() <<" -> "<<pktInfo.GetAddress()<<
				"] through interface "<<incomingIf);

		m_packetRxTrace(GetNode()->GetDevice(incomingIf),
				packet->m_packet, from6.GetIpv6(), pktInfo.GetAddress());

		delete packet;
		return pktSize;
	}
	return -1;
}

void DncpApplication::StartApplication(void)
{
	NS_LOG_FUNCTION (this);
	if (!this->DncpInit()) {
		NS_LOG_ERROR ("can not initiate dncp");
		return;
	}

	memset(&m_ext_s.subscriber, 0, sizeof(m_ext_s.subscriber));
	m_ext_s.subscriber.tlv_change_cb = DncpApplication::DncpTlvCb;

	m_running = true;
	int num = this->GetNode()->GetNDevices();
	for(int i=1; i<num; i++){
		NS_LOG_FUNCTION (this<<i<<1);
		char ifname[IFNAMSIZ];
		std::sprintf(ifname,"%d",i);
		dncp_ep ep;
		if((ep = dncp_find_ep_by_name(m_dncp, ifname)) && !dncp_ep_is_enabled(ep))
			dncp_ext_ep_ready(ep, 1);
		else
			NS_LOG_ERROR("unable to enable endpoint "<< ifname);
	}

	dncp_subscribe(m_dncp, &m_ext_s.subscriber);
}

void DncpApplication::StopApplication(void)
{
	NS_LOG_FUNCTION (this);
	dncp_unsubscribe(m_dncp, &m_ext_s.subscriber);

	this->DncpUninit();
	NS_LOG_INFO("At time " <<Simulator::Now ().GetSeconds () << "s Node "<<GetNode()->GetId()<<" stopped, " <<m_packetsSent
			<<" packets sent in total");
}

void DncpApplication::SetQueueProperties(Time processing_delay)
{
	m_processing_delay = processing_delay;
}

DncpApplication::~DncpApplication()
{

}

bool DncpApplication::DncpInit()
{
	NS_LOG_FUNCTION(this);

	memset(&m_ext_s.ext, 0, sizeof(m_ext_s.ext));
	m_ext_s.ext.conf.per_ep.trickle_imin = HNCP_TRICKLE_IMIN;
	m_ext_s.ext.conf.per_ep.trickle_imax = HNCP_TRICKLE_IMAX;
	m_ext_s.ext.conf.per_ep.trickle_k = HNCP_TRICKLE_K;
	m_ext_s.ext.conf.per_ep.keepalive_interval = HNCP_KEEPALIVE_INTERVAL;
	m_ext_s.ext.conf.per_ep.maximum_multicast_size = HNCP_MAXIMUM_MULTICAST_SIZE;
	m_ext_s.ext.conf.per_ep.accept_node_data_updates_via_multicast = false;

	m_ext_s.ext.conf.node_id_length = HNCP_NI_LEN;
	m_ext_s.ext.conf.hash_length = HNCP_HASH_LEN;
	m_ext_s.ext.conf.keepalive_multiplier_percent = HNCP_KEEPALIVE_MULTIPLIER * 100;
	m_ext_s.ext.conf.grace_interval = HNCP_PRUNE_GRACE_PERIOD;
	m_ext_s.ext.conf.minimum_prune_interval = HNCP_MINIMUM_PRUNE_INTERVAL;
	m_ext_s.ext.conf.ext_node_data_size = 0;
	m_ext_s.ext.conf.ext_ep_data_size = 0;

	m_ext_s.ext.cb.hash = DncpHash;
	m_ext_s.ext.cb.validate_node_data = DncpValidateNodeData;
	m_ext_s.ext.cb.handle_collision = DncpHandleCollision;
	m_ext_s.ext.cb.recv = DncpRecv;
	m_ext_s.ext.cb.send = DncpSend;
	m_ext_s.ext.cb.get_hwaddrs = DncpGetHwAddrs;
	m_ext_s.ext.cb.get_time = DncpGetTime;
	m_ext_s.ext.cb.schedule_timeout = DncpScheduleTimeout;

	m_ext_s.app = this;

	if (inet_pton(AF_INET6, HNCP_MCAST_GROUP, &m_multicast_address) < 1) {
		NS_LOG_ERROR("unable to inet_pton multicast group address");
		return false;
	}

	m_socket6 = Socket::CreateSocket(GetNode(), TypeId::LookupByName("ns3::UdpSocketFactory"));
	m_socket6->Bind(Inet6SocketAddress(Ipv6Address::GetAny(), HNCP_PORT));
	m_socket6->SetRecvPktInfo(true);
	m_socket6->SetRecvCallback(MakeCallback(&DncpApplication::RecvFromDevice, this));

	return !!(m_dncp = dncp_create(&m_ext_s.ext));
}

void DncpApplication::DncpUninit()
{
	NS_LOG_FUNCTION (this<<m_dncp);
	dncp_destroy(m_dncp);

	if (m_timeoutEvent.IsRunning())
		Simulator::Cancel(m_timeoutEvent);

	if (m_readEvent.IsRunning())
		Simulator::Cancel(m_readEvent);

	if (m_socket6) {
		m_socket6->Close();
		m_socket6->SetRecvCallback(MakeNullCallback<void, Ptr<Socket> > ());
	}
}

TypeId DncpApplication::GetTypeId (void)
{
	static TypeId t = TypeId("ns3::DncpApplication").SetParent<Application>()
	.AddConstructor<DncpApplication>()
	.AddTraceSource("NetworkHash",
	                 "The network hash value calculated by this node",
	                 MakeTraceSourceAccessor (&DncpApplication::m_net_hash),
					 "ns3::DncpApplication::TracedDouble")
	.AddTraceSource("PktRx",
					"Trace source indicating a packet was received",
					MakeTraceSourceAccessor(&DncpApplication::m_packetRxTrace),
					"ns3::DncpApplication::DncpCallback")
	.AddTraceSource("PktTx",
					"Trace source indicating a packet was sent",
					MakeTraceSourceAccessor(&DncpApplication::m_packetTxTrace),
					"ns3::DncpApplication::DncpCallback")
	.AddTraceSource("DncpTlv",
					"Trace source indicating a change in the TLV set",
					MakeTraceSourceAccessor(&DncpApplication::m_dncpTlvTrace),
					"ns3::DncpApplication::DncpTlvCallback");
	return t;
}

DncpApplication::DncpApplication()
  : m_socket6 (0),
    m_timeoutEvent (),
    m_running (true),
    m_packetsSent (0),
    m_processing_delay(10)
{
	NS_LOG_FUNCTION(this);
}

}/*End ns3 namespace*/

