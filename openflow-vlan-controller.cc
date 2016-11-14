/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "openflow-vlan-controller.h"
#include "ns3/openflow-switch-net-device.h"
#include "ns3/assert.h"

NS_LOG_COMPONENT_DEFINE ("VlanController");
NS_OBJECT_ENSURE_REGISTERED(VlanController);

ns3::TypeId VlanController::GetTypeId (void)
{
	static ns3::TypeId tid = ns3::TypeId ("VlanController")
		.SetParent<ns3::ofi::LearningController> ()
		.SetGroupName ("OpenFlow")
		.AddConstructor<VlanController> ()
		.AddAttribute ("TerminationTime",
			"Time it takes for learned MAC state entry/created flow to expire.",
			ns3::TimeValue (ns3::Seconds (0)),
			ns3::MakeTimeAccessor (&VlanController::m_terminationTime),
			ns3::MakeTimeChecker ())
		;
	return tid;
}

ns3::TypeId VlanController::GetInstanceTypeId () const
{
	return GetTypeId ();
}

void
VlanController::SetVlanId (const ns3::Ptr<ns3::OpenFlowSwitchNetDevice> swtch, const int port, const uint16_t vid)
{
	Vid_map_t::iterator mm_iterator = vid_map.find(swtch);
	if(mm_iterator == vid_map.end())
	{
		vid_map.insert(std::make_pair(swtch, boost::shared_ptr<PortVidMap>(new PortVidMap())));	
		mm_iterator = vid_map.find(swtch);
	} 
	assert(mm_iterator != vid_map.end());
	boost::shared_ptr<PortVidMap> p_port_vid_map = (mm_iterator->second);
	if(p_port_vid_map->find(port) != p_port_vid_map->end())
	{
		p_port_vid_map->erase(port);
	}
	p_port_vid_map->insert(std::make_pair(port,vid));
}

uint16_t
VlanController::GetVlanId (const ns3::Ptr<ns3::OpenFlowSwitchNetDevice> swtch, const int port)
{
	uint16_t vid;
	Vid_map_t::iterator mm_iterator = vid_map.find(swtch);
	if (mm_iterator != this->vid_map.end())
	{
		boost::shared_ptr<PortVidMap> p_port_vid_map = (mm_iterator->second);
		PortVidMap::iterator itr = p_port_vid_map->find(port);
		if (itr != p_port_vid_map->end())
		{
			vid = itr->second;
	}
		else
		{	
			NS_LOG_ERROR("Not Found VLAN ID : switch = " << swtch << ", port = " << port << ".");
		}
	}
	else
	{
		NS_LOG_ERROR("Not Found VLAN ID : switch = " << swtch << ", port = " << port << ".");
	}
	return vid;
}

std::vector<int>
VlanController::EnumeratePorts (const ns3::Ptr<ns3::OpenFlowSwitchNetDevice> swtch, const uint16_t vid)
{
	std::vector<int> v;
	Vid_map_t::iterator mm_iterator = vid_map.find(swtch);
	if (mm_iterator != this->vid_map.end())
	{
		boost::shared_ptr<PortVidMap> p_port_vid_map = (mm_iterator->second);
		for (PortVidMap::iterator itr = p_port_vid_map->begin(); itr != p_port_vid_map->end(); ++itr)
		{
			if (itr->second == vid)
			{
				v.push_back(itr->first);
			}
			else
			{
				NS_LOG_INFO ("Not Found : This switch(" << swtch << ") does not have VID(" << vid << ")");
			}
		}
		
	}
	else
	{
		NS_LOG_INFO ("Not Found : This switch(" << swtch << ") does not exist");
	}
	return v;
}

std::vector<int>
VlanController::EnumeratePortsWithoutInport (const ns3::Ptr<ns3::OpenFlowSwitchNetDevice> swtch, const int port, const uint16_t vid)
{
	std::vector<int> v;
	Vid_map_t::iterator mm_iterator = vid_map.find(swtch);
	if (mm_iterator != this->vid_map.end())
	{
		boost::shared_ptr<PortVidMap> p_port_vid_map = (mm_iterator->second);
		for (PortVidMap::iterator itr = p_port_vid_map->begin(); itr != p_port_vid_map->end(); ++itr)
		{
			if (itr->second == vid)
			{
				if (itr->first == port)
				{
					NS_LOG_INFO ("port and itr->first = same");
				}
				else
				{
					v.push_back(itr->first);
				}

			}
			else
			{
				NS_LOG_INFO ("Not Found : This switch(" << swtch << ") does not have VID(" << vid << ")");
			}
		}
		
	}
	else
	{
		NS_LOG_INFO ("Not Found : This switch(" << swtch << ") does not exist");
	}
	return v;
}

void
VlanController::ReceiveFromSwitch (ns3::Ptr<ns3::OpenFlowSwitchNetDevice> swtch, ofpbuf* buffer)
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

		uint16_t vid = key.flow.dl_vlan;

		// Set VLAN ID, if buffer do not have VLAN ID
		if (vid == OFP_VLAN_NONE)
		{
			vid = GetVlanId (swtch, port);
			
			ofp_action_vlan_vid v[1];
			v[0].type = htons (OFPAT_SET_VLAN_VID);
			v[0].len = htons (sizeof(ofp_action_vlan_vid));
			v[0].vlan_vid = vid;

			ofp_flow_mod* ofm = ns3::ofi::Controller::BuildFlow (key, opi->buffer_id, OFPFC_ADD, v, sizeof(v), OFP_FLOW_PERMANENT, m_terminationTime.IsZero () ? OFP_FLOW_PERMANENT : m_terminationTime.GetSeconds ());
			ns3::ofi::Controller::SendToSwitch (swtch, ofm, ofm->header.length);

			NS_LOG_INFO ("Set VLAN ID : switch=" << swtch << ", port=" << port << ", VLAN ID=" << vid);
		}

		// Check VLAN ID
		if (vid != GetVlanId (swtch, port))
		{
			NS_LOG_ERROR ("Fatal Error : different VLAN ID has been set between two!");
		}

		std::vector<int> v = EnumeratePortsWithoutInport (swtch, port, vid);

		ns3::Mac48Address dst_addr;
		dst_addr.CopyFrom (key.flow.dl_dst);

		uint16_t out_port;
		uint16_t in_port = ntohs (key.flow.in_port);

		if (!dst_addr.IsBroadcast ())
		{
			LearnState_t::iterator st = m_learnState.find (dst_addr);
			if (st != m_learnState.end ())
			{
				out_port = st->second.port;
				
				// Create output-to-port action if already learned
				ofp_action_output x[1];
				x[0].type = htons (OFPAT_OUTPUT);
				x[0].len = htons (sizeof(ofp_action_output));
				x[0].port = out_port;
				if (GetVlanId(swtch, out_port) == 0)
				{
					ofp_action_output v[1];
					v[0].type = htons (OFPAT_STRIP_VLAN);
					v[0].len = htons (sizeof(ofp_action_output));
					ofp_flow_mod* ofm = ns3::ofi::Controller::BuildFlow (key, opi->buffer_id, OFPFC_MODIFY, v, sizeof(v), OFP_FLOW_PERMANENT, m_terminationTime.IsZero () ? OFP_FLOW_PERMANENT : m_terminationTime.GetSeconds ());
					ns3::ofi::Controller::SendToSwitch (swtch, ofm, ofm->header.length);
				}

				// Create a new flow
				ofp_flow_mod* ofm = ns3::ofi::Controller::BuildFlow (key, opi->buffer_id, OFPFC_MODIFY, x, sizeof(x), OFP_FLOW_PERMANENT, m_terminationTime.IsZero () ? OFP_FLOW_PERMANENT : m_terminationTime.GetSeconds ());
				ns3::ofi::Controller::SendToSwitch (swtch, ofm, ofm->header.length);
			}
			else
			{
				NS_LOG_INFO ("Setting Multicast : Don't know yet what port " << dst_addr << " is connected to");
				
				// Create output-to-port action 
				ofp_action_output x[(int)v.size()];

				for (int i = 0; i < (int)v.size(); i++)
				{
					x[i].type = htons (OFPAT_OUTPUT);
					x[i].len = htons (sizeof(ofp_action_output));
					x[i].port = v[i];
				}
				// Create a new flow
				ofp_flow_mod* ofm = ns3::ofi::Controller::BuildFlow (key, opi->buffer_id, OFPFC_MODIFY, x, sizeof(x), OFP_FLOW_PERMANENT, m_terminationTime.IsZero () ? OFP_FLOW_PERMANENT : m_terminationTime.GetSeconds ());
				ns3::ofi::Controller::SendToSwitch (swtch, ofm, ofm->header.length);
			}
		}
		else
		{
			NS_LOG_INFO ("Setting Multicast : this packet is a broadcast");

			// Create output-to-port action 
			ofp_action_output x[(int)v.size()];

			for (int i = 0; i < (int)v.size(); i++)
			{
				x[i].type = htons (OFPAT_OUTPUT);
				x[i].len = htons (sizeof(ofp_action_output));
				x[i].port = v[i];
			}
			// Create a new flow
			ofp_flow_mod* ofm = ns3::ofi::Controller::BuildFlow (key, opi->buffer_id, OFPFC_MODIFY, x, sizeof(x), OFP_FLOW_PERMANENT, m_terminationTime.IsZero () ? OFP_FLOW_PERMANENT : m_terminationTime.GetSeconds ());
			ns3::ofi::Controller::SendToSwitch (swtch, ofm, ofm->header.length);
		}


		// We can learn a specific port for the source address for future use.
		ns3::Mac48Address src_addr;
		src_addr.CopyFrom (key.flow.dl_src);
		LearnState_t::iterator st = m_learnState.find (src_addr);
		if (st == m_learnState.end ()) // We haven't learned our source MAC Address yet.
		{
			ns3::ofi::LearningController::LearnedState ls;
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
			ofp_flow_mod* ofm2 = ns3::ofi::Controller::BuildFlow (key, -1, OFPFC_MODIFY, x2, sizeof(x2), OFP_FLOW_PERMANENT, m_terminationTime.IsZero () ? OFP_FLOW_PERMANENT : m_terminationTime.GetSeconds ());
			SendToSwitch (swtch, ofm2, ofm2->header.length);
		}
	}
}
