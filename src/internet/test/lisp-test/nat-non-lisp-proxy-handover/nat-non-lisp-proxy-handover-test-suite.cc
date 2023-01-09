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

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/virtual-net-device.h"
#include "ns3/dhcp-helper.h"

#include "ns3/test.h"

using namespace ns3;
NS_LOG_COMPONENT_DEFINE ("NatNonLispProxyHandoverTestSuite");

class NatNonLispProxyHandoverTestCase : public TestCase
{
public:
  NatNonLispProxyHandoverTestCase ();
  virtual ~NatNonLispProxyHandoverTestCase ();

private:
  virtual void DoRun (void);

  bool m_receivedPacket_first_interface;
  bool m_receivedPacket_second_interface;
  Time m_handTime = Seconds(8.0);
  void RxSink (Ptr<const Packet> p);
};

NatNonLispProxyHandoverTestCase::NatNonLispProxyHandoverTestCase ()
  : TestCase ("MN LISP test case: checks nat -> nonLisp handover"), m_receivedPacket_first_interface(false), m_receivedPacket_second_interface(false)
{
}

NatNonLispProxyHandoverTestCase::~NatNonLispProxyHandoverTestCase ()
{
}

void 
NatNonLispProxyHandoverTestCase::RxSink (Ptr<const Packet> p)
{
  if (Simulator::Now () < m_handTime)
    m_receivedPacket_first_interface = true;
  else
    m_receivedPacket_second_interface = true;
}

void
NatNonLispProxyHandoverTestCase::DoRun (void)
{
  
  /* Topology:  
                                         R2 <--------------> LISP-MN (n11)
                             MR/MS     /(n3)                 /
                            (n6/n7)   /                     /
                                |    /                     /
                                |   /                     /
    n0 <----> R4 (n1) <----> R1 (n2)                     /
      (non-LISP)              / |   \                   /
                             /  |    \                 /
                  PETR/PITR /   |     \               /
                  (n9/n10)      |      \ NAT <----> R3
                               RTR        (n4)      (n5)
                               (n8)
  */

  PacketMetadata::Enable();
  
  /* -------------------------------------- *\
                CONFIGURATION
  \* -------------------------------------- */

  LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);
  //LogComponentEnable ("NatNonLispProxyHandoverTestSuite", LOG_LEVEL_INFO);
  //LogComponentEnable ("LispHelper", LOG_LEVEL_ALL);
  //LogComponentEnable ("Ipv4L3Protocol", LOG_LEVEL_DEBUG);
  //LogComponentEnable ("Ipv4Nat", LOG_LEVEL_DEBUG);
  //LogComponentEnable ("MapResolverDdt", LOG_LEVEL_DEBUG);
  //LogComponentEnable ("MapServerDdt", LOG_LEVEL_DEBUG);
  //LogComponentEnable ("LispEtrItrApplication", LOG_LEVEL_DEBUG);
  //LogComponentEnable ("LispOverIp", LOG_LEVEL_DEBUG);
  //LogComponentEnable ("LispOverIpv4Impl", LOG_LEVEL_DEBUG);
  //LogComponentEnable ("SimpleMapTables", LOG_LEVEL_DEBUG);

  Time echoServerStart = Seconds(0.0);
  Time echoClientStart = Seconds(5.0);
  Time xTRAppStart = Seconds(2.0);
  Time mrStart = Seconds(0.0);
  Time msStart = Seconds(0.0);
  Time dhcpServerStart = Seconds(0.0);
  Time EndTime = Seconds(15.0);

  float dhcp_collect = 1.0;
  Config::SetDefault ("ns3::DhcpClient::Collect", TimeValue (Seconds (dhcp_collect)));
    

  /* -------------------------------------- *\
                NODE CREATION
  \* -------------------------------------- */
  NodeContainer nodes;
  nodes.Create (12);

  NodeContainer n0_R4 = NodeContainer (nodes.Get (0), nodes.Get (1));
  NodeContainer R4_R1 = NodeContainer (nodes.Get (1), nodes.Get (2));
  NodeContainer R1_R2 = NodeContainer (nodes.Get (2), nodes.Get (3));
  NodeContainer R1_NAT = NodeContainer (nodes.Get (2), nodes.Get (4));
  NodeContainer NAT_R3 = NodeContainer (nodes.Get (4), nodes.Get (5));
  NodeContainer R2_MN = NodeContainer (nodes.Get (3), nodes.Get (11));
  NodeContainer R3_MN = NodeContainer (nodes.Get (5), nodes.Get (11));
  NodeContainer R1_MR = NodeContainer (nodes.Get (2), nodes.Get (6));
  NodeContainer R1_MS = NodeContainer (nodes.Get (2), nodes.Get (7));
  NodeContainer R1_RTR = NodeContainer (nodes.Get (2), nodes.Get (8));
  NodeContainer R1_PETR = NodeContainer (nodes.Get (2), nodes.Get (9));
  NodeContainer R1_PITR = NodeContainer (nodes.Get (2), nodes.Get (10));

  InternetStackHelper internet;
  internet.Install(nodes);

  /* -------------------------------------- *\
                P2P LINKS
  \* -------------------------------------- */
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer dn0_dR4 = p2p.Install (n0_R4);
  NetDeviceContainer dR4_dR1 = p2p.Install (R4_R1);
  NetDeviceContainer dR1_dR2 = p2p.Install (R1_R2);
  NetDeviceContainer dR1_dNAT = p2p.Install (R1_NAT);
  NetDeviceContainer dNAT_dR3 = p2p.Install (NAT_R3);
  NetDeviceContainer dR1_dMR = p2p.Install (R1_MR);
  NetDeviceContainer dR1_dMS = p2p.Install (R1_MS);
  NetDeviceContainer dR1_dRTR = p2p.Install (R1_RTR);
  NetDeviceContainer dR1_dPETR = p2p.Install (R1_PETR);
  NetDeviceContainer dR1_dPITR = p2p.Install (R1_PITR);

  /* -------------------------------------- *\
                ADDRESSES
  \* -------------------------------------- */
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer in0_iR4 = ipv4.Assign (dn0_dR4);
  ipv4.SetBase ("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer iR4_iR1 = ipv4.Assign (dR4_dR1);
  ipv4.SetBase ("192.168.2.0", "255.255.255.0");
  Ipv4InterfaceContainer iR1_iR2 = ipv4.Assign (dR1_dR2);
  ipv4.SetBase ("192.168.3.0", "255.255.255.0");
  Ipv4InterfaceContainer iR1_iNAT = ipv4.Assign (dR1_dNAT);
  ipv4.SetBase ("192.168.4.0", "255.255.255.0");
  Ipv4InterfaceContainer iR1_iMR = ipv4.Assign (dR1_dMR);
  ipv4.SetBase ("192.168.5.0", "255.255.255.0");
  Ipv4InterfaceContainer iR1_iMS = ipv4.Assign (dR1_dMS);
  ipv4.SetBase ("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer iNAT_iR3 = ipv4.Assign (dNAT_dR3);
  ipv4.SetBase ("192.168.6.0", "255.255.255.0");
  Ipv4InterfaceContainer iR1_iRTR = ipv4.Assign (dR1_dRTR);
  ipv4.SetBase ("192.168.7.0", "255.255.255.0");
  Ipv4InterfaceContainer iR1_iPETR = ipv4.Assign (dR1_dPETR);
  ipv4.SetBase ("192.168.8.0", "255.255.255.0");
  Ipv4InterfaceContainer iR1_iPITR = ipv4.Assign (dR1_dPITR);



  /* -------------------------------------- *\
                ROUTING
  \* -------------------------------------- */
  /* --- Global Routing --- */
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
  /* --- Static routing --- */
  Ipv4StaticRoutingHelper ipv4SrHelper;
  // For R2
  Ptr<Ipv4> ipv4Protocol = nodes.Get (3)->GetObject<Ipv4> ();
  Ptr<Ipv4StaticRouting> ipv4Stat = ipv4SrHelper.GetStaticRouting (ipv4Protocol);
  ipv4Stat->AddNetworkRouteTo (Ipv4Address ("10.1.2.0"),
             Ipv4Mask ("255.255.255.0"),
             Ipv4Address ("10.1.2.254"), 2, 0);

  // For R3
  ipv4Protocol = nodes.Get (5)->GetObject<Ipv4> ();
  ipv4Stat = ipv4SrHelper.GetStaticRouting (ipv4Protocol);
  ipv4Stat->AddNetworkRouteTo (Ipv4Address ("10.1.3.0"),
             Ipv4Mask ("255.255.255.0"),
             Ipv4Address ("10.1.3.254"), 2, 0);

  // For NAT
  ipv4Protocol = nodes.Get (4)->GetObject<Ipv4> ();
  ipv4Stat = ipv4SrHelper.GetStaticRouting (ipv4Protocol); 
  ipv4Stat->AddNetworkRouteTo (Ipv4Address ("10.1.3.0"),
             Ipv4Mask ("255.255.255.0"),
             Ipv4Address ("10.1.4.2"), 2, 0);
  
  // For R4
  ipv4Protocol = nodes.Get (1)->GetObject<Ipv4> ();
  ipv4Stat = ipv4SrHelper.GetStaticRouting (ipv4Protocol); 
  ipv4Stat->AddNetworkRouteTo (Ipv4Address ("172.16.0.1"), //forwards legacy traffic
             Ipv4Mask ("255.255.255.0"),
             Ipv4Address ("192.168.1.2"), 2, 0);

  // For MS
  ipv4Protocol = nodes.Get (7)->GetObject<Ipv4> ();
  ipv4Stat = ipv4SrHelper.GetStaticRouting (ipv4Protocol); 
  ipv4Stat->AddNetworkRouteTo (Ipv4Address ("10.1.3.0"), 
             Ipv4Mask ("255.255.255.0"),
             Ipv4Address ("192.168.5.1"), 1, 0);
  ipv4Stat->AddNetworkRouteTo (Ipv4Address ("10.1.2.0"), 
             Ipv4Mask ("255.255.255.0"),
             Ipv4Address ("192.168.5.1"), 1, 0);

  // For R1
  ipv4Protocol = nodes.Get (2)->GetObject<Ipv4> ();
  ipv4Stat = ipv4SrHelper.GetStaticRouting (ipv4Protocol); 
  ipv4Stat->AddNetworkRouteTo (Ipv4Address ("10.1.2.0"), //R1 can accept traffic towards 10.1.2.0
             Ipv4Mask ("255.255.255.0"),
             Ipv4Address ("192.168.2.2"), 2, 0);
  ipv4Stat->AddNetworkRouteTo (Ipv4Address ("172.16.0.1"), //forwards legacy traffic
             Ipv4Mask ("255.255.255.0"),
             Ipv4Address ("192.168.8.2"), 8, 0);

  // For RTR
  ipv4Protocol = nodes.Get (8)->GetObject<Ipv4> ();
  ipv4Stat = ipv4SrHelper.GetStaticRouting (ipv4Protocol); 
  ipv4Stat->AddNetworkRouteTo (Ipv4Address ("10.1.3.0"), //RTR can accept traffic towards 10.1.3.0
             Ipv4Mask ("255.255.255.0"),
             Ipv4Address ("192.168.6.1"), 1, 0);

  // For PITR
  ipv4Protocol = nodes.Get (10)->GetObject<Ipv4> ();
  ipv4Stat = ipv4SrHelper.GetStaticRouting (ipv4Protocol); 
  ipv4Stat->AddNetworkRouteTo (Ipv4Address ("10.1.2.0"), 
             Ipv4Mask ("255.255.255.0"),
             Ipv4Address ("192.168.8.1"), 1, 0);
  ipv4Stat->AddNetworkRouteTo (Ipv4Address ("172.16.0.1"), //PITR can encapsulate traffic towards 172.16.0.1
             Ipv4Mask ("255.255.255.0"),
             Ipv4Address ("192.168.8.1"), 1, 0);

  /* -------------------------------------- *\
                      NAT
  \* -------------------------------------- */
  
  Ipv4NatHelper natHelper;
  Ptr<Ipv4Nat> nat = natHelper.Install (nodes.Get (4));
  nat->SetInside (2);
  nat->SetOutside (1);
  nat->AddAddressPool (Ipv4Address ("192.168.3.2"), Ipv4Mask ("255.255.255.255"));
  nat->AddPortPool (49153, 49163);
  Ipv4DynamicNatRule rule (Ipv4Address ("10.1.4.0"), Ipv4Mask ("255.255.255.0"));
  nat->AddDynamicRule (rule);
  Ipv4DynamicNatRule rule2 (Ipv4Address ("10.1.3.0"), Ipv4Mask ("255.255.255.0"));
  nat->AddDynamicRule (rule2);

  /* -------------------------------------- *\
                    LISP-MN
  \* -------------------------------------- */
  int MN = 11;
  NS_LOG_DEBUG ("Installing LISP-MN");

  LispMNHelper lispMnHelper(nodes.Get (MN), Ipv4Address ("172.16.0.1"));
  lispMnHelper.SetPointToPointHelper (p2p);

  lispMnHelper.SetDhcpServerStartTime (dhcpServerStart);
  lispMnHelper.SetEndTime (EndTime);

  NodeContainer attachementPoints = NodeContainer (nodes.Get (5), nodes.Get (3));
  lispMnHelper.SetRoutersAttachementPoints (attachementPoints);
  lispMnHelper.SetRouterSubnet (Ipv4Address ("10.1.3.0"), Ipv4Mask ("255.255.255.0"));
  lispMnHelper.SetRouterSubnet (Ipv4Address ("10.1.2.0"), Ipv4Mask ("255.255.255.0"));

  lispMnHelper.ScheduleHandover (0, 1, m_handTime);
  lispMnHelper.Install ();

  NS_LOG_DEBUG ("Finish Installing LISP-MN");

  /* -------------------------------------- *\
                      LISP
  \* -------------------------------------- */
  NS_LOG_INFO("Installing LISP...");
  NodeContainer lispRouters = NodeContainer (nodes.Get (6), nodes.Get (7), nodes.Get (MN));
  lispRouters.Add (nodes.Get (8)); // RTR
  lispRouters.Add (nodes.Get (9)); // PETR
  lispRouters.Add (nodes.Get (10)); // PITR
  NodeContainer xTRs = NodeContainer (nodes.Get (MN), nodes.Get (8), nodes.Get (9), nodes.Get (10));
  NodeContainer MR = NodeContainer(nodes.Get (6));
  NodeContainer MS = NodeContainer(nodes.Get (7));

  // Data Plane
  LispHelper lispHelper;
  lispHelper.SetPetrAddress(iR1_iPETR.GetAddress (1));
  lispHelper.SetPetrs(NodeContainer(nodes.Get (9)));
  lispHelper.SetPitrs(NodeContainer(nodes.Get (10)));
  lispHelper.BuildRlocsSet ("src/internet/test/lisp-test/nat-non-lisp-proxy-handover/non_lisp_nat_proxy_handover_lisp_rlocs.txt");
  lispHelper.SetRtrs(NodeContainer(nodes.Get (8)));
  lispHelper.Install(lispRouters);
  NS_LOG_INFO("Lisp router installed");
  lispHelper.BuildMapTables2("src/internet/test/lisp-test/nat-non-lisp-proxy-handover/non_lisp_nat_proxy_handover_lisp_rlocs_config_xml.txt");
  lispHelper.InstallMapTables(lispRouters);

  NS_LOG_INFO("Map tables installed");

  // Control Plane
  LispEtrItrAppHelper lispAppHelper;
  Ptr<Locator> mrLocator = Create<Locator> (iR1_iMR.GetAddress (1));
  lispAppHelper.AddMapResolverRlocs (mrLocator);
  lispAppHelper.AddMapServerAddress (static_cast<Address> (iR1_iMS.GetAddress (1)));
  ApplicationContainer mapResClientApps = lispAppHelper.Install (xTRs);
  mapResClientApps.Start (xTRAppStart);
  mapResClientApps.Stop (EndTime);

  // MR application
  MapResolverDdtHelper mrHelper;
  NS_LOG_INFO("Address of MS: " << iR1_iMS.GetAddress (1));
  mrHelper.SetMapServerAddress (static_cast<Address> (iR1_iMS.GetAddress (1)));
  ApplicationContainer mrApps = mrHelper.Install (MR);
  mrApps.Start (mrStart);
  mrApps.Stop (EndTime);

  // MS application
  MapServerDdtHelper msHelper;
  msHelper.SetRtrAddress(iR1_iRTR.GetAddress (1));
  ApplicationContainer msApps = msHelper.Install (MS);
  msApps.Start (msStart);
  msApps.Stop (EndTime);

  NS_LOG_INFO("LISP succesfully aggregated");
  
  /* -------------------------------------- *\
                APPLICATIONS
  \* -------------------------------------- */

  /* UDP echo application */
  UdpEchoServerHelper echoServer (9);

  ApplicationContainer serverApps = echoServer.Install (nodes.Get (0));
  serverApps.Start (echoServerStart);
  serverApps.Stop (EndTime);

  UdpEchoClientHelper echoClient (in0_iR4.GetAddress (0), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (100));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps = echoClient.Install (nodes.Get (MN));
  clientApps.Start (echoClientStart);
  clientApps.Stop (EndTime);

  clientApps.Get (0)->TraceConnectWithoutContext ("Rx", MakeCallback (&NatNonLispProxyHandoverTestCase::RxSink, this));
  
  /* -------------------------------------- *\
                TRACING
  \* -------------------------------------- */
  //p2p.EnablePcapAll ("nat_non_lisp_proxy_handover");
  
  /* -------------------------------------- *\
                SIMULATION
  \* -------------------------------------- */

  Simulator::Run ();
  Simulator::Destroy ();
  

  /*--------------------*\
           CHECKS
  \*--------------------*/
  NS_TEST_ASSERT_MSG_EQ (m_receivedPacket_first_interface, true, "No communication between both ends");
  NS_TEST_ASSERT_MSG_EQ (m_receivedPacket_second_interface, true, "Handover failed");
}

// ===================================================================================
class NatNonLispProxyHandoverTestSuite : public TestSuite
{
public:
 NatNonLispProxyHandoverTestSuite ();
};
NatNonLispProxyHandoverTestSuite::NatNonLispProxyHandoverTestSuite ()
  : TestSuite ("nat-non-lisp-proxy-handover", UNIT)
{
  AddTestCase (new NatNonLispProxyHandoverTestCase, TestCase::QUICK);
}

static NatNonLispProxyHandoverTestSuite natNonLispProxyHandoverTestSuite;