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
#include "openflow-core-switch-controller.h"
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
		ns3::LogComponentEnable ("OpenFlowInterface", ns3::LOG_LEVEL_INFO);
		ns3::LogComponentEnable ("OpenFlowSwitchNetDevice", ns3::LOG_LEVEL_INFO);
		ns3::LogComponentEnable ("OpenFlowBasicController", ns3::LOG_LEVEL_INFO);
		ns3::LogComponentEnable ("OpenFlowCoreSwitchController", ns3::LOG_LEVEL_INFO);
		ns3::LogComponentEnable ("IpsImitation", ns3::LOG_LEVEL_INFO);
		ns3::LogComponentEnable ("SuperCoreTest", ns3::LOG_LEVEL_INFO);
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
NS_LOG_INFO("81:");
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
		for (int j = 1; j <= 2; j++)
		{
			ns3::NetDeviceContainer link = csma.Install (ns3::NodeContainer (csmaSwitch.Get (j), csmaSwitch.Get (i)));
			switchDevices[j].Add (link.Get (0));
			switchDevices[i].Add (link.Get (1));
		}
	}

	// for i in 3-6; [switch i] -- [switch 2*i+1, 2*i+2]
NS_LOG_INFO("104:");
	
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
	
NS_LOG_INFO("117:");
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
NS_LOG_INFO("128:");
	
	ns3::OpenFlowSwitchHelper openFlowSwitchHelper;
	ns3::Ptr <ns3::Node> switchNode[n_switches];
	
	for (int i = 0; i < n_switches; i++)
	{
		switchNode[i] = csmaSwitch.Get (i);
	}

	// controller create
	ns3::Ptr<IpsImitation> ipsImitation = ns3::CreateObject<IpsImitation> ();
	ns3::Ptr<OpenFlowBasicController> openFlowBasicController = ns3::CreateObject<OpenFlowBasicController> ();
	ns3::Ptr<OpenFlowCoreSwitchController> openFlowCoreSwitchController = ns3::CreateObject<OpenFlowCoreSwitchController> ();

	if (!timeout.IsZero ())
	{
		openFlowBasicController->SetAttribute ("ExpirationTime", ns3::TimeValue (timeout));
		openFlowCoreSwitchController->SetAttribute ("ExpirationTime", ns3::TimeValue (timeout));
	}
	
	// [ips imitation (switch 0)] -- [ipsImitation]
	openFlowSwitchHelper.Install (switchNode[0], switchDevices[0], ipsImitation);

	// [switch 1-2, 7-14] -- [openFlowBasicController]
	for (int i = 1; i <= 2; i++)
	{
		openFlowSwitchHelper.Install (switchNode[i], switchDevices[i], openFlowBasicController);
	}

	for (int i = 7; i < n_switches; i++)
	{
		openFlowSwitchHelper.Install (switchNode[i], switchDevices[i], openFlowBasicController);
	}

	// [switch 3-6] -- [openFlowCoreSwitchController]
	for (int i = 3; i <= 6; i++)
	{
		openFlowSwitchHelper.Install (switchNode[i], switchDevices[i], openFlowCoreSwitchController);
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
	
	// Add internet stack to the terminals
	ns3::InternetStackHelper internet;
	internet.Install (terminals);
	
	// We have got the "hardware" in place. Now we need to add IP Addresses.
	NS_LOG_INFO ("Assign IP Addresses.");
	ns3::Ipv4AddressHelper ipv4;
	ipv4.SetBase ("10.1.1.0", "255.255.255.0");
	ipv4.Assign (terminalDevices);

	// Create an On-Off application to send UDP datagrams from n0 to n1.
	NS_LOG_INFO ("Create Applications.");
	uint16_t port = 9; // Discard port
	
	ns3::OnOffHelper onoff ("ns3::TcpSocketFactory", ns3::Address (ns3::InetSocketAddress (ns3::Ipv4Address ("10.1.1.6"), port)));
	onoff.SetConstantRate (ns3::DataRate ("500kb/s"));

	ns3::ApplicationContainer app = onoff.Install (terminals.Get (8));

	// Start the application
	app.Start (ns3::Seconds (1.0));
	app.Stop (ns3::Seconds (10.0));

	// Create ana optional packet sink to receive these packets
	ns3::PacketSinkHelper sink ("ns3::TcpSocketFactory", ns3::Address (ns3::InetSocketAddress (ns3::Ipv4Address::GetAny(), port)));
	app = sink.Install (terminals.Get (5));
	app.Start (ns3::Seconds (0.0));

	//
	// Create a similar flow from n3 to n2, starting at time 1.1 seconds
	//
	ns3::OnOffHelper onoff2 ("ns3::TcpSocketFactory", ns3::Address (ns3::InetSocketAddress (ns3::Ipv4Address ("10.1.1.11"), port)));
	onoff2.SetConstantRate (ns3::DataRate ("500kb/s"));

	app = onoff2.Install (terminals.Get (8));
	app.Start (ns3::Seconds (2.0));
	app.Stop (ns3::Seconds (10.0));

	// Create ana optional packet sink to receive these packets
	app = sink.Install (terminals.Get (10));
	app.Start (ns3::Seconds (0.0));

	NS_LOG_INFO ("Configure Tracing.");

	//
	// Configure tracing of all enqueue, dequeue, and NetDevice receive events.
	// Trace output will be sent to the file "openflow-switch.tr"
	//
	ns3::AsciiTraceHelper ascii;
	csma.EnableAsciiAll (ascii.CreateFileStream ("supercore.tr"));

	//
	// Also configure some tcpdump traces; each interface will be traced.
	// The output files will be named: openflow-vlan-switch-<nodeId>-<interfaceId>.pcap
	// and can be read by the "tcpdump -r" command (use "-tt" option to display timestamps correctly)
	//
	csma.EnablePcapAll ("supercore", false);

	//
	// Now, do the actual simulation.
	//
	NS_LOG_INFO ("Run Simulation.");
	ns3::Simulator::Run ();
	ns3::Simulator::Destroy ();
	NS_LOG_INFO ("Done.");
}
