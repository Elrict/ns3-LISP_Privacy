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
#include "ns3/config-store-module.h"
#include "ns3/object-factory.h"
#include "ns3/map-server-ddt.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("NonLISPNATProxyHandover");

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
void HandOver(Ptr<Node> node, Ptr<VirtualNetDevice> tun, Ptr<NetDevice> dev)
{
  Ptr<Ipv4> ipv4MN = node->GetObject<Ipv4> ();
  /* Set down interfaces with R2 */
  ipv4MN->SetDown (1);
  ipv4MN->SetDown (3);
  /* Set up interfaces with R3 */
  ipv4MN->SetUp (2);
  ipv4MN->SetUp (4);

  Ipv4StaticRoutingHelper ipv4SrHelper;
  Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();
  Ptr<Ipv4StaticRouting> ipv4Stat = ipv4SrHelper.GetStaticRouting (ipv4);
  /* Modifies routing tables of MN so that packets now leave host from
   * NetDevice 4, that is to say, from the virtual NetDevice associated
   * with R3 */
  ipv4Stat->AddNetworkRouteTo(Ipv4Address("0.0.0.0"), Ipv4Mask("/1"), 4);
  //ipv4Stat->AddNetworkRouteTo(Ipv4Address("128.0.0.0"), Ipv4Mask("/1"), 4); //Not necessary I think
  // ifIndexTap2 = 4
  NS_LOG_DEBUG("================= HANDOVER ===================");
  
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
                                         R2 <--------------> LISP-MN (n11)
  									         MR/MS     /(n3)                 /
                            (n6/n7)		/                     /
                              	|		 /                     /
																|		/                     /
	 	n0 <---->	R4 (n1) <----> R1 (n2)                     /
      (non-LISP)              / |   \                   /
                             /  |    \                 /
                  PETR/PITR /   |     \               /
                  (n9/n10)      |      \ NAT <----> R3
                               RTR        (n4)      (n5)
                               (n8)
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
    //LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
    //LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);
    //LogComponentEnable ("NonLISPNATProxyHandover", LOG_LEVEL_INFO);
    //LogComponentEnable ("LispHelper", LOG_LEVEL_ALL);
    //LogComponentEnable ("Ipv4L3Protocol", LOG_LEVEL_DEBUG);
    //LogComponentEnable ("Ipv4Nat", LOG_LEVEL_DEBUG);
    //LogComponentEnable ("MapResolverDdt", LOG_LEVEL_DEBUG);
    //LogComponentEnable ("MapServerDdt", LOG_LEVEL_DEBUG);
    //LogComponentEnable ("LispEtrItrApplication", LOG_LEVEL_DEBUG);
    //LogComponentEnable ("LispOverIp", LOG_LEVEL_DEBUG);
    //LogComponentEnable ("LispOverIpv4Impl", LOG_LEVEL_DEBUG);
    //LogComponentEnable ("SimpleMapTables", LOG_LEVEL_DEBUG);
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
  NetDeviceContainer dR2_dMN = p2p.Install (R2_MN);
  NetDeviceContainer dR3_dMN = p2p.Install (R3_MN);
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
  Address lispMnEidAddr = Ipv4Address ("172.16.0.1");
  int MN = 11;

  /* --- Add interface to existing device ---*/
  // R2
  Ptr<Ipv4> ipv4MN = nodes.Get (MN)->GetObject<Ipv4> ();
  uint32_t ifIndex_MN_R2 = ipv4MN->AddInterface (dR2_dMN.Get (1)); //Add a new interface to the device of MN
  ipv4MN->AddAddress (
      ifIndex_MN_R2,
      Ipv4InterfaceAddress (Ipv4Address ("0.0.0.0"), Ipv4Mask ("/0"))); //Add a new address, attached to the new interface
  // Due to DHCP program implementation constraint, we have to first assign a 0.0.0.0/0 for DHCP client
  // An interface is an association between an IP address and a device
  ipv4MN->SetForwarding (ifIndex_MN_R2, true);
  ipv4MN->SetUp (ifIndex_MN_R2);

  // R3
  uint32_t ifIndex_MN_R3 = ipv4MN->AddInterface (dR3_dMN.Get (1)); //Add a new interface to the device of MN
  ipv4MN->AddAddress (
      ifIndex_MN_R3,
      Ipv4InterfaceAddress (Ipv4Address ("0.0.0.0"), Ipv4Mask ("/0"))); //Add a new address, attached to the new interface
  // Due to DHCP program implementation constraint, we have to first assign a 0.0.0.0/0 for DHCP client
  // An interface is an association between an IP address and a device
  ipv4MN->SetForwarding (ifIndex_MN_R3, true);
  ipv4MN->SetUp (ifIndex_MN_R3);

  /* --- Complete new virtual device (with EID address of MN) --- */
  //R2
  Ptr<VirtualNetDevice> mnVirtualDevice1 = CreateObject<VirtualNetDevice> ();
  mnVirtualDevice1->SetAddress (Mac48Address ("11:00:01:02:03:01"));
  nodes.Get (MN)->AddDevice (mnVirtualDevice1); //Add new virtual device to MN
  Ptr<Ipv4> ipv4Tun = nodes.Get (MN)->GetObject<Ipv4> ();
  uint32_t ifIndexTap1 = ipv4MN->AddInterface (mnVirtualDevice1); //Add interface to new device
  ipv4Tun->AddAddress (
      ifIndexTap1,
      Ipv4InterfaceAddress (Ipv4Address::ConvertFrom(lispMnEidAddr),
          Ipv4Mask ("255.255.255.255"))); //Add a new address, attached to new virtual device
  ipv4Tun->SetForwarding (ifIndexTap1, true);
  ipv4Tun->SetUp (ifIndexTap1);
  //R3
  Ptr<VirtualNetDevice> mnVirtualDevice2 = CreateObject<VirtualNetDevice> ();
  mnVirtualDevice2->SetAddress (Mac48Address ("11:00:01:02:03:02"));
  nodes.Get (MN)->AddDevice (mnVirtualDevice2); //Add new virtual device to MN
  uint32_t ifIndexTap2 = ipv4MN->AddInterface (mnVirtualDevice2); //Add interface to new device
  ipv4Tun->AddAddress (
      ifIndexTap2,
      Ipv4InterfaceAddress (Ipv4Address::ConvertFrom(lispMnEidAddr),
          Ipv4Mask ("255.255.255.255"))); //Add a new address, attached to new virtual device
  ipv4Tun->SetForwarding (ifIndexTap2, true);
  ipv4Tun->SetUp (ifIndexTap2);

  /**
   * It is obligatory to set TransmitCallBack for virtual-net-device.
   * Otherwise when transmitting packet, virtual-net-device does not know what to do,
   * because it doesn't have a physical NIC.
   */
  CallBackForVirtualDeviceClass (mnVirtualDevice1, dR2_dMN.Get (1));
  CallBackForVirtualDeviceClass (mnVirtualDevice2, dR3_dMN.Get (1));

  /* --- Manually set interface for router (for existing device) --- */
  // R2
  Ptr<Ipv4> ipv4R2 = nodes.Get (3)->GetObject<Ipv4> ();
  uint32_t ifIndexR2 = ipv4R2->AddInterface (dR2_dMN.Get (0)); //Get existing device of R2
  ipv4R2->AddAddress (
      ifIndexR2,
      Ipv4InterfaceAddress (Ipv4Address ("10.1.2.254"), Ipv4Mask ("255.255.255.0")));
  ipv4R2->SetForwarding (ifIndexR2, true);
  ipv4R2->SetMetric (ifIndexR2, 1);
  ipv4R2->SetUp (ifIndexR2);
  Ipv4InterfaceContainer ap1Interface;
  ap1Interface.Add (ipv4R2, ifIndexR2);

  // R3
  Ptr<Ipv4> ipv4R3 = nodes.Get (5)->GetObject<Ipv4> ();
  uint32_t ifIndexR3 = ipv4R3->AddInterface (dR3_dMN.Get (0)); //Get existing device of R3
  ipv4R3->AddAddress (
      ifIndexR3,
      Ipv4InterfaceAddress (Ipv4Address ("10.1.3.254"), Ipv4Mask ("255.255.255.0")));
  ipv4R3->SetForwarding (ifIndexR3, true);
  ipv4R3->SetMetric (ifIndexR3, 1);
  ipv4R3->SetUp (ifIndexR3);
  Ipv4InterfaceContainer ap2Interface;
  ap2Interface.Add (ipv4R3, ifIndexR3);

  /* Must modify the routing table of MN so that the packets coming from 
            the application always use the EID address. */
  ipv4Protocol = nodes.Get (MN)->GetObject<Ipv4> ();
  ipv4Stat = ipv4SrHelper.GetStaticRouting (ipv4Protocol);
  ipv4Stat->AddNetworkRouteTo(Ipv4Address("0.0.0.0"), Ipv4Mask("/1"), ifIndexTap1);
  

  /*
  NOTE: All this code is "equivalent" to "ipv4.Assign (dR2_dMN);", except that we
  do everything manually by assigning addresses to devices ourselves. We do this because of DHCP
  */

  /* -------------------------------------- *\
                      DHCP
  \* -------------------------------------- */
  /* === (1) === */
  DhcpServerHelper dhcpServerHelper (
    Ipv4Address ("10.1.2.0"), //Pool of addresses the DHCP server can assign
    Ipv4Mask ("255.255.255.0"), // Mask of the pool
    Ipv4Address("10.1.2.254"), // Address of the DHCP server
    Ipv4Address ("10.1.2.0"), // Min address
    Ipv4Address ("10.1.2.252"), // Max address
    ap1Interface.GetAddress (0)); // Address of gateway router

  ApplicationContainer appDhcpServer = dhcpServerHelper.Install (nodes.Get (3)); //Install DHCP on R2
  appDhcpServer.Start (dhcpServerStart);
  appDhcpServer.Stop (EndTime);

  //The interface on which DHCP client has to be installed
  // -> Actually, it's not the interface but the device!
  DhcpClientHelper dhcpClientHelper (1); //device 1 corresponds to device installed by p2pHelper
  ApplicationContainer appDhcpClient = dhcpClientHelper.Install (nodes.Get (MN)); // Install it on MN
  appDhcpClient.Start (dhcpClientStart);
  appDhcpClient.Stop (EndTime);

  /* === (2) === */
  DhcpServerHelper dhcpServerHelper2 (
    Ipv4Address ("10.1.3.0"), //Pool of addresses the DHCP server can assign
    Ipv4Mask ("255.255.255.0"), // Mask of the pool
    Ipv4Address("10.1.3.254"), // Address of the DHCP server
    Ipv4Address ("10.1.3.0"), // Min address
    Ipv4Address ("10.1.3.252"), // Max address
    ap2Interface.GetAddress (0)); // Address of gateway router

  ApplicationContainer appDhcpServer2 = dhcpServerHelper2.Install (nodes.Get (5)); //Install DHCP on R3
  appDhcpServer2.Start (dhcpServerStart);
  appDhcpServer2.Stop (EndTime);

  //The interface on which DHCP client has to be installed
  // -> Actually, it's not the interface but the device!
  DhcpClientHelper dhcpClientHelper2 (2); //device 2 corresponds to device installed by p2pHelper
  ApplicationContainer appDhcpClient2 = dhcpClientHelper2.Install (nodes.Get (MN)); // Install it on MN
  appDhcpClient2.Start (HandTime);
  appDhcpClient2.Stop (EndTime);

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
  lispHelper.BuildRlocsSet ("./non_lisp_nat_proxy_handover_lisp_rlocs.txt");
  lispHelper.SetRtrs(NodeContainer(nodes.Get (8)));
  lispHelper.Install(lispRouters);
  NS_LOG_INFO("Lisp router installed");
  lispHelper.BuildMapTables2("./non_lisp_nat_proxy_handover_lisp_rlocs_config_xml.txt");
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
  echoClient.SetAttribute ("MaxPackets", UintegerValue (1000));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (0.01)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps = echoClient.Install (nodes.Get (MN));
  clientApps.Start (echoClientStart);
  clientApps.Stop (EndTime);
  

  /* OnOff TCP application */
  /*
  uint16_t port = 50000;
  Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), port)); //Local address
  PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", sinkLocalAddress);
  ApplicationContainer sinkApp = sinkHelper.Install (nodes.Get (0));
  sinkApp.Start (echoClientStart);
  sinkApp.Stop (EndTime);

  Address remoteAddress(InetSocketAddress (in0_iR4.GetAddress (0), port)); // Address of the destination (packet sink)
  OnOffHelper clientHelper ("ns3::TcpSocketFactory", remoteAddress);
  clientHelper.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  clientHelper.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
  clientHelper.SetAttribute ("DataRate", DataRateValue (DataRate ("500kb/s")));
  clientHelper.SetAttribute ("PacketSize", UintegerValue (512));
  //clientHelper.SetAttribute ("MaxBytes", UintegerValue (5120));

  ApplicationContainer clientApps = clientHelper.Install (nodes.Get (MN));
  clientApps.Start (echoClientStart);
  clientApps.Stop (EndTime);
  */

  /* -------------------------------------- *\
                SIMULATION
  \* -------------------------------------- */

  /* --- Tracing --- */
  p2p.EnablePcapAll ("non_lisp_nat_proxy_handover");
  //p2p.EnablePcap ("Apcap_RUN_"+RUN_NUMBER+"__", NodeContainer (nodes.Get (MN)));

  AsciiTraceHelper ascii;
  Ptr<OutputStreamWrapper> stream1 = ascii.CreateFileStream ("MN_interface1_run"+RUN_NUMBER+".tr");
  p2p.EnableAscii(stream1, MN, 1);
  Ptr<OutputStreamWrapper> stream2 = ascii.CreateFileStream ("MN_interface2_run"+RUN_NUMBER+".tr");
  p2p.EnableAscii(stream2, MN, 2);
  
  Simulator::Schedule(HandTime, &HandOver, nodes.Get(MN), mnVirtualDevice1, dR3_dMN.Get (1));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;




}