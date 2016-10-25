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

#include "ns3/openflow-vlan-controller.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("OpenFlowVlanControllerExample");

bool vlan = false;

bool
SetVlan (std::string)
{
	vlan = true;
	return true;
}

int
main (int argc, char *argv[])
{
	#ifdef NS3_OPENFLOW_VLAN_EXAMPLE

	CommandLine cmd;
	cmd.AddValue ("vlan", "Enable VLAN Mode", MakeCallback (&SetVlan));

	cmd.Parse (argc, argv);

	if (vlan)
	{
		LogComponentEnable ("OpenFlowCsmaSwitchExample", LOG_LEVEL_INFO);
		LogComponentEnable ("OpenFlowInterface", LOG_LEVEL_INFO);
		LogComponentEnable ("OpenFlowSwitchNetDevice", LOG_LEVEL_INFO);
	}

	NS_LOG_INFO ("Create nodes.");
	NodeContainer terminals;
	terminals.Create (4);

	NodeContainer csmaSwitch;
	csmaSwitch.Create (1);

	NS_LOG_INFO ("Build Topology.");
	CsmaHelper csma;
	csma.SetChannelAttribute ("DataRate", DataRateValue (5000000));
	csma.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));

	// Create the csma links, from each terminal to the switch.
	NetDeviceContainer terminalDevices;
	NetDeviceContainer switchDevices;
	for (int i = 0; i < 4; i++)
	{
		NetDeviceContainer link = csma.Install (NodeContainer (terminals.Get (i), csmaSwitch));
		terminalDevices.Add (link.Get (0));
		switchDevices.Add (link.Get (1));
	}

	// Create the switch netdevice, which will do the packet switching
	Ptr<Node> switchNode = csmaSwitch.Get (0);
	OpenFlowSwitchHelper swtch;

	if (vlan)
	{
		Ptr<ns3::ofi::VlanController> controller = CreateObject<ns3::ofi::VlanController> ();
		swtch.Install (switchNode, switchDevices, controller);
	}

	// Add internet stack to the terminals
	InternetStackHelper internet;
	internet.Install (terminals);

	// We have got the "hardware" in place. Now we need to add IP addresses.
	NS_LOG_INFO ("Assign IP Addresses.");
	Ipv4AddressHelper ipv4;
	ipv4.SetBase ("10.1.1.0", "255.255.255.0");
	ipv4.Assign (terminalDevices);

	// Create an On-Off application to send UDP datagrams from n0 to n1.
	NS_LOG_INFO ("Create Applications.");
	uint16_t port = 9; // Discard port

	OnOffHelper onoff ("ns3::UdpSocketFactory", Address (InetSocketAddress (Ipv4Address ("10.1.1.2"), port)));
	onoff.SetConstantRate (DataRate ("500kb/s"));

	ApplicationContainer app = onoff.Install (terminals.Get (0));

	// Start the application
	app.Start (Seconds (1.0));
	app.Stop (Seconds (10.0));

	// Create ana optional packet sink to receive these packets
	PacketSinkHelpert  sink ("ns3::UdpSocketFactory", Address (InetSocketAddress (Ipv4Address::GetAny(), port)));
	app = sink.Install (terminals.Get (1));
	app.Start (Seconds (0.0));

	// openflow-switch.cc line 174
