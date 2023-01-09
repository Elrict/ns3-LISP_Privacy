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

#include "ns3/test.h"

using namespace ns3;
NS_LOG_COMPONENT_DEFINE ("XtrBehindNatTestSuite");
// ================================================================================================

class XtrBehindNatTestCase : public TestCase
{
public:
  XtrBehindNatTestCase ();
  virtual ~XtrBehindNatTestCase ();

private:
  virtual void DoRun (void);

  bool m_receivedPacket;
  void RxSink (Ptr<const Packet> p);
};

XtrBehindNatTestCase::XtrBehindNatTestCase ()
  : TestCase ("XTR behind NAT test case: checks N_xTR_NAT_xTR_N configuration"), m_receivedPacket(false)
{
}

XtrBehindNatTestCase::~XtrBehindNatTestCase ()
{
}

void 
XtrBehindNatTestCase::RxSink (Ptr<const Packet> p)
{
  m_receivedPacket = true;
}

void
XtrBehindNatTestCase::DoRun (void)
{
  /* Topology:                    MR/MS (n6/n7)         
                                    |
                xTR1 (n1) <----> R (n2) <-----> NAT (n3) <-----> xTR2 (n4)
                /               (non-LISP)                         \
               /                    |                               \
            n0 (non-LISP)          RTR                            n5 (non-LISP)
  */

  /*--------------------*\
           SETUP
  \*--------------------*/
  PacketMetadata::Enable();
  LogComponentEnable ("XtrBehindNatTestSuite", LOG_LEVEL_DEBUG);

  /* Node creation */
  NodeContainer nodes;
  nodes.Create (9);

  NodeContainer n0_xTR1 = NodeContainer (nodes.Get (0), nodes.Get (1));
  NodeContainer xTR1_R = NodeContainer (nodes.Get (1), nodes.Get (2));
  NodeContainer R_NAT = NodeContainer (nodes.Get (2), nodes.Get (3));
  NodeContainer NAT_xTR2 = NodeContainer (nodes.Get (3), nodes.Get (4));
  NodeContainer xTR2_n4 = NodeContainer (nodes.Get (4), nodes.Get (5));
  NodeContainer R_MR = NodeContainer (nodes.Get (2), nodes.Get (6));
  NodeContainer R_MS = NodeContainer (nodes.Get (2), nodes.Get (7));
  NodeContainer R_RTR = NodeContainer (nodes.Get (2), nodes.Get (8));

  InternetStackHelper internet;
  internet.Install(nodes);

  /* P2P links */
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer dn0_dxTR1 = p2p.Install (n0_xTR1);
  NetDeviceContainer dxTR1_dR = p2p.Install (xTR1_R);
  NetDeviceContainer dR_dNAT = p2p.Install (R_NAT);
  NetDeviceContainer dNAT_dxTR2 = p2p.Install (NAT_xTR2);
  NetDeviceContainer dxTR2_dn4 = p2p.Install (xTR2_n4);
  NetDeviceContainer dR_dMR = p2p.Install (R_MR);
  NetDeviceContainer dR_dMS = p2p.Install (R_MS);
  NetDeviceContainer dR_dRTR = p2p.Install (R_RTR);

  /* Addresses */
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer in0_ixTR1 = ipv4.Assign (dn0_dxTR1);
  ipv4.SetBase ("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer ixTR1_iR = ipv4.Assign (dxTR1_dR);
  ipv4.SetBase ("192.168.2.0", "255.255.255.0");
  Ipv4InterfaceContainer iR_iNAT = ipv4.Assign (dR_dNAT);
  ipv4.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer iNAT_ixTR2 = ipv4.Assign (dNAT_dxTR2);
  ipv4.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer ixTR2_in4 = ipv4.Assign (dxTR2_dn4);
  ipv4.SetBase ("192.168.3.0", "255.255.255.0");
  Ipv4InterfaceContainer iR_iMR = ipv4.Assign (dR_dMR);
  ipv4.SetBase ("192.168.4.0", "255.255.255.0");
  Ipv4InterfaceContainer iR_iMS = ipv4.Assign (dR_dMS);
  ipv4.SetBase ("192.168.5.0", "255.255.255.0");
  Ipv4InterfaceContainer iR_iRTR = ipv4.Assign (dR_dRTR);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  /* NAT */
  
  Ipv4NatHelper natHelper;
  Ptr<Ipv4Nat> nat = natHelper.Install (nodes.Get (3));
  nat->SetInside (2);
  nat->SetOutside (1);
  nat->AddAddressPool (Ipv4Address ("192.168.2.2"), Ipv4Mask ("255.255.255.255"));
  nat->AddPortPool (49153, 49163);
  Ipv4DynamicNatRule rule (Ipv4Address ("10.1.2.0"), Ipv4Mask ("255.255.255.0"));
  nat->AddDynamicRule (rule);
  
  /* ------------ LISP ------------- */
  NS_LOG_DEBUG("Installing LISP...");
  NodeContainer lispRouters = NodeContainer (nodes.Get (1), nodes.Get (4), nodes.Get (6), nodes.Get (7));
  lispRouters.Add (nodes.Get (8));
  NodeContainer xTRs = NodeContainer (nodes.Get (1), nodes.Get (4));
  xTRs.Add (nodes.Get (8));
  NodeContainer MR = NodeContainer(nodes.Get (6));
  NodeContainer MS = NodeContainer(nodes.Get (7));

  // Data Plane
  LispHelper lispHelper;
  lispHelper.BuildRlocsSet ("src/internet/test/lisp-test/xtr-behind-nat/xTR_behind_NAT_lisp_rlocs.txt");
  lispHelper.SetRtrs(NodeContainer(nodes.Get (8)));
  lispHelper.Install(lispRouters);
  NS_LOG_INFO("Lisp router installed");
  lispHelper.BuildMapTables2("src/internet/test/lisp-test/xtr-behind-nat/xTR_behind_NAT_lisp_rlocs_config_xml.txt");
  lispHelper.InstallMapTables(lispRouters);

  NS_LOG_INFO("Map tables installed");

  // Control Plane
  LispEtrItrAppHelper lispAppHelper;
  Ptr<Locator> mrLocator = Create<Locator> (iR_iMR.GetAddress (1));
  lispAppHelper.AddMapResolverRlocs (mrLocator);
  lispAppHelper.AddMapServerAddress (static_cast<Address> (iR_iMS.GetAddress (1)));
  ApplicationContainer mapResClientApps = lispAppHelper.Install (xTRs);
  mapResClientApps.Start (Seconds (1.0));
  mapResClientApps.Stop (Seconds (20.0));

  // MR application
  MapResolverDdtHelper mrHelper;
  mrHelper.SetMapServerAddress (static_cast<Address> (iR_iMS.GetAddress (1)));
  ApplicationContainer mrApps = mrHelper.Install (MR);
  mrApps.Start (Seconds (0.0));
  mrApps.Stop (Seconds (20.0));

  // MS application
  MapServerDdtHelper msHelper;
  msHelper.SetRtrAddress(iR_iRTR.GetAddress (1));
  ApplicationContainer msApps = msHelper.Install (MS);
  msApps.Start (Seconds (0.0));
  msApps.Stop (Seconds (20.0));

  NS_LOG_INFO("LISP succesfully aggregated");
  /* ------------ LISP ------------- */

  /* Applications */
  UdpEchoServerHelper echoServer (9);

  ApplicationContainer serverApps = echoServer.Install (nodes.Get (5));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (20.0));

  UdpEchoClientHelper echoClient (ixTR2_in4.GetAddress (1), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (10));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
  clientApps.Start (Seconds (4.0));
  clientApps.Stop (Seconds (20.0));

  clientApps.Get (0)->TraceConnectWithoutContext ("Rx", MakeCallback (&XtrBehindNatTestCase::RxSink, this));

  //p2p.EnablePcapAll ("xtr-behind-nat");

  Simulator::Run ();
  Simulator::Destroy ();

  /*--------------------*\
           CHECKS
  \*--------------------*/
  NS_TEST_ASSERT_MSG_EQ (m_receivedPacket, true, "No communication between both ends");
}

// ===================================================================================
class XtrBehindNatTestSuite : public TestSuite
{
public:
  XtrBehindNatTestSuite ();
};

XtrBehindNatTestSuite::XtrBehindNatTestSuite ()
  : TestSuite ("xtr-behind-nat", UNIT)
{
  AddTestCase (new XtrBehindNatTestCase, TestCase::QUICK);
}

static XtrBehindNatTestSuite xtrBehindNatTestSuite;