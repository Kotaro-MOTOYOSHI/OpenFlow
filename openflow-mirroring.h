/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#ifndef OPENFLOW_MIRRORING_H
#define OPENFLOW_MIRRORING_H

#include "openflow-vlan-controller.h"

class MirroringController : public VlanController
{
public:
	static ns3::TypeId GetTypeId (void);

	ns3::TypeId GetInstanceTypeId () const;

	// continue...
