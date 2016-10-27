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

		int vid = key_flow.dl_vlan;		
		std::vector<int> v = EnumeratePortsWithoutInport (swtch, port, vid);

		Mac48Address dst_addr;
		dst_addr.CopyFrom (key.flow.dl_dst);

		if (!dst_addr.IsBroadcast ())
		{
			LearnState_t::iterator st = m_learnState.find (dst_addr);
			if (st != m_learnState.end ())
			{
				int out_port = st->second.port;
				
				// Create output-to-port action if already learned
				ofp_action_output x[1];
				x[0].type = htons (OFPAT_OUTPUT);
				x[0].len = htons (sizeof(ofp_action_output));
				x[0].port = out_port;
			}
			else
			{
				NS_LOG_INFO ("Setting Multicast : Don't know yet what port " << dst_addr << " is connected to");

				// Create output-to-port action 
				ofp_action_output x[v.size()];
	
				for (int i = 0; i < v.size(); i++)
				{
					x[i].type = htons (OFPAT_OUTPUT);
					x[i].len = htons (sizeof(ofp_action_output));
					x[i].port = v[i];
				}
			}
		}
		else
		{
			NS_LOG_INFO ("Setting Multicast : this packet is a broadcast");

			// Create output-to-port action 
			ofp_action_output x[v.size()];

			for (int i = 0; i < v.size(); i++)
			{
				x[i].type = htons (OFPAT_OUTPUT);
				x[i].len = htons (sizeof(ofp_action_output));
				x[i].port = v[i];
			}
		}

		// Create a new flow
		ofp_flow_mod* ofm = BuildFlow (key, opi->buffer_id, OFPFC_ADD, x, sizeof(x), OFP_FLOW_PERMANENT, OFP_FLOW_PERMANENT);
		SendToSwitch (swtch, ofm, ofm->header.length);

		// We can learn a specific port for the source address for future use.
		Mac48Address src_addr;
		src_addr.CopyFrom (key.flow.dl_src);
		LearnState_t::iterator st = m_learnState.find (src_addr);
		if (st == m_learnState.end ()) // We haven't learned our source MAC Address yet.
		{
			LearnState ls;
			ls.port = in_port;
			m_learnState.insert (std::make_pair (src_addr, ls));
			NS_LOG_INFO ("Learned that " << src_addr << " can be found over port " << in_port);

			// Learn src_addr goes to a certain port.
			ofp_action_output x2[1];
			x2[0].type = htons (OFPAT_OUTPUT);
			x2[0].len = htons (sizeof(ofp_action_output));
			x2[0].port = in_port;

			// Switch MAC Addresses and ports to the flow we're modifying
			src_addr.CopyTo (key.flow.dl_dst);
			dst_addr.CopyTo (key.flow.dl_src);
			key.flow.in_port = out_port;
			ofp_flow_mod* ofm2 = BuildFlow (key, -1, OFPFC_MODIFY, x2, sizeof(x2), OFP_FLOW_PERMANENT, OFP_FLOW_PERMANENT);
			SendToSwitch (swtch, ofm2, ofm2->header.length);
		}
	}
}

}

}

#endif /* NS3_OPENFLOW_VLAN_CONTROLLER */
