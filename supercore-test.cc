/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * The arrangement of the topology is described in loose-leaf
 */

#include <iostream>
#include <fstream>

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/openflow-module.h"
#include "ns3/log.h"

#include "openflow-basic-controller.h"
#include "openflow-special-controller.h"
#include "ips-imitation.h"

NS_LOG_COMPONENT_DEFINE ("SuperCoreTest");

bool verbose = false;
ns3::Time timeout = ns3::Seconds (0);

bool
SetVerbose (std::string value)
{
	verbose = true;
	return true;
}

bool
SetTimeout (std::string value)
{
	try {
		timeout = ns3::Seconds (atof (value.c_str ()));
		return true;
	}
	catch (...) { return false; }
	return false;
}

int
main (int argc, char *argv[])
{
	ns3::CommandLine cmd;
	cmd.AddValue ("verbose", "Verbose (turns on logging).", ns3::MakeCallback (&SetVerbose));
	cmd.AddValue ("timeout", "Expiration Timeout.", ns3::MakeCallback (&SetTimeout));

	cmd.Parse (argc, argv);

	if (verbose)
	{
		ns3::LogComponentEnable ("OpenFlowInterface", ns3::LOG_LEVEL_ALL);
		ns3::LogComponentEnable ("OpenFlowSwitchNetDevice", ns3::LOG_LEVEL_ALL);
		ns3::LogComponentEnable ("OpenFlowBasicController", ns3::LOG_LEVEL_ALL);
		ns3::LogComponentEnable ("OpenFlowSpecialController", ns3::LOG_LEVEL_ALL);
		ns3::LogComponentEnable ("IpsImitation", ns3::LOG_LEVEL_ALL);
		ns3::LogComponentEnable ("SuperCoreTest", ns3::LOG_LEVEL_ALL);
	}

	int n_terminals = 16;
	int n_switches = 15;

	NS_LOG_INFO ("Create Nodes.");
	ns3::NodeContainer terminals;
	terminals.Create (n_terminals);

	ns3::NodeContainer csmaSwitch;
	csmaSwitch.Create (n_switches);

	NS_LOG_INFO ("Build Topology.");
	ns3::CsmaHelper csma;
	csma.SetChannelAttribute ("DataRate", ns3::DataRateValue (5000000));
	csma.SetChannelAttribute ("Delay", ns3::TimeValue (ns3::MilliSeconds (2)));

	// Create the csma links, from each terminal to the switch.
	ns3::NetDeviceContainer terminalDevices;
	ns3::NetDeviceContainer switchDevices[n_switches];

	// [switch 1] -- [ips imitation(switch 0)] -- [switch 2]
	
	for (int i = 1; i <= 2; i++)
	{
		ns3::NetDeviceContainer link = csma.Install (ns3::NodeContainer (csmaSwitch.Get (0), csmaSwitch.Get (i)));
		switchDevices[0].Add (link.Get (0));
		switchDevices[i].Add (link.Get (1));
	}

	// [switch 1,2] -- [switch 3-6]
	
	for (int i = 3; i <= 6; i++)
	{
		for (int j = 1; j <= 2; i++)
		{
			ns3::NetDeviceContainer link = csma.Install (ns3::NodeContainer (csmaSwitch.Get (j), csmaSwitch.Get (i)));
			switchDevices[j].Add (link.Get (0));
			switchDevices[i].Add (link.Get (1));
		}
	}

	// for i in 3-6; [switch i] -- [switch 2*i+1, 2*i+2]
	
	for (int i = 3; i <= 6; i++)
	{
		for (int j = 2 * i + 1; j <= 2 * i + 2; j++)
		{
			ns3::NetDeviceContainer link = csma.Install (ns3::NodeContainer (csmaSwitch.Get (i), csmaSwitch.Get (j)));
			switchDevices[i].Add (link.Get (0));
			switchDevices[j].Add (link.Get (1));
		}
	}

	// for i in 7-14 [switch i] -- [2 terminals]
	
	for (int i = 7; i < n_switches; i++)
	{
		for (int j = 2 * i - 14; j <= 2 * i - 13; j++)
		{
			ns3::NetDeviceContainer link = csma.Install (ns3::NodeContainer (csmaSwitch.Get (i), terminals.Get (j)));
			switchDevices[i].Add (link.Get (0));
			terminalDevices.Add (link.Get (1));
		}
	}
	
	// End of csma link
	
	ns3::OpenFlowSwitchHelper openFlowSwitchHelper;
        ns3::Ptr <ns3::Node> switchNode[n_switches];
	
	for (int i = 0; i < n_switches; i++)
	{
		switchNode[i] = csmaSwitch.Get (i);
	}

	// controller create
	ns3::Ptr<IpsImitation> ipsImitation = ns3::CreateObject<IpsImitation> ();
	ns3::Ptr<OpenFlowBasicController> openFlowBasicController = ns3::CreateObject<OpenFlowBasicController> ();
	ns3::Ptr<OpenFlowSpecialController> openFlowSpecialController = ns3::CreateObject<OpenFlowSpecialController> ();

	if (!timeout.IsZero ())
	{
		openFlowBasicController->SetAttribute ("ExpirationTime", ns3::TimeValue (timeout));
		openFlowSpecialController->SetAttribute ("ExpirationTime", ns3::TimeValue (timeout));
	}
	
	// [ips imitation (switch 0)] -- [ipsImitation]
	openFlowSwitchHelper.Install (switchNode[0], switchDevices[0], ipsImitation);

	// [switch 1-2, 7-14] -- [openFlowBasicController]
	for (int i = 1; i <= 2; i++)
	{
		openFlowSwitchHelper.Intall (switchNode[i], switchDevices[i], openFlowBasicController);
	}

	for (int i = 7; i < n_switches; i++)
	{
		openFlowSwitchHelper.Intall (switchNode[i], switchDevices[i], openFlowBasicController);
	}

	// [switch 3-6] -- [openFlowSpecialController]
	for (int i = 3; i <= 6; i++)
	{
		openFlowSwitchHelper.Intall (switchNode[i], switchDevices[i], openFlowSpecialController);
	}

	ns3::Ptr<ns3::NetDevice> p_NetDevice;
	ns3::Ptr<ns3::OpenFlowSwitchNetDevice> p_OpenFlowSwitchNetDevice[n_switches];
	for (unsigned i = 0; i < (unsigned) n_switches; i++)
	{
		for (unsigned j = 0; j < switchNode[i]->GetNDevices (); j++)
		{
			p_NetDevice = switchNode[i]->GetDevice (j);
			NS_LOG_INFO ("device type = " << p_NetDevice->GetInstanceTypeId ().GetName ());

			if(p_NetDevice->GetInstanceTypeId () == ns3::OpenFlowSwitchNetDevice::GetTypeId ())
			{
				p_OpenFlowSwitchNetDevice[i] = p_NetDevice->GetObject<ns3::OpenFlowSwitchNetDevice> ();
				NS_LOG_INFO ("OpenFlowSwitchNetDevice was found. " << p_OpenFlowSwitchNetDevice[i]->GetTypeId ().GetName ());
			}
		}
	}

