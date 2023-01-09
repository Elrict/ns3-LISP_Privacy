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
NS_LOG_COMPONENT_DEFINE ("MnBehindNatTestSuite");

class MnBehindNatTestCase : public TestCase
{
public:
  MnBehindNatTestCase ();
  virtual ~MnBehindNatTestCase ();

private:
  virtual void DoRun (void);

  bool m_receivedPacket;
  void RxSink (Ptr<const Packet> p);
};

MnBehindNatTestCase::MnBehindNatTestCase ()
  : TestCase ("MN LISP test case: checks MN behind NAT configuration"), m_receivedPacket(false)
{
}

MnBehindNatTestCase::~MnBehindNatTestCase ()
{
}

void 
MnBehindNatTestCase::RxSink (Ptr<const Packet> p)
{
  m_receivedPacket = true;
}

void
MnBehindNatTestCase::DoRun (void)
{
  
  /* Topology:                    MR/MS (n6/n7)         
                                    |
                xTR1 (n1) <----> R1 (n2) <-----> NAT (n3) <-----> R2 (DHCP) (n4)
                /               (non-LISP)                         (non-LISP)
               /                    |                                 \
            n0 (non-LISP)          RTR (n8)                       LISP-MN (n5)
  */

  PacketMetadata::Enable();

  LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("MnBehindNatTestSuite", LOG_LEVEL_INFO);
  //LogComponentEnable ("LispHelper", LOG_LEVEL_ALL);
  //LogComponentEnable ("Ipv4L3Protocol", LOG_LEVEL_DEBUG);
  //LogComponentEnable ("Ipv4Nat", LOG_LEVEL_DEBUG);
  //LogComponentEnable ("MapResolverDdt", LOG_LEVEL_DEBUG);
  //LogComponentEnable ("MapServerDdt", LOG_LEVEL_DEBUG);
  //LogComponentEnable ("LispEtrItrApplication", LOG_LEVEL_DEBUG);
  //LogComponentEnable ("LispOverIp", LOG_LEVEL_DEBUG);
  //LogComponentEnable ("LispOverIpv4Impl", LOG_LEVEL_DEBUG);
  //LogComponentEnable ("SimpleMapTables", LOG_LEVEL_DEBUG);
  //LogComponentEnable ("InfoRequestMsg", LOG_LEVEL_DEBUG);
  //LogComponentEnable ("NatLcaf", LOG_LEVEL_DEBUG);

  float echoServerStart = 0.0;
  float echoClientStart = 5.0;
  float xTRAppStart = 2.0;
  float mrStart = 0.0;
  float msStart = 0.0;
  float dhcpServerStart = 0.0;

  float dhcp_collect = 1.0;
  Config::SetDefault ("ns3::DhcpClient::Collect", TimeValue (Seconds (dhcp_collect)));

  /* -------------------------------------- *\
                NODE CREATION
  \* -------------------------------------- */
  NodeContainer nodes;
  nodes.Create (9);

  NodeContainer n0_xTR1 = NodeContainer (nodes.Get (0), nodes.Get (1));
  NodeContainer xTR1_R1 = NodeContainer (nodes.Get (1), nodes.Get (2));
  NodeContainer R1_NAT = NodeContainer (nodes.Get (2), nodes.Get (3));
  NodeContainer NAT_R2 = NodeContainer (nodes.Get (3), nodes.Get (4));
  NodeContainer R2_MN = NodeContainer (nodes.Get (4), nodes.Get (5));
  NodeContainer R1_MR = NodeContainer (nodes.Get (2), nodes.Get (6));
  NodeContainer R1_MS = NodeContainer (nodes.Get (2), nodes.Get (7));
  NodeContainer R1_RTR = NodeContainer (nodes.Get (2), nodes.Get (8));

  InternetStackHelper internet;
  internet.Install(nodes);

  /* -------------------------------------- *\
                P2P LINKS
  \* -------------------------------------- */
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer dn0_dxTR1 = p2p.Install (n0_xTR1);
  NetDeviceContainer dxTR1_dR1 = p2p.Install (xTR1_R1);
  NetDeviceContainer dR1_dNAT = p2p.Install (R1_NAT);
  NetDeviceContainer dNAT_dR2 = p2p.Install (NAT_R2);
  NetDeviceContainer dR1_dMR = p2p.Install (R1_MR);
  NetDeviceContainer dR1_dMS = p2p.Install (R1_MS);
  NetDeviceContainer dR1_dRTR = p2p.Install (R1_RTR);

  /* -------------------------------------- *\
                ADDRESSES
  \* -------------------------------------- */
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer in0_ixTR1 = ipv4.Assign (dn0_dxTR1);
  ipv4.SetBase ("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer ixTR1_iR1 = ipv4.Assign (dxTR1_dR1);
  ipv4.SetBase ("192.168.2.0", "255.255.255.0");
  Ipv4InterfaceContainer iR1_iNAT = ipv4.Assign (dR1_dNAT);
  ipv4.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer iNAT_iR2 = ipv4.Assign (dNAT_dR2);
  ipv4.SetBase ("192.168.3.0", "255.255.255.0");
  Ipv4InterfaceContainer iR1_iMR = ipv4.Assign (dR1_dMR);
  ipv4.SetBase ("192.168.4.0", "255.255.255.0");
  Ipv4InterfaceContainer iR1_iMS = ipv4.Assign (dR1_dMS);
  ipv4.SetBase ("192.168.5.0", "255.255.255.0");
  Ipv4InterfaceContainer iR1_iRTR = ipv4.Assign (dR1_dRTR);

  /* -------------------------------------- *\
                ROUTING
  \* -------------------------------------- */
  /* --- Global Routing --- */
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
  /* --- Static routing --- */
  Ipv4StaticRoutingHelper ipv4SrHelper;
  // For R21
  Ptr<Ipv4> ipv4Protocol = nodes.Get (4)->GetObject<Ipv4> ();
  Ptr<Ipv4StaticRouting> ipv4Stat = ipv4SrHelper.GetStaticRouting (ipv4Protocol);
  ipv4Stat->AddNetworkRouteTo (Ipv4Address ("10.1.3.0"),
             Ipv4Mask ("255.255.255.0"),
             Ipv4Address ("10.1.3.254"), 2, 0);
  // For NAT
  ipv4Protocol = nodes.Get (3)->GetObject<Ipv4> ();
  ipv4Stat = ipv4SrHelper.GetStaticRouting (ipv4Protocol);
  ipv4Stat->AddNetworkRouteTo (Ipv4Address ("10.1.3.0"),
             Ipv4Mask ("255.255.255.0"),
             Ipv4Address ("10.1.2.2"), 2, 0);
  
  // For xTR1
  ipv4Protocol = nodes.Get (1)->GetObject<Ipv4> ();
  ipv4Stat = ipv4SrHelper.GetStaticRouting (ipv4Protocol); 
  ipv4Stat->AddNetworkRouteTo (Ipv4Address ("172.16.0.1"), //ITR can accept traffic towards 172.16.0.1
             Ipv4Mask ("255.255.255.0"),
             Ipv4Address ("192.168.1.2"), 2, 0);

  // For RTR
  ipv4Protocol = nodes.Get (8)->GetObject<Ipv4> ();
  ipv4Stat = ipv4SrHelper.GetStaticRouting (ipv4Protocol); 
  ipv4Stat->AddNetworkRouteTo (Ipv4Address ("10.1.3.0"), //RTR can accept traffic towards 10.1.3.0
             Ipv4Mask ("255.255.255.0"),
             Ipv4Address ("192.168.5.1"), 1, 0);

  // For MS
  ipv4Protocol = nodes.Get (7)->GetObject<Ipv4> ();
  ipv4Stat = ipv4SrHelper.GetStaticRouting (ipv4Protocol); 
  ipv4Stat->AddNetworkRouteTo (Ipv4Address ("10.1.3.0"), //RTR can accept traffic towards 10.1.3.0
             Ipv4Mask ("255.255.255.0"),
             Ipv4Address ("192.168.4.1"), 1, 0);

  // For R1
  ipv4Protocol = nodes.Get (2)->GetObject<Ipv4> ();
  ipv4Stat = ipv4SrHelper.GetStaticRouting (ipv4Protocol); 
  ipv4Stat->AddNetworkRouteTo (Ipv4Address ("10.1.3.0"), //RTR can accept traffic towards 10.1.3.0
             Ipv4Mask ("255.255.255.0"),
             Ipv4Address ("192.168.2.2"), 2, 0);

  /* -------------------------------------- *\
                      NAT
  \* -------------------------------------- */
  
  Ipv4NatHelper natHelper;
  Ptr<Ipv4Nat> nat = natHelper.Install (nodes.Get (3));
  nat->SetInside (2);
  nat->SetOutside (1);
  nat->AddAddressPool (Ipv4Address ("192.168.2.2"), Ipv4Mask ("255.255.255.255"));
  nat->AddPortPool (49153, 49163);
  Ipv4DynamicNatRule rule (Ipv4Address ("10.1.2.0"), Ipv4Mask ("255.255.255.0"));
  nat->AddDynamicRule (rule);
  Ipv4DynamicNatRule rule2 (Ipv4Address ("10.1.3.0"), Ipv4Mask ("255.255.255.0"));
  nat->AddDynamicRule (rule2);

  /* -------------------------------------- *\
                    LISP-MN
  \* -------------------------------------- */
  NS_LOG_DEBUG ("Installing LISP-MN");
  LispMNHelper lispMnHelper(nodes.Get (5), Ipv4Address ("172.16.0.1"));
  lispMnHelper.SetPointToPointHelper (p2p);

  lispMnHelper.SetDhcpServerStartTime (Seconds(dhcpServerStart));
  lispMnHelper.SetEndTime (Seconds(20.0));

  NodeContainer attachementPoints = NodeContainer (nodes.Get (4));
  lispMnHelper.SetRoutersAttachementPoints (attachementPoints);
  lispMnHelper.SetRouterSubnet (Ipv4Address ("10.1.3.0"), Ipv4Mask ("255.255.255.0"));

  lispMnHelper.Install ();
  NS_LOG_DEBUG ("Finish Installing LISP-MN");
  
  /* -------------------------------------- *\
                      LISP
  \* -------------------------------------- */
  NS_LOG_INFO("Installing LISP...");
  NodeContainer lispRouters = NodeContainer (nodes.Get (1), nodes.Get (5), nodes.Get (6), nodes.Get (7));
  lispRouters.Add (nodes.Get (8));
  NodeContainer xTRs = NodeContainer (nodes.Get (1), nodes.Get (5));
  xTRs.Add (nodes.Get (8));
  NodeContainer MR = NodeContainer(nodes.Get (6));
  NodeContainer MS = NodeContainer(nodes.Get (7));

  // Data Plane
  LispHelper lispHelper;
  lispHelper.BuildRlocsSet ("src/internet/test/lisp-test/mn-behind-nat/MN_behind_NAT_lisp_rlocs.txt");
  lispHelper.SetRtrs(NodeContainer(nodes.Get (8)));
  lispHelper.Install(lispRouters);
  NS_LOG_INFO("Lisp router installed");
  lispHelper.BuildMapTables2("src/internet/test/lisp-test/mn-behind-nat/MN_behind_NAT_lisp_rlocs_config_xml.txt");
  lispHelper.InstallMapTables(lispRouters);

  NS_LOG_INFO("Map tables installed");

  // Control Plane
  LispEtrItrAppHelper lispAppHelper;
  Ptr<Locator> mrLocator = Create<Locator> (iR1_iMR.GetAddress (1));
  lispAppHelper.AddMapResolverRlocs (mrLocator);
  lispAppHelper.AddMapServerAddress (static_cast<Address> (iR1_iMS.GetAddress (1)));
  ApplicationContainer mapResClientApps = lispAppHelper.Install (xTRs);
  mapResClientApps.Start (Seconds (xTRAppStart));
  mapResClientApps.Stop (Seconds (20.0));

  // MR application
  MapResolverDdtHelper mrHelper;
  NS_LOG_INFO("Address of MS: " << iR1_iMS.GetAddress (1));
  mrHelper.SetMapServerAddress (static_cast<Address> (iR1_iMS.GetAddress (1)));
  ApplicationContainer mrApps = mrHelper.Install (MR);
  mrApps.Start (Seconds (mrStart));
  mrApps.Stop (Seconds (20.0));

  // MS application
  MapServerDdtHelper msHelper;
  msHelper.SetRtrAddress(iR1_iRTR.GetAddress (1));
  ApplicationContainer msApps = msHelper.Install (MS);
  msApps.Start (Seconds (msStart));
  msApps.Stop (Seconds (20.0));

  NS_LOG_INFO("LISP succesfully aggregated");
  
  /* -------------------------------------- *\
                APPLICATIONS
  \* -------------------------------------- */
  UdpEchoServerHelper echoServer (9);

  ApplicationContainer serverApps = echoServer.Install (nodes.Get (0));
  serverApps.Start (Seconds (echoServerStart));
  serverApps.Stop (Seconds (20.0));

  UdpEchoClientHelper echoClient (in0_ixTR1.GetAddress (0), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (10));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps = echoClient.Install (nodes.Get (5));
  clientApps.Start (Seconds (echoClientStart));
  clientApps.Stop (Seconds (20.0));
  clientApps.Get (0)->TraceConnectWithoutContext ("Rx", MakeCallback (&MnBehindNatTestCase::RxSink, this));

  /* -------------------------------------- *\
                SIMULATION
  \* -------------------------------------- */
  //p2p.EnablePcapAll ("MN_behind_NAT_test-suite");

  Simulator::Run ();
  Simulator::Destroy ();
  
  /*--------------------*\
           CHECKS
  \*--------------------*/
  NS_TEST_ASSERT_MSG_EQ (m_receivedPacket, true, "No communication between both ends");
}

// ===================================================================================
class MnBehindNatTestSuite : public TestSuite
{
public:
 MnBehindNatTestSuite ();
};
MnBehindNatTestSuite::MnBehindNatTestSuite ()
  : TestSuite ("mn-behind-nat-lisp", UNIT)
{
  AddTestCase (new MnBehindNatTestCase, TestCase::QUICK);
}

static MnBehindNatTestSuite mnBehindNatTestSuite;