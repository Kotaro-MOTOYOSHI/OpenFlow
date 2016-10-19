/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#ifdef NS3_OPENFLOW_VLAN_CONTROLLER

#include "openflow-vlan-controller.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("OpenFlowVlanController");

namespace ofi {

TypeId VlanController::GetTypeId (void)
{
	static TypeId tid = TypeId ("ns3::ofi::VlanController")
		.SetParent <LearningController> ()
		.SetGroupName ("Openflow")
		.AddConstructor<VlanController> ()
		;
	return tid;
}

void
VlanController::SetVlanId (const Ptr<OpenFlowSwitchNetDevice> swtch, const int port, const int vid)
{
	vid_map[swtch][port] = vid;
}

int VlanController::GetVlanId (const Ptr<OpenFlowSwitchNetDevice> swtch, const int port)
{
	int vid;
	if (vid_map.count(swtch) > 0 && vid_map[swtch].count(port) > 0)
	{
		vid = vid_map[swtch][port];
	}
	else
	{
		NS_LOG_ERROR("Not Found VLAN ID : switch = " << swtch << ", port = " << port << ".");
	}
	return vid;
}

std::vector<int>
VlanController::EnumeratePorts (const Ptr<OpenFlowSwitchNetDevice> swtch, const int vid)
{
	std::vector<int> v;
	for (auto itr = vid_map.begin(); itr != vid_map.end(); ++itr)
	{
		if (itr->second == vid)
		{
			v.push_back(itr->first);
		}
	}
	return v;
}

std::vector<int>
VlanController::EnumeratePortsWithoutInport (const Ptr<OpenFlowSwitchNetDevice> swtch, const int port, const int vid)
{
	std::vector<int> v;
	for (auto itr = vid_map.begin(); itr != vid_map.end(); ++itr)
	{
		if (itr->second == vid)
		{
			if (itr->first != port)
			{
				v.push_back(itr->first);
			}
		}
	}
	return v;
}

void
VlanController::ReceiveFromSwitch (Ptr<OpenflowSwitchNetDevice> swtch, ofpbuf* buffer)
{
	if (m_switches.find (swtch) == m_switches.end ())
	{
		NS_LOG_ERROR ("Can't receive from this switch, not registered to the Controller.");
		return;
	}

	// We have received any packet at this point, so we pull the header to figure out what type of packet we're handling.
	uint8_t type = GetPacketType (buffer);

	if (type == OFPT_PACKET_IN) // The switch didn't understand the packet it received, so it forwarded it to the controller.
	{
		ofp_packet_in * opi = (ofp_packet_in*)ofpbuf_try_pull (buffer, offsetof (ofp_packet_in, data));
		int port = ntohs (opi->in_port);

		// Create matching key
		sw_flow_key key;
		key.wildcards = 0;
		flow_extract (buffer, port != -1 ? port : OFPP_NONE, &key_flow);

		int vid = GetVlanId(swtch, port);		
		std::vector<int> v = EnumeratePortsWithoutInport (swtch, port, vid);
		
		// Create output-to-port action
		ofp_action_output x[v.size()];

		for (int i = 0; i < v.size(); i++)
		{
			x[i].type = htons (OFPAT_OUTPUT);
			x[i].len = htons (sizeof(ofp_action_output));
			x[i].port = v[i];
		}

		// Create a new flow
		ofp_flow_mod* ofm = BuildFlow (key, opi->buffer_id, OFPFC_ADD, x, sizeof(x), OFP_FLOW_PERMANENT, OFP_FLOW_PERMANENT);
		SendToSwitch (swtch, ofm, ofm->header.length);
	}
}

}

}

#endif /* NS3_OPENFLOW_VLAN_CONTROLLER */
