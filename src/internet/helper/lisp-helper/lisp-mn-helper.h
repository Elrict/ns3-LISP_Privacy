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

#ifndef SRC_INTERNET_HELPER_LISP_HELPER_LISP_MN_HELPER_H_
#define SRC_INTERNET_HELPER_LISP_HELPER_LISP_MN_HELPER_H_

#include "ns3/virtual-net-device.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"
#include "ns3/assert.h"
#include "ns3/simple-ref-count.h"
#include "ns3/dhcp-helper.h"

namespace ns3
{

// ===================================================================================================
class CallBackForVirtualDevice : public SimpleRefCount<CallBackForVirtualDevice>
{
  Ptr<VirtualNetDevice> mnVirtualDevice;
  Ptr<NetDevice> mnDevice;

public:
  bool
  TapVirtualSend (Ptr<Packet> packet, const Address& source,
      const Address& dest, uint16_t protocolNumber)
  {
    mnDevice->Send (packet, dest, protocolNumber);
    return true;
  }

  CallBackForVirtualDevice (Ptr<VirtualNetDevice> n0Tap, Ptr<NetDevice> netDve) :
      mnVirtualDevice (n0Tap), mnDevice (netDve)
  {
    //mnVirtualDevice->SetSendCallback (MakeCallback (&CallBackForVirtualDevice::TapVirtualSend, this));
  }

  virtual
  ~CallBackForVirtualDevice (){}


};
// ===================================================================================================


class LispMNHelper
	{
public:
	/**
	 * mn: the mobile node
	 * addr: the EID address of the mobile node
	 */
	LispMNHelper (Ptr<Node> mn, Address addr);
	virtual ~LispMNHelper ();

	/**
	 * nodes: All the nodes to which the LISP-MN will be attached.
	 * 
	 * Must be called after [SetDhcpServerStartTime] has been called.
	 *
	 * Note: The first node of the container (index 0) is the starting point
	 * for the LISP-MN. 
	 * Thus, the first handover that is scheduled with [ScheduleHandover] must
	 * start with the first node.
	 */
	void SetRoutersAttachementPoints(NodeContainer& nodes);

  void SetPointToPointHelper (const PointToPointHelper &p2p);

  void SetRouterSubnet (Ipv4Address address, Ipv4Mask mask);

  void SetEndTime (Time endTime);

  void SetDhcpServerStartTime (Time time);

  /**
   * Function that performs the actual handover
   */
  void HandOver (Ptr<Node> node, uint32_t interface, uint32_t v_interface, uint32_t new_interface, uint32_t new_v_interface);
  
  /**
   * Schedules a handover from 'attachementPoint1' to 'attachementPOint2' at time handTime.
   *
   * Attention: by default, we consider that the first attachement point is the first router
   * of SetRoutersAttaachementPoints. Therefore, the first handover that is scheduled must
	 * start with the first node.
   */
  void ScheduleHandover (uint32_t attachementPoint1, uint32_t attachementPoint2, Time handTime);

	/**
	 * This function will perform all necessary operations to set up a LISP-MN
	 */
	void Install ();

private:
	Ptr<Node> m_lispMn;
	Address m_lispMnEidAddr;

	PointToPointHelper m_p2p;

	NodeContainer m_routersAttachementPoints;
	Time m_endTime;
	Time m_dhcpServerStartTime;

	class RouterSubnet {
public:
    Ipv4Address m_address;
    Ipv4Mask m_mask;
  };

	std::vector<RouterSubnet> m_routerSubnets;
	std::vector<Time> m_handOverTimes; //

	NetDeviceContainer m_virtualNetDevices; // All virtual NetDevices of the LISP-MN (one by attachement point)
};

} /* namespace ns3 */

#endif /* SRC_INTERNET_HELPER_LISP_HELPER_LISP_MN_HELPER_H_ */
