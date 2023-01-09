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

NS_LOG_COMPONENT_DEFINE ("PxTRsAndMN");

//==================================================================================
class CallBackForVirtualDeviceClass
{
  Ptr<VirtualNetDevice> mnVirtualDevice;
  Ptr<NetDevice> mnDevice;

  bool
  TapVirtualSend (Ptr<Packet> packet, const Address& source,
      const Address& dest, uint16_t protocolNumber)
  {
    /**
     * Surprisingly, the input parameter is already a IP packet (with double encapsulation!!!)
     */
    NS_LOG_DEBUG("Transmitted packet(I want to know the packet is IP or Ethernet or Wifi): " << *packet);
    mnDevice->Send (packet, dest, protocolNumber);
    NS_LOG_DEBUG("Transmitted packet sent correctly");
    return true;
  }

  //TODO: Maybe should set receive callback? for

public:
  CallBackForVirtualDeviceClass (Ptr<VirtualNetDevice> n0Tap, Ptr<NetDevice> netDve) :
      mnVirtualDevice (n0Tap), mnDevice (netDve)
  {
    mnVirtualDevice->SetSendCallback (MakeCallback (&CallBackForVirtualDeviceClass::TapVirtualSend, this));
  }

};
//==================================================================================

int main(int argc, char *argv[])
{
	/* Topology:										MR/MS (n5/n6)					
																		|
	               R1 (n1) <----> R2 (n2) <-----> R3 (n3)
							 (non-LISP)			 (non-LISP)			 (non-LISP)
                / DHCP              |              \
							 /										|							  \
						LISP-MN	(n0)		PETR/PITR (n7/n8)			n4 (non-LISP)
  */

  PacketMetadata::Enable();

	LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("PxTRsAndMN", LOG_LEVEL_INFO);
  //LogComponentEnable ("LispHelper", LOG_LEVEL_ALL);
  LogComponentEnable ("Ipv4L3Protocol", LOG_LEVEL_DEBUG);
  //LogComponentEnable ("MapResolverDdt", LOG_LEVEL_DEBUG);
  //LogComponentEnable ("MapServerDdt", LOG_LEVEL_DEBUG);
  //LogComponentEnable ("LispEtrItrApplication", LOG_LEVEL_DEBUG);
  //LogComponentEnable ("DhcpClient", LOG_LEVEL_FUNCTION);
  LogComponentEnable ("LispOverIp", LOG_LEVEL_DEBUG);
  //LogComponentEnable ("Ipv4Interface", LOG_LEVEL_DEBUG);
  LogComponentEnable ("SimpleMapTables", LOG_LEVEL_ERROR);

  float echoServerStart = 0.0;
  float echoClientStart = 5.0;
  float xTRAppStart = 2.0;
  float mrStart = 0.0;
  float msStart = 0.0;
  float dhcpServerStart = 0.0;
  float dhcpClientStart = 0.0;

  float dhcp_collect = 1.0;
  Config::SetDefault ("ns3::DhcpClient::Collect", TimeValue (Seconds (dhcp_collect)));
  
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
  
  /* -------------------------------------- *\
                NODE CREATION
  \* -------------------------------------- */
  NodeContainer nodes;
  nodes.Create (9);

  NodeContainer MN_R1 = NodeContainer (nodes.Get (0), nodes.Get (1));
  NodeContainer R1_R2 = NodeContainer (nodes.Get (1), nodes.Get (2));
  NodeContainer R2_R3 = NodeContainer (nodes.Get (2), nodes.Get (3));
  NodeContainer R3_n4 = NodeContainer (nodes.Get (3), nodes.Get (4));
  NodeContainer R2_MR = NodeContainer (nodes.Get (2), nodes.Get (5));
  NodeContainer R2_MS = NodeContainer (nodes.Get (2), nodes.Get (6));
  NodeContainer R2_PETR = NodeContainer (nodes.Get (2), nodes.Get (7));
  NodeContainer R2_PITR = NodeContainer (nodes.Get (2), nodes.Get (8));

  InternetStackHelper internet;
  internet.Install(nodes);

  /* -------------------------------------- *\
                P2P LINKS
  \* -------------------------------------- */
  PointToPointHelper p2p;
  p2p.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  p2p.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer dMN_dR1 = p2p.Install (MN_R1);
  NetDeviceContainer dR1_dR2 = p2p.Install (R1_R2);
  NetDeviceContainer dR2_dR3 = p2p.Install (R2_R3);
  NetDeviceContainer dR3_dn4 = p2p.Install (R3_n4);
  NetDeviceContainer dR2_dMR = p2p.Install (R2_MR);
  NetDeviceContainer dR2_dMS = p2p.Install (R2_MS);
  NetDeviceContainer dR2_dPETR = p2p.Install (R2_PETR);
  NetDeviceContainer dR2_dPITR = p2p.Install (R2_PITR);

  /* -------------------------------------- *\
                ADDRESSES
  \* -------------------------------------- */
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer iR1_iR2 = ipv4.Assign (dR1_dR2);
  ipv4.SetBase ("192.168.2.0", "255.255.255.0");
  Ipv4InterfaceContainer iR2_ixTR = ipv4.Assign (dR2_dR3);
  ipv4.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer iR3_in4 = ipv4.Assign (dR3_dn4);
  ipv4.SetBase ("192.168.3.0", "255.255.255.0");
  Ipv4InterfaceContainer iR2_iMR = ipv4.Assign (dR2_dMR);
  ipv4.SetBase ("192.168.4.0", "255.255.255.0");
  Ipv4InterfaceContainer iR2_iMS = ipv4.Assign (dR2_dMS);
  ipv4.SetBase ("192.168.5.0", "255.255.255.0");
  Ipv4InterfaceContainer iR2_iPETR = ipv4.Assign (dR2_dPETR);
  ipv4.SetBase ("192.168.6.0", "255.255.255.0");
  Ipv4InterfaceContainer iR2_iPITR = ipv4.Assign (dR2_dPITR);


  /* -------------------------------------- *\
                ROUTING
  \* -------------------------------------- */
  /* --- Global Routing --- */
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
  /* --- Static routing --- */
  Ipv4StaticRoutingHelper ipv4SrHelper;
  // For R1
  Ptr<Ipv4> ipv4Protocol = nodes.Get (1)->GetObject<Ipv4> ();
  Ptr<Ipv4StaticRouting> ipv4Stat = ipv4SrHelper.GetStaticRouting (ipv4Protocol);
  ipv4Stat->AddNetworkRouteTo (Ipv4Address ("10.1.1.0"),
             Ipv4Mask ("255.255.255.0"),
             Ipv4Address ("10.1.1.254"), 2, 0);
  // For R2
  ipv4Protocol = nodes.Get (2)->GetObject<Ipv4> ();
  ipv4Stat = ipv4SrHelper.GetStaticRouting (ipv4Protocol);
  ipv4Stat->AddNetworkRouteTo (Ipv4Address ("10.1.1.0"),
             Ipv4Mask ("255.255.255.0"),
             Ipv4Address ("192.168.1.1"), 1, 0);
  ipv4Stat->AddNetworkRouteTo (Ipv4Address ("172.16.0.1"),
             Ipv4Mask ("255.255.255.0"),
             Ipv4Address ("192.168.6.2"), 6, 0); //PITR attracts traffic
  
  // For R3
  ipv4Protocol = nodes.Get (3)->GetObject<Ipv4> ();
  ipv4Stat = ipv4SrHelper.GetStaticRouting (ipv4Protocol); 
  ipv4Stat->AddNetworkRouteTo (Ipv4Address ("172.16.0.1"),
             Ipv4Mask ("255.255.255.0"),
             Ipv4Address ("192.168.2.1"), 1, 0); // Route non-LISP traffic towards PITR

  // For n4
  ipv4Protocol = nodes.Get (4)->GetObject<Ipv4> ();
  ipv4Stat = ipv4SrHelper.GetStaticRouting (ipv4Protocol);
  ipv4Stat->AddNetworkRouteTo (Ipv4Address ("172.16.0.1"),
             Ipv4Mask ("255.255.255.0"),
             Ipv4Address ("10.1.2.1"), 1, 0); // Route non-LISP traffic towards PITR
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

  // For PITR
  ipv4Protocol = nodes.Get (8)->GetObject<Ipv4> ();
  ipv4Stat = ipv4SrHelper.GetStaticRouting (ipv4Protocol);
  ipv4Stat->AddNetworkRouteTo (Ipv4Address ("172.16.0.1"), //PITR accepts traffic towards 172.16.0.1
             Ipv4Mask ("255.255.255.0"),
             Ipv4Address ("192.168.6.1"), 1, 0);
  ipv4Stat->AddNetworkRouteTo (Ipv4Address ("10.1.1.0"),
             Ipv4Mask ("255.255.255.0"),
             Ipv4Address ("192.168.6.1"), 1, 0);

  //Ipv4GlobalRoutingHelper helper;
  //Ptr<OutputStreamWrapper> stream1 = Create<OutputStreamWrapper> ("Table2", std::ios::out);
  //helper.PrintRoutingTableAllAt (Seconds (5.0), stream1);

  

  /* -------------------------------------- *\
                    LISP-MN
  \* -------------------------------------- */
  Address lispMnEidAddr = Ipv4Address ("172.16.0.1");

  /* --- Add interface to existing device ---*/
  Ptr<Ipv4> ipv4MN = nodes.Get (0)->GetObject<Ipv4> ();
  uint32_t ifIndex_MN = ipv4MN->AddInterface (dMN_dR1.Get (0)); //Add a new interface to the device of MN
  ipv4MN->AddAddress (
      ifIndex_MN,
      Ipv4InterfaceAddress (Ipv4Address ("0.0.0.0"), Ipv4Mask ("/0"))); //Add a new address, attached to the new interface
  // Due to DHCP program implementation constraint, we have to first assign a 0.0.0.0/0 for DHCP client
  // An interface is an association between an IP address and a device
  ipv4MN->SetForwarding (ifIndex_MN, true);
  ipv4MN->SetUp (ifIndex_MN);

  /* --- Complete new virtual device (with EID address of MN) --- */
  Ptr<VirtualNetDevice> mnVirtualDevice = CreateObject<VirtualNetDevice> ();
  mnVirtualDevice->SetAddress (Mac48Address ("11:00:01:02:03:01"));
  nodes.Get (0)->AddDevice (mnVirtualDevice); //Add new virtual device to MN
  Ptr<Ipv4> ipv4Tun = nodes.Get (0)->GetObject<Ipv4> ();
  uint32_t ifIndexTap = ipv4MN->AddInterface (mnVirtualDevice); //Add interface to new device
  ipv4Tun->AddAddress (
      ifIndexTap,
      Ipv4InterfaceAddress (Ipv4Address::ConvertFrom(lispMnEidAddr),
          Ipv4Mask ("255.255.255.255"))); //Add a new address, attached to new virtual device
  ipv4Tun->SetForwarding (ifIndexTap, true);
  ipv4Tun->SetUp (ifIndexTap);

  /**
   * It is obligatory to set TransmitCallBack for virtual-net-device.
   * Otherwise when transmitting packet, virtual-net-device does not know what to do,
   * because it doesn't have a physical NIC.
   */
  CallBackForVirtualDeviceClass (mnVirtualDevice, dMN_dR1.Get (0));

  /* --- Manually set interface for router (for existing device) --- */
  Ptr<Ipv4> ipv4R1 = nodes.Get (1)->GetObject<Ipv4> ();
  uint32_t ifIndex = ipv4R1->AddInterface (dMN_dR1.Get (1)); //Get existing device of R1
  ipv4R1->AddAddress (
      ifIndex,
      Ipv4InterfaceAddress (Ipv4Address ("10.1.1.254"), Ipv4Mask ("255.255.255.0")));
  ipv4R1->SetForwarding (ifIndex, true);
  ipv4R1->SetMetric (ifIndex, 1);
  ipv4R1->SetUp (ifIndex);
  Ipv4InterfaceContainer ap1Interface;
  ap1Interface.Add (ipv4R1, ifIndex);

  /* Must modify the routing table of MN so that the packets coming from 
            the application always use the EID address. */
  ipv4Protocol = nodes.Get (0)->GetObject<Ipv4> ();
  ipv4Stat = ipv4SrHelper.GetStaticRouting (ipv4Protocol);
  ipv4Stat->AddNetworkRouteTo(Ipv4Address("0.0.0.0"), Ipv4Mask("/1"), ifIndexTap);
  //ipv4Stat->AddNetworkRouteTo(Ipv4Address("128.0.0.0"), Ipv4Mask("/1"), tunDevIndex);
  //Later: for handover

  /*
  NOTE: All this code is "equivalent" to "ipv4.Assign (dMN_dR1);", except that we
  do everything manually by assigning addresses to devices ourselves. We do this because of DHCP
  */

  /* -------------------------------------- *\
                      DHCP
  \* -------------------------------------- */
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
  DhcpClientHelper dhcpClientHelper (1); //device 1 corresponds to device installed by p2pHelper
  ApplicationContainer appDhcpClient = dhcpClientHelper.Install (nodes.Get (0)); // Install it on MN
  appDhcpClient.Start (Seconds(dhcpClientStart));
  appDhcpClient.Stop (Seconds(20.0));


  /* -------------------------------------- *\
                      LISP
  \* -------------------------------------- */
  NS_LOG_INFO("Installing LISP...");
  NodeContainer lispRouters = NodeContainer (nodes.Get (0), nodes.Get (5), nodes.Get (6), nodes.Get (7), nodes.Get (8));
  NodeContainer xTRs = NodeContainer (nodes.Get (0), nodes.Get (7), nodes.Get (8));
  
  //TODO: check if MR/MS should be made lisp routers... (Yue)
  NodeContainer MR = NodeContainer(nodes.Get (5));
  NodeContainer MS = NodeContainer(nodes.Get (6));

  // Data Plane
  LispHelper lispHelper;
  lispHelper.SetPetrAddress(Ipv4Address("192.168.5.2"));
  lispHelper.SetPetrs(NodeContainer(nodes.Get (7)));
  lispHelper.SetPitrs(NodeContainer(nodes.Get (8)));
  lispHelper.BuildRlocsSet ("./PxTRs_and_MN_lisp_rlocs.txt");
  lispHelper.Install(lispRouters);
  //lispHelper.BuildMapTables2("./scratch/simple_lisp_rlocs_config_xml.txt");
  lispHelper.BuildMapTables2("./PxTRs_and_MN_lisp_rlocs_config_xml.txt");
  lispHelper.InstallMapTables(lispRouters);

  // Control Plane
  LispEtrItrAppHelper lispAppHelper;
  Ptr<Locator> mrLocator = Create<Locator> (iR2_iMR.GetAddress (1));
  lispAppHelper.AddMapResolverRlocs (mrLocator);
  lispAppHelper.AddMapServerAddress (static_cast<Address> (iR2_iMS.GetAddress (1)));
  ApplicationContainer mapResClientApps = lispAppHelper.Install (xTRs);
  mapResClientApps.Start (Seconds (xTRAppStart));
  mapResClientApps.Stop (Seconds (20.0));

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

  UdpEchoClientHelper echoClient (iR3_in4.GetAddress (1), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (10));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
  clientApps.Start (Seconds (echoClientStart));
  clientApps.Stop (Seconds (20.0));

  /* -------------------------------------- *\
                SIMULATION
  \* -------------------------------------- */
  p2p.EnablePcapAll ("MN_R_xTR_N_lisp_example");

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;




}