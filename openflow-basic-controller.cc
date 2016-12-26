/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "openflow-basic-controller.h"
#include "ns3/openflow-switch-net-device.h"
#include "ns3/assert.h"

NS_LOG_COMPONENT_DEFINE ("OpenFlowBasicController");
NS_OBJECT_ENSURE_REGISTERED (OpenFlowBasicController);

ns3::TypeId
OpenFlowBasicController::GetTypeId (void)
{
	static ns3::TypeId tid = ns3::TypeId ("OpenFlowBasicController")
		.SetParent<ns3::ofi::Controller> ()
		.SetGroupName ("OpenFlow")
		.AddConstructor<OpenFlowBasicController> ()
		.AddAttribute ("ExpirationTime",
			"Time it takes for learned MAC state entry/created flow to expire.",
			ns3::TimeValue (ns3::Seconds (0)),
			ns3::MakeTimeAccessor (&OpenFlowBasicController::m_expirationTime),
			ns3::MakeTimeChecker ())
		;
	return tid;
}

ns3::TypeId
OpenFlowBasicController::GetInstanceTypeId () const
{
	return GetTypeId ();
}

void
OpenFlowBasicController::ReceiveFromSwitch (ns3::Ptr<ns3::OpenFlowSwitchNetDevice> swtch, ofpbuf* buffer)
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

			// Create output-to-port action
			ofp_action_output x[1];

			if (in_port == 0)
			{
				x[0].type = htons (OFPAT_OUTPUT);
				x[0].len = htons (sizeof(ofp_action_output));
				x[0].port = OFPP_FLOOD;
			}
			else
			{
				x[0].type = htons (OFPAT_OUTPUT);
				x[0].len = htons (sizeof(ofp_action_output));
				x[0].port = 0;
			}
			ofp_flow_mod* ofm = ns3::ofi::Controller::BuildFlow (key, opi->buffer_id, OFPFC_ADD, x, sizeof(x), OFP_FLOW_PERMANENT, m_expirationTime.IsZero () ? OFP_FLOW_PERMANENT : m_expirationTime.GetSeconds ());
			ns3::ofi::Controller::SendToSwitch (swtch, ofm, ofm->header.length);
		}
		else
		{
			ofp_action_output x[1];

			if (in_port != 0)
			{
				x[0].type = htons (OFPAT_OUTPUT);
				x[0].len = htons (sizeof(ofp_action_output));
				x[0].port = 0;
			}
			else
			{
				SwitchMap_t::iterator smitr = m_switchMap.find (swtch);
				if (smitr != this->m_switchMap.end ())
				{
					boost::shared_ptr<LearnedState> m_learnedState = (smitr->second);
					LearnedState::iterator lsitr = m_learnedState->find (dst_addr);
					if (lsitr != m_learnedState->end ())
					{
						out_port = lsitr->second;

						x[0].type = htons (OFPAT_OUTPUT);
						x[0].len = htons (sizeof(ofp_action_output));
						x[0].port = out_port;
					}
				}
			}
			ofp_flow_mod* ofm = ns3::ofi::Controller::BuildFlow (key, opi->buffer_id, OFPFC_ADD, x, sizeof(x), OFP_FLOW_PERMANENT, m_expirationTime.IsZero () ? OFP_FLOW_PERMANENT : m_expirationTime.GetSeconds ());
			ns3::ofi::Controller::SendToSwitch (swtch, ofm, ofm->header.length);
		}

		// We can learn a specific port for the source address for future use,
		ns3::Mac48Address src_addr;
		src_addr.CopyFrom (key.flow.dl_src);

		if (in_port != 0)
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
