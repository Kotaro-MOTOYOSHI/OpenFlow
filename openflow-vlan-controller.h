/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#ifndef OPENFLOW_VLAN_CONTROLLER_H
#define OPENFLOW_VLAN_CONTROLLER_H

#include "ns3/openflow-interface.h"

#include <map>
#include <vector>
#include <iostream>

//namespace ns3 {

//namespace ofi {

class VlanController : public ns3::ofi::LearningController
{
public:
	static ns3::TypeId GetTypeId (void);

	ns3::TypeId GetInstanceTypeId () const;

	void SetVlanId (const ns3::Ptr<ns3::OpenFlowSwitchNetDevice> swtch, const int port, const uint16_t vid);

	uint16_t GetVlanId (const ns3::Ptr<ns3::OpenFlowSwitchNetDevice> swtch, const int port);

	std::vector<int> EnumeratePorts (const ns3::Ptr<ns3::OpenFlowSwitchNetDevice> swtch, const uint16_t vid);

	std::vector<int> EnumeratePortsWithoutInport (const ns3::Ptr<ns3::OpenFlowSwitchNetDevice> swtch, const int port, const uint16_t vid);

	void ReceiveFromSwitch (ns3::Ptr<ns3::OpenFlowSwitchNetDevice> swtch, ofpbuf* buffer);


private:
	typedef std::multimap<ns3::Ptr<ns3::OpenFlowSwitchNetDevice>, std::map<int, uint16_t> > Vid_map_t;
	Vid_map_t vid_map;
};

//}

//}

#endif /* OPENFLOW_VLAN_CONTROLLER_H */
