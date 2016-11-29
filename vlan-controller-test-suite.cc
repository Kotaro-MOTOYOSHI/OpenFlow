/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Network topology is the same as "src/openflow/examples/openflow-switch.cc"
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

#include "openflow-vlan-controller.h"

NS_LOG_COMPONENT_DEFINE ("VlanControllerTest");

bool verbose = false;
bool vlan = false;
ns3::Time timeout = ns3::Seconds (0);

bool
SetVerbose (std::string value)
{
	verbose = true;
	return true;
}

bool
SetVlan (std::string value)
{
	vlan = true;
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
	cmd.AddValue ("vlan", "Enable VLAN Mode", ns3::MakeCallback (&SetVlan));
	cmd.AddValue ("timeout", "Learning Controller Timeout", ns3::MakeCallback (&SetTimeout));

	cmd.Parse (argc, argv);

	if (verbose)
	{
		ns3::LogComponentEnable ("VlanController", ns3::LOG_LEVEL_INFO);
		ns3::LogComponentEnable ("VlanControllerTest", ns3::LOG_LEVEL_INFO);
		ns3::LogComponentEnable ("OpenFlowInterface", ns3::LOG_LEVEL_INFO);
		ns3::LogComponentEnable ("OpenFlowSwitchNetDevice", ns3::LOG_LEVEL_INFO);
	}

	NS_LOG_INFO ("Create nodes.");
	ns3::NodeContainer terminals;
	terminals.Create (5);

	ns3::NodeContainer csmaSwitch;
	csmaSwitch.Create (1);

	NS_LOG_INFO ("Build Topology.");
	ns3::CsmaHelper csma;
	csma.SetChannelAttribute ("DataRate", ns3::DataRateValue (5000000));
	csma.SetChannelAttribute ("Delay", ns3::TimeValue (ns3::MilliSeconds (2)));
	
	// Create the csma links, from each terminal to the switch.
	ns3::NetDeviceContainer terminalDevices;
	ns3::NetDeviceContainer switchDevices;
	for (int i = 0; i < 5; i++)
	{
		ns3::NetDeviceContainer link = csma.Install (ns3::NodeContainer (terminals.Get (i), csmaSwitch));
		terminalDevices.Add (link.Get (0));
		switchDevices.Add (link.Get (1));
	}

	// Create the switch netdevice, which will do the packet switching
	ns3::Ptr<ns3::Node> switchNode = csmaSwitch.Get (0);
	ns3::OpenFlowSwitchHelper open_flow_switch_helper;

	if (vlan)
	{
		ns3::Ptr<VlanController> controller = ns3::CreateObject<VlanController> ();
		if (!timeout.IsZero ())
		{
			controller->SetAttribute ("TerminationTime", ns3::TimeValue (timeout));
		}
		open_flow_switch_helper.Install (switchNode, switchDevices, controller);
		ns3::Ptr<ns3::OpenFlowSwitchNetDevice> p_open_flow_switch_net_device;
		
		for (unsigned i = 0; i < switchNode->GetNDevices (); ++i)
		{
			ns3::Ptr<ns3::NetDevice> p_net_device = switchNode->GetDevice (i);
			NS_LOG_INFO("device type = " << p_net_device->GetInstanceTypeId ().GetName ());
			
			if (p_net_device->GetInstanceTypeId () == ns3::OpenFlowSwitchNetDevice::GetTypeId ())
			{
				p_open_flow_switch_net_device = p_net_device->GetObject<ns3::OpenFlowSwitchNetDevice> ();
				NS_LOG_INFO("OpenFlowSwitchNetDevice was found. " << p_open_flow_switch_net_device->GetTypeId ().GetName ());
			}
		}
		controller->SetVlanId (p_open_flow_switch_net_device, 0, 1);
		controller->SetVlanId (p_open_flow_switch_net_device, 1, 1);
		controller->SetVlanId (p_open_flow_switch_net_device, 2, 2);
		controller->SetVlanId (p_open_flow_switch_net_device, 3, 2);
		controller->SetVlanId (p_open_flow_switch_net_device, 4, 99);
	}
	else
	{
		ns3::Ptr<ns3::ofi::LearningController> controller = ns3::CreateObject<ns3::ofi::LearningController> ();
		if (!timeout.IsZero ())
		{
			controller->SetAttribute ("ExpirationTime", ns3::TimeValue (timeout));
		}
		open_flow_switch_helper.Install (switchNode, switchDevices, controller);
	}

	// Add internet stack to the terminals
	ns3::InternetStackHelper internet;
	internet.Install (terminals);

	// We have got the "hardware" in place. Now we need to add IP addresses.
	NS_LOG_INFO ("Assign IP Addresses.");
	ns3::Ipv4AddressHelper ipv4;
	ipv4.SetBase ("10.1.1.0", "255.255.255.0");
	ipv4.Assign (terminalDevices);

	// Create an On-Off application to send UDP datagrams from n0 to n1.
	NS_LOG_INFO ("Create Applications.");
	uint16_t port = 9; // Discard port

	ns3::OnOffHelper onoff ("ns3::UdpSocketFactory", ns3::Address (ns3::InetSocketAddress (ns3::Ipv4Address ("10.1.1.2"), port)));
	onoff.SetConstantRate (ns3::DataRate ("500kb/s"));

	ns3::ApplicationContainer app = onoff.Install (terminals.Get (0));

	// Start the application
	app.Start (ns3::Seconds (1.0));
	app.Stop (ns3::Seconds (10.0));

	// Create ana optional packet sink to receive these packets
	ns3::PacketSinkHelper sink ("ns3::UdpSocketFactory", ns3::Address (ns3::InetSocketAddress (ns3::Ipv4Address::GetAny(), port)));
	ns3::ApplicationContainer app2 = sink.Install (terminals.Get (1));
	app2.Start (ns3::Seconds (0.0));

	//
	// Create a similar flow from n3 to n2, starting at time 1.1 seconds
	//
	ns3::OnOffHelper onoff2 ("ns3::UdpSocketFactory", ns3::Address (ns3::InetSocketAddress (ns3::Ipv4Address ("10.1.1.4"), port)));
	onoff2.SetConstantRate (ns3::DataRate ("500kb/s"));

	ns3::ApplicationContainer app3 = onoff2.Install (terminals.Get (2));
	app3.Start (ns3::Seconds (1.1));
	app3.Stop (ns3::Seconds (10.0));

	// Create ana optional packet sink to receive these packets
	ns3::PacketSinkHelper sink2 ("ns3::UdpSocketFactory", ns3::Address (ns3::InetSocketAddress (ns3::Ipv4Address::GetAny(), port)));
	ns3::ApplicationContainer app4 = sink2.Install (terminals.Get (3));
	app4.Start (ns3::Seconds (0.0));

	ns3::PacketSinkHelper sink3 ("ns3::UdpSocketFactory", ns3::Address (ns3::InetSocketAddress (ns3::Ipv4Address::GetAny(), port)));
	ns3::ApplicationContainer app5 = sink3.Install (terminals.Get (4));
	app5.Start (ns3::Seconds (0.0));

	NS_LOG_INFO ("Configure Tracing.");

	//
	// Configure tracing of all enqueue, dequeue, and NetDevice receive events.
	// Trace output will be sent to the file "openflow-switch.tr"
	//
	ns3::AsciiTraceHelper ascii;
	csma.EnableAsciiAll (ascii.CreateFileStream ("openflow-vlan-switch.tr"));

	//
	// Also configure some tcpdump traces; each interface will be traced.
	// The output files will be named: openflow-vlan-switch-<nodeId>-<interfaceId>.pcap
	// and can be read by the "tcpdump -r" command (use "-tt" option to display timestamps correctly)
	//
	csma.EnablePcapAll ("openflow-vlan-switch", false);

	//
	// Now, do the actual simulation.
	//
	NS_LOG_INFO ("Run Simulation.");
	ns3::Simulator::Run ();
	ns3::Simulator::Destroy ();
	NS_LOG_INFO ("Done.");
}
