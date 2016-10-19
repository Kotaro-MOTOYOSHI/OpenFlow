/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Blake Hurd  <naimorai@gmail.com>
 */
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
	int i;

	for (i = 0; i <= 65535; i++) {
		if (vid == vid_map[swtch][i]) {
			v.push_back(i);
		}
	}

	return v;
}

void // Now Programming...
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
