/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "openflow-mirroring.h"

NS_LOG_COMPONENT_DEFINE ("MirroringController");
NS_OBJECT_ENSURE_REGISTERED (MirroringController);

ns3::TypeId MirroringController::GetTypeId (void)
{
	static ns3::TypeId tid = ns3::TypeId ("MirroringController")
		.SetParent<VlanController> ()
		.SetGroupName ("OpenFlow")
		.AddConstructor<MirroringController> ()
		;
	return tid;
}

ns3::TypeId MirroringController::GetInstanceTypeId () const
{
	return GetTypeId ();
}

void MirroringController::ReceiveFromSwitch (ns3::Ptr<ns3::OpenFlowSwitchNetDevice> swtch, ofpbuf* buffer)
{
	// Send To DPI
	if (!dst_addr.IsBroadcast ()) // 条件は後日
	{
		// DPI -> VLAN ID = 99
		NS_LOG_INFO ("Send To DPI (buffer_id = " << opi->buffer_id << ")");

		// VLAN ID Re-Set
		ofp_action_vlan_vid vl[1];

		vl[0].type = htons (OFPAT_SET_VLAN_VID);
		vl[0].len = htons (sizeof(ofp_action_vlan_vid) * 2);
		vl[0].vlan_vid = (uint16_t) 99;
		
		// Destination IPv4 Address Re-Set
		ofp_action_nw_addr ad[1];
		
		ad[0].type = htons (OFPAT_SET_NW_DST);
		ad[0].len = htons (sizeof(ofp_action_nw_addr));
		ad[0].nw_addr = htons((uint32_t) 0x0a010105); // 10.1.1.5
		
		// Destination MAC Address Re-Set
		ofp_action_dl_addr ma[1];
		
		ma[0].type = htons (OFPAT_SET_DL_DST);
		ma[0].len = htons (sizeof(ofp_action_dl_addr));
		
		// Mac Address -> 00:00:00:00:00:09
		for(int i = 0; i < 5; i++)
		{
			ma[0].dl_addr[i] = 0x00;
		}
		ma[0].dl_addr[5] = 0x09;
		
		// output
		std::vector<int> s = VlanController::EnumeratePortsWithoutInport (swtch, port, 99);
		assert (s.size () == 1);
		
		ofp_action_output x[1];
		
		x[0].type = htons (OFPAT_OUTPUT);
		x[0].len = htons (sizeof(ofp_action_output));
		x[0].port = s[0];

		// flow-entry (1 flow in 4 acts)
		size_t vl_offset = 0;
		size_t ad_offset = vl_offset + sizeof(vl) * 2;
		size_t ma_offset = ad_offset + sizeof(ad);
		size_t x_offset = ma_offset + sizeof(ma);

		unsigned char acts[sizeof(vl) * 2 + sizeof(ad) + sizeof(ma) + sizeof(x)];
		memcpy(acts + vl_offset, vl, sizeof(vl) * 2);
		memcpy(acts + ad_offset, ad, sizeof(ad));
		memcpy(acts + ma_offset, ma, sizeof(ma));
		memcpy(acts + x_offset, x, sizeof(x));

		ofp_flow_mod* ofm = ns3::ofi::Controller::BuildFlow (key, opi->buffer_id, OFPFC_ADD, acts, sizeof(acts), OFP_FLOW_PERMANENT, m_terminationTime.IsZero () ? OFP_FLOW_PERMANENT : m_terminationTime.GetSeconds ());
		ns3::ofi::Controller::SendToSwitch (swtch, ofm, ofm->header.length);
	}
}
