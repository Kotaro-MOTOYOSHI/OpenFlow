/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#ifndef OPENFLOW_SPECIAL_CONTROLLER_H
#define OPENFLOW_SPECIAL_CONTROLLER_H

#include "ns3/openflow-interface.h"

#include <map>
#include <vector>
#include <iostream>
#include <memory>
#include <boost/shared_ptr.hpp>

class OpenFlowSpecialController : public ns3::ofi::Controller
{
public:
	static ns3::TypeId GetTypeId (void);
	
	ns3::TypeId GetInstanceTypeId () const;
	
	std::vector<int> EnumeratePorts (const ns3::Ptr<ns3::OpenFlowSwitchNetDevice> swtch, int port);

	void ReceiveFromSwitch (ns3::Ptr<ns3::OpenFlowSwitchNetDevice> swtch, ofpbuf* buffer);

private:
	typedef std::map<ns3::Mac48Address, int> LearnedState;
	typedef std::map<ns3::Ptr<ns3::OpenFlowSwitchNetDevice>, boost::shared_ptr<LearnedState> > SwitchMap_t;

	LearnedState m_learnedState;
	SwitchMap_t m_switchMap;

protected:
	ns3::Time m_expirationTime;
};

#endif /* OPENFLOW_SPECIAL_CONTROLLER_H */
