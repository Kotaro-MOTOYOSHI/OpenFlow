/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#ifndef OPENFLOW_IPS_IMITATION_H
#define OPENFLOW_IPS_IMITATION_H

#include "ns3/openflow-interface.h"

#include <iostream>
#include <memory>

class IpsImitation : public ns3::ofi::Controller
{
public:
	static ns3::TypeId GetTypeId (void);
	
	ns3::TypeId GetInstanceTypeId () const;

	void ReceiveFromSwitch (ns3::Ptr<ns3::OpenFlowSwitchNetDevice> swtch, ofpbuf* buffer);
};

#endif /* OPENFLOW_IPS_IMITATION_H */
