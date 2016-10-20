/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#ifndef OPENFLOW_VLAN_CONTROLLER_H
#define OPENFLOW_VLAN_CONTROLLER_H

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

	void SetVlanId (const Ptr<OpenFlowSwitchNetDevice> swtch, const int port, const int vid);

	int GetVlanId (const Ptr<OpenFlowSwitchNetDevice> swtch, const int port);

	std::vector<int> EnumeratePorts (const Ptr<OpenFlowSwitchNetDevice> swtch, const int vid);

	std::vector<int> EnumeratePortsWithoutInport (const Ptr<OpenFlowSwitchNetDevice> swtch, const int port, const int vid);
	
	void ReceiveFromSwitch (Ptr<OpenFlowSwitchNetDevice> swtch, ofpbuf* buffer);


private:
	std::map<OpenFlowSwitchNetDevice*, std::map<int, int> > vid_map;
};

}

}

#endif /* OPENFLOW_VLAN_CONTROLLER_H */
