/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2007 INRIA
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

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("PxTRs");

int main(int argc, char *argv[])
{
	/* Topology:										MR/MS (n5/n6)					
																		|
	 					 		xTR1 (n1) <----> R1 (n2) <-----> R2 (n3)
								/								(non-LISP)				\
							 /										|						   \
						n0 (non-LISP)			PETR/PITR	(n7/n8)		n4 (non-LISP)
  */

  PacketMetadata::Enable();

	LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("PxTRs", LOG_LEVEL_INFO);
  //LogComponentEnable ("LispHelper", LOG_LEVEL_ALL);
  LogComponentEnable ("Ipv4L3Protocol", LOG_LEVEL_DEBUG);
  //LogComponentEnable ("MapResolverDdt", LOG_LEVEL_DEBUG);
  //LogComponentEnable ("MapServerDdt", LOG_LEVEL_DEBUG);
  LogComponentEnable ("LispEtrItrApplication", LOG_LEVEL_DEBUG);
  LogComponentEnable ("LispOverIp", LOG_LEVEL_DEBUG);
  LogComponentEnable ("LispOverIpv4Impl", LOG_LEVEL_DEBUG);
  LogComponentEnable ("SimpleMapTables", LOG_LEVEL_DEBUG);

  std::string g_mdsModel = "ns3::ConstantRandomVariable";
  CommandLine cmd;
  cmd.AddValue ("mds-model", "The RandomVariableStream that is used to model MDS response time", g_mdsModel);
  cmd.Parse (argc, argv);

  /*--- MDS model ---*/
  if (g_mdsModel != "ns3::ConstantRandomVariable"){
    ObjectFactory factory;
    const std::string typeId = g_mdsModel;
    factory.SetTypeId (typeId);
    Ptr<Object> rv = factory.Create <Object> ();
    Config::SetDefault ("ns3::MapServerDdt::SearchTimeVariable", PointerValue (rv));
  }

  /* Node creation */
  NodeContainer nodes;
  nodes.Create (9);

  NodeContainer n0_xTR1 = NodeContainer (nodes.Get (0), nodes.Get (1));
  NodeContainer xTR1_R1 = NodeContainer (nodes.Get (1), nodes.Get (2));
  NodeContainer R1_R2 = NodeContainer (nodes.Get (2), nodes.Get (3));
  NodeContainer R2_n4 = NodeContainer (nodes.Get (3), nodes.Get (4));
  NodeContainer R1_MR = NodeContainer (nodes.Get (2), nodes.Get (5));
  NodeContainer R1_MS = NodeContainer (nodes.Get (2), nodes.Get (6));
  NodeContainer R1_PETR = NodeContainer (nodes.Get (2), nodes.Get (7));
  NodeContainer R1_PITR = NodeContainer (nodes.Get (2), nodes.Get (8));

  InternetStackHelper internet;
  internet.Install(nodes);

  /* P2P links */
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer dn0_dxTR1 = p2p.Install (n0_xTR1);
  NetDeviceContainer dxTR1_dR1 = p2p.Install (xTR1_R1);
  NetDeviceContainer dR1_dR2 = p2p.Install (R1_R2);
  NetDeviceContainer dR2_dn4 = p2p.Install (R2_n4);
  NetDeviceContainer dR1_dMR = p2p.Install (R1_MR);
  NetDeviceContainer dR1_dMS = p2p.Install (R1_MS);
  NetDeviceContainer dR1_dPETR = p2p.Install (R1_PETR);
  NetDeviceContainer dR1_dPITR = p2p.Install (R1_PITR);

  /* Addresses */
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer in0_ixTR1 = ipv4.Assign (dn0_dxTR1);
  ipv4.SetBase ("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer ixTR1_iR1 = ipv4.Assign (dxTR1_dR1);
  ipv4.SetBase ("192.168.2.0", "255.255.255.0");
  Ipv4InterfaceContainer iR1_iR2 = ipv4.Assign (dR1_dR2);
  ipv4.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer iR2_in4 = ipv4.Assign (dR2_dn4);
  ipv4.SetBase ("192.168.3.0", "255.255.255.0");
  Ipv4InterfaceContainer iR1_iMR = ipv4.Assign (dR1_dMR);
  ipv4.SetBase ("192.168.4.0", "255.255.255.0");
  Ipv4InterfaceContainer iR1_iMS = ipv4.Assign (dR1_dMS);
  ipv4.SetBase ("192.168.5.0", "255.255.255.0");
  Ipv4InterfaceContainer iR1_iPETR = ipv4.Assign (dR1_dPETR);
  ipv4.SetBase ("192.168.6.0", "255.255.255.0");
  Ipv4InterfaceContainer iR1_iPITR = ipv4.Assign (dR1_dPITR);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  /* Static routing for redirecting non-LISP traffic towards PITR */
  Ipv4StaticRoutingHelper ipv4SrHelper;
  // For R1
  Ptr<Ipv4> ipv4Protocol = nodes.Get (2)->GetObject<Ipv4> (); 
  Ptr<Ipv4StaticRouting> ipv4Stat = ipv4SrHelper.GetStaticRouting (ipv4Protocol);
  ipv4Stat->AddNetworkRouteTo (Ipv4Address ("10.1.1.0"), // Route traffic towards PITR
             Ipv4Mask ("255.255.255.0"),
             Ipv4Address ("192.168.6.2"), 6, 0);

  /* ------------ LISP ------------- */
  NS_LOG_INFO("Installing LISP...");
  NodeContainer lispRouters = NodeContainer (nodes.Get (1), nodes.Get (5), nodes.Get (6), nodes.Get(7), nodes.Get (8));
  NodeContainer xTRs = NodeContainer (nodes.Get (1), nodes.Get(7), nodes.Get (8));
  
  //TODO: check if MR/MS should be made lisp routers... (Yue)
  NodeContainer MR = NodeContainer(nodes.Get (5));
  NodeContainer MS = NodeContainer(nodes.Get (6));

  // Data Plane
  LispHelper lispHelper;
  lispHelper.SetPetrAddress(Ipv4Address("192.168.5.2"));
  lispHelper.SetPetrs(NodeContainer(nodes.Get (7)));
  lispHelper.SetPitrs(NodeContainer(nodes.Get (8)));
  lispHelper.BuildRlocsSet ("./lisp_non_lisp_rlocs.txt");
  lispHelper.Install(lispRouters);
  lispHelper.BuildMapTables2("./lisp_non_lisp_rlocs_config_xml.txt");
  lispHelper.InstallMapTables(lispRouters);

  // Control Plane
  LispEtrItrAppHelper lispAppHelper;
  Ptr<Locator> mrLocator = Create<Locator> (iR1_iMR.GetAddress (1));
  lispAppHelper.AddMapResolverRlocs (mrLocator);
  lispAppHelper.AddMapServerAddress (static_cast<Address> (iR1_iMS.GetAddress (1)));
  ApplicationContainer mapResClientApps = lispAppHelper.Install (xTRs);
  mapResClientApps.Start (Seconds (1.0));
  mapResClientApps.Stop (Seconds (20.0));

  // MR application
  MapResolverDdtHelper mrHelper;
  NS_LOG_INFO("Address of MS: " << iR1_iMS.GetAddress (1));
  mrHelper.SetMapServerAddress (static_cast<Address> (iR1_iMS.GetAddress (1)));
  ApplicationContainer mrApps = mrHelper.Install (MR);
  mrApps.Start (Seconds (0.0));
  mrApps.Stop (Seconds (20.0));

  // MS application
  MapServerDdtHelper msHelper;
  ApplicationContainer msApps = msHelper.Install (MS);
  msApps.Start (Seconds (0.0));
  msApps.Stop (Seconds (20.0));

  NS_LOG_INFO("LISP succesfully aggregated");
  /* ------------ LISP ------------- */

  /* Applications */
  UdpEchoServerHelper echoServer (9);

  ApplicationContainer serverApps = echoServer.Install (nodes.Get (4));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (20.0));

  UdpEchoClientHelper echoClient (iR2_in4.GetAddress (1), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (10));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
  clientApps.Start (Seconds (4.0));
  clientApps.Stop (Seconds (20.0));

  /* SIMULATION */
  p2p.EnablePcapAll ("PxTRs_example");

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;




}