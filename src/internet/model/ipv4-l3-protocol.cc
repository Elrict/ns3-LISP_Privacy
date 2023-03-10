// -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*-
//
// Copyright (c) 2006 Georgia Tech Research Corporation
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation;
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
// Author: George F. Riley<riley@ece.gatech.edu>
//

#include "ns3/packet.h"
#include "ns3/log.h"
#include "ns3/callback.h"
#include "ns3/ipv4-address.h"
#include "ns3/ipv4-route.h"
#include "ns3/node.h"
#include "ns3/socket.h"
#include "ns3/net-device.h"
#include "ns3/uinteger.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/object-vector.h"
#include "ns3/ipv4-header.h"
#include "ns3/boolean.h"
#include "ns3/ipv4-routing-table-entry.h"
#include "ns3/traffic-control-layer.h"

#include "loopback-net-device.h"
#include "arp-l3-protocol.h"
#include "arp-cache.h"
#include "ipv4-l3-protocol.h"
#include "icmpv4-l4-protocol.h"
#include "ipv4-interface.h"
#include "ipv4-raw-socket-impl.h"
#include "ipv4-netfilter.h"

#include "ns3/simple-map-tables.h" //to support LISP&LISP-MN
#include "ns3/lisp-over-ipv4.h"    //to support LISP&LISP-MN
#include "ns3/map-notify-msg.h"

namespace ns3
{

  NS_LOG_COMPONENT_DEFINE("Ipv4L3Protocol");

  const uint16_t Ipv4L3Protocol::PROT_NUMBER = 0x0800;

  NS_OBJECT_ENSURE_REGISTERED(Ipv4L3Protocol);

  TypeId
  Ipv4L3Protocol::GetTypeId(void)
  {
    static TypeId tid = TypeId("ns3::Ipv4L3Protocol")
                            .SetParent<Ipv4>()
                            .SetGroupName("Internet")
                            .AddConstructor<Ipv4L3Protocol>()
                            .AddAttribute("DefaultTtl",
                                          "The TTL value set by default on "
                                          "all outgoing packets generated on this node.",
                                          UintegerValue(64),
                                          MakeUintegerAccessor(&Ipv4L3Protocol::m_defaultTtl),
                                          MakeUintegerChecker<uint8_t>())
                            .AddAttribute("FragmentExpirationTimeout",
                                          "When this timeout expires, the fragments "
                                          "will be cleared from the buffer.",
                                          TimeValue(Seconds(30)),
                                          MakeTimeAccessor(&Ipv4L3Protocol::m_fragmentExpirationTimeout),
                                          MakeTimeChecker())
                            .AddTraceSource("Tx",
                                            "Send ipv4 packet to outgoing interface.",
                                            MakeTraceSourceAccessor(&Ipv4L3Protocol::m_txTrace),
                                            "ns3::Ipv4L3Protocol::TxRxTracedCallback")
                            .AddTraceSource("Rx",
                                            "Receive ipv4 packet from incoming interface.",
                                            MakeTraceSourceAccessor(&Ipv4L3Protocol::m_rxTrace),
                                            "ns3::Ipv4L3Protocol::TxRxTracedCallback")
                            .AddTraceSource("Drop",
                                            "Drop ipv4 packet",
                                            MakeTraceSourceAccessor(&Ipv4L3Protocol::m_dropTrace),
                                            "ns3::Ipv4L3Protocol::DropTracedCallback")
                            .AddAttribute("InterfaceList",
                                          "The set of Ipv4 interfaces associated to this Ipv4 stack.",
                                          ObjectVectorValue(),
                                          MakeObjectVectorAccessor(&Ipv4L3Protocol::m_interfaces),
                                          MakeObjectVectorChecker<Ipv4Interface>())

                            .AddTraceSource("SendOutgoing",
                                            "A newly-generated packet by this node is "
                                            "about to be queued for transmission",
                                            MakeTraceSourceAccessor(&Ipv4L3Protocol::m_sendOutgoingTrace),
                                            "ns3::Ipv4L3Protocol::SentTracedCallback")
                            .AddTraceSource("UnicastForward",
                                            "A unicast IPv4 packet was received by this node "
                                            "and is being forwarded to another node",
                                            MakeTraceSourceAccessor(&Ipv4L3Protocol::m_unicastForwardTrace),
                                            "ns3::Ipv4L3Protocol::SentTracedCallback")
                            .AddTraceSource("LocalDeliver",
                                            "An IPv4 packet was received by/for this node, "
                                            "and it is being forward up the stack",
                                            MakeTraceSourceAccessor(&Ipv4L3Protocol::m_localDeliverTrace),
                                            "ns3::Ipv4L3Protocol::SentTracedCallback")

        ;
    return tid;
  }

  Ipv4L3Protocol::Ipv4L3Protocol() : m_netfilter(0)
  {
    NS_LOG_FUNCTION(this);
  }

  Ipv4L3Protocol::~Ipv4L3Protocol()
  {
    NS_LOG_FUNCTION(this);
  }

  void
  Ipv4L3Protocol::Insert(Ptr<IpL4Protocol> protocol)
  {
    NS_LOG_FUNCTION(this << protocol);
    L4ListKey_t key = std::make_pair(protocol->GetProtocolNumber(), -1);
    if (m_protocols.find(key) != m_protocols.end())
    {
      NS_LOG_WARN("Overwriting default protocol " << int(protocol->GetProtocolNumber()));
    }
    m_protocols[key] = protocol;
  }

  void
  Ipv4L3Protocol::Insert(Ptr<IpL4Protocol> protocol, uint32_t interfaceIndex)
  {
    NS_LOG_FUNCTION(this << protocol << interfaceIndex);

    L4ListKey_t key = std::make_pair(protocol->GetProtocolNumber(), interfaceIndex);
    if (m_protocols.find(key) != m_protocols.end())
    {
      NS_LOG_WARN("Overwriting protocol " << int(protocol->GetProtocolNumber()) << " on interface " << int(interfaceIndex));
    }
    m_protocols[key] = protocol;
  }

  void
  Ipv4L3Protocol::Remove(Ptr<IpL4Protocol> protocol)
  {
    NS_LOG_FUNCTION(this << protocol);

    L4ListKey_t key = std::make_pair(protocol->GetProtocolNumber(), -1);
    L4List_t::iterator iter = m_protocols.find(key);
    if (iter == m_protocols.end())
    {
      NS_LOG_WARN("Trying to remove an non-existent default protocol " << int(protocol->GetProtocolNumber()));
    }
    else
    {
      m_protocols.erase(key);
    }
  }

  void
  Ipv4L3Protocol::Remove(Ptr<IpL4Protocol> protocol, uint32_t interfaceIndex)
  {
    NS_LOG_FUNCTION(this << protocol << interfaceIndex);

    L4ListKey_t key = std::make_pair(protocol->GetProtocolNumber(), interfaceIndex);
    L4List_t::iterator iter = m_protocols.find(key);
    if (iter == m_protocols.end())
    {
      NS_LOG_WARN("Trying to remove an non-existent protocol " << int(protocol->GetProtocolNumber()) << " on interface " << int(interfaceIndex));
    }
    else
    {
      m_protocols.erase(key);
    }
  }

  Ptr<IpL4Protocol>
  Ipv4L3Protocol::GetProtocol(int protocolNumber) const
  {
    NS_LOG_FUNCTION(this << protocolNumber);

    return GetProtocol(protocolNumber, -1);
  }

  Ptr<IpL4Protocol>
  Ipv4L3Protocol::GetProtocol(int protocolNumber, int32_t interfaceIndex) const
  {
    NS_LOG_FUNCTION(this << protocolNumber << interfaceIndex);

    L4ListKey_t key;
    L4List_t::const_iterator i;
    if (interfaceIndex >= 0)
    {
      // try the interface-specific protocol.
      key = std::make_pair(protocolNumber, interfaceIndex);
      i = m_protocols.find(key);
      if (i != m_protocols.end())
      {
        return i->second;
      }
    }
    // try the generic protocol.
    key = std::make_pair(protocolNumber, -1);
    i = m_protocols.find(key);
    if (i != m_protocols.end())
    {
      return i->second;
    }

    return 0;
  }

  void
  Ipv4L3Protocol::SetNode(Ptr<Node> node)
  {
    NS_LOG_FUNCTION(this << node);
    m_node = node;
    // Add a LoopbackNetDevice if needed, and an Ipv4Interface on top of it
    SetupLoopback();
  }

  Ptr<Socket>
  Ipv4L3Protocol::CreateRawSocket(void)
  {
    NS_LOG_FUNCTION(this);
    Ptr<Ipv4RawSocketImpl> socket = CreateObject<Ipv4RawSocketImpl>();
    socket->SetNode(m_node);
    m_sockets.push_back(socket);
    return socket;
  }
  void
  Ipv4L3Protocol::DeleteRawSocket(Ptr<Socket> socket)
  {
    NS_LOG_FUNCTION(this << socket);
    for (SocketList::iterator i = m_sockets.begin(); i != m_sockets.end(); ++i)
    {
      if ((*i) == socket)
      {
        m_sockets.erase(i);
        return;
      }
    }
    return;
  }
  /*
   * This method is called by AddAgregate and completes the aggregation
   * by setting the node in the ipv4 stack
   */
  void
  Ipv4L3Protocol::NotifyNewAggregate()
  {
    NS_LOG_FUNCTION(this);
    if (m_node == 0)
    {
      Ptr<Node> node = this->GetObject<Node>();
      // verify that it's a valid node and that
      // the node has not been set before
      if (node != 0)
      {
        this->SetNode(node);
      }
    }
    Ipv4::NotifyNewAggregate();
  }

  void
  Ipv4L3Protocol::SetRoutingProtocol(Ptr<Ipv4RoutingProtocol> routingProtocol)
  {
    NS_LOG_FUNCTION(this << routingProtocol);
    m_routingProtocol = routingProtocol;
    m_routingProtocol->SetIpv4(this);
  }

  Ptr<Ipv4RoutingProtocol>
  Ipv4L3Protocol::GetRoutingProtocol(void) const
  {
    NS_LOG_FUNCTION(this);
    return m_routingProtocol;
  }

  void
  Ipv4L3Protocol::SetNetfilter(Ptr<Ipv4Netfilter> netfilter)
  {
    NS_LOG_FUNCTION(this << netfilter);
    m_netfilter = netfilter;
  }

  Ptr<Ipv4Netfilter>
  Ipv4L3Protocol::GetNetfilter(void) const
  {
    return m_netfilter;
  }

  void
  Ipv4L3Protocol::DoDispose(void)
  {
    NS_LOG_FUNCTION(this);
    for (L4List_t::iterator i = m_protocols.begin(); i != m_protocols.end(); ++i)
    {
      i->second = 0;
    }
    m_protocols.clear();

    for (Ipv4InterfaceList::iterator i = m_interfaces.begin(); i != m_interfaces.end(); ++i)
    {
      *i = 0;
    }
    m_interfaces.clear();
    m_reverseInterfacesContainer.clear();

    m_sockets.clear();
    m_node = 0;
    m_routingProtocol = 0;

    for (MapFragments_t::iterator it = m_fragments.begin(); it != m_fragments.end(); it++)
    {
      it->second = 0;
    }

    for (MapFragmentsTimers_t::iterator it = m_fragmentsTimers.begin(); it != m_fragmentsTimers.end(); it++)
    {
      if (it->second.IsRunning())
      {
        it->second.Cancel();
      }
    }

    m_fragments.clear();
    m_fragmentsTimers.clear();

    Object::DoDispose();
  }

  void
  Ipv4L3Protocol::SetupLoopback(void)
  {
    NS_LOG_FUNCTION(this);

    Ptr<Ipv4Interface> interface = CreateObject<Ipv4Interface>();
    Ptr<LoopbackNetDevice> device = 0;
    // First check whether an existing LoopbackNetDevice exists on the node
    for (uint32_t i = 0; i < m_node->GetNDevices(); i++)
    {
      if ((device = DynamicCast<LoopbackNetDevice>(m_node->GetDevice(i))))
      {
        break;
      }
    }
    if (device == 0)
    {
      device = CreateObject<LoopbackNetDevice>();
      m_node->AddDevice(device);
    }
    interface->SetDevice(device);
    interface->SetNode(m_node);
    Ipv4InterfaceAddress ifaceAddr = Ipv4InterfaceAddress(Ipv4Address::GetLoopback(), Ipv4Mask::GetLoopback());
    interface->AddAddress(ifaceAddr);
    uint32_t index = AddIpv4Interface(interface);
    Ptr<Node> node = GetObject<Node>();
    node->RegisterProtocolHandler(MakeCallback(&Ipv4L3Protocol::Receive, this),
                                  Ipv4L3Protocol::PROT_NUMBER, device);
    interface->SetUp();
    if (m_routingProtocol != 0)
    {
      m_routingProtocol->NotifyInterfaceUp(index);
    }
  }

  void
  Ipv4L3Protocol::SetDefaultTtl(uint8_t ttl)
  {
    NS_LOG_FUNCTION(this << static_cast<uint32_t>(ttl));
    m_defaultTtl = ttl;
  }

  uint32_t
  Ipv4L3Protocol::AddInterface(Ptr<NetDevice> device)
  {
    NS_LOG_FUNCTION(this << device);
    NS_ASSERT(m_node != 0);

    Ptr<TrafficControlLayer> tc = m_node->GetObject<TrafficControlLayer>();

    NS_ASSERT(tc != 0);

    m_node->RegisterProtocolHandler(MakeCallback(&TrafficControlLayer::Receive, tc),
                                    Ipv4L3Protocol::PROT_NUMBER, device);
    m_node->RegisterProtocolHandler(MakeCallback(&TrafficControlLayer::Receive, tc),
                                    ArpL3Protocol::PROT_NUMBER, device);

    tc->RegisterProtocolHandler(MakeCallback(&Ipv4L3Protocol::Receive, this),
                                Ipv4L3Protocol::PROT_NUMBER, device);
    tc->RegisterProtocolHandler(MakeCallback(&ArpL3Protocol::Receive, PeekPointer(GetObject<ArpL3Protocol>())),
                                ArpL3Protocol::PROT_NUMBER, device);

    Ptr<Ipv4Interface> interface = CreateObject<Ipv4Interface>();
    interface->SetNode(m_node);
    interface->SetDevice(device);
    interface->SetTrafficControl(tc);
    interface->SetForwarding(m_ipForward);
    tc->SetupDevice(device);
    return AddIpv4Interface(interface);
  }

  uint32_t
  Ipv4L3Protocol::AddIpv4Interface(Ptr<Ipv4Interface> interface)
  {
    NS_LOG_FUNCTION(this << interface);
    uint32_t index = m_interfaces.size();
    m_interfaces.push_back(interface);
    m_reverseInterfacesContainer[interface->GetDevice()] = index;
    return index;
  }

  Ptr<Ipv4Interface>
  Ipv4L3Protocol::GetInterface(uint32_t index) const
  {
    NS_LOG_FUNCTION(this << index);
    if (index < m_interfaces.size())
    {
      return m_interfaces[index];
    }
    return 0;
  }

  uint32_t
  Ipv4L3Protocol::GetNInterfaces(void) const
  {
    NS_LOG_FUNCTION(this);
    return m_interfaces.size();
  }

  int32_t
  Ipv4L3Protocol::GetInterfaceForAddress(
      Ipv4Address address) const
  {
    NS_LOG_FUNCTION(this << address);
    int32_t interface = 0;
    for (Ipv4InterfaceList::const_iterator i = m_interfaces.begin();
         i != m_interfaces.end();
         i++, interface++)
    {
      for (uint32_t j = 0; j < (*i)->GetNAddresses(); j++)
      {
        if ((*i)->GetAddress(j).GetLocal() == address)
        {
          return interface;
        }
      }
    }

    return -1;
  }

  int32_t
  Ipv4L3Protocol::GetInterfaceForPrefix(
      Ipv4Address address,
      Ipv4Mask mask) const
  {
    NS_LOG_FUNCTION(this << address << mask);
    int32_t interface = 0;
    for (Ipv4InterfaceList::const_iterator i = m_interfaces.begin();
         i != m_interfaces.end();
         i++, interface++)
    {
      for (uint32_t j = 0; j < (*i)->GetNAddresses(); j++)
      {
        if ((*i)->GetAddress(j).GetLocal().CombineMask(mask) == address.CombineMask(mask))
        {
          return interface;
        }
      }
    }

    return -1;
  }

  int32_t
  Ipv4L3Protocol::GetInterfaceForDevice(
      Ptr<const NetDevice> device) const
  {
    NS_LOG_FUNCTION(this << device);

    Ipv4InterfaceReverseContainer::const_iterator iter = m_reverseInterfacesContainer.find(device);
    if (iter != m_reverseInterfacesContainer.end())
    {
      return (*iter).second;
    }

    return -1;
  }

  bool
  Ipv4L3Protocol::IsDestinationAddress(Ipv4Address address, uint32_t iif) const
  {
    NS_LOG_FUNCTION(this << address << iif);
    // First check the incoming interface for a unicast address match
    for (uint32_t i = 0; i < GetNAddresses(iif); i++)
    {
      Ipv4InterfaceAddress iaddr = GetAddress(iif, i);
      if (address == iaddr.GetLocal())
      {
        NS_LOG_LOGIC("For me (destination " << address << " match)");
        return true;
      }
      if (address == iaddr.GetBroadcast())
      {
        NS_LOG_LOGIC("For me (interface broadcast address)");
        return true;
      }
    }

    if (address.IsMulticast())
    {
#ifdef NOTYET
      if (MulticastCheckGroup(iif, address))
#endif
        if (true)
        {
          NS_LOG_LOGIC("For me (Ipv4Addr multicast address");
          return true;
        }
    }

    if (address.IsBroadcast())
    {
      NS_LOG_LOGIC("For me (Ipv4Addr broadcast address)");
      return true;
    }

    if (GetWeakEsModel()) // Check other interfaces
    {
      for (uint32_t j = 0; j < GetNInterfaces(); j++)
      {
        if (j == uint32_t(iif))
          continue;
        for (uint32_t i = 0; i < GetNAddresses(j); i++)
        {
          Ipv4InterfaceAddress iaddr = GetAddress(j, i);
          if (address == iaddr.GetLocal())
          {
            NS_LOG_LOGIC("For me (destination " << address << " match) on another interface");
            return true;
          }
          //  This is a small corner case:  match another interface's broadcast address
          if (address == iaddr.GetBroadcast())
          {
            NS_LOG_LOGIC("For me (interface broadcast address on another interface)");
            return true;
          }
        }
      }
    }
    return false;
  }

  void
  Ipv4L3Protocol::Receive(Ptr<NetDevice> device, Ptr<const Packet> p, uint16_t protocol, const Address &from,
                          const Address &to, NetDevice::PacketType packetType)
  {
    NS_LOG_FUNCTION(this << device << p << protocol << from << to << packetType);

    NS_LOG_LOGIC("Packet from " << from << " received on node " << m_node->GetId());
    //===================================Adaptation to support LISP==============================
    /**
     * ATTENTION: It's important to save the input parameter device into
     * LispOverIpv4 object. In case of double encapsulation, LispInput will
     * find the inner is still a LISP packet. Thus, it need to call Ipv4L3Protocol::Receive()
     * to re-inject the inner IP packet on the device.
     * Here the device number  (or pointer) is that saved in LispOverIpv4 object.
     *
     * Yue do this. not Lionel.
     */
    Ptr<LispOverIpv4> lisp = m_node->GetObject<LispOverIpv4>();
    if (lisp != 0)
    {
      // We save receive parameter in order to use them later in lispInput if needed
      lisp->RecordReceiveParams(device, protocol, packetType);
    }
    //===================================End of Adaptation to support LISP=======================

    int32_t interface = GetInterfaceForDevice(device);
    NS_ASSERT_MSG(interface != -1, "Received a packet from an interface that is not known to IPv4");

    Ptr<Packet> packet = p->Copy();
    Ptr<Ipv4Interface> ipv4Interface = m_interfaces[interface];

    if (ipv4Interface->IsUp())
    {
      m_rxTrace(packet, m_node->GetObject<Ipv4>(), interface);
    }
    else
    {
      NS_LOG_LOGIC("Dropping received packet -- interface is down");
      Ipv4Header ipHeader;
      packet->RemoveHeader(ipHeader);
      m_dropTrace(ipHeader, packet, DROP_INTERFACE_DOWN, m_node->GetObject<Ipv4>(), interface);
      return;
    }

    // ========================== Adaptation to support NetFilter =============================
    if (m_netfilter != 0)
    {
      NS_LOG_DEBUG("NF_INET_PRE_ROUTING Hookon node " << m_node->GetId());
      Verdicts_t verdict = (Verdicts_t)m_netfilter->ProcessHook(PF_INET, NF_INET_PRE_ROUTING, packet, device, 0);
      if (verdict == NF_DROP)
      {
        NS_LOG_DEBUG("NF_INET_PRE_ROUTING packet not accepted");
        Ipv4Header ipHeader;
        packet->RemoveHeader(ipHeader);
        m_dropTrace(ipHeader, packet, DROP_NETFILTER, m_node->GetObject<Ipv4>(), interface);
        return;
      }
    }
    // ==================== End of adaptation to support NetFilter ====================

    Ipv4Header ipHeader;
    if (Node::ChecksumEnabled())
    {
      ipHeader.EnableChecksum();
    }
    packet->RemoveHeader(ipHeader);

    // Trim any residual frame padding from underlying devices
    if (ipHeader.GetPayloadSize() < packet->GetSize())
    {
      packet->RemoveAtEnd(packet->GetSize() - ipHeader.GetPayloadSize());
    }

    if (!ipHeader.IsChecksumOk())
    {
      NS_LOG_LOGIC("Dropping received packet -- checksum not ok");
      m_dropTrace(ipHeader, packet, DROP_BAD_CHECKSUM, m_node->GetObject<Ipv4>(), interface);
      return;
    }

    // the packet is valid, we update the ARP cache entry (if present)
    Ptr<ArpCache> arpCache = ipv4Interface->GetArpCache();
    if (arpCache)
    {
      // case one, it's a a direct routing.
      ArpCache::Entry *entry = arpCache->Lookup(ipHeader.GetSource());
      if (entry)
      {
        if (entry->IsAlive())
        {
          entry->UpdateSeen();
        }
      }
      else
      {
        // It's not in the direct routing, so it's the router, and it could have multiple IP addresses.
        // In doubt, update all of them.
        // Note: it's a confirmed behavior for Linux routers.
        std::list<ArpCache::Entry *> entryList = arpCache->LookupInverse(from);
        std::list<ArpCache::Entry *>::iterator iter;
        for (iter = entryList.begin(); iter != entryList.end(); iter++)
        {
          if ((*iter)->IsAlive())
          {
            (*iter)->UpdateSeen();
          }
        }
      }
    }

    for (SocketList::iterator i = m_sockets.begin(); i != m_sockets.end(); ++i)
    {
      NS_LOG_DEBUG("Forwarding to raw socket");
      Ptr<Ipv4RawSocketImpl> socket = *i;
      socket->ForwardUp(packet, ipHeader, ipv4Interface);
    }

    NS_ASSERT_MSG(m_routingProtocol != 0, "Need a routing protocol object to process packets");
    if (!m_routingProtocol->RouteInput(packet, ipHeader, device,
                                       MakeCallback(&Ipv4L3Protocol::IpForward, this),
                                       MakeCallback(&Ipv4L3Protocol::IpMulticastForward, this),
                                       MakeCallback(&Ipv4L3Protocol::LocalDeliver, this),
                                       MakeCallback(&Ipv4L3Protocol::RouteInputError, this)))
    {
      NS_LOG_WARN("No route found for forwarding packet.  Drop.");
      m_dropTrace(ipHeader, packet, DROP_NO_ROUTE, m_node->GetObject<Ipv4>(), interface);
    }
  }

  Ptr<Icmpv4L4Protocol>
  Ipv4L3Protocol::GetIcmp(void) const
  {
    NS_LOG_FUNCTION(this);
    Ptr<IpL4Protocol> prot = GetProtocol(Icmpv4L4Protocol::GetStaticProtocolNumber());
    if (prot != 0)
    {
      return prot->GetObject<Icmpv4L4Protocol>();
    }
    else
    {
      return 0;
    }
  }

  bool
  Ipv4L3Protocol::IsUnicast(Ipv4Address ad) const
  {
    NS_LOG_FUNCTION(this << ad);

    if (ad.IsBroadcast() || ad.IsMulticast())
    {
      return false;
    }
    else
    {
      // check for subnet-broadcast
      for (uint32_t ifaceIndex = 0; ifaceIndex < GetNInterfaces(); ifaceIndex++)
      {
        for (uint32_t j = 0; j < GetNAddresses(ifaceIndex); j++)
        {
          Ipv4InterfaceAddress ifAddr = GetAddress(ifaceIndex, j);
          NS_LOG_LOGIC("Testing address " << ad << " with subnet-directed broadcast " << ifAddr.GetBroadcast());
          if (ad == ifAddr.GetBroadcast())
          {
            return false;
          }
        }
      }
    }

    return true;
  }

  bool
  Ipv4L3Protocol::IsUnicast(Ipv4Address ad, Ipv4Mask interfaceMask) const
  {
    NS_LOG_FUNCTION(this << ad << interfaceMask);
    return !ad.IsMulticast() && !ad.IsSubnetDirectedBroadcast(interfaceMask);
  }

  void
  Ipv4L3Protocol::SendWithHeader(Ptr<Packet> packet,
                                 Ipv4Header ipHeader,
                                 Ptr<Ipv4Route> route)
  {
    NS_LOG_FUNCTION(this << packet << ipHeader << route);
    if (route)
    {
      NS_LOG_DEBUG("The content of received Ipv4 route: " << *route);
    }
    else
    {
      NS_LOG_DEBUG("No route is given as input parameter!");
    }
    if (Node::ChecksumEnabled())
    {
      ipHeader.EnableChecksum();
    }
    SendRealOut(route, packet, ipHeader);
  }

  void
  Ipv4L3Protocol::CallTxTrace(const Ipv4Header &ipHeader, Ptr<Packet> packet,
                              Ptr<Ipv4> ipv4, uint32_t interface)
  {
    Ptr<Packet> packetCopy = packet->Copy();
    packetCopy->AddHeader(ipHeader);
    m_txTrace(packetCopy, ipv4, interface);
  }

  void
  Ipv4L3Protocol::Send(Ptr<Packet> packet,
                       Ipv4Address source,
                       Ipv4Address destination,
                       uint8_t protocol,
                       Ptr<Ipv4Route> route)
  {
    NS_LOG_FUNCTION(this << packet << source << destination << uint32_t(protocol) << route);
    Ipv4Header ipHeader;
    bool mayFragment = true;
    uint8_t ttl = m_defaultTtl;

    // ============= Adaptation for NetFilter =============
    Ptr<NetDevice> device;

    if (route)
    {
      device = route->GetOutputDevice();
    }
    // =========== End of adaptation for NetFilter ============

    SocketIpTtlTag tag;
    bool found = packet->RemovePacketTag(tag);
    if (found)
    {
      ttl = tag.GetTtl();
    }

    uint8_t tos = 0;
    SocketIpTosTag ipTosTag;
    found = packet->RemovePacketTag(ipTosTag);
    if (found)
    {
      tos = ipTosTag.GetTos();
    }
    //==================================Adaption to support LISP=================================
    // before going further, check if packet must be encapsulated and
    // encapsulate
    Ptr<LispOverIpv4> lispOverIpv4;
    lispOverIpv4 = m_node->GetObject<LispOverIpv4>();
    int nbEntriesDB = 0;
    /**
     * Only and only if lispOverIpv4 object is present and not a DHCP request packet
     * we pass to lisp processing part.
     */
    if (lispOverIpv4 and not destination.IsBroadcast())
    {
      nbEntriesDB =
          lispOverIpv4->GetMapTablesV4()->GetNMapEntriesLispDataBase();
    }
    /**
     * It should be pointers to lispOverIpv4 and number of entries in
     * Lisp database both not 0. Otherwise DHCP does not work.
     * 1) if no mapTables entries check in Ipv4L3Protocol::Send() method,
     * program enters in the LISP-processing code, which leads to no layer 2
     * frame send out. => No DHCP request (0.0.0.0 -> 255.255.255.255)
     * 2) In Ipv4L3Protocol::IpForward() method, LispOverIpv4Impl::NeedEncapsulation
     * will be called, which causes segmentation fault
     */
    // lisp procedure will executed.
    if (route)
    {
      NS_LOG_DEBUG("The content of received Ipv4 route: " << *route);
    }
    else
    {
      NS_LOG_DEBUG("No route is given as input parameter!");
    }
    if (lispOverIpv4 != 0 and (nbEntriesDB != 0 || lispOverIpv4->GetPitr()))
    {
      Ptr<MapEntry> srcMapEntry = 0;
      Ptr<MapEntry> destMapEntry = 0;
      int32_t interface = 0;

      // here we build the inner header
      Ipv4Header innerIpHeader = BuildHeader(source, destination, protocol,
                                             packet->GetSize(), ttl, tos,
                                             mayFragment);

      Ptr<Ipv4Route> lispRoute;
      // we get the mask thanks to the outgoing interface
      if (route)
      {
        interface = GetInterfaceForDevice(route->GetOutputDevice());
        lispRoute = route;
      }
      else
      {
        NS_LOG_DEBUG("The given Ipv4 route is 0. Need to create one...");
        Socket::SocketErrno errno_;
        Ptr<NetDevice> oif(0); // unused for now
        Ptr<Ipv4Route> newRoute;
        if (m_routingProtocol != 0)
        {
          newRoute = m_routingProtocol->RouteOutput(packet,
                                                    innerIpHeader, oif,
                                                    errno_);
        }
        else
        {
          NS_ASSERT_MSG(m_routingProtocol != 0, "Ipv4L3Protocol::Send: m_routingProtocol should never be 0!");
          //							NS_LOG_ERROR("Ipv4L3Protocol::Send: m_routingProtocol == 0");
        }
        if (newRoute)
        {
          interface = GetInterfaceForDevice(
              newRoute->GetOutputDevice());
          lispRoute = newRoute;
        }
      }

      Ipv4InterfaceAddress ifAddr = GetAddress(interface, 0);
      NS_LOG_DEBUG("We check if we need Encapsulation for " << destination);
      /*
       * if the packet has been decapsulated in Receive,
       * OK as we will check if it needs encapsulation
       * before going further
       */
      bool destIsRloc = lispOverIpv4->IsLocatorInList(
          static_cast<Address>(destination));
      bool srcIsRloc = lispOverIpv4->IsLocatorInList(
          static_cast<Address>(source));
      
      LispOverIp::EcmEncapsulation ecmEncap = LispOverIp::ECM_NO;
      if (destIsRloc && srcIsRloc)
      {
        NS_LOG_DEBUG(
            "Both dst and src are in RLOC list known by xTR.");

        /*
         * MapRegister messages arrive here (because both source and destination
         * are RLOCs).
         * If LISP device is NATed:
         *    MapRegister message must be ECM encapsulated towards RTR, instead of going to
         *    no_encap.
         * If LISP device is RTR:
         *    MapRegister messages from other xTRs must be ECM encapsulated towards MS.
         *    Own MapRegister messages musn't be encapsulated.
         */

        /* Check if this is a MapRegister message */
        Ptr<Packet> packetCopy = packet->Copy();
        UdpHeader udpHeader;
        packetCopy->RemoveHeader(udpHeader);

        uint8_t buf[packetCopy->GetSize()];
        packetCopy->CopyData(buf, packetCopy->GetSize());
        uint8_t msg_type = (buf[0] >> 4);
        if (msg_type == static_cast<uint8_t>(MapRegisterMsg::GetMsgType()))
        {
          if (lispOverIpv4->IsNated())
          {
            NS_LOG_DEBUG("This is a MapRegister and device is NATed -> Encapsulate");
            ecmEncap = LispOverIp::ECM_XTR;
            Ptr<Locator> srcRloc = Create<Locator>(static_cast<Address>(source));
            srcMapEntry = Create<MapEntryImpl>(srcRloc); // Like case where source is RLOC
            goto ecm;
          }
          else if (lispOverIpv4->IsRtr())
          {
            /* Change inner header source with RTR RLOC (found in local database)
            //Assumption: there is a unique eid space and a unique locator in database, or several
              eid space but all under the same RLOC */
            Ptr<MapTables> mapTables = lispOverIpv4->GetMapTablesV4();
            std::list<Ptr<MapEntry>> entryList;
            mapTables->GetMapEntryList(MapTables::IN_DATABASE, entryList);
            Ptr<MapEntry> mapEntry = entryList.front();
            Address maddr = mapEntry->RlocSelection()->GetRlocAddress();

            // Own MapRegisters musn't be encapsulated.
            if (innerIpHeader.GetSource().IsEqual(Ipv4Address::ConvertFrom(maddr)))
              goto no_encap;

            innerIpHeader.SetSource(Ipv4Address::ConvertFrom(maddr));

            ecmEncap = LispOverIp::ECM_RTR;
            Ptr<Locator> dstRloc = Create<Locator>(static_cast<Address>(destination)); // Encapsulate towards MS
            destMapEntry = Create<MapEntryImpl>(dstRloc);
            goto ecm;
          }
          else
          { // LISP device is neither NATed nor an RTR -> Usual behaviour
            goto no_encap;
          }
        }
        /*
         * MapNotify messages arrive here (because both source and destination
         * are RLOCs).
         * If LISP device is RTR:
         *    MapNotify messages destined to other xTRs must be DataMapNotifyEncapsulated
         *    towards NATed xTR
         */
        else if (msg_type == static_cast<uint8_t>(MapNotifyMsg::GetMsgType()) && lispOverIpv4->IsRtr())
        {
          /* Source locator is RTR RLOC (found in local database)
          //Assumption: there is a unique eid space and a unique locator in database,
          or several eid space but all under the same RLOC */
          NS_LOG_DEBUG("MapNotify message");
        data_encapsulate:
          Ptr<MapTables> mapTables = lispOverIpv4->GetMapTablesV4();
          std::list<Ptr<MapEntry>> entryList;
          mapTables->GetMapEntryList(MapTables::IN_DATABASE, entryList);
          Ptr<MapEntry> mapEntry = entryList.front();
          Address maddr = mapEntry->RlocSelection()->GetRlocAddress();

          Ptr<Locator> srcRloc = Create<Locator>(static_cast<Address>(maddr));
          srcMapEntry = Create<MapEntryImpl>(srcRloc); // Like case where source is RLOC
          ecmEncap = LispOverIp::ECM_NO;

          goto ecm;
        }
        /* MapRequests arrive here (because both source and destination are RLOCs).
         * If LISP device is RTR:
         *    RTR must reoriginate SMRs coming from registered NATed devices
         *    RTR must data encapsulate MapRequests for NATed EID.
         */
        else if (msg_type == static_cast<uint8_t>(MapRequestMsg::GetMsgType()))
        {
          // TODO: differentiate SMR from MapRequest, or it's fine?

          if (lispOverIpv4->IsNated())
          {
            NS_LOG_DEBUG("This is an SMR and device is NATed -> Encapsulate");
            ecmEncap = LispOverIp::ECM_XTR;
            Ptr<Locator> srcRloc = Create<Locator>(static_cast<Address>(source));
            srcMapEntry = Create<MapEntryImpl>(srcRloc); // Like case where source is RLOC
            goto ecm;
          }
          else if (lispOverIpv4->IsRtr())
          {

            /* RTR must data encapsulate MapRequests for NATed EID */
            Ptr<MapEntry> m;
            if (lispOverIpv4->IsMapRequestForNatedXtr(packet, innerIpHeader, m))
            {
              NS_LOG_DEBUG("This is a MapRequest for a NATed EID -> Data encapsulate towards NATed LISP device");
              goto data_encapsulate;
            }

            /* Change inner header source with RTR RLOC (found in local database)
            //Assumption: there is a unique eid space and a unique locator in database, or several
            eid space but all under the same RLOC */
            NS_LOG_DEBUG("This is an SMR and device is RTR -> Reoriginates");
            // Attention, we reoriginate both SMRs sent by RTR, and by NATed device
            // Here it makes no difference, but should be aware of that still.
            Ptr<MapTables> mapTables = lispOverIpv4->GetMapTablesV4();
            std::list<Ptr<MapEntry>> entryList;
            mapTables->GetMapEntryList(MapTables::IN_DATABASE, entryList);
            Ptr<MapEntry> mapEntry = entryList.front();
            Address maddr = mapEntry->RlocSelection()->GetRlocAddress();

            source = Ipv4Address::ConvertFrom(maddr);

            /* Change ITR RLOC field in SMR */
            lispOverIpv4->ChangeItrRloc(packet, Ipv4Address::ConvertFrom(maddr));

            goto no_encap;
          }
          else // LISP device is neither NATed nor an RTR -> Usual behaviour
            goto no_encap;
        }
        else // Not a MapRegister, not a MapNotify, not an SMR -> Usual behaviour
          goto no_encap;
      }

      if (srcIsRloc || lispOverIpv4->GetPitr() || lispOverIpv4->IsRtr())
      {
        NS_LOG_DEBUG("Source is RLOC (or PITR, or RTR)");
        Ptr<Locator> srcRloc = Create<Locator>(
            static_cast<Address>(source));
        srcMapEntry = Create<MapEntryImpl>(srcRloc);
      }

      if (destIsRloc)
      {
        NS_LOG_DEBUG("Destination is RLOC");
        Ptr<Locator> destRloc = Create<Locator>(
            static_cast<Address>(destination));
        destMapEntry = Create<MapEntryImpl>(destRloc);
      }
    ecm:
      LispOverIpv4::MapStatus isMapForEncap =
          lispOverIpv4->IsMapForEncapsulation(innerIpHeader, srcMapEntry,
                                              destMapEntry,
                                              ifAddr.GetMask());
      if (isMapForEncap == LispOverIpv4::Mapping_Exist)
      {
        NS_LOG_DEBUG("Ready to Encapsulate");
        lispOverIpv4->LispOutput(packet, innerIpHeader, srcMapEntry,
                                 destMapEntry, lispRoute, ecmEncap);
        // For NATed device, destMapEntry is supposed to correspond to RTR RLOC
        NS_LOG_DEBUG("After LispOutput");
        return;
      }
      else if (isMapForEncap == LispOverIpv4::No_Need_Encap)
      {
        NS_LOG_DEBUG("No encapsulation needed");
        goto no_encap;
      }
      else if (isMapForEncap == LispOverIpv4::Not_Registered)
      {
        NS_LOG_DEBUG("LISP device is not yet registered to the MDS. Cannot send data packets.");
        return;
      }
      else
      {
        // ns3-privacy addition
        lispOverIpv4->BufferPacket(packet, source, destination, protocol, route);
        NS_LOG_DEBUG("Cache miss");
        return; // if no mapping, cache miss
      }
    }

  no_encap:
    /**
     * TODO Here we could make the encapsulation if needed when the packet comes from the upper layer
     */
    //==========================================End of Adaptation=================================
    // Handle a few cases:
    // 1) packet is destined to limited broadcast address
    // 2) packet is destined to a subnet-directed broadcast address
    // 3) packet is not broadcast, and is passed in with a route entry
    // 4) packet is not broadcast, and is passed in with a route entry but route->GetGateway is not set (e.g., on-demand)
    // 5) packet is not broadcast, and route is NULL (e.g., a raw socket call, or ICMP)

    // 1) packet is destined to limited broadcast address or link-local multicast address
    if (destination.IsBroadcast() || destination.IsLocalMulticast())
    {
      NS_LOG_LOGIC("Ipv4L3Protocol::Send case 1:  limited broadcast");
      ipHeader = BuildHeader(source, destination, protocol, packet->GetSize(), ttl, tos, mayFragment);
      uint32_t ifaceIndex = 0;
      for (Ipv4InterfaceList::iterator ifaceIter = m_interfaces.begin();
           ifaceIter != m_interfaces.end(); ifaceIter++, ifaceIndex++)
      {
        Ptr<Ipv4Interface> outInterface = *ifaceIter;
        bool sendIt = false;
        if (source == Ipv4Address::GetAny())
        {
          sendIt = true;
        }
        for (uint32_t index = 0; index < outInterface->GetNAddresses(); index++)
        {
          if (outInterface->GetAddress(index).GetLocal() == source)
          {
            sendIt = true;
          }
        }
        if (sendIt)
        {
          Ptr<Packet> packetCopy = packet->Copy();

          NS_ASSERT(packetCopy->GetSize() <= outInterface->GetDevice()->GetMtu());

          m_sendOutgoingTrace(ipHeader, packetCopy, ifaceIndex);

          // ============= Adptation for NetFilter =================
          if (m_netfilter != 0)
          {
            packetCopy->AddHeader(ipHeader);
            NS_LOG_DEBUG("NF_INET_LOCAL_OUT Hook on node " << m_node->GetId());
            Verdicts_t verdict = (Verdicts_t)m_netfilter->ProcessHook(PF_INET, NF_INET_LOCAL_OUT, packetCopy, 0, device);
            if (verdict == NF_DROP)
            {
              NS_LOG_DEBUG("NF_INET_LOCAL_OUT packet not accepted");
              m_dropTrace(ipHeader, packetCopy, DROP_NETFILTER, m_node->GetObject<Ipv4>(), ifaceIndex);
              return;
            }
            packetCopy->RemoveHeader(ipHeader);
          }
          // Do not call SendRealOut () (which requires passing in a route)
          // instead, just send the packet on the interface
          if (m_netfilter != 0)
          {
            packetCopy->AddHeader(ipHeader);
            NS_LOG_DEBUG("NF_INET_POST_ROUTING Hook on node " << m_node->GetId());
            Callback<uint32_t, Ptr<Packet>> ccb = MakeCallback(&Ipv4Netfilter::NetfilterConntrackConfirm, m_netfilter);
            Verdicts_t verdict = (Verdicts_t)m_netfilter->ProcessHook(PF_INET, NF_INET_POST_ROUTING, packetCopy, 0, device, ccb);
            if (verdict == NF_DROP)
            {

              NS_LOG_DEBUG("NF_INET_POST_ROUTING packet not accepted");
              m_dropTrace(ipHeader, packetCopy, DROP_NETFILTER, m_node->GetObject<Ipv4>(), ifaceIndex);
              return;
            }
            packetCopy->RemoveHeader(ipHeader);
          }

          // ============== End Adaptation for NetFilter =============
          CallTxTrace(ipHeader, packetCopy, m_node->GetObject<Ipv4>(), ifaceIndex);
          outInterface->Send(packetCopy, ipHeader, destination);
        }
      }
      return;
    }

    // 2) check: packet is destined to a subnet-directed broadcast address
    uint32_t ifaceIndex = 0;
    for (Ipv4InterfaceList::iterator ifaceIter = m_interfaces.begin();
         ifaceIter != m_interfaces.end(); ifaceIter++, ifaceIndex++)
    {
      Ptr<Ipv4Interface> outInterface = *ifaceIter;
      for (uint32_t j = 0; j < GetNAddresses(ifaceIndex); j++)
      {
        Ipv4InterfaceAddress ifAddr = GetAddress(ifaceIndex, j);
        NS_LOG_LOGIC("Testing address " << ifAddr.GetLocal() << " with mask " << ifAddr.GetMask());
        if (destination.IsSubnetDirectedBroadcast(ifAddr.GetMask()) &&
            destination.CombineMask(ifAddr.GetMask()) == ifAddr.GetLocal().CombineMask(ifAddr.GetMask()))
        {
          NS_LOG_LOGIC("Ipv4L3Protocol::Send case 2:  subnet directed bcast to " << ifAddr.GetLocal());
          ipHeader = BuildHeader(source, destination, protocol, packet->GetSize(), ttl, tos, mayFragment);
          Ptr<Packet> packetCopy = packet->Copy();
          m_sendOutgoingTrace(ipHeader, packetCopy, ifaceIndex);

          // =========== Adaptation for NetFilter =================
          if (m_netfilter != 0)
          {
            NS_LOG_DEBUG("NF_INET_LOCAL_OUT Hook on node " << m_node->GetId());
            packetCopy->AddHeader(ipHeader);
            Verdicts_t verdict = (Verdicts_t)m_netfilter->ProcessHook(PF_INET, NF_INET_LOCAL_OUT, packetCopy, 0, device);
            if (verdict == NF_DROP)
            {
               
              NS_LOG_DEBUG("NF_INET_LOCAL_OUT packet not accepted");
              m_dropTrace(ipHeader, packetCopy, DROP_NETFILTER, m_node->GetObject<Ipv4>(), ifaceIndex);
              return;
            }
            packetCopy->RemoveHeader(ipHeader);
          }
          // Do not call SendRealOut () (which requires passing in a route)
          // instead, just send the packet on the interface
          if (m_netfilter != 0)
          {
            NS_LOG_DEBUG("NF_INET_POST_ROUTING Hook on node " << m_node->GetId());
            packetCopy->AddHeader(ipHeader);
            Callback<uint32_t, Ptr<Packet>> ccb = MakeCallback(&Ipv4Netfilter::NetfilterConntrackConfirm, m_netfilter);
            Verdicts_t verdict = (Verdicts_t)m_netfilter->ProcessHook(PF_INET, NF_INET_POST_ROUTING, packetCopy, 0, device, ccb);
            
            if (verdict == NF_DROP)
            {
               
              NS_LOG_DEBUG("NF_INET_POST_ROUTING packet not accepted");
              m_dropTrace(ipHeader, packetCopy, DROP_NETFILTER, m_node->GetObject<Ipv4>(), ifaceIndex);
              return;
            }
            packetCopy->RemoveHeader(ipHeader);
          }
          // =====================End adaptation for Netfilter ========================

          CallTxTrace(ipHeader, packetCopy, m_node->GetObject<Ipv4>(), ifaceIndex);
          outInterface->Send(packetCopy, ipHeader, destination);
          return;
        }
      }
    }

    // 3) packet is not broadcast, and is passed in with a route entry
    //    with a valid Ipv4Address as the gateway
    if (route && route->GetGateway() != Ipv4Address())
    {
      NS_LOG_LOGIC("Ipv4L3Protocol::Send case 3:  passed in with route");
      ipHeader = BuildHeader(source, destination, protocol, packet->GetSize(), ttl, tos, mayFragment);
      int32_t interface = GetInterfaceForDevice(route->GetOutputDevice());

      // ========== Adaptation for NetFilter ===============
      Ptr<Packet> packetCopy = packet->Copy();
      if (m_netfilter != 0)
      {
        NS_LOG_DEBUG("NF_INET_LOCAL_OUT Hook on node " << m_node->GetId());
        // the LOCAL_OUT hook expects an IP header on the packet, but
        // SendRealOut () (below) is where it is added.  So add one here.
        packetCopy->AddHeader(ipHeader);
        Verdicts_t verdict = (Verdicts_t)m_netfilter->ProcessHook(PF_INET, NF_INET_LOCAL_OUT, packetCopy, 0, device);
        if (verdict == NF_DROP)
        {
           
          NS_LOG_DEBUG("NF_INET_LOCAL_OUT packet not accepted");
          m_dropTrace(ipHeader, packetCopy, DROP_NETFILTER, m_node->GetObject<Ipv4>(), interface);
          return;
        }
        packetCopy->RemoveHeader(ipHeader);
      }
      // ========= End of adpatation for NetFilter ==========

      m_sendOutgoingTrace(ipHeader, packetCopy, interface);
      SendRealOut(route, packetCopy, ipHeader);
      return;
    }
    // 4) packet is not broadcast, and is passed in with a route entry but route->GetGateway is not set (e.g., on-demand)
    if (route && route->GetGateway() == Ipv4Address())
    {
      // This could arise because the synchronous RouteOutput() call
      // returned to the transport protocol with a source address but
      // there was no next hop available yet (since a route may need
      // to be queried).
      NS_FATAL_ERROR("Ipv4L3Protocol::Send case 4: This case not yet implemented");
    }
    // 5) packet is not broadcast, and route is NULL (e.g., a raw socket call)
    NS_LOG_LOGIC("Ipv4L3Protocol::Send case 5:  passed in with no route " << destination);
    Socket::SocketErrno errno_;
    Ptr<NetDevice> oif(0); // unused for now
    ipHeader = BuildHeader(source, destination, protocol, packet->GetSize(), ttl, tos, mayFragment);
    Ptr<Ipv4Route> newRoute;

    // ============= Adaptation for NetFilter ==================
    Ptr<Packet> packetCopy = packet->Copy();
    if (m_netfilter != 0)
    {
      NS_LOG_DEBUG("NF_INET_LOCAL_OUT Hook on node " << m_node->GetId());
      // the LOCAL_OUT hook expects an IP header on the packet, but
      // SendRealOut () (below) is where it is added.  So add one here.

      packetCopy->AddHeader(ipHeader);
      Verdicts_t verdict = (Verdicts_t)m_netfilter->ProcessHook(PF_INET, NF_INET_LOCAL_OUT, packetCopy, 0, device);
      if (verdict == NF_DROP)
      {
         
        NS_LOG_DEBUG("NF_INET_LOCAL_OUT packet not accepted");
        m_dropTrace(ipHeader, packet, DROP_NETFILTER, m_node->GetObject<Ipv4>(), 0);
        return;
      }
      packetCopy->RemoveHeader(ipHeader);
    }
    // ============= End of adaptation for NetFilter ==============

    if (m_routingProtocol != 0)
    {
      newRoute = m_routingProtocol->RouteOutput(packetCopy, ipHeader, oif, errno_);
    }
    else
    {
      NS_LOG_ERROR("Ipv4L3Protocol::Send: m_routingProtocol == 0");
    }
    if (newRoute)
    {
      int32_t interface = GetInterfaceForDevice(newRoute->GetOutputDevice());
      m_sendOutgoingTrace(ipHeader, packetCopy, interface);
      SendRealOut(newRoute, packetCopy, ipHeader);
    }
    else
    {
      NS_LOG_WARN("No route to host.  Drop.");
      m_dropTrace(ipHeader, packetCopy, DROP_NO_ROUTE, m_node->GetObject<Ipv4>(), 0);
    }
  }

  // \todo when should we set ip_id?   check whether we are incrementing
  // m_identification on packets that may later be dropped in this stack
  // and whether that deviates from Linux
  Ipv4Header
  Ipv4L3Protocol::BuildHeader(
      Ipv4Address source,
      Ipv4Address destination,
      uint8_t protocol,
      uint16_t payloadSize,
      uint8_t ttl,
      uint8_t tos,
      bool mayFragment)
  {
    NS_LOG_FUNCTION(this << source << destination << (uint16_t)protocol << payloadSize << (uint16_t)ttl << (uint16_t)tos << mayFragment);
    Ipv4Header ipHeader;
    ipHeader.SetSource(source);
    ipHeader.SetDestination(destination);
    ipHeader.SetProtocol(protocol);
    ipHeader.SetPayloadSize(payloadSize);
    ipHeader.SetTtl(ttl);
    ipHeader.SetTos(tos);

    uint64_t src = source.Get();
    uint64_t dst = destination.Get();
    uint64_t srcDst = dst | (src << 32);
    std::pair<uint64_t, uint8_t> key = std::make_pair(srcDst, protocol);

    if (mayFragment == true)
    {
      ipHeader.SetMayFragment();
      ipHeader.SetIdentification(m_identification[key]);
      m_identification[key]++;
    }
    else
    {
      ipHeader.SetDontFragment();
      // RFC 6864 does not state anything about atomic datagrams
      // identification requirement:
      // >> Originating sources MAY set the IPv4 ID field of atomic datagrams
      //    to any value.
      ipHeader.SetIdentification(m_identification[key]);
      m_identification[key]++;
    }
    if (Node::ChecksumEnabled())
    {
      ipHeader.EnableChecksum();
    }
    return ipHeader;
  }

  void
  Ipv4L3Protocol::SendRealOut(Ptr<Ipv4Route> route,
                              Ptr<Packet> packet,
                              Ipv4Header &ipHeader)
  {
    NS_LOG_FUNCTION(this << route << packet << &ipHeader);

    if (route == 0)
    {
      NS_LOG_WARN("No route to host.  Drop.");
      m_dropTrace(ipHeader, packet, DROP_NO_ROUTE, m_node->GetObject<Ipv4>(), 0);
      return;
    }
    Ptr<NetDevice> outDev = route->GetOutputDevice();
    int32_t interface = GetInterfaceForDevice(outDev);
    NS_ASSERT(interface >= 0);
    Ptr<Ipv4Interface> outInterface = GetInterface(interface);
    NS_LOG_LOGIC("Send via NetDevice ifIndex " << outDev->GetIfIndex() << " ipv4InterfaceIndex " << interface);

    // ========== Adaptation for NetFilter ===============
    Ptr<Packet> packetCopy = packet->Copy();
    if (m_netfilter != 0)
    {
      NS_LOG_DEBUG("NF_INET_POST_ROUTING Hook on node " << m_node->GetId());

      packetCopy->AddHeader(ipHeader);
      Ptr<NetDevice> device = route->GetOutputDevice();
      ContinueCallback ccb = MakeCallback(&Ipv4Netfilter::NetfilterConntrackConfirm, m_netfilter);
      Verdicts_t verdict = (Verdicts_t)m_netfilter->ProcessHook(PF_INET, NF_INET_POST_ROUTING, packetCopy, 0, device, ccb);
      if (verdict == NF_DROP)
      {
         
        NS_LOG_DEBUG("NF_INET_POST_ROUTING packet not accepted");
        m_dropTrace(ipHeader, packetCopy, DROP_NETFILTER, m_node->GetObject<Ipv4>(), interface);
        return;
      }
      packetCopy->RemoveHeader(ipHeader);
    }

    // ======== End of adaptation for NetFilter ============

    if (!route->GetGateway().IsEqual(Ipv4Address("0.0.0.0")))
    {
      if (outInterface->IsUp())
      {
        NS_LOG_LOGIC("Send to gateway " << route->GetGateway());
        if (packetCopy->GetSize() + ipHeader.GetSerializedSize() > outInterface->GetDevice()->GetMtu())
        {
          std::list<Ipv4PayloadHeaderPair> listFragments;
          DoFragmentation(packetCopy, ipHeader, outInterface->GetDevice()->GetMtu(), listFragments);
          for (std::list<Ipv4PayloadHeaderPair>::iterator it = listFragments.begin(); it != listFragments.end(); it++)
          {
            CallTxTrace(it->second, it->first, m_node->GetObject<Ipv4>(), interface);
            outInterface->Send(it->first, it->second, route->GetGateway());
          }
        }
        else
        {
          CallTxTrace(ipHeader, packetCopy, m_node->GetObject<Ipv4>(), interface);
          outInterface->Send(packetCopy, ipHeader, route->GetGateway());
        }
      }
      else
      {
        NS_LOG_LOGIC("Dropping -- outgoing interface is down: " << route->GetGateway());
        m_dropTrace(ipHeader, packetCopy, DROP_INTERFACE_DOWN, m_node->GetObject<Ipv4>(), interface);
      }
    }
    else
    {
      if (outInterface->IsUp())
      {
        NS_LOG_LOGIC("Send to destination " << ipHeader.GetDestination());
        if (packetCopy->GetSize() + ipHeader.GetSerializedSize() > outInterface->GetDevice()->GetMtu())
        {
          std::list<Ipv4PayloadHeaderPair> listFragments;
          DoFragmentation(packetCopy, ipHeader, outInterface->GetDevice()->GetMtu(), listFragments);
          for (std::list<Ipv4PayloadHeaderPair>::iterator it = listFragments.begin(); it != listFragments.end(); it++)
          {
            NS_LOG_LOGIC("Sending fragment " << *(it->first));
            CallTxTrace(it->second, it->first, m_node->GetObject<Ipv4>(), interface);
            outInterface->Send(it->first, it->second, ipHeader.GetDestination());
          }
        }
        else
        {
          CallTxTrace(ipHeader, packetCopy, m_node->GetObject<Ipv4>(), interface);
          outInterface->Send(packetCopy, ipHeader, ipHeader.GetDestination());
        }
      }
      else
      {
        NS_LOG_LOGIC("Dropping -- outgoing interface is down: " << ipHeader.GetDestination());
        m_dropTrace(ipHeader, packetCopy, DROP_INTERFACE_DOWN, m_node->GetObject<Ipv4>(), interface);
      }
    }
  }

  // This function analogous to Linux ip_mr_forward()
  void
  Ipv4L3Protocol::IpMulticastForward(Ptr<Ipv4MulticastRoute> mrtentry, Ptr<const Packet> p, const Ipv4Header &header)
  {
    NS_LOG_FUNCTION(this << mrtentry << p << header);
    NS_LOG_LOGIC("Multicast forwarding logic for node: " << m_node->GetId());

    std::map<uint32_t, uint32_t> ttlMap = mrtentry->GetOutputTtlMap();
    std::map<uint32_t, uint32_t>::iterator mapIter;

    for (mapIter = ttlMap.begin(); mapIter != ttlMap.end(); mapIter++)
    {
      uint32_t interfaceId = mapIter->first;
      // uint32_t outputTtl = mapIter->second;  // Unused for now

      Ptr<Packet> packet = p->Copy();
      Ipv4Header h = header;
      h.SetTtl(header.GetTtl() - 1);
      if (h.GetTtl() == 0)
      {
        NS_LOG_WARN("TTL exceeded.  Drop.");
        m_dropTrace(header, packet, DROP_TTL_EXPIRED, m_node->GetObject<Ipv4>(), interfaceId);
        return;
      }
      NS_LOG_LOGIC("Forward multicast via interface " << interfaceId);
      Ptr<Ipv4Route> rtentry = Create<Ipv4Route>();
      rtentry->SetSource(h.GetSource());
      rtentry->SetDestination(h.GetDestination());
      rtentry->SetGateway(Ipv4Address::GetAny());
      rtentry->SetOutputDevice(GetNetDevice(interfaceId));
      SendRealOut(rtentry, packet, h);
      continue;
    }
  }

  // This function analogous to Linux ip_forward()
  void
  Ipv4L3Protocol::IpForward(Ptr<Ipv4Route> rtentry, Ptr<const Packet> p, const Ipv4Header &header)
  {
    NS_LOG_FUNCTION(this << rtentry << p << header);
    NS_LOG_LOGIC("Forwarding logic for node: " << m_node->GetId());
    // Forwarding
    Ipv4Header ipHeader = header;
    Ptr<Packet> packet = p->Copy();
    Ptr<NetDevice> device = rtentry->GetOutputDevice(); // NetFilter adaptation
    int32_t interface = GetInterfaceForDevice(rtentry->GetOutputDevice());
    ipHeader.SetTtl(ipHeader.GetTtl() - 1);
    if (ipHeader.GetTtl() == 0)
    {
      // Do not reply to ICMP or to multicast/broadcast IP address
      if (ipHeader.GetProtocol() != Icmpv4L4Protocol::PROT_NUMBER &&
          ipHeader.GetDestination().IsBroadcast() == false &&
          ipHeader.GetDestination().IsMulticast() == false)
      {
        Ptr<Icmpv4L4Protocol> icmp = GetIcmp();
        icmp->SendTimeExceededTtl(ipHeader, packet);
      }
      NS_LOG_WARN("TTL exceeded.  Drop.");
      m_dropTrace(header, packet, DROP_TTL_EXPIRED, m_node->GetObject<Ipv4>(), interface);
      return;
    }

    /**
     * Yue:
     * 2017-04-28: Add check for map tables. Otherwise program is possibe to crash
     * due to segmentation fault.
     * 2018-01-31: Copy this from ns-3.26 to ns-3.27
     */
    Ptr<LispOverIpv4> lisp = m_node->GetObject<LispOverIpv4>();
    int nbEntriesDB = 0;
    if (lisp)
    {
      NS_LOG_DEBUG("first check to enter in lisp code block passed!");
      nbEntriesDB = lisp->GetMapTablesV4()->GetNMapEntriesLispDataBase();
      /**
       * Yue's comment: I observe that for the received DHCP offer message from
       * DHCP server. NeedEncapsulation check is true! This leads to the simulation
       * program crash, since without DHCP exchange, lisp database is still empty!!!
       * So, to support LISP-DHCP, we should modify in this file or modify the creation
       * of lisp Database?
       */
      if (nbEntriesDB || lisp->GetPitr())
      {
        NS_LOG_DEBUG("Second check to enter in lisp code block passed!");
        Ipv4InterfaceAddress ifAddr = GetAddress(interface, 0);
        if (lisp->NeedEncapsulation(header, ifAddr.GetMask()))
        {
          NS_LOG_DEBUG("OK Ready to encapsulation in FORWARD");
          Send(packet, header.GetSource(), header.GetDestination(),
               header.GetProtocol(), 0);
          return;
        }
      }
    }
    // in case the packet still has a priority tag attached, remove it
    SocketPriorityTag priorityTag;
    packet->RemovePacketTag(priorityTag);
    uint8_t priority = Socket::IpTos2Priority(ipHeader.GetTos());
    // add a priority tag if the priority is not null
    if (priority)
    {
      priorityTag.SetPriority(priority);
      packet->AddPacketTag(priorityTag);
    }

    m_unicastForwardTrace(ipHeader, packet, interface);

    // ============ Adaptation for NetFilter =============
    if (m_netfilter != 0)
    {
      NS_LOG_DEBUG("NF_INET_FORWARD Hook on node " << m_node->GetId());
      Verdicts_t verdict = (Verdicts_t)m_netfilter->ProcessHook(PF_INET, NF_INET_FORWARD, packet, 0, device);
      if (verdict == NF_DROP)
      {
         
        NS_LOG_DEBUG("NF_INET_FORWARD packet not accepted");
        m_dropTrace(header, packet, DROP_NETFILTER, m_node->GetObject<Ipv4>(), interface);
        return;
      }
    }
    // ======== End of adaptation for NetFilter ==============

    SendRealOut(rtentry, packet, ipHeader);
  }

  void
  Ipv4L3Protocol::LocalDeliver(Ptr<const Packet> packet, Ipv4Header const &ip, uint32_t iif)
  {
    NS_LOG_FUNCTION(this << packet << &ip << iif);

    // ========== Adaptation for NetFilter ====================
    Ptr<Packet> pkt = packet->Copy();
    Ptr<NetDevice> device = GetNetDevice(iif);
    pkt->AddHeader(ip);
    if (m_netfilter != 0)
    {
      NS_LOG_DEBUG("NF_INET_LOCAL_IN Hook on node " << m_node->GetId());
      Callback<uint32_t, Ptr<Packet>> ccb = MakeCallback(&Ipv4Netfilter::NetfilterConntrackConfirm, m_netfilter);
      Verdicts_t verdict = (Verdicts_t)m_netfilter->ProcessHook(PF_INET, NF_INET_LOCAL_IN, pkt, 0, device, ccb);
      if (verdict == NF_DROP)
      {
         
        NS_LOG_DEBUG("NF_INET_LOCAL_IN packet not accepted");
        m_dropTrace(ip, packet, DROP_NETFILTER, m_node->GetObject<Ipv4>(), iif);
        return;
      }
    }
    // ========== End of adaptation for NetFilter ================

    Ptr<Packet> p = packet->Copy(); // need to pass a non-const packet up

    Ipv4Header ipHeader = ip;
    // ================================Adaption to support LISP===========================
    NS_LOG_DEBUG("We are in local delivery on node " << m_node->GetId());
    /**
     * Yue: I think it is better to do a mapTable check here also
     */
    Ptr<LispOverIpv4> lisp = m_node->GetObject<LispOverIpv4>();
    /*int nbEntriesDB = 0;
    if (lisp)
      {
        nbEntriesDB = lisp->GetMapTablesV4 ()->GetNMapEntriesLispDataBase ();
      }*/
    if (lisp) //!= 0 and nbEntriesDB != 0) emeline: for PETR
    {
      NS_LOG_DEBUG(
          "Checking if we need decapsulation on node " << m_node->GetId());
      /**
       * At this point the outer header has been checked
       * now we can safely remove it if needed
       */
      lisp = m_node->GetObject<LispOverIpv4>();
      /*----------------------------------------------
        Packet addressed to xTR for
        decapsulation.
        Checks if UDP dest port is LISP_DATA_PORT
        ----------------------------------------------*/
      if (lisp->NeedDecapsulation(p, ip, LispOverIp::LISP_DATA_PORT))
      {
        // copy initial packet with outer and inner ip headers
        NS_LOG_DEBUG("We need decapsulation");
        Ptr<Packet> lispPacket = p->Copy();
        lisp->LispInput(lispPacket, ipHeader, true);
        return;
      }
      /*----------------------------------------------
        Control packet encapsulated in ECM header
        ----------------------------------------------*/
      else if (lisp->NeedDecapsulation(p, ip, LispOverIp::LISP_SIG_PORT))
      {

        // TODO: differentiate between MapRegister and MapNotify,and possibly other control msg
        NS_LOG_DEBUG("LISP devices receives an ECM encapsulated control message");

        /* Differenciation between MapRegister and SMR */
        if (lisp->IsMapRegister(p->Copy()))
        {
          NS_LOG_DEBUG("ECM encapsulated message is MapRegister");
          /* If device is RTR, set new entry in cache and in database to
           * record NAT information.
           * If not RTR, it is a MS, and inner packet needs to be delivered to
           * MS application
           */
          if (lisp->IsRtr())
            lisp->SetNatedEntry(p->Copy(), ipHeader);
        }

        lisp->LispInput(p->Copy(), ipHeader, false);
        return;
      }
      /*----------------------------------------------
        Control packet addressed to xTR (not encapsulated)
        ----------------------------------------------*/
      else
      {
        NS_LOG_DEBUG(
            "OK we did not need decapsulation on node " << m_node->GetId());
        /*
         * For NATed device registration process, MS is required to send back
         * an ECM encapsulated MapNotify to RTR.
         * However, IETF draft is not precise about the different fields in it (Moreover,
         * with current implementation, MS has no way of knowing that a certain MapNotify
         * must be ECM encapsulated).
         * Therefore, MS will send a classic MapNotify to RTR, and RTR will detect
         * HERE, and encapsulate it in a DataMapNotify to send back to NATed device.
         */
        Ptr<Packet> lispPacket = p->Copy();
        Ptr<MapEntry> mapEntry;
        if (lisp->IsRtr() && lisp->IsMapNotifyForNatedXtr(p, ip, mapEntry))
        {
          NS_LOG_DEBUG("This is a MapNotify msg for NATed device");
          // This is a non encapsulated MapNotify
          // Inject it back in IP stack so that it can be encapsulated in
          // a DataMapNotify and sent back to NATed xTR.
          ipHeader.SetDestination(Ipv4Address::ConvertFrom(mapEntry->GetXtrLloc()->GetRlocAddress()));
          p->AddHeader(ipHeader);

          Receive(lisp->m_currentDevice,
                  p,
                  lisp->m_ipProtocol,
                  static_cast<Address>(ipHeader.GetSource()),
                  static_cast<Address>(ipHeader.GetDestination()),
                  lisp->m_currentPacketType);
          return; // MapNotify is not delivered to RTR application
        }
        /*
         * For a NATed device to be aware of the remote (P)ITRs with which
         * it communicates, MapRequests for a NATed LISP-MN EID must be
         * encapsulated in a Data header and forwarded to the NATed device.
         */
        if (lisp->IsRtr() && lisp->IsMapRequestForNatedXtr(p, ip, mapEntry))
        {
          NS_LOG_DEBUG("This is a MapRequest msg for NATed device");

          ipHeader.SetDestination(Ipv4Address::ConvertFrom(mapEntry->GetXtrLloc()->GetRlocAddress()));
          lispPacket->AddHeader(ipHeader); // lispHeader is a copy of the the packet

          Receive(lisp->m_currentDevice,
                  lispPacket,
                  lisp->m_ipProtocol,
                  static_cast<Address>(ipHeader.GetSource()),
                  static_cast<Address>(ipHeader.GetDestination()),
                  lisp->m_currentPacketType);
          // MapRequest is delivered to RTR application, as RTR must answer with a MapReply
        }
      }
    }
    // NS_LOG_DEBUG("OK on node " << m_node->GetId ()<<"-> No decapsulation when local deliver");
    //  ================================Adaption to support LISP===========================

    if (!ipHeader.IsLastFragment() || ipHeader.GetFragmentOffset() != 0)
    {
      NS_LOG_LOGIC("Received a fragment, processing " << *p);
      bool isPacketComplete;
      isPacketComplete = ProcessFragment(p, ipHeader, iif);
      if (isPacketComplete == false)
      {
        return;
      }
      NS_LOG_LOGIC("Got last fragment, Packet is complete " << *p);
      ipHeader.SetFragmentOffset(0);
      ipHeader.SetPayloadSize(p->GetSize());
    }

    m_localDeliverTrace(ipHeader, p, iif);

    Ptr<IpL4Protocol> protocol = GetProtocol(ipHeader.GetProtocol(), iif);
    if (protocol != 0)
    {
      // we need to make a copy in the unlikely event we hit the
      // RX_ENDPOINT_UNREACH codepath
      Ptr<Packet> copy = p->Copy();
      enum IpL4Protocol::RxStatus status =
          protocol->Receive(p, ipHeader, GetInterface(iif));
      switch (status)
      {
      case IpL4Protocol::RX_OK:
      // fall through
      case IpL4Protocol::RX_ENDPOINT_CLOSED:
      // fall through
      case IpL4Protocol::RX_CSUM_FAILED:
        break;
      case IpL4Protocol::RX_ENDPOINT_UNREACH:
        if (ipHeader.GetDestination().IsBroadcast() == true ||
            ipHeader.GetDestination().IsMulticast() == true)
        {
          break; // Do not reply to broadcast or multicast
        }
        // Another case to suppress ICMP is a subnet-directed broadcast
        bool subnetDirected = false;
        for (uint32_t i = 0; i < GetNAddresses(iif); i++)
        {
          Ipv4InterfaceAddress addr = GetAddress(iif, i);
          if (addr.GetLocal().CombineMask(addr.GetMask()) == ipHeader.GetDestination().CombineMask(addr.GetMask()) &&
              ipHeader.GetDestination().IsSubnetDirectedBroadcast(addr.GetMask()))
          {
            subnetDirected = true;
          }
        }
        if (subnetDirected == false)
        {
          NS_LOG_DEBUG("Destination unreachable port");
          GetIcmp()->SendDestUnreachPort(ipHeader, copy);
        }
      }
    }
  }

  bool
  Ipv4L3Protocol::AddAddress(uint32_t i, Ipv4InterfaceAddress address)
  {
    NS_LOG_FUNCTION(this << i << address);
    Ptr<Ipv4Interface> interface = GetInterface(i);
    bool retVal = interface->AddAddress(address);
    if (m_routingProtocol != 0)
    {
      m_routingProtocol->NotifyAddAddress(i, address);
    }
    return retVal;
  }

  Ipv4InterfaceAddress
  Ipv4L3Protocol::GetAddress(uint32_t interfaceIndex, uint32_t addressIndex) const
  {
    NS_LOG_FUNCTION(this << interfaceIndex << addressIndex);
    Ptr<Ipv4Interface> interface = GetInterface(interfaceIndex);
    return interface->GetAddress(addressIndex);
  }

  uint32_t
  Ipv4L3Protocol::GetNAddresses(uint32_t interface) const
  {
    NS_LOG_FUNCTION(this << interface);
    Ptr<Ipv4Interface> iface = GetInterface(interface);
    return iface->GetNAddresses();
  }

  bool
  Ipv4L3Protocol::RemoveAddress(uint32_t i, uint32_t addressIndex)
  {
    NS_LOG_FUNCTION(this << i << addressIndex);
    Ptr<Ipv4Interface> interface = GetInterface(i);
    Ipv4InterfaceAddress address = interface->RemoveAddress(addressIndex);
    if (address != Ipv4InterfaceAddress())
    {
      if (m_routingProtocol != 0)
      {
        m_routingProtocol->NotifyRemoveAddress(i, address);
      }
      return true;
    }
    return false;
  }

  bool
  Ipv4L3Protocol::RemoveAddress(uint32_t i, Ipv4Address address)
  {
    NS_LOG_FUNCTION(this << i << address);
    if (address == Ipv4Address::GetLoopback())
    {
      NS_LOG_WARN("Cannot remove loopback address.");
      return false;
    }
    Ptr<Ipv4Interface> interface = GetInterface(i);
    Ipv4InterfaceAddress ifAddr = interface->RemoveAddress(address);
    if (ifAddr != Ipv4InterfaceAddress())
    {
      if (m_routingProtocol != 0)
      {
        m_routingProtocol->NotifyRemoveAddress(i, ifAddr);
      }
      return true;
    }
    return false;
  }

  Ipv4Address
  Ipv4L3Protocol::SourceAddressSelection(uint32_t interfaceIdx, Ipv4Address dest)
  {
    NS_LOG_FUNCTION(this << interfaceIdx << " " << dest);
    if (GetNAddresses(interfaceIdx) == 1) // common case
    {
      NS_LOG_DEBUG("Only one address on interface : " << interfaceIdx << " on node " << m_node->GetId() << ". The selected source address: " << GetAddress(interfaceIdx, 0).GetLocal());
      return GetAddress(interfaceIdx, 0).GetLocal();
    }
    // no way to determine the scope of the destination, so adopt the
    // following rule:  pick the first available address (index 0) unless
    // a subsequent address is on link (in which case, pick the primary
    // address if there are multiple)
    Ipv4Address candidate = GetAddress(interfaceIdx, 0).GetLocal();
    for (uint32_t i = 0; i < GetNAddresses(interfaceIdx); i++)
    {
      Ipv4InterfaceAddress test = GetAddress(interfaceIdx, i);
      if (test.GetLocal().CombineMask(test.GetMask()) == dest.CombineMask(test.GetMask()))
      {
        if (test.IsSecondary() == false)
        {
          NS_LOG_DEBUG("The selected source address: " << test.GetLocal());
          return test.GetLocal();
        }
      }
    }
    return candidate;
  }

  Ipv4Address
  Ipv4L3Protocol::SelectSourceAddress(Ptr<const NetDevice> device,
                                      Ipv4Address dst, Ipv4InterfaceAddress::InterfaceAddressScope_e scope)
  {
    NS_LOG_FUNCTION(this << device << dst << scope);
    Ipv4Address addr("0.0.0.0");
    Ipv4InterfaceAddress iaddr;
    bool found = false;

    if (device != 0)
    {
      int32_t i = GetInterfaceForDevice(device);
      NS_ASSERT_MSG(i >= 0, "No device found on node");
      for (uint32_t j = 0; j < GetNAddresses(i); j++)
      {
        iaddr = GetAddress(i, j);
        if (iaddr.IsSecondary())
          continue;
        if (iaddr.GetScope() > scope)
          continue;
        if (dst.CombineMask(iaddr.GetMask()) == iaddr.GetLocal().CombineMask(iaddr.GetMask()))
        {
          NS_LOG_DEBUG("The selected source address: " << iaddr.GetLocal());
          return iaddr.GetLocal();
        }
        if (!found)
        {
          addr = iaddr.GetLocal();
          found = true;
        }
      }
    }
    if (found)
    {
      NS_LOG_DEBUG("The found source address: " << iaddr.GetLocal());
      return addr;
    }

    // Iterate among all interfaces
    for (uint32_t i = 0; i < GetNInterfaces(); i++)
    {
      for (uint32_t j = 0; j < GetNAddresses(i); j++)
      {
        iaddr = GetAddress(i, j);
        if (iaddr.IsSecondary())
          continue;
        if (iaddr.GetScope() != Ipv4InterfaceAddress::LINK && iaddr.GetScope() <= scope)
        {
          NS_LOG_DEBUG("The found source address: " << iaddr.GetLocal());
          return iaddr.GetLocal();
        }
      }
    }
    NS_LOG_WARN("Could not find source address for " << dst << " and scope "
                                                     << scope << ", returning 0");
    return addr;
  }

  void
  Ipv4L3Protocol::SetMetric(uint32_t i, uint16_t metric)
  {
    NS_LOG_FUNCTION(this << i << metric);
    Ptr<Ipv4Interface> interface = GetInterface(i);
    interface->SetMetric(metric);
  }

  uint16_t
  Ipv4L3Protocol::GetMetric(uint32_t i) const
  {
    NS_LOG_FUNCTION(this << i);
    Ptr<Ipv4Interface> interface = GetInterface(i);
    return interface->GetMetric();
  }

  uint16_t
  Ipv4L3Protocol::GetMtu(uint32_t i) const
  {
    NS_LOG_FUNCTION(this << i);
    Ptr<Ipv4Interface> interface = GetInterface(i);
    return interface->GetDevice()->GetMtu();
  }

  bool
  Ipv4L3Protocol::IsUp(uint32_t i) const
  {
    NS_LOG_FUNCTION(this << i);
    Ptr<Ipv4Interface> interface = GetInterface(i);
    return interface->IsUp();
  }

  void
  Ipv4L3Protocol::SetUp(uint32_t i)
  {
    NS_LOG_FUNCTION(this << i);
    Ptr<Ipv4Interface> interface = GetInterface(i);

    // RFC 791, pg.25:
    //  Every internet module must be able to forward a datagram of 68
    //  octets without further fragmentation.  This is because an internet
    //  header may be up to 60 octets, and the minimum fragment is 8 octets.
    if (interface->GetDevice()->GetMtu() >= 68)
    {
      interface->SetUp();

      if (m_routingProtocol != 0)
      {
        m_routingProtocol->NotifyInterfaceUp(i);
      }
    }
    else
    {
      NS_LOG_LOGIC("Interface " << int(i) << " is set to be down for IPv4. Reason: not respecting minimum IPv4 MTU (68 octects)");
    }
  }

  void
  Ipv4L3Protocol::SetDown(uint32_t ifaceIndex)
  {
    NS_LOG_FUNCTION(this << ifaceIndex);
    Ptr<Ipv4Interface> interface = GetInterface(ifaceIndex);
    interface->SetDown();

    if (m_routingProtocol != 0)
    {
      m_routingProtocol->NotifyInterfaceDown(ifaceIndex);
    }
  }

  bool
  Ipv4L3Protocol::IsForwarding(uint32_t i) const
  {
    NS_LOG_FUNCTION(this << i);
    Ptr<Ipv4Interface> interface = GetInterface(i);
    NS_LOG_LOGIC("Forwarding state: " << interface->IsForwarding());
    return interface->IsForwarding();
  }

  void
  Ipv4L3Protocol::SetForwarding(uint32_t i, bool val)
  {
    NS_LOG_FUNCTION(this << i);
    Ptr<Ipv4Interface> interface = GetInterface(i);
    interface->SetForwarding(val);
  }

  Ptr<NetDevice>
  Ipv4L3Protocol::GetNetDevice(uint32_t i)
  {
    NS_LOG_FUNCTION(this << i);
    return GetInterface(i)->GetDevice();
  }

  void
  Ipv4L3Protocol::SetIpForward(bool forward)
  {
    NS_LOG_FUNCTION(this << forward);
    m_ipForward = forward;
    for (Ipv4InterfaceList::const_iterator i = m_interfaces.begin(); i != m_interfaces.end(); i++)
    {
      (*i)->SetForwarding(forward);
    }
  }

  bool
  Ipv4L3Protocol::GetIpForward(void) const
  {
    NS_LOG_FUNCTION(this);
    return m_ipForward;
  }

  void
  Ipv4L3Protocol::SetWeakEsModel(bool model)
  {
    NS_LOG_FUNCTION(this << model);
    m_weakEsModel = model;
  }

  bool
  Ipv4L3Protocol::GetWeakEsModel(void) const
  {
    NS_LOG_FUNCTION(this);
    return m_weakEsModel;
  }

  void
  Ipv4L3Protocol::RouteInputError(Ptr<const Packet> p, const Ipv4Header &ipHeader, Socket::SocketErrno sockErrno)
  {
    NS_LOG_FUNCTION(this << p << ipHeader << sockErrno);
    NS_LOG_LOGIC("Route input failure-- dropping packet to " << ipHeader << " with errno " << sockErrno);
    m_dropTrace(ipHeader, p, DROP_ROUTE_ERROR, m_node->GetObject<Ipv4>(), 0);

    // \todo Send an ICMP no route.
  }

  void
  Ipv4L3Protocol::DoFragmentation(Ptr<Packet> packet, const Ipv4Header &ipv4Header, uint32_t outIfaceMtu, std::list<Ipv4PayloadHeaderPair> &listFragments)
  {
    // BEWARE: here we do assume that the header options are not present.
    // a much more complex handling is necessary in case there are options.
    // If (when) IPv4 option headers will be implemented, the following code shall be changed.
    // Of course also the reassemby code shall be changed as well.

    NS_LOG_FUNCTION(this << *packet << outIfaceMtu << &listFragments);

    Ptr<Packet> p = packet->Copy();

    NS_ASSERT_MSG((ipv4Header.GetSerializedSize() == 5 * 4),
                  "IPv4 fragmentation implementation only works without option headers.");

    uint16_t offset = 0;
    bool moreFragment = true;
    uint16_t originalOffset = ipv4Header.GetFragmentOffset();
    bool isLastFragment = ipv4Header.IsLastFragment();
    uint32_t currentFragmentablePartSize = 0;

    // IPv4 fragments are all 8 bytes aligned but the last.
    // The IP payload size is:
    // floor( ( outIfaceMtu - ipv4Header.GetSerializedSize() ) /8 ) *8
    uint32_t fragmentSize = (outIfaceMtu - ipv4Header.GetSerializedSize()) & ~uint32_t(0x7);

    NS_LOG_LOGIC("Fragmenting - Target Size: " << fragmentSize);

    do
    {
      Ipv4Header fragmentHeader = ipv4Header;

      if (p->GetSize() > offset + fragmentSize)
      {
        moreFragment = true;
        currentFragmentablePartSize = fragmentSize;
        fragmentHeader.SetMoreFragments();
      }
      else
      {
        moreFragment = false;
        currentFragmentablePartSize = p->GetSize() - offset;
        if (!isLastFragment)
        {
          fragmentHeader.SetMoreFragments();
        }
        else
        {
          fragmentHeader.SetLastFragment();
        }
      }

      NS_LOG_LOGIC("Fragment creation - " << offset << ", " << currentFragmentablePartSize);
      Ptr<Packet> fragment = p->CreateFragment(offset, currentFragmentablePartSize);
      NS_LOG_LOGIC("Fragment created - " << offset << ", " << fragment->GetSize());

      fragmentHeader.SetFragmentOffset(offset + originalOffset);
      fragmentHeader.SetPayloadSize(currentFragmentablePartSize);

      if (Node::ChecksumEnabled())
      {
        fragmentHeader.EnableChecksum();
      }

      NS_LOG_LOGIC("Fragment check - " << fragmentHeader.GetFragmentOffset());

      NS_LOG_LOGIC("New fragment Header " << fragmentHeader);

      std::ostringstream oss;
      oss << fragmentHeader;
      fragment->Print(oss);

      NS_LOG_LOGIC("New fragment " << *fragment);

      listFragments.push_back(Ipv4PayloadHeaderPair(fragment, fragmentHeader));

      offset += currentFragmentablePartSize;

    } while (moreFragment);

    return;
  }

  bool
  Ipv4L3Protocol::ProcessFragment(Ptr<Packet> &packet, Ipv4Header &ipHeader, uint32_t iif)
  {
    NS_LOG_FUNCTION(this << packet << ipHeader << iif);

    uint64_t addressCombination = uint64_t(ipHeader.GetSource().Get()) << 32 | uint64_t(ipHeader.GetDestination().Get());
    uint32_t idProto = uint32_t(ipHeader.GetIdentification()) << 16 | uint32_t(ipHeader.GetProtocol());
    std::pair<uint64_t, uint32_t> key;
    bool ret = false;
    Ptr<Packet> p = packet->Copy();

    key.first = addressCombination;
    key.second = idProto;

    Ptr<Fragments> fragments;

    MapFragments_t::iterator it = m_fragments.find(key);
    if (it == m_fragments.end())
    {
      fragments = Create<Fragments>();
      m_fragments.insert(std::make_pair(key, fragments));
      m_fragmentsTimers[key] = Simulator::Schedule(m_fragmentExpirationTimeout,
                                                   &Ipv4L3Protocol::HandleFragmentsTimeout, this,
                                                   key, ipHeader, iif);
    }
    else
    {
      fragments = it->second;
    }

    NS_LOG_LOGIC("Adding fragment - Size: " << packet->GetSize() << " - Offset: " << (ipHeader.GetFragmentOffset()));

    fragments->AddFragment(p, ipHeader.GetFragmentOffset(), !ipHeader.IsLastFragment());

    if (fragments->IsEntire())
    {
      packet = fragments->GetPacket();
      fragments = 0;
      m_fragments.erase(key);
      if (m_fragmentsTimers[key].IsRunning())
      {
        NS_LOG_LOGIC("Stopping WaitFragmentsTimer at " << Simulator::Now().GetSeconds() << " due to complete packet");
        m_fragmentsTimers[key].Cancel();
      }
      m_fragmentsTimers.erase(key);
      ret = true;
    }

    return ret;
  }

  Ipv4L3Protocol::Fragments::Fragments()
      : m_moreFragment(0)
  {
    NS_LOG_FUNCTION(this);
  }

  Ipv4L3Protocol::Fragments::~Fragments()
  {
    NS_LOG_FUNCTION(this);
  }

  void
  Ipv4L3Protocol::Fragments::AddFragment(Ptr<Packet> fragment, uint16_t fragmentOffset, bool moreFragment)
  {
    NS_LOG_FUNCTION(this << fragment << fragmentOffset << moreFragment);

    std::list<std::pair<Ptr<Packet>, uint16_t>>::iterator it;

    for (it = m_fragments.begin(); it != m_fragments.end(); it++)
    {
      if (it->second > fragmentOffset)
      {
        break;
      }
    }

    if (it == m_fragments.end())
    {
      m_moreFragment = moreFragment;
    }

    m_fragments.insert(it, std::pair<Ptr<Packet>, uint16_t>(fragment, fragmentOffset));
  }

  bool
  Ipv4L3Protocol::Fragments::IsEntire() const
  {
    NS_LOG_FUNCTION(this);

    bool ret = !m_moreFragment && m_fragments.size() > 0;

    if (ret)
    {
      uint16_t lastEndOffset = 0;

      for (std::list<std::pair<Ptr<Packet>, uint16_t>>::const_iterator it = m_fragments.begin(); it != m_fragments.end(); it++)
      {
        // overlapping fragments do exist
        NS_LOG_LOGIC("Checking overlaps " << lastEndOffset << " - " << it->second);

        if (lastEndOffset < it->second)
        {
          ret = false;
          break;
        }
        // fragments might overlap in strange ways
        uint16_t fragmentEnd = it->first->GetSize() + it->second;
        lastEndOffset = std::max(lastEndOffset, fragmentEnd);
      }
    }

    return ret;
  }

  Ptr<Packet>
  Ipv4L3Protocol::Fragments::GetPacket() const
  {
    NS_LOG_FUNCTION(this);

    std::list<std::pair<Ptr<Packet>, uint16_t>>::const_iterator it = m_fragments.begin();

    Ptr<Packet> p = it->first->Copy();
    uint16_t lastEndOffset = p->GetSize();
    it++;

    for (; it != m_fragments.end(); it++)
    {
      if (lastEndOffset > it->second)
      {
        // The fragments are overlapping.
        // We do not overwrite the "old" with the "new" because we do not know when each arrived.
        // This is different from what Linux does.
        // It is not possible to emulate a fragmentation attack.
        uint32_t newStart = lastEndOffset - it->second;
        if (it->first->GetSize() > newStart)
        {
          uint32_t newSize = it->first->GetSize() - newStart;
          Ptr<Packet> tempFragment = it->first->CreateFragment(newStart, newSize);
          p->AddAtEnd(tempFragment);
        }
      }
      else
      {
        NS_LOG_LOGIC("Adding: " << *(it->first));
        p->AddAtEnd(it->first);
      }
      lastEndOffset = p->GetSize();
    }

    return p;
  }

  Ptr<Packet>
  Ipv4L3Protocol::Fragments::GetPartialPacket() const
  {
    NS_LOG_FUNCTION(this);

    std::list<std::pair<Ptr<Packet>, uint16_t>>::const_iterator it = m_fragments.begin();

    Ptr<Packet> p = Create<Packet>();
    uint16_t lastEndOffset = 0;

    if (m_fragments.begin()->second > 0)
    {
      return p;
    }

    for (it = m_fragments.begin(); it != m_fragments.end(); it++)
    {
      if (lastEndOffset > it->second)
      {
        uint32_t newStart = lastEndOffset - it->second;
        uint32_t newSize = it->first->GetSize() - newStart;
        Ptr<Packet> tempFragment = it->first->CreateFragment(newStart, newSize);
        p->AddAtEnd(tempFragment);
      }
      else if (lastEndOffset == it->second)
      {
        NS_LOG_LOGIC("Adding: " << *(it->first));
        p->AddAtEnd(it->first);
      }
      lastEndOffset = p->GetSize();
    }

    return p;
  }

  void
  Ipv4L3Protocol::HandleFragmentsTimeout(std::pair<uint64_t, uint32_t> key, Ipv4Header &ipHeader, uint32_t iif)
  {
    NS_LOG_FUNCTION(this << &key << &ipHeader << iif);

    MapFragments_t::iterator it = m_fragments.find(key);
    Ptr<Packet> packet = it->second->GetPartialPacket();

    // if we have at least 8 bytes, we can send an ICMP.
    if (packet->GetSize() > 8)
    {
      Ptr<Icmpv4L4Protocol> icmp = GetIcmp();
      icmp->SendTimeExceededTtl(ipHeader, packet);
    }
    m_dropTrace(ipHeader, packet, DROP_FRAGMENT_TIMEOUT, m_node->GetObject<Ipv4>(), iif);

    // clear the buffers
    it->second = 0;

    m_fragments.erase(key);
    m_fragmentsTimers.erase(key);
  }
} // namespace ns3
