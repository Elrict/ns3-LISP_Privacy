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
NS_LOG_COMPONENT_DEFINE ("MnBehindXtrTestSuite");

class MnBehindXtrTestCase : public TestCase
{
public:
  MnBehindXtrTestCase ();
  virtual ~MnBehindXtrTestCase ();

private:
  virtual void DoRun (void);

  bool m_receivedPacket;
  void RxSink (Ptr<const Packet> p);
};

MnBehindXtrTestCase::MnBehindXtrTestCase ()
  : TestCase ("MN LISP test case: checks MN_xTR_xTR_N configuration"), m_receivedPacket(false)
{
}

MnBehindXtrTestCase::~MnBehindXtrTestCase ()
{
}

void 
MnBehindXtrTestCase::RxSink (Ptr<const Packet> p)
{
  m_receivedPacket = true;
}

void
MnBehindXtrTestCase::DoRun (void)
{

  /* Topology:                    MR/MS (n5/n6)         
                                    |
                 xTR1 (n1) <----> R2 (n2) <-----> xTR2 (n3)
                 /DHCP           (non-LISP)         \
                /                                    \
               /                                      \
            LISP-MN (n0)                        n4 (non-LISP)
  */

  PacketMetadata::Enable();

  LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("MnBehindXtrTestSuite", LOG_LEVEL_INFO);
  //LogComponentEnable ("LispHelper", LOG_LEVEL_ALL);
  //LogComponentEnable ("Ipv4L3Protocol", LOG_LEVEL_DEBUG);
  //LogComponentEnable ("MapResolverDdt", LOG_LEVEL_DEBUG);
  //LogComponentEnable ("MapServerDdt", LOG_LEVEL_DEBUG);
  //LogComponentEnable ("LispEtrItrApplication", LOG_LEVEL_DEBUG);
  //LogComponentEnable ("DhcpClient", LOG_LEVEL_FUNCTION);
  //LogComponentEnable ("LispOverIp", LOG_LEVEL_DEBUG);
  //LogComponentEnable ("LispOverIpv4Impl", LOG_LEVEL_DEBUG);
  //LogComponentEnable ("Ipv4Interface", LOG_LEVEL_DEBUG);
  //LogComponentEnable ("SimpleMapTables", LOG_LEVEL_DEBUG);

  float echoServerStart = 0.0;
  float echoClientStart = 6.0;
  float xTRAppStart = 2.0;
  float MNAppStart = 4.0; /* Need to start later, to allow registration 
  process of LISP sites to happen, so that MS can then answer (and encapsulate) 
  the InfoRequest of LISP-MN */
  float mrStart = 0.0;
  float msStart = 0.0;
  float dhcpServerStart = 0.0;

  float dhcp_collect = 1.0;
  Config::SetDefault ("ns3::DhcpClient::Collect", TimeValue (Seconds (dhcp_collect)));
  
  /* -------------------------------------- *\
                NODE CREATION
  \* -------------------------------------- */
  NodeContainer nodes;
  nodes.Create (7);

  NodeContainer MN_xTR1 = NodeContainer (nodes.Get (0), nodes.Get (1));
  NodeContainer xTR1_R2 = NodeContainer (nodes.Get (1), nodes.Get (2));
  NodeContainer R2_xTR2 = NodeContainer (nodes.Get (2), nodes.Get (3));
  NodeContainer xTR2_n4 = NodeContainer (nodes.Get (3), nodes.Get (4));
  NodeContainer R2_MR = NodeContainer (nodes.Get (2), nodes.Get (5));
  NodeContainer R2_MS = NodeContainer (nodes.Get (2), nodes.Get (6));

  InternetStackHelper internet;
  internet.Install(nodes);

  /* -------------------------------------- *\
                P2P LINKS
  \* -------------------------------------- */
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer dxTR1_dR2 = p2p.Install (xTR1_R2);
  NetDeviceContainer dR2_dxTR2 = p2p.Install (R2_xTR2);
  NetDeviceContainer dxTR2_dn4 = p2p.Install (xTR2_n4);
  NetDeviceContainer dR2_dMR = p2p.Install (R2_MR);
  NetDeviceContainer dR2_dMS = p2p.Install (R2_MS);

  /* -------------------------------------- *\
                ADDRESSES
  \* -------------------------------------- */
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer ixTR1_iR2 = ipv4.Assign (dxTR1_dR2);
  ipv4.SetBase ("192.168.2.0", "255.255.255.0");
  Ipv4InterfaceContainer iR2_ixTR2 = ipv4.Assign (dR2_dxTR2);
  ipv4.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer ixTR2_in4 = ipv4.Assign (dxTR2_dn4);
  ipv4.SetBase ("192.168.3.0", "255.255.255.0");
  Ipv4InterfaceContainer iR2_iMR = ipv4.Assign (dR2_dMR);
  ipv4.SetBase ("192.168.4.0", "255.255.255.0");
  Ipv4InterfaceContainer iR2_iMS = ipv4.Assign (dR2_dMS);


  /* -------------------------------------- *\
                ROUTING
  \* -------------------------------------- */
  /* --- Global Routing --- */
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
  /* --- Static routing --- */
  Ipv4StaticRoutingHelper ipv4SrHelper;
  // For xTR1
  Ptr<Ipv4> ipv4Protocol = nodes.Get (1)->GetObject<Ipv4> ();
  Ptr<Ipv4StaticRouting> ipv4Stat = ipv4SrHelper.GetStaticRouting (ipv4Protocol);
  ipv4Stat->AddNetworkRouteTo (Ipv4Address ("10.1.1.0"),
             Ipv4Mask ("255.255.255.0"),
             Ipv4Address ("10.1.1.254"), 2, 0); 

  // For xTR2
  ipv4Protocol = nodes.Get (3)->GetObject<Ipv4> ();
  ipv4Stat = ipv4SrHelper.GetStaticRouting (ipv4Protocol);
  ipv4Stat->AddNetworkRouteTo (Ipv4Address ("10.1.1.0"),
             Ipv4Mask ("255.255.255.0"),
             Ipv4Address ("192.168.2.1"), 1, 0); 
  ipv4Stat->AddNetworkRouteTo (Ipv4Address ("172.16.0.0"),
             Ipv4Mask ("255.255.255.0"),
             Ipv4Address ("192.168.2.1"), 1, 0); 

  // For R2
  ipv4Protocol = nodes.Get (2)->GetObject<Ipv4> ();
  ipv4Stat = ipv4SrHelper.GetStaticRouting (ipv4Protocol);
  ipv4Stat->AddNetworkRouteTo (Ipv4Address ("10.1.1.0"),
             Ipv4Mask ("255.255.255.0"),
             Ipv4Address ("192.168.1.1"), 1, 0); 

  // For MR
  ipv4Protocol = nodes.Get (5)->GetObject<Ipv4> ();
  ipv4Stat = ipv4SrHelper.GetStaticRouting (ipv4Protocol);
  ipv4Stat->AddNetworkRouteTo (Ipv4Address ("10.1.1.0"),
             Ipv4Mask ("255.255.255.0"),
             Ipv4Address ("192.168.3.1"), 1, 0);
  // For MS
  ipv4Protocol = nodes.Get (6)->GetObject<Ipv4> ();
  ipv4Stat = ipv4SrHelper.GetStaticRouting (ipv4Protocol);
  ipv4Stat->AddNetworkRouteTo (Ipv4Address ("10.1.1.0"),
             Ipv4Mask ("255.255.255.0"),
             Ipv4Address ("192.168.4.1"), 1, 0);

  //Ipv4GlobalRoutingHelper helper;
  //Ptr<OutputStreamWrapper> stream1 = Create<OutputStreamWrapper> ("Table2", std::ios::out);
  //helper.PrintRoutingTableAllAt (Seconds (5.0), stream1);

  

  /* -------------------------------------- *\
                    LISP-MN
  \* -------------------------------------- */
  NS_LOG_DEBUG ("Installing LISP-MN");
  LispMNHelper lispMnHelper(nodes.Get (0), Ipv4Address ("172.16.0.1"));
  lispMnHelper.SetPointToPointHelper (p2p);

  lispMnHelper.SetDhcpServerStartTime (Seconds(dhcpServerStart));
  lispMnHelper.SetEndTime (Seconds(20.0));

  NodeContainer attachementPoints = NodeContainer (nodes.Get (1));
  lispMnHelper.SetRoutersAttachementPoints (attachementPoints);
  lispMnHelper.SetRouterSubnet (Ipv4Address ("10.1.1.0"), Ipv4Mask ("255.255.255.0"));

  lispMnHelper.Install ();
  NS_LOG_DEBUG ("Finish Installing LISP-MN");


  /* -------------------------------------- *\
                      LISP
  \* -------------------------------------- */
  NS_LOG_INFO("Installing LISP...");
  NodeContainer lispRouters = NodeContainer (nodes.Get (0), nodes.Get (1), nodes.Get (3), nodes.Get (5), nodes.Get (6));
  NodeContainer xTRs = NodeContainer (nodes.Get (1), nodes.Get (3));
  NodeContainer MN = NodeContainer(nodes.Get (0));
  //TODO: check if MR/MS should be made lisp routers... (Yue)
  NodeContainer MR = NodeContainer(nodes.Get (5));
  NodeContainer MS = NodeContainer(nodes.Get (6));

  // Data Plane
  LispHelper lispHelper;
  lispHelper.BuildRlocsSet ("src/internet/test/lisp-test/mn-behind-xtr/MN_xTR_xTR_N_lisp_rlocs.txt");
  lispHelper.Install(lispRouters);
  NS_LOG_INFO("RLOCS list installed");
  lispHelper.BuildMapTables2("src/internet/test/lisp-test/mn-behind-xtr/MN_xTR_xTR_N_lisp_rlocs_config_xml.txt");
  lispHelper.InstallMapTables(lispRouters);
  NS_LOG_INFO("Map table installed");

  // Control Plane of xTRs
  LispEtrItrAppHelper lispAppHelper;
  Ptr<Locator> mrLocator = Create<Locator> (iR2_iMR.GetAddress (1));
  lispAppHelper.AddMapResolverRlocs (mrLocator);
  lispAppHelper.AddMapServerAddress (static_cast<Address> (iR2_iMS.GetAddress (1)));
  ApplicationContainer mapResClientApps = lispAppHelper.Install (xTRs);
  mapResClientApps.Start (Seconds (xTRAppStart));
  mapResClientApps.Stop (Seconds (20.0));

  // Control Plane of MN
  /* Set a different start time */
  LispEtrItrAppHelper lispMNAppHelper;
  lispMNAppHelper.AddMapResolverRlocs (mrLocator);
  lispMNAppHelper.AddMapServerAddress (static_cast<Address> (iR2_iMS.GetAddress (1)));
  ApplicationContainer MNApps = lispMNAppHelper.Install (MN);
  MNApps.Start (Seconds (MNAppStart));
  MNApps.Stop (Seconds (20.0));

  // MR application
  MapResolverDdtHelper mrHelper;
  mrHelper.SetMapServerAddress (static_cast<Address> (iR2_iMS.GetAddress (1)));
  ApplicationContainer mrApps = mrHelper.Install (MR);
  mrApps.Start (Seconds (mrStart));
  mrApps.Stop (Seconds (20.0));

  // MS application
  MapServerDdtHelper msHelper;
  ApplicationContainer msApps = msHelper.Install (MS);
  msApps.Start (Seconds (msStart));
  msApps.Stop (Seconds (20.0));

  NS_LOG_INFO("LISP succesfully aggregated");
  
  /* -------------------------------------- *\
                APPLICATIONS
  \* -------------------------------------- */
  UdpEchoServerHelper echoServer (9);

  ApplicationContainer serverApps = echoServer.Install (nodes.Get (4));
  serverApps.Start (Seconds (echoServerStart));
  serverApps.Stop (Seconds (20.0));

  UdpEchoClientHelper echoClient (ixTR2_in4.GetAddress (1), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (10));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
  clientApps.Start (Seconds (echoClientStart));
  clientApps.Stop (Seconds (20.0));

  clientApps.Get (0)->TraceConnectWithoutContext ("Rx", MakeCallback (&MnBehindXtrTestCase::RxSink, this));

  /* -------------------------------------- *\
                SIMULATION
  \* -------------------------------------- */
  //p2p.EnablePcapAll ("mn-behind-xtr_lisp_testsuite");

  Simulator::Run ();
  Simulator::Destroy ();
  
  
  /*--------------------*\
           CHECKS
  \*--------------------*/
  NS_TEST_ASSERT_MSG_EQ (m_receivedPacket, true, "No communication between both ends");
}

// ===================================================================================
class MnBehindXtrTestSuite : public TestSuite
{
public:
 MnBehindXtrTestSuite ();
};
MnBehindXtrTestSuite::MnBehindXtrTestSuite ()
  : TestSuite ("mn-behind-xtr-lisp", UNIT)
{
  AddTestCase (new MnBehindXtrTestCase, TestCase::QUICK);
}

static MnBehindXtrTestSuite mnBehindXtrTestSuite;