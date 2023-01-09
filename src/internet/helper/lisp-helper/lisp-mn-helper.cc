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

#include "lisp-mn-helper.h"
#include "ns3/log.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("LispMNHelper");


LispMNHelper::LispMNHelper (Ptr<Node> mn, Address addr): m_lispMn (mn), m_lispMnEidAddr (addr)
{

}

LispMNHelper::~LispMNHelper ()
{

}

void
LispMNHelper::SetRoutersAttachementPoints (NodeContainer& routers)
{
	m_routersAttachementPoints = routers;
	m_handOverTimes = std::vector<Time> (m_routersAttachementPoints.GetN ());
	m_handOverTimes.at (0) = m_dhcpServerStartTime;
}

void 
LispMNHelper::SetPointToPointHelper (const PointToPointHelper &p2p)
{
	m_p2p = p2p;
}

void 
LispMNHelper::SetRouterSubnet (Ipv4Address address, Ipv4Mask mask)
{
	RouterSubnet routerSubnet;
	routerSubnet.m_address = address;
	routerSubnet.m_mask = mask;
	m_routerSubnets.push_back (routerSubnet);
}

void 
LispMNHelper::SetEndTime (Time endTime)
{
	m_endTime = endTime;
}

void 
LispMNHelper::SetDhcpServerStartTime (Time time)
{
	m_dhcpServerStartTime = time;
}

void 
LispMNHelper::HandOver(Ptr<Node> node, uint32_t interface, uint32_t v_interface, uint32_t new_interface, uint32_t new_v_interface)
{
  Ptr<Ipv4> ipv4MN = node->GetObject<Ipv4> ();

  /* Set down interfaces with current attachement point */
  ipv4MN->SetDown (interface); //current interface
  ipv4MN->SetDown (v_interface); // current virtual interface

  /* Set up interfaces with new attachement point */
  ipv4MN->SetUp (new_interface); //new interface
  ipv4MN->SetUp (new_v_interface); // new virtual interface

  NS_LOG_DEBUG ("interface: " << interface);
  NS_LOG_DEBUG ("v_interface: " << v_interface);
  NS_LOG_DEBUG ("new_interface: " << new_interface);
  NS_LOG_DEBUG ("new_v_interface: " << new_v_interface);

  Ipv4StaticRoutingHelper ipv4SrHelper;
  Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();
  Ptr<Ipv4StaticRouting> ipv4Stat = ipv4SrHelper.GetStaticRouting (ipv4);

  /* Modifies routing tables of MN so that packets now leave host from
   * new_v_interface. 
   */
  ipv4Stat->AddNetworkRouteTo(Ipv4Address("0.0.0.0"), Ipv4Mask("/1"), new_v_interface);
  NS_LOG_DEBUG ("========================== HANDOVER ======================== ");
}

void 
LispMNHelper::ScheduleHandover (uint32_t attachementPoint1, uint32_t attachementPoint2, Time handTime)
{
	uint32_t interface = attachementPoint1 * 2 +1;
	uint32_t v_interface = interface + 1;
	uint32_t new_interface = attachementPoint2 * 2 +1;
	uint32_t new_v_interface = new_interface + 1;
	Simulator::Schedule(handTime, &LispMNHelper::HandOver, this, m_lispMn, interface, v_interface, new_interface, new_v_interface);

	m_handOverTimes.at (attachementPoint2) = handTime;
}


void 
LispMNHelper::Install ()
{
	int32_t firstIndexTap= -2;
	NS_ASSERT_MSG (m_routersAttachementPoints.GetN () >=1, "LISP-MN must have at least one attachement point to the network");
	NS_ASSERT_MSG (m_routersAttachementPoints.GetN () == m_routerSubnets.size (), "Number of routers and router addresses must match");

	NS_LOG_DEBUG ("Installing LISP-MN...");
	uint32_t j = 0;

	/**
	 * Perform the same routine for each attachement point:
	 * * On MN:
	 *   - Set new interface with address 0.0.0.0 on ptp NetDevice
	 *   - Set new VirtualNetDevice, with new interface with EID address
	 *     (don't forget to set SendCallback for VirtualNetDevice)
	 * * On Router:
	 *   - Set new interface on ptp NetDevice
	 *
	 * NOTE: All this code is "equivalent" to "ipv4.Assign (dMN_dR);", except that we
   * do everything manually by assigning addresses to devices ourselves. We do this because of DHCP, and
   * because we need a virtual device for MN.
	 */
	for (NodeContainer::Iterator i = m_routersAttachementPoints.Begin (); i != m_routersAttachementPoints.End (); ++i)
  {
    Ptr<Node> router = (*i);
    /* Point to point link between router and MN */
    NodeContainer MN_R = NodeContainer (m_lispMn, router);
    NetDeviceContainer dMN_dR = m_p2p.Install (MN_R);

    /* ------------------- *\
           Mobile Node
    \* ------------------- */
    /* --- Add interface to existing device of MN ---*/
    Ptr<Ipv4> ipv4MN = m_lispMn->GetObject<Ipv4> ();
    uint32_t ifIndex_MN = ipv4MN->AddInterface (dMN_dR.Get (0)); //Add a new interface to the device of MN
    ipv4MN->AddAddress (
      ifIndex_MN,
      Ipv4InterfaceAddress (Ipv4Address ("0.0.0.0"), Ipv4Mask ("/0"))); //Add a new address, attached to the new interface
    // Due to DHCP program implementation constraint, we have to first assign a 0.0.0.0/0 for DHCP client
    // An interface is an association between an IP address and a device
    ipv4MN->SetForwarding (ifIndex_MN, true);
    ipv4MN->SetUp (ifIndex_MN);

    /* --- Complete new virtual device (with EID address of MN) --- */
    Ptr<VirtualNetDevice> mnVirtualDevice = CreateObject<VirtualNetDevice> ();
    //mnVirtualDevice->SetAddress (Mac48Address ("11:00:01:02:03:01"));
    mnVirtualDevice->SetAddress (Mac48Address::Allocate ());
    m_lispMn->AddDevice (mnVirtualDevice); //Add new virtual device to MN
    Ptr<Ipv4> ipv4Tun = m_lispMn->GetObject<Ipv4> ();
    uint32_t ifIndexTap = ipv4MN->AddInterface (mnVirtualDevice); //Add interface to new device
    ipv4Tun->AddAddress (
      ifIndexTap,
      Ipv4InterfaceAddress (Ipv4Address::ConvertFrom(m_lispMnEidAddr),
          Ipv4Mask ("255.255.255.255"))); //Add a new address, attached to new virtual device
    ipv4Tun->SetForwarding (ifIndexTap, true);
    ipv4Tun->SetUp (ifIndexTap);

    m_virtualNetDevices.Add (mnVirtualDevice);
    firstIndexTap = (firstIndexTap == -2) ? ifIndexTap : firstIndexTap; //Keep track of first virtual net device created

    /**
     * It is obligatory to set TransmitCallBack for virtual-net-device.
     * Otherwise when transmitting packet, virtual-net-device does not know what to do,
     * because it doesn't have a physical NIC.
     */
    Ptr<CallBackForVirtualDevice> cb = Create<CallBackForVirtualDevice>(mnVirtualDevice, dMN_dR.Get (0));
    mnVirtualDevice->SetSendCallback (MakeCallback (&CallBackForVirtualDevice::TapVirtualSend, cb));

    /* ------------------- *\
           Router
    \* ------------------- */
    /* --- Prepare various addresses --- */
    Ipv4Address pool = m_routerSubnets.at (j).m_address;
    uint8_t buf[4];
    pool.Serialize (buf);
    buf[3] = (uint8_t) 254;
    Ipv4Address dhcpServerAddress = Ipv4Address::Deserialize (buf);

    /* --- Manually set interface for router (for existing device) --- */
    Ptr<Ipv4> ipv4R = router->GetObject<Ipv4> ();
    uint32_t ifIndex = ipv4R->AddInterface (dMN_dR.Get (1)); //Get existing device of R
    ipv4R->AddAddress (
      ifIndex,
      //Ipv4InterfaceAddress (Ipv4Address ("10.1.1.254"), Ipv4Mask ("255.255.255.0")));
      Ipv4InterfaceAddress (Ipv4Address::ConvertFrom (m_routerSubnets.at (j).m_address), m_routerSubnets.at (j).m_mask));
    
    ipv4R->SetForwarding (ifIndex, true);
    ipv4R->SetMetric (ifIndex, 1);
    ipv4R->SetUp (ifIndex);
    Ipv4InterfaceContainer ap1Interface;
    ap1Interface.Add (ipv4R, ifIndex);

    /* -------------------------------------- *\
                      DHCP
    \* -------------------------------------- */
    /*
    DhcpServerHelper dhcpServerHelper (
    Ipv4Address ("10.1.2.0"), //Pool of addresses the DHCP server can assign
    Ipv4Mask ("255.255.255.0"), // Mask of the pool
    Ipv4Address("10.1.2.254"), // Address of the DHCP server
    Ipv4Address ("10.1.2.0"), // Min address
    Ipv4Address ("10.1.2.252"), // Max address
    Ipv4Address ("10.1.2.254")); // Address of gateway router
		*/
		/* --- DHCP Server --- */
		Ipv4Mask mask = m_routerSubnets.at (j).m_mask;
		pool.Serialize (buf);
    buf[3] = (uint8_t) 252;
    Ipv4Address maxAddress = Ipv4Address::Deserialize (buf);

    DhcpServerHelper dhcpServerHelper (
    	pool, //Pool of addresses the DHCP server can assign
    	mask, // Mask of the pool
    	dhcpServerAddress, // Address of the DHCP server
    	pool, // Min address
    	maxAddress, // Max address
    	dhcpServerAddress); // Address of gateway router

	  ApplicationContainer appDhcpServer = dhcpServerHelper.Install (router); //Install DHCP on router
	  appDhcpServer.Start (m_dhcpServerStartTime);
	  appDhcpServer.Stop (m_endTime);

	  /* --- DHCP Client --- */
	  DhcpClientHelper dhcpClientHelper (ifIndex_MN); //The interface on which DHCP client has to be installed
	  ApplicationContainer appDhcpClient = dhcpClientHelper.Install (m_lispMn); // Install it on MN
	  appDhcpClient.Start (m_handOverTimes.at (j));
	  appDhcpClient.Stop (m_endTime);

	   j++;

  }

  /* Must modify the routing table of MN so that the packets coming from 
     the application always use the EID address. 
     By default, the first virtualNetDevice is used*/
  Ipv4StaticRoutingHelper ipv4SrHelper;
  Ptr<Ipv4> ipv4Protocol = m_lispMn->GetObject<Ipv4> ();
  Ptr<Ipv4StaticRouting> ipv4Stat = ipv4SrHelper.GetStaticRouting (ipv4Protocol);
  ipv4Stat->AddNetworkRouteTo(Ipv4Address("0.0.0.0"), Ipv4Mask("/1"), firstIndexTap);
}

} /* namespace ns3 */


