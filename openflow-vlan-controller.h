/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#define OPENFLOW_VLAN_CONTROLLER_H
#ifndef OPENFLOW_VLAN_CONTROLLER_H

#include "ns3/openflow-interface.h"

#include <map>
#include <vector>
#include <iostream>

namespace ns3 {

namespace ofi {

class VlanController : public LearningController
{
public:
	static TypeId GetTypeId (void);
	TypeId GetInstanceTypeId () const;

	void SetVlanId (const Ptr<OpenFlowSwitchNetDevice> swtch, const int port, const uint16_t vid);

	uint16_t GetVlanId (const Ptr<OpenFlowSwitchNetDevice> swtch, const int port);

	std::vector<int> EnumeratePorts (const Ptr<OpenFlowSwitchNetDevice> swtch, const uint16_t vid);

	std::vector<int> EnumeratePortsWithoutInport (const Ptr<OpenFlowSwitchNetDevice> swtch, const int port, const uint16_t vid);

	void ReceiveFromSwitch (Ptr<OpenFlowSwitchNetDevice> swtch, ofpbuf* buffer);


private:
	std::map<OpenFlowSwitchNetDevice*, std::map<int, uint16_t> > vid_map;
};

}

}

#endif /* OPENFLOW_VLAN_CONTROLLER_H */
