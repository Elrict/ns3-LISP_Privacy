/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright 2019 University of Liège
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Emeline Marechal <emeline.marechal1@gmail.com>
 */

#include "ns3/test.h"
#include "ns3/ptr.h"
#include "ns3/log.h"
#include "ns3/abort.h"
#include "ns3/uinteger.h"
#include "ns3/object.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;
NS_LOG_COMPONENT_DEFINE ("NatTestSuite");
static uint16_t DONT_CARE = 0;

class PacketTuple {

public:
    PacketTuple (Address src, Address dest, uint16_t srcPort, uint16_t destPort);
    Address m_src;
    Address m_dest;
    uint16_t m_srcPort;
    uint16_t m_destPort;

    bool IsEqual (const PacketTuple& rhs);
  };

PacketTuple::PacketTuple (Address src, Address dest, uint16_t srcPort, uint16_t destPort)
: m_src (src), m_dest (dest), m_srcPort (srcPort), m_destPort (destPort)
{

}

bool
PacketTuple::IsEqual(const PacketTuple& rhs)
{
  if (m_src == rhs.m_src && m_dest == rhs.m_dest)
    {
      return ( ( (m_srcPort == rhs.m_srcPort) || (m_srcPort == DONT_CARE) || (rhs.m_srcPort == DONT_CARE) )
      && ((m_destPort == rhs.m_destPort) || (m_destPort == DONT_CARE) || (rhs.m_destPort == DONT_CARE)) );
    }
    else
      return false;
}

PacketTuple 
ExtractPacketTuple (const Ipv4Header & ipHeader, Ptr<Packet> p)
{
  uint16_t srcPort;
  uint16_t destPort;
  if (ipHeader.GetProtocol () == MY_IPPROTO_TCP)
    {
      TcpHeader tcp;
      p->RemoveHeader (tcp);
      srcPort = tcp.GetSourcePort ();
      destPort = tcp.GetDestinationPort ();
    }
    else if (ipHeader.GetProtocol () == MY_IPPROTO_UDP)
    {
      UdpHeader udp;
      p->RemoveHeader (udp);
      srcPort = udp.GetSourcePort ();
      destPort = udp.GetDestinationPort ();
    }

  return PacketTuple (ipHeader.GetSource (), ipHeader.GetDestination (), srcPort, destPort);
}

// ================================================================================================

class NatDynamic : public TestCase 
{
public:
  NatDynamic ();
  virtual ~NatDynamic ();
private:
  virtual void DoRun (void);
  virtual void DoSetup (void);

  TestVectors<PacketTuple> m_eventsDrop;
  TestVectors<PacketTuple> m_eventsSend;
  TestVectors<PacketTuple> m_eventsReceive;

  std::vector<PacketTuple> m_expectedEventsSend;
  std::vector<PacketTuple> m_expectedEventsReceive;

  void DropSink (const Ipv4Header & ipHeader, Ptr<const Packet> p, Ipv4L3Protocol::DropReason reason, Ptr<Ipv4> ipv4, uint32_t interface);
  void RxSink (Ptr<const Packet> p, Ptr<Ipv4> ipv4, uint32_t interface);
  void TxSink (Ptr<const Packet> p, Ptr<Ipv4> ipv4, uint32_t interface);

};

NatDynamic::NatDynamic ()
  : TestCase ("Check dynamic NAT: outgoing and incoming packets")
{
}

NatDynamic::~NatDynamic ()
{
}

void 
NatDynamic::DropSink (const Ipv4Header & ipHeader, Ptr<const Packet> p, Ipv4L3Protocol::DropReason reason, Ptr<Ipv4> ipv4, uint32_t interface)
{
  if (reason == Ipv4L3Protocol::DROP_NETFILTER)
  {
    m_eventsDrop.Add (ExtractPacketTuple (ipHeader, p->Copy ()));
  }
}

void
NatDynamic::RxSink (Ptr<const Packet> p, Ptr<Ipv4> ipv4, uint32_t interface)
{
  Ptr<Packet> packetCopy = p->Copy ();
  Ipv4Header ipHeader;
  packetCopy->RemoveHeader (ipHeader);
  m_eventsReceive.Add (ExtractPacketTuple (ipHeader, packetCopy));
}


void
NatDynamic::TxSink (Ptr<const Packet> p, Ptr<Ipv4> ipv4, uint32_t interface)
{
  Ptr<Packet> packetCopy = p->Copy ();
  Ipv4Header ipHeader;
  packetCopy->RemoveHeader (ipHeader);
  m_eventsSend.Add (ExtractPacketTuple (ipHeader, packetCopy));
}

void 
NatDynamic::DoSetup (void)
{
  m_expectedEventsReceive.push_back (PacketTuple (Ipv4Address ("10.1.2.2"), Ipv4Address ("192.168.1.1"), DONT_CARE, 9));
  m_expectedEventsSend.push_back (PacketTuple (Ipv4Address ("192.168.1.2"), Ipv4Address ("192.168.1.1"), 49163, 9));
  m_expectedEventsReceive.push_back (PacketTuple (Ipv4Address ("192.168.1.1"), Ipv4Address ("192.168.1.2"), 9, 49163));
  m_expectedEventsSend.push_back (PacketTuple (Ipv4Address ("192.168.1.1"), Ipv4Address ("10.1.2.2"), 9, DONT_CARE));
}

void
NatDynamic::DoRun (void)
{
  // Topology: n0 <----> n1 <-----> n2
  //               Out        NATed


  /*--------------------*\
           SETUP
  \*--------------------*/
  NodeContainer nodes;
  nodes.Create (3);

  NodeContainer n0n1 = NodeContainer (nodes.Get (0), nodes.Get (1));
  NodeContainer n1n2 = NodeContainer (nodes.Get (1), nodes.Get (2));

  InternetStackHelper internet;
  internet.Install(nodes);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer d0d1 = p2p.Install (n0n1);
  NetDeviceContainer d1d2 = p2p.Install (n1n2);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i0i1 = ipv4.Assign (d0d1);
  ipv4.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i1i2 = ipv4.Assign (d1d2);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  /* NAT */
  Ipv4NatHelper natHelper;
  Ptr<Ipv4Nat> nat = natHelper.Install (nodes.Get (1));

  NS_TEST_ASSERT_MSG_EQ (nat->GetNDynamicRules (), 0, "dynamic list is not initialized empty");
  NS_TEST_ASSERT_MSG_EQ (nat->GetNDynamicTuples (), 0, "dynamic tuple is not initialized empty");

  nat->SetInside (2);
  nat->SetOutside (1);
  nat->AddAddressPool (Ipv4Address ("192.168.1.2"), Ipv4Mask ("255.255.255.255"));
  nat->AddPortPool (49163, 49173);
  Ipv4DynamicNatRule rule (Ipv4Address ("10.1.2.0"), Ipv4Mask ("255.255.255.0"));
  nat->AddDynamicRule (rule);

  NS_TEST_ASSERT_MSG_EQ (nat->GetNDynamicRules (), 1, "Failed to add dynamic rule");

  /* Applications */
  UdpEchoServerHelper echoServer (9);

  ApplicationContainer serverApps = echoServer.Install (nodes.Get (0));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClient (i0i1.GetAddress (0), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (1));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps = echoClient.Install (nodes.Get (2));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  /* Trace sinks */
  Ptr<Ipv4L3Protocol> ipv4L3 = nodes.Get (1)->GetObject<Ipv4L3Protocol> ();
  ipv4L3->TraceConnectWithoutContext ("Drop", MakeCallback (&NatDynamic::DropSink, this));
  ipv4L3->TraceConnectWithoutContext ("Rx", MakeCallback (&NatDynamic::RxSink, this));
  ipv4L3->TraceConnectWithoutContext ("Tx", MakeCallback (&NatDynamic::TxSink, this));
  

  /* SIMULATION */
  //p2p.EnablePcapAll ("NAT_testSuite");
  Simulator::Run ();
  Simulator::Destroy ();

  /*--------------------*\
           CHECKS
  \*--------------------*/
  const uint32_t N_EVENTS = 2;
  /* Check that no packet was dropped */
  NS_TEST_ASSERT_MSG_EQ (m_eventsDrop.GetN (), 0, "Unexpected drop of packet");

  /* Check chain of events of packet through NAT */
  NS_TEST_ASSERT_MSG_EQ (m_eventsSend.GetN (), N_EVENTS, "Unexpected number of Send events");
  NS_TEST_ASSERT_MSG_EQ (m_eventsReceive.GetN (), N_EVENTS, "Unexpected number of Receive events");

  
  for (uint32_t i = 0; i < N_EVENTS; ++i)
  {
    /* Check receive events */
    PacketTuple pt = m_eventsReceive.Get (i);
    PacketTuple expected_pt = m_expectedEventsReceive.at (i);

    NS_TEST_ASSERT_MSG_EQ (pt.IsEqual(expected_pt), true, "Unexpected Receive event: " << i <<": Got " 
      << Ipv4Address::ConvertFrom(pt.m_src) <<" "
      << Ipv4Address::ConvertFrom(pt.m_dest) << " " 
      << pt.m_srcPort << " " 
      << pt.m_destPort <<". Expected " 
      << Ipv4Address::ConvertFrom(expected_pt.m_src) << " "
      << Ipv4Address::ConvertFrom(expected_pt.m_dest) << " " 
      << expected_pt.m_srcPort << " " 
      << expected_pt.m_destPort);

    /* Check Send events */
    pt = m_eventsSend.Get (i);
    expected_pt = m_expectedEventsSend.at (i);

    NS_TEST_ASSERT_MSG_EQ (pt.IsEqual(expected_pt), true, "Unexpected Send event: " << i <<": Got " 
      << Ipv4Address::ConvertFrom(pt.m_src) <<" "
      << Ipv4Address::ConvertFrom(pt.m_dest) << " " 
      << pt.m_srcPort << " " 
      << pt.m_destPort <<". Expected " 
      << Ipv4Address::ConvertFrom(expected_pt.m_src) << " " 
      << Ipv4Address::ConvertFrom(expected_pt.m_dest) << " " 
      << expected_pt.m_srcPort << " " 
      << expected_pt.m_destPort);
  }
  
}
// ===================================================================================

class NatDynamicDrop : public TestCase 
{
public:
  NatDynamicDrop ();
  virtual ~NatDynamicDrop ();
private:
  virtual void DoRun (void);
  virtual void DoSetup (void);

  TestVectors<PacketTuple> m_eventsDrop;

  std::vector<PacketTuple> m_expectedDrops;

  void DropSink (const Ipv4Header & ipHeader, Ptr<const Packet> p, Ipv4L3Protocol::DropReason reason, Ptr<Ipv4> ipv4, uint32_t interface);

};

NatDynamicDrop::NatDynamicDrop ()
  : TestCase ("Check dynamic NAT: drop of incoming packet with no match to a rule")
{
}

NatDynamicDrop::~NatDynamicDrop ()
{
}

void 
NatDynamicDrop::DropSink (const Ipv4Header & ipHeader, Ptr<const Packet> p, Ipv4L3Protocol::DropReason reason, Ptr<Ipv4> ipv4, uint32_t interface)
{
  if (reason == Ipv4L3Protocol::DROP_NETFILTER)
  {
    m_eventsDrop.Add (ExtractPacketTuple (ipHeader, p->Copy ()));
  }
}

void 
NatDynamicDrop::DoSetup (void)
{
  m_expectedDrops.push_back (PacketTuple (Ipv4Address ("192.168.1.1"), Ipv4Address ("10.1.2.2"), DONT_CARE, 9));
}

void
NatDynamicDrop::DoRun (void)
{
  // Topology: n0 <----> n1 <-----> n2
  //               Out        NATed


  /*--------------------*\
           SETUP
  \*--------------------*/
  NodeContainer nodes;
  nodes.Create (3);

  NodeContainer n0n1 = NodeContainer (nodes.Get (0), nodes.Get (1));
  NodeContainer n1n2 = NodeContainer (nodes.Get (1), nodes.Get (2));

  InternetStackHelper internet;
  internet.Install(nodes);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer d0d1 = p2p.Install (n0n1);
  NetDeviceContainer d1d2 = p2p.Install (n1n2);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i0i1 = ipv4.Assign (d0d1);
  ipv4.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i1i2 = ipv4.Assign (d1d2);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  /* NAT */
  Ipv4NatHelper natHelper;
  Ptr<Ipv4Nat> nat = natHelper.Install (nodes.Get (1));

  NS_TEST_ASSERT_MSG_EQ (nat->GetNDynamicRules (), 0, "dynamic list is not initialized empty");
  NS_TEST_ASSERT_MSG_EQ (nat->GetNDynamicTuples (), 0, "dynamic tuple is not initialized empty");

  nat->SetInside (2);
  nat->SetOutside (1);
  nat->AddAddressPool (Ipv4Address ("192.168.1.2"), Ipv4Mask ("255.255.255.255"));
  nat->AddPortPool (49163, 49173);
  Ipv4DynamicNatRule rule (Ipv4Address ("10.1.2.0"), Ipv4Mask ("255.255.255.0"));
  nat->AddDynamicRule (rule);

  NS_TEST_ASSERT_MSG_EQ (nat->GetNDynamicRules (), 1, "Failed to add dynamic rule");

  /* Applications */
  UdpEchoServerHelper echoServer (9);

  ApplicationContainer serverApps = echoServer.Install (nodes.Get (2));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClient (i1i2.GetAddress (1), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (1));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  /* Trace sinks */
  Ptr<Ipv4L3Protocol> ipv4L3 = nodes.Get (1)->GetObject<Ipv4L3Protocol> ();
  ipv4L3->TraceConnectWithoutContext ("Drop", MakeCallback (&NatDynamicDrop::DropSink, this));
  

  /* SIMULATION */
  //p2p.EnablePcapAll ("NAT_testSuiteDrop");
  Simulator::Run ();
  Simulator::Destroy ();

  /*--------------------*\
           CHECKS
  \*--------------------*/
  const uint32_t N_EVENTS = 1;
  /* Check that no packet was dropped */
  NS_TEST_ASSERT_MSG_EQ (m_eventsDrop.GetN (), 1, "Unexpected number of dropped packets");

  
  for (uint32_t i = 0; i < N_EVENTS; ++i)
  {
    /* Check Drop events */
    PacketTuple pt = m_eventsDrop.Get (i);
    PacketTuple expected_pt = m_expectedDrops.at (i);

    NS_TEST_ASSERT_MSG_EQ (pt.IsEqual(expected_pt), true, "Unexpected Drop event: " << i <<": Got " 
      << Ipv4Address::ConvertFrom(pt.m_src) <<" "
      << Ipv4Address::ConvertFrom(pt.m_dest) << " " 
      << pt.m_srcPort << " " 
      << pt.m_destPort <<". Expected " 
      << Ipv4Address::ConvertFrom(expected_pt.m_src) << " " 
      << Ipv4Address::ConvertFrom(expected_pt.m_dest) << " " 
      << expected_pt.m_srcPort << " " 
      << expected_pt.m_destPort);
  }
  
}

// ===================================================================================

class Ipv4NatTestSuite : public TestSuite
{
public:
  Ipv4NatTestSuite ();
};

Ipv4NatTestSuite::Ipv4NatTestSuite ()
  : TestSuite ("ipv4-nat", UNIT)
{
  AddTestCase (new NatDynamic, TestCase::QUICK);
  AddTestCase (new NatDynamicDrop, TestCase::QUICK);
}

static Ipv4NatTestSuite ipv4NatTestSuite;

