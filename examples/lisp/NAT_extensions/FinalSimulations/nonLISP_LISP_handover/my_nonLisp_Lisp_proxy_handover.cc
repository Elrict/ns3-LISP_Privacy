/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2019 University of Liege
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

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("nonLispLispProxyHandover");

//==================================================================================
void
TraceSink (Ptr<OutputStreamWrapper> stream, Ptr<const Packet> packet) {
  *stream->GetStream () << Simulator::Now ().GetSeconds () << "\n";
}
//==================================================================================
/** Reads the Environment Variable specified by 'var'
  */
std::string GetEnv( const std::string & var ) {
     const char * val = ::getenv( var.c_str() );
     if ( val == 0 ) {
         return "";
     }
     else {
         return val;
     }
}
//==================================================================================
int main(int argc, char *argv[])
{
	/* Topology:	
                                         R2 <--------------> LISP-MN (n9)
  									         MR/MS     /(n3)                 /
                            (n5/n6)		/                     /
                              	|		 /                     /
																|		/                     /
	 	n0 <---->	R4 (n1) <----> R1 (n2)                     /
      (non-LISP)              /     \                   /
                             /       \                 /
                  PETR/PITR /         \               /
                  (n7/n8)              \ xTR1 --------
                                          (n4)      
                               
  */

  PacketMetadata::Enable();

	/* --- COMMAND LINE --- */
  std::string g_mdsModel = "ns3::ConstantRandomVariable";
  bool verbose = true;
  uint32_t i = 0;

  CommandLine cmd;
  cmd.AddValue ("mds-model", "The RandomVariableStream that is used to model MDS response time", g_mdsModel);
  cmd.AddValue ("verbose", "Enable logging", verbose);
  cmd.AddValue ("i", "i", i);
  cmd.Parse (argc, argv);

  /* -------------------------------------- *\
                CONFIGURATION
  \* -------------------------------------- */

  if (verbose)
  {
    LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);
    LogComponentEnable ("nonLispLispProxyHandover", LOG_LEVEL_INFO);
    //LogComponentEnable ("LispHelper", LOG_LEVEL_ALL);
    //LogComponentEnable ("Ipv4L3Protocol", LOG_LEVEL_DEBUG);
    //LogComponentEnable ("Ipv4Nat", LOG_LEVEL_DEBUG);
    //LogComponentEnable ("MapResolverDdt", LOG_LEVEL_DEBUG);
    //LogComponentEnable ("MapServerDdt", LOG_LEVEL_DEBUG);
    //LogComponentEnable ("LispEtrItrApplication", LOG_LEVEL_DEBUG);
    //LogComponentEnable ("LispOverIp", LOG_LEVEL_DEBUG);
    //LogComponentEnable ("LispOverIpv4Impl", LOG_LEVEL_DEBUG);
    //LogComponentEnable ("SimpleMapTables", LOG_LEVEL_DEBUG);
    //LogComponentEnable ("LispHelper", LOG_LEVEL_DEBUG);
  }

  /*--- RANDOMNESS ---*/
  ns3::RngSeedManager::SetSeed (5);
  std::string RUN_NUMBER = GetEnv("RUN_NUMBER");
  if (RUN_NUMBER != ""){
    NS_LOG_DEBUG ("RUN_NUMBER is " << RUN_NUMBER);
    int RUN_NUMBER_INT = std::stoi(RUN_NUMBER);
    ns3::RngSeedManager::SetRun (RUN_NUMBER_INT);
  }
  else
  {
    NS_LOG_DEBUG ("No RUN_NUMBER set");
  }

  /*--- MDS model ---*/
  if (g_mdsModel == "ns3::EmpiricalRandomVariable"){
    Ptr<Object> rv = MapServerDdt::GetMdsModel ();
    Config::SetDefault ("ns3::MapServer::SearchTimeVariable", PointerValue (rv));
  }
  else if (g_mdsModel != "ns3::ConstantRandomVariable"){
    ObjectFactory factory;
    const std::string typeId = g_mdsModel;
    factory.SetTypeId (typeId);
    Ptr<Object> rv = factory.Create <Object> ();
    Config::SetDefault ("ns3::MapServer::SearchTimeVariable", PointerValue (rv));
  }

  Time echoServerStart = Seconds(0.0);
  Time echoClientStart = Seconds(5.0);
  Time xTRAppStart = Seconds(2.0);
  Time mrStart = Seconds(0.0);
  Time msStart = Seconds(0.0);
  Time dhcpServerStart = Seconds(0.0);
  Time dhcpClientStart = Seconds(0.0);
  Time HandTime = Seconds(8.0);
  Time EndTime = Seconds(15.0);

  float dhcp_collect = 1.0;
  Config::SetDefault ("ns3::DhcpClient::Collect", TimeValue (Seconds (dhcp_collect)));
  

  /* -------------------------------------- *\
                NODE CREATION
  \* -------------------------------------- */
  NodeContainer nodes;
  nodes.Create (10);

  NodeContainer n0_R4 = NodeContainer (nodes.Get (0), nodes.Get (1));
  NodeContainer R4_R1 = NodeContainer (nodes.Get (1), nodes.Get (2));
  NodeContainer R1_R2 = NodeContainer (nodes.Get (2), nodes.Get (3));
  NodeContainer R1_xTR1 = NodeContainer (nodes.Get (2), nodes.Get (4));
  NodeContainer R2_MN = NodeContainer (nodes.Get (3), nodes.Get (9));
  NodeContainer xTR1_MN = NodeContainer (nodes.Get (4), nodes.Get (9));
  NodeContainer R1_MR = NodeContainer (nodes.Get (2), nodes.Get (5));
  NodeContainer R1_MS = NodeContainer (nodes.Get (2), nodes.Get (6));
  NodeContainer R1_PETR = NodeContainer (nodes.Get (2), nodes.Get (7));
  NodeContainer R1_PITR = NodeContainer (nodes.Get (2), nodes.Get (8));

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
  NetDeviceContainer dR1_dxTR1 = p2p.Install (R1_xTR1);
  NetDeviceContainer dR1_dMR = p2p.Install (R1_MR);
  NetDeviceContainer dR1_dMS = p2p.Install (R1_MS);
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
  Ipv4InterfaceContainer iR1_ixTR1 = ipv4.Assign (dR1_dxTR1);
  ipv4.SetBase ("192.168.4.0", "255.255.255.0");
  Ipv4InterfaceContainer iR1_iMR = ipv4.Assign (dR1_dMR);
  ipv4.SetBase ("192.168.5.0", "255.255.255.0");
  Ipv4InterfaceContainer iR1_iMS = ipv4.Assign (dR1_dMS);
  ipv4.SetBase ("192.168.6.0", "255.255.255.0");
  Ipv4InterfaceContainer iR1_iPETR = ipv4.Assign (dR1_dPETR);
  ipv4.SetBase ("192.168.7.0", "255.255.255.0");
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

  // For xTR1
  ipv4Protocol = nodes.Get (4)->GetObject<Ipv4> ();
  ipv4Stat = ipv4SrHelper.GetStaticRouting (ipv4Protocol);
  ipv4Stat->AddNetworkRouteTo (Ipv4Address ("10.1.3.0"),
             Ipv4Mask ("255.255.255.0"),
             Ipv4Address ("10.1.3.254"), 2, 0);
  
  // For R4
  ipv4Protocol = nodes.Get (1)->GetObject<Ipv4> ();
  ipv4Stat = ipv4SrHelper.GetStaticRouting (ipv4Protocol); 
  ipv4Stat->AddNetworkRouteTo (Ipv4Address ("172.16.0.1"), //forwards legacy traffic
             Ipv4Mask ("255.255.255.0"),
             Ipv4Address ("192.168.1.2"), 2, 0);

  // For MS
  ipv4Protocol = nodes.Get (6)->GetObject<Ipv4> ();
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
  ipv4Stat->AddNetworkRouteTo (Ipv4Address ("10.1.3.0"), //R1 can accept traffic towards 10.1.3.0
             Ipv4Mask ("255.255.255.0"),
             Ipv4Address ("192.168.3.2"), 3, 0);
  ipv4Stat->AddNetworkRouteTo (Ipv4Address ("172.16.0.1"), //forwards legacy traffic
             Ipv4Mask ("255.255.255.0"),
             Ipv4Address ("192.168.8.2"), 7, 0);

  // For PITR
  ipv4Protocol = nodes.Get (8)->GetObject<Ipv4> ();
  ipv4Stat = ipv4SrHelper.GetStaticRouting (ipv4Protocol); 
  ipv4Stat->AddNetworkRouteTo (Ipv4Address ("10.1.2.0"), 
             Ipv4Mask ("255.255.255.0"),
             Ipv4Address ("192.168.8.1"), 1, 0);
  ipv4Stat->AddNetworkRouteTo (Ipv4Address ("10.1.3.0"), 
             Ipv4Mask ("255.255.255.0"),
             Ipv4Address ("192.168.8.1"), 1, 0);
  ipv4Stat->AddNetworkRouteTo (Ipv4Address ("172.16.0.1"), //PITR can encapsulate traffic towards 172.16.0.1
             Ipv4Mask ("255.255.255.0"),
             Ipv4Address ("192.168.8.1"), 1, 0);

  /* -------------------------------------- *\
                    LISP-MN
  \* -------------------------------------- */
  int MN = 9;

  NS_LOG_DEBUG ("Installing LISP-MN");

  LispMNHelper lispMnHelper(nodes.Get (MN), Ipv4Address ("172.16.0.1"));
  lispMnHelper.SetPointToPointHelper (p2p);

  lispMnHelper.SetDhcpServerStartTime (dhcpServerStart);
  lispMnHelper.SetEndTime (EndTime);

  NodeContainer attachementPoints = NodeContainer (nodes.Get (3), nodes.Get (4));
  lispMnHelper.SetRoutersAttachementPoints (attachementPoints);
  lispMnHelper.SetRouterSubnet (Ipv4Address ("10.1.2.0"), Ipv4Mask ("255.255.255.0"));
  lispMnHelper.SetRouterSubnet (Ipv4Address ("10.1.3.0"), Ipv4Mask ("255.255.255.0"));

  lispMnHelper.ScheduleHandover (0, 1, HandTime);
  lispMnHelper.Install ();

  NS_LOG_DEBUG ("Finish Installing LISP-MN");

  /* -------------------------------------- *\
                      LISP
  \* -------------------------------------- */
  NS_LOG_INFO("Installing LISP...");
  NodeContainer lispRouters = NodeContainer (nodes.Get (5), nodes.Get (6), nodes.Get (MN), nodes.Get (4));
  lispRouters.Add (nodes.Get (7)); // PETR
  lispRouters.Add (nodes.Get (8)); // PITR
  NodeContainer xTRs = NodeContainer (nodes.Get (MN), nodes.Get (7), nodes.Get (8), nodes.Get (4));
  NodeContainer MR = NodeContainer(nodes.Get (5));
  NodeContainer MS = NodeContainer(nodes.Get (6));

  // Data Plane
  LispHelper lispHelper;
  lispHelper.SetPetrAddress(iR1_iPETR.GetAddress (1));
  lispHelper.SetPetrs(NodeContainer(nodes.Get (7)));
  lispHelper.SetPitrs(NodeContainer(nodes.Get (8)));
  lispHelper.BuildRlocsSet ("./nonLisp_Lisp_proxy_handover_lisp_rlocs.txt");
  lispHelper.Install(lispRouters);
  NS_LOG_INFO("Lisp router installed");
  lispHelper.BuildMapTables2("./nonLisp_Lisp_proxy_handover_lisp_rlocs_config_xml.txt");
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
  ApplicationContainer msApps = msHelper.Install (MS);
  msApps.Start (msStart);
  msApps.Stop (EndTime);

  NS_LOG_INFO("LISP succesfully aggregated");
  
  /* -------------------------------------- *\
                APPLICATIONS
  \* -------------------------------------- */
  UdpEchoServerHelper echoServer (9);

  ApplicationContainer serverApps = echoServer.Install (nodes.Get (0));
  serverApps.Start (echoServerStart);
  serverApps.Stop (EndTime);

  UdpEchoClientHelper echoClient (in0_iR4.GetAddress (0), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (30));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps = echoClient.Install (nodes.Get (MN));
  clientApps.Start (echoClientStart);
  clientApps.Stop (EndTime);

  /* -------------------------------------- *\
                TRACING
  \* -------------------------------------- */
  p2p.EnablePcapAll ("nonLisp_Lisp_proxy_handover");

  /*
  AsciiTraceHelper ascii;
  Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream ("result_"+RUN_NUMBER+".txt");
  clientApps.Get (0)->TraceConnectWithoutContext ("Rx", MakeBoundCallback (&TraceSink, stream));
  */

  /* -------------------------------------- *\
                SIMULATION
  \* -------------------------------------- */
  Simulator::Stop (EndTime);
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;




}