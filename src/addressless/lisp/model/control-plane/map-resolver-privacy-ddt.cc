/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2016 University of Liege
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
 * Author: Lionel Agbodjan <lionel.agbodjan@gmail.com>
 */
#include "map-resolver-privacy-ddt.h"
#include "algorithm"
#include "ns3/log.h"
#include "ns3/nstime.h"
#include "ns3/socket.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/map-request-msg.h"

namespace ns3
{
  NS_LOG_COMPONENT_DEFINE("MapResolverPrivacyDdt");

  NS_OBJECT_ENSURE_REGISTERED(MapResolverPrivacyDdt);

  MapResolverPrivacyDdt::MapResolverPrivacyDdt()
  {
    NS_LOG_DEBUG("MapResolverPrivacyDdt Application created");
  }

  MapResolverPrivacyDdt::~MapResolverPrivacyDdt()
  {
    m_socket = 0;
    m_mrClientSocket = 0;
    m_requestWaiting = 0;
  }

  TypeId MapResolverPrivacyDdt::GetTypeId(void)
  {
    static TypeId tid = TypeId("ns3::MapResolverPrivacyDdt")
                            .SetParent<MapResolver>()
                            .SetGroupName("Lisp")
                            .AddConstructor<MapResolverPrivacyDdt>();
    return tid;
  }

  void MapResolverPrivacyDdt::StartApplication(void)
  {
    NS_LOG_FUNCTION(this);

    NS_LOG_DEBUG("STARTING MR");
    if (m_socket == 0)
    {
      TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
      m_socket = Socket::CreateSocket(GetNode(), tid);
    }
    m_socket->SetRecvCallback(MakeCallback(&MapResolverPrivacyDdt::HandleRead, this));
    if (m_mrClientSocket == 0)
    {
      TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
      m_mrClientSocket = Socket::CreateSocket(GetNode(), tid);
      InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), LispOverIp::LISP_SIG_PORT);
      if (m_mrClientSocket->Bind(local) == -1)
      {
        NS_LOG_DEBUG("Failed to bind socket MR");
        // NS_FATAL_ERROR ("Failed to bind socket MR");
      }
    }

    if (m_mrClientSocket6 == 0)
    {
      TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
      m_mrClientSocket6 = Socket::CreateSocket(GetNode(), tid);
      Inet6SocketAddress local6 = Inet6SocketAddress(Ipv6Address::GetAny(), LispOverIp::LISP_SIG_PORT);
      m_mrClientSocket6->Bind(local6);
    }
    m_mrClientSocket6->SetRecvCallback(MakeCallback(&MapResolverPrivacyDdt::HandleReadFromClient, this));
    m_mrClientSocket->SetRecvCallback(MakeCallback(&MapResolverPrivacyDdt::HandleReadFromClient, this));

    NS_LOG_DEBUG("MR APPLICATION STARTED");
  }

  void MapResolverPrivacyDdt::HandleReadFromClient(Ptr<Socket> socket)
  {
    Ptr<Packet> packet;
    Address from;

    while ((packet = socket->RecvFrom(from)))
    {
      if (InetSocketAddress::IsMatchingType(from))
      {
        NS_LOG_INFO("At time " << Simulator::Now().GetSeconds() << "s MR received " << packet->GetSize() << " bytes from " << InetSocketAddress::ConvertFrom(from).GetIpv4() << " port " << InetSocketAddress::ConvertFrom(from).GetPort());
      }
      else if (Inet6SocketAddress::IsMatchingType(from))
      {
      }
      uint8_t buf[packet->GetSize()];
      packet->CopyData(buf, packet->GetSize());
      uint8_t msg_type = buf[0] >> 4;
      // m_mapServerAddress
      //
      if (msg_type == static_cast<uint8_t>(MapRequestMsg::GetMsgType()))
      {
        NS_LOG_DEBUG("------- GET map request on map resolver and Forward it to Map Server!");
        Ptr<MapRequestMsg> mapReqMsg = MapRequestMsg::Deserialize(buf);
        Ptr<MapRequestRecord> record = mapReqMsg->GetMapRequestRecord();
        uint32_t prefix = Ipv4Address::ConvertFrom(record->GetEidPrefix()).CombineMask("/24").Get();
        uint32_t source = Ipv4Address::ConvertFrom(mapReqMsg->GetSourceEidAddr()).Get(); 
        auto it = std::find(m_alreadyAsked[source].begin(), m_alreadyAsked[source].end(), prefix) ;
        
        if(it != m_alreadyAsked[source].end())
          return;

        m_alreadyAsked[source].push_back(prefix);
        
        if (m_cachedMapping.find(prefix) == m_cachedMapping.end())
        {
          m_cachedMapping[prefix] = false;
          m_timeMapping[prefix] = Simulator::Now();
          Simulator::Schedule(Seconds(m_searchTimeVariable->GetValue()), &MapResolverPrivacyDdt::SendMapRequest, this, mapReqMsg);
        }
        else if (m_cachedMapping[prefix] == false)
        {
          Time t = Seconds(m_searchTimeVariable->GetValue() - (Simulator::Now() - m_timeMapping[prefix]).GetSeconds()) + Seconds(0.000000001) * m_requestWaiting;
          Simulator::Schedule(t, &MapResolverPrivacyDdt::SendMapRequest, this, mapReqMsg);
        }
        else
        {
          Simulator::Schedule(Seconds(0), &MapResolverPrivacyDdt::SendMapRequest, this, mapReqMsg);
        }
        m_requestWaiting += 1;
      }
    }
  }

  void MapResolverPrivacyDdt::StopApplication(void)
  {
    NS_LOG_FUNCTION(this);

    if (m_socket != 0)
    {
      m_socket->Close();
      m_socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
      m_socket = 0;
    }

    if (m_mrClientSocket)
    {
      m_mrClientSocket->Close();
      m_mrClientSocket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
      m_mrClientSocket = 0;
    }

    if (m_mrClientSocket6)
    {
      m_mrClientSocket6->Close();
      m_mrClientSocket6->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
      m_mrClientSocket6 = 0;
    }

    Simulator::Cancel(m_event);
  }

  void MapResolverPrivacyDdt::SendMapRequest(Ptr<MapRequestMsg> mapRequestMsg)
  {
    Ptr<MapRequestRecord> record = mapRequestMsg->GetMapRequestRecord();
    uint32_t prefix = Ipv4Address::ConvertFrom(record->GetEidPrefix()).CombineMask("/24").Get();
    m_cachedMapping[prefix] = true;

    NS_LOG_FUNCTION(this);
    NS_ASSERT(m_event.IsExpired());

    uint8_t buf[64];
    mapRequestMsg->Serialize(buf);
    ConnectToPeerAddress(m_mapServerAddress, m_peerPort, m_socket);
    Ptr<Packet> p = Create<Packet>(buf, 64);
    m_socket->Send(p);
  }

  void MapResolverPrivacyDdt::HandleRead(Ptr<Socket> socket)
  {
    NS_LOG_DEBUG("--------------HandleRead MR");
    NS_LOG_FUNCTION(this);
  }

  void MapResolverPrivacyDdt::SetMapServerAddress(Address mapServer)
  {
    m_mapServerAddress = mapServer;
  }

} /* namespace ns3 */
