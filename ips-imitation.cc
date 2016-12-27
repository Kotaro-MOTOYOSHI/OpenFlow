/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ips-imitation.h"
#include "ns3/openflow-switch-net-device.h"
#include "ns3/assert.h"

NS_LOG_COMPONENT_DEFINE ("IpsImitation");
NS_OBJECT_ENSURE_REGISTERED (IpsImitation);

ns3::TypeId
IpsImitation::GetTypeId (void)
{
	static ns3::TypeId tid = ns3::TypeId ("IpsImitation")
		.SetParent<ns3::ofi::Controller> ()
		.SetGroupName ("OpenFlow")
		.AddConstructor<IpsImitation> ()
		;
	return tid;
}

ns3::TypeId
IpsImitation::GetInstanceTypeId () const
{
	return GetTypeId ();
}

void
IpsImitation::ReceiveFromSwitch (ns3::Ptr<ns3::OpenFlowSwitchNetDevice> swtch, ofpbuf* buffer)
{
	if (m_switches.find (swtch) == m_switches.end ())
	{
		 NS_LOG_ERROR ("Can't receive from this switch, not registered to the Controller.");
		return;
	}

	// We have received any packet at this point, so we pull the header to figure out what type of packet we're handling.
	uint8_t type = ns3::ofi::Controller::GetPacketType (buffer);

	if (type == OFPT_PACKET_IN) // The switch didn't understand the packet it received, so it forwarded it to the controller.
	{
		ofp_packet_in * opi = (ofp_packet_in*)ofpbuf_try_pull (buffer, offsetof (ofp_packet_in, data));
		int port = ntohs (opi->in_port);
		
		// Create matching key
		sw_flow_key key;
		key.wildcards = 0;
		flow_extract (buffer, port != -1 ? port : OFPP_NONE, &key.flow);

		uint16_t in_port = ntohs (key.flow.in_port);

		ofp_action_output x[1];
		
		if (in_port == 0)
		{
			NS_LOG_INFO ("Packet in from port:0");

			x[0].type = htons (OFPAT_OUTPUT);
			x[0].len = htons (sizeof(ofp_action_output));
			x[0].port = 1;
		}
		else
		{
			assert (in_port == 1);
			NS_LOG_INFO ("Packet in from port:1");

			x[0].type = htons (OFPAT_OUTPUT);
			x[0].len = htons (sizeof(ofp_action_output));
			x[0].port = 0;
		}

		ofp_flow_mod* ofm = ns3::ofi::Controller::BuildFlow (key, opi->buffer_id, OFPFC_ADD, x, sizeof(x), OFP_FLOW_PERMANENT, OFP_FLOW_PERMANENT);
		ns3::ofi::Controller::SendToSwitch (swtch, ofm, ofm->header.length);
	}
}
