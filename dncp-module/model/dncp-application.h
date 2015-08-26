/*
 * DNCP Module
 *
 */

#ifndef DNCP2_APPLICATION_H
#define DNCP2_APPLICATION_H

extern "C" {
#include <dncp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#undef LOG_INFO
#undef LOG_WARN
#undef LOG_DEBUG
}

#include <fstream>
#include <string>
#include <cstdlib>
#include <stdint.h>
#include "ns3/type-id.h"
#include "ns3/socket.h"
#include "ns3/packet.h"
#include "ns3/abort.h"
#include "ns3/ipv6.h"
#include "ns3/application.h"
#include "ns3/simulator.h"
#include "ns3/ipv6-packet-info-tag.h"
#include <iostream>
#include "ns3/object.h"
#include "ns3/traced-value.h"
#include "ns3/trace-source-accessor.h"

namespace ns3 {

class Socket;
class Packet;

class DncpApplication : public Application
{
public:
	static TypeId GetTypeId (void);
	DncpApplication();
	virtual ~DncpApplication();

private:
	virtual void StartApplication(void);
	virtual void StopApplication(void);

	// DNCP callback functions
	static void DncpHash(const void *buf, size_t len, void *dest);
	static ssize_t DncpRecv(dncp_ext ext, dncp_ep *ep,
	      struct sockaddr_in6 **src_store, struct sockaddr_in6 **dst_store,
	      int *flags, void *buf, size_t len);
	static void DncpSend(dncp_ext ext, dncp_ep ep,
		      struct sockaddr_in6 *src, struct sockaddr_in6 *dst,
		      void *buf, size_t len);
	static struct tlv_attr *DncpValidateNodeData(dncp_node n, struct tlv_attr *a);
	static bool DncpHandleCollision(dncp_ext ext);
	static void DncpScheduleTimeout(dncp_ext ext, int msecs);
	static int DncpGetHwAddrs(dncp_ext ext, unsigned char *buf, int buf_left);
	static hnetd_time_t DncpGetTime(dncp_ext ext);

	// Per instance function
	bool DncpInit();
	void DncpUninit();
	void DncpSendTo(int ifname,struct sockaddr_in6 *src,
			  struct sockaddr_in6 *dst,void *buf, size_t len);
	ssize_t DncpRecvFrom(void *buf, size_t len, dncp_ep *ep,
			  struct sockaddr_in6 *src, struct sockaddr_in6 *dst);
	void DncpScheduleTimeout(int msecs);


	void DncpTimeout();
	void HandleRead();
	void ScheduleRead(Ptr<Socket> socket);

	struct {
		DncpApplication *app;
		dncp_ext_s		 ext;
	} m_ext_s;
	struct in6_addr 			m_multicast_address;
	dncp   				 	    m_dncp;
	Ptr<Socket>					m_socket6;
	EventId         	    	m_timeoutEvent;
	EventId						m_readEvent;
	bool            			m_running;
	uint32_t        			m_packetsSent;
	TracedValue<uint64_t> 		m_net_hash;

	TracedCallback<Ipv6Address,Ipv6Address,uint32_t,uint32_t,uint64_t,struct tlv_attr*,bool > m_packetRxTrace;
	TracedCallback<Ipv6Address,Ipv6Address,uint32_t,uint32_t,uint64_t,struct tlv_attr*,bool > m_packetTxTrace;
	TracedCallback<Ptr<Packet>,uint32_t,bool> m_packetRxTrace1;
};

}


#endif /* DNCP_APPLICATION_H */

