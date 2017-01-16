/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "openflow-core-switch-controller.h"
#include "ns3/openflow-switch-net-device.h"
#include "ns3/assert.h"

NS_LOG_COMPONENT_DEFINE ("OpenFlowCoreSwitchController");
NS_OBJECT_ENSURE_REGISTERED (OpenFlowCoreSwitchController);

ns3::TypeId
OpenFlowCoreSwitchController::GetTypeId (void)
{
	static ns3::TypeId tid = ns3::TypeId ("OpenFlowCoreSwitchController")
		.SetParent<ns3::ofi::Controller> ()
		.SetGroupName ("OpenFlow")
		.AddConstructor<OpenFlowCoreSwitchController> ()
		.AddAttribute ("ExpirationTime",
			"Time it takes for learned MAC state entry/created flow to expire.",
			ns3::TimeValue (ns3::Seconds (0)),
			ns3::MakeTimeAccessor (&OpenFlowCoreSwitchController::m_expirationTime),
			ns3::MakeTimeChecker ())
		;
	return tid;
}

ns3::TypeId
OpenFlowCoreSwitchController::GetInstanceTypeId () const
{
	return GetTypeId ();
}

std::vector<int>
OpenFlowCoreSwitchController::EnumeratePorts (const ns3::Ptr<ns3::OpenFlowSwitchNetDevice> swtch, int port)
{
	std::vector<int> v;
	int n_ports = swtch->GetNSwitchPorts ();
	
	if (port == 0 || port == 1)
	{
		for (int i = 2; i < n_ports; i++)
		{
			v.push_back (i);
		}
	}
	else
	{
		v.push_back(0);
		v.push_back(1);
	}
	return v;
}

void
OpenFlowCoreSwitchController::ReceiveFromSwitch (ns3::Ptr<ns3::OpenFlowSwitchNetDevice> swtch, ofpbuf* buffer)
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

		ns3::Mac48Address dst_addr;
		dst_addr.CopyFrom (key.flow.dl_dst);

		uint16_t out_port;
		uint16_t in_port = ntohs (key.flow.in_port);

		if (dst_addr.IsBroadcast ())
		{
			NS_LOG_INFO ("Setting Broadcast : this packet is a broadcast packet");

			std::vector<int> v = OpenFlowCoreSwitchController::EnumeratePorts(swtch, in_port);

			// Create output-to-port action
			ofp_action_output x[v.size()];

			for (int i = 0; i < (int)v.size (); i++)
			{
				x[i].type = htons (OFPAT_OUTPUT);
				x[i].len = htons (sizeof(ofp_action_output));
				x[i].port = v[i];
			}

			ofp_flow_mod* ofm = ns3::ofi::Controller::BuildFlow (key, opi->buffer_id, OFPFC_ADD, x, sizeof(x), OFP_FLOW_PERMANENT, m_expirationTime.IsZero () ? OFP_FLOW_PERMANENT : m_expirationTime.GetSeconds ());
			ns3::ofi::Controller::SendToSwitch (swtch, ofm, ofm->header.length);
		}
		else
		{
#if 0
			if (in_port != 0)
			{
				x[0].type = htons (OFPAT_OUTPUT);
				x[0].len = htons (sizeof(ofp_action_output));
				x[0].port = 0;
			}
#endif
			if (in_port == 0 || in_port == 1)
			{
				SwitchMap_t::iterator smitr = m_switchMap.find (swtch);
				if (smitr != this->m_switchMap.end ())
				{
					boost::shared_ptr<LearnedState> m_learnedState = (smitr->second);
					LearnedState::iterator lsitr = m_learnedState->find (dst_addr);
					if (lsitr != m_learnedState->end ())
					{
						out_port = lsitr->second;

						ofp_action_output x[1];

						x[0].type = htons (OFPAT_OUTPUT);
						x[0].len = htons (sizeof(ofp_action_output));
						x[0].port = out_port;
			
						ofp_flow_mod* ofm = ns3::ofi::Controller::BuildFlow (key, opi->buffer_id, OFPFC_ADD, x, sizeof(x), OFP_FLOW_PERMANENT, m_expirationTime.IsZero () ? OFP_FLOW_PERMANENT : m_expirationTime.GetSeconds ());
						ns3::ofi::Controller::SendToSwitch (swtch, ofm, ofm->header.length);
					}
				}
			}
			else
			{
				ns3::Mac48Address src_addr;
				src_addr.CopyFrom (key.flow.dl_src);

				ofp_action_output x[1];
				if (src_addr < dst_addr)
				{
					x[0].type = htons (OFPAT_OUTPUT);
					x[0].len = htons (sizeof(ofp_action_output));
					x[0].port = 0;
				}
				else
				{
					assert(dst_addr < src_addr);
					x[0].type = htons (OFPAT_OUTPUT);
					x[0].len = htons (sizeof(ofp_action_output));
					x[0].port = 1;
				}

				ofp_flow_mod* ofm = ns3::ofi::Controller::BuildFlow (key, opi->buffer_id, OFPFC_ADD, x, sizeof(x), OFP_FLOW_PERMANENT, m_expirationTime.IsZero () ? OFP_FLOW_PERMANENT : m_expirationTime.GetSeconds ());
				ns3::ofi::Controller::SendToSwitch (swtch, ofm, ofm->header.length);
			}
		}

		// We can learn a specific port for the source address for future use,
		ns3::Mac48Address src_addr;
		src_addr.CopyFrom (key.flow.dl_src);

		if (in_port >= 2)
		{
			SwitchMap_t::iterator smitr = m_switchMap.find (swtch);
			if (smitr == m_switchMap.end ()) // We haven't learned our source MAC Address yet.
			{
				m_switchMap.insert (std::make_pair (swtch, boost::shared_ptr<LearnedState> (new LearnedState ())));
				smitr = m_switchMap.find (swtch);
			}
			assert (smitr != m_switchMap.end ());
			boost::shared_ptr<LearnedState> m_learnedState = (smitr->second);
			if (m_learnedState->find (src_addr) != m_learnedState->end ())
			{
				m_learnedState->erase (src_addr);
			}
			m_learnedState->insert (std::make_pair (src_addr, in_port));
			NS_LOG_INFO ("Learned that swtch:" << swtch << ", addr:" << src_addr << " can be found over port " << in_port);

			// Learn src_addr goes to a certain port.
			ofp_action_output x2[1];
			x2[0].type = htons (OFPAT_OUTPUT);
			x2[0].len = htons (sizeof(ofp_action_output));
			x2[0].port = in_port;

			// Switch MAC Addresses and ports to the flow we're modifying
			src_addr.CopyTo (key.flow.dl_dst);
			dst_addr.CopyTo (key.flow.dl_src);
			key.flow.in_port = out_port;
			
			ofp_flow_mod* ofm2 = ns3::ofi::Controller::BuildFlow (key, -1, OFPFC_ADD, x2, sizeof(x2), OFP_FLOW_PERMANENT, m_expirationTime.IsZero () ? OFP_FLOW_PERMANENT : m_expirationTime.GetSeconds ());
			ns3::ofi::Controller::SendToSwitch (swtch, ofm2, ofm2->header.length);
		}
	}
}
