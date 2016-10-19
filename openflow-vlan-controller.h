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
#ifndef OPENFLOW_VLAN_CONTROLLER_H
#define OPENFLOW_VLAN_CONTROLLER_H

#include "ns3/openflow-vlan-controller.h"

#include <map>
#include <vector>
#include <iostream>

#define list List

namespace ns3 {

class OpenFlowSwitchNetDevice:

namespace ofi {

class VlanController : public LearningController
{
public:
	static TypeId GetTypeId (void);

	void SetVlanId (const Ptr<OpenFlowSwitchNetDevice> swtch, const int port, const int vid);

	int GetVlanId (const Ptr<OpenFlowSwitchNetDevice> swtch, const int port);

	std::vector<int> EnumeratePorts (const Ptr<OpenFlowSwitchNetDevice> swtch, const int vid);

	std::vector<int> EnumeratePortsWithoutInport (const Ptr<OpenFlowSwitchNetDevice> swtch, const int port, const int vid);
	
	void ReceiveFromSwitch (Ptr<OpenFlowSwitchNetDevice> swtch, ofpbuf* buffer);


private:
	std::map<OpenFlowSwitchNetDevice*, std::map<int, int>> vid_map;
};
