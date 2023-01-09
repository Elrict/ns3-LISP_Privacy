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
#include "ns3/virtual-net-device.h"
#include "ns3/dhcp-helper.h"
#include "ns3/ipv4-static-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("SimpleDHCPExample");


int main(int argc, char const *argv[])
{
	/* Topology:									
	               R1 (n1) <----> R2 (n2) <-----> R3 (n3)
                / DHCP                             \
							 /																	  \
						  n0													          n4
  */

  PacketMetadata::Enable();

	LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("SimpleDHCPExample", LOG_LEVEL_INFO);
  //LogComponentEnable ("Ipv4L3Protocol", LOG_LEVEL_DEBUG);
  //LogComponentEnable ("DhcpClient", LOG_LEVEL_DEBUG);
  //LogComponentEnable ("Ipv4Interface", LOG_LEVEL_DEBUG);

  float echoServerStart = 0.0;
  float echoClientStart = 5.0; //Must wait to get IP Address, or client will send with source address 0.0.0.0
  float dhcpServerStart = 0.0;
  float dhcpClientStart = 0.0;

  float dhcp_collect = 0.5;
  //Config::SetDefault ("ns3::Ipv4GlobalRouting::RespondToInterfaceEvents", BooleanValue (true));
  Config::SetDefault ("ns3::DhcpClient::Collect", TimeValue (Seconds (dhcp_collect)));
  
  /* Node creation */
  NodeContainer nodes;
  nodes.Create (5);

  NodeContainer n0_R1 = NodeContainer (nodes.Get (0), nodes.Get (1));
  NodeContainer R1_R2 = NodeContainer (nodes.Get (1), nodes.Get (2));
  NodeContainer R2_R3 = NodeContainer (nodes.Get (2), nodes.Get (3));
  NodeContainer R3_n4 = NodeContainer (nodes.Get (3), nodes.Get (4));

  InternetStackHelper internet;
  internet.Install(nodes);

  /* P2P links */
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer dn0_dR1 = p2p.Install (n0_R1);
  NetDeviceContainer dR1_dR2 = p2p.Install (R1_R2);
  NetDeviceContainer dR2_dR3 = p2p.Install (R2_R3);
  NetDeviceContainer dR3_dn4 = p2p.Install (R3_n4);

  /* Addresses */
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer iR1_iR2 = ipv4.Assign (dR1_dR2);
  ipv4.SetBase ("192.168.2.0", "255.255.255.0");
  Ipv4InterfaceContainer iR2_iR3 = ipv4.Assign (dR2_dR3);
  ipv4.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer iR3_in4 = ipv4.Assign (dR3_dn4);

  /* Routing */
  // Global routing
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
  // Static routing (for subnet with DHCP, because not done automatically by global routing)
  Ipv4StaticRoutingHelper ipv4SrHelper;
  // For R1
  Ptr<Ipv4> ipv4Protocol = nodes.Get (1)->GetObject<Ipv4> ();
  Ptr<Ipv4StaticRouting> ipv4Stat = ipv4SrHelper.GetStaticRouting (ipv4Protocol);
  ipv4Stat->AddNetworkRouteTo (Ipv4Address ("10.1.1.0"),
             Ipv4Mask ("255.255.255.0"),
             Ipv4Address ("10.1.1.254"), 2, 0); //TODO check 2
  // For R2
  ipv4Protocol = nodes.Get (2)->GetObject<Ipv4> ();
  ipv4Stat = ipv4SrHelper.GetStaticRouting (ipv4Protocol);
  ipv4Stat->AddNetworkRouteTo (Ipv4Address ("10.1.1.0"),
             Ipv4Mask ("255.255.255.0"),
             Ipv4Address ("192.168.1.1"), 1, 0);
  // For R3
  ipv4Protocol = nodes.Get (3)->GetObject<Ipv4> ();
  ipv4Stat = ipv4SrHelper.GetStaticRouting (ipv4Protocol);
  ipv4Stat->AddNetworkRouteTo (Ipv4Address ("10.1.1.0"),
             Ipv4Mask ("255.255.255.0"),
             Ipv4Address ("192.168.2.1"), 1, 0); 
  // For n4
  ipv4Protocol = nodes.Get (4)->GetObject<Ipv4> ();
  ipv4Stat = ipv4SrHelper.GetStaticRouting (ipv4Protocol);
  ipv4Stat->AddNetworkRouteTo (Ipv4Address ("10.1.1.0"),
             Ipv4Mask ("255.255.255.0"),
             Ipv4Address ("10.1.2.1"), 1, 0);


  Ipv4GlobalRoutingHelper helper;
  Ptr<OutputStreamWrapper> stream1 = Create<OutputStreamWrapper> ("Table2", std::ios::out);
  helper.PrintRoutingTableAllAt (Seconds (5.0), stream1);

  /* ================= DHCP ================= */
  /* --- Add interface to existing device ---*/
  Ptr<Ipv4> ipv4MN = nodes.Get (0)->GetObject<Ipv4> ();
  uint32_t ifIndex_MN = ipv4MN->AddInterface (dn0_dR1.Get (0)); //Add a new interface to the device of MN
  ipv4MN->AddAddress (
      ifIndex_MN,
      Ipv4InterfaceAddress (Ipv4Address ("0.0.0.0"), Ipv4Mask ("/0"))); //Add a new address, attached to the new interface
  // Due to DHCP program implementation constraint, we have to first assign a 0.0.0.0/0 for DHCP client
  // An interface is an association between an IP address and a device
  ipv4MN->SetForwarding (ifIndex_MN, true);
  ipv4MN->SetUp (ifIndex_MN);
  
  /* --- Manually set interface for router (for existing device) --- */
  Ptr<Ipv4> ipv4R1 = nodes.Get (1)->GetObject<Ipv4> ();
  uint32_t ifIndex = ipv4R1->AddInterface (dn0_dR1.Get (1)); //Get existing device of R1
  ipv4R1->AddAddress (
      ifIndex,
      Ipv4InterfaceAddress (Ipv4Address ("10.1.1.254"), Ipv4Mask ("255.255.255.0")));
  ipv4R1->SetForwarding (ifIndex, true);
  ipv4R1->SetMetric (ifIndex, 1);
  ipv4R1->SetUp (ifIndex);
  Ipv4InterfaceContainer ap1Interface;
  ap1Interface.Add (ipv4R1, ifIndex);

  /*
  NOTE: All this code is "equivalent" to "ipv4.Assign (dMN_dR1);", except that we
  do everything manually by assigning addresses to devices ourselves. We do this because of DHCP
  */
  /* ================= LISP-MN ================= */

  /* DHCP */
  DhcpServerHelper dhcpServerHelper (
    Ipv4Address ("10.1.1.0"), //Pool of addresses the DHCP server can assign
    Ipv4Mask ("255.255.255.0"), // Mask of the pool
    Ipv4Address("10.1.1.254"), // Address of the DHCP server
    Ipv4Address ("10.1.1.0"), // Min address
    Ipv4Address ("10.1.1.252"), // Max address
    ap1Interface.GetAddress (0)); // Address of gateway router

  ApplicationContainer appDhcpServer = dhcpServerHelper.Install (nodes.Get (1)); //Install DHCP on R1
  appDhcpServer.Start (Seconds(dhcpServerStart));
  appDhcpServer.Stop (Seconds(20.0));

  //The interface on which DHCP client has to be installed
  // -> Actually, it's not the interface but the device!
  DhcpClientHelper dhcpClientHelper (1); 
  ApplicationContainer appDhcpClient = dhcpClientHelper.Install (nodes.Get (0)); // Install it on MN
  appDhcpClient.Start (Seconds(dhcpClientStart));
  appDhcpClient.Stop (Seconds(20.0));

  /* Applications */
  UdpEchoServerHelper echoServer (9);

  ApplicationContainer serverApps = echoServer.Install (nodes.Get (4));
  serverApps.Start (Seconds (echoServerStart));
  serverApps.Stop (Seconds (20.0));

  UdpEchoClientHelper echoClient (iR3_in4.GetAddress (1), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (10));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
  clientApps.Start (Seconds (echoClientStart));
  clientApps.Stop (Seconds (20.0));

  /* SIMULATION */
  p2p.EnablePcapAll ("DHCP_example");

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;




}