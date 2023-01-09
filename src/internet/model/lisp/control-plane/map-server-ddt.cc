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
#include "map-server-ddt.h"
#include "ns3/simple-map-tables.h"
#include "ns3/map-tables.h"
#include "ns3/log.h"
#include "ns3/nstime.h"
#include "ns3/socket.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include "ns3/trace-source-accessor.h"
#include "map-register-msg.h"
#include "map-resolver.h"

namespace ns3
{
	NS_LOG_COMPONENT_DEFINE("MapServerDdt");

	NS_OBJECT_ENSURE_REGISTERED(MapServerDdt);

	TypeId
	MapServerDdt::GetTypeId(void)
	{
		static TypeId tid = TypeId("ns3::MapServerDdt")
								.SetParent<MapServer>()
								.SetGroupName("Lisp")
								.AddConstructor<MapServerDdt>();
		return tid;
	}

	Ptr<RandomVariableStream>
	MapServerDdt::GetMdsModel(void)
	{
		Ptr<EmpiricalRandomVariable> mds = CreateObject<EmpiricalRandomVariable>();
		mds->CDF(0.0, 0.0);
		mds->CDF(0.05, 0.1);
		mds->CDF(0.10, 0.2);
		mds->CDF(0.12, 0.3);
		mds->CDF(0.18, 0.4);
		mds->CDF(0.20, 0.5);
		mds->CDF(0.23, 0.6);
		mds->CDF(0.26, 0.7);
		mds->CDF(0.30, 0.8);
		mds->CDF(0.50, 0.9);
		mds->CDF(1.0, 0.98);
		mds->CDF(1.5, 0.999);
		mds->CDF(2.0, 1.0);

		return mds;
	}

	MapServerDdt::MapServerDdt()
	{
		/**
		 * MapServerDdt and LispOverIp should point to the same MapTables!
		 */
		m_mapTablesv4 = Create<SimpleMapTables>();
		m_mapTablesv6 = Create<SimpleMapTables>();
	}

	MapServerDdt::~MapServerDdt()
	{
		m_socket = 0;
		m_msClientSocket = 0;
		m_msClientSocket6 = 0;
	}

	Ptr<MapTables>
	MapServerDdt::GetMapTablesV4()
	{
		return m_mapTablesv4;
	}

	Ptr<MapTables>
	MapServerDdt::GetMapTablesV6()
	{
		return m_mapTablesv6;
	}

	void
	MapServerDdt::SetMapTables(Ptr<MapTables> mapTablesV4,
							   Ptr<MapTables> mapTablesV6)
	{
		NS_LOG_FUNCTION(this);
		m_mapTablesv4 = mapTablesV4;
		m_mapTablesv6 = mapTablesV6;
	}

	void
	MapServerDdt::StartApplication(void)
	{
		NS_LOG_FUNCTION(this);

		NS_LOG_DEBUG("STARTING MS");
		if (m_socket == 0)
		{
			TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
			m_socket = Socket::CreateSocket(GetNode(), tid);
			// ConnectToPeerAddress (m_peerAddress, m_peerPort);
		}
		m_socket->SetRecvCallback(MakeCallback(&MapServerDdt::HandleRead, this));
		if (m_msClientSocket == 0)
		{
			TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
			m_msClientSocket = Socket::CreateSocket(GetNode(), tid);
			InetSocketAddress local = InetSocketAddress(
				Ipv4Address::GetAny(), LispOverIp::LISP_SIG_PORT);
			m_msClientSocket->Bind(local);
		}

		if (m_msClientSocket6 == 0)
		{
			TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
			m_msClientSocket6 = Socket::CreateSocket(GetNode(), tid);
			Inet6SocketAddress local6 = Inet6SocketAddress(
				Ipv6Address::GetAny(), LispOverIp::LISP_SIG_PORT);
			m_msClientSocket6->Bind(local6);
		}
		m_msClientSocket6->SetRecvCallback(
			MakeCallback(&MapServerDdt::HandleReadFromClient, this));
		m_msClientSocket->SetRecvCallback(
			MakeCallback(&MapServerDdt::HandleReadFromClient, this));
	}

	void
	MapServerDdt::StopApplication(void)
	{
		NS_LOG_FUNCTION(this);

		if (m_socket != 0)
		{
			m_socket->Close();
			m_socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
			m_socket = 0;
		}

		if (m_msClientSocket)
		{
			m_msClientSocket->Close();
			m_msClientSocket->SetRecvCallback(
				MakeNullCallback<void, Ptr<Socket>>());
			m_msClientSocket = 0;
		}

		if (m_msClientSocket6)
		{
			m_msClientSocket6->Close();
			m_msClientSocket6->SetRecvCallback(
				MakeNullCallback<void, Ptr<Socket>>());
			m_msClientSocket6 = 0;
		}
		Simulator::Cancel(m_event);
	}

	Ptr<MapNotifyMsg>
	MapServerDdt::GenerateMapNotifyMsg(Ptr<MapRegisterMsg> msg)
	{
		// TODO implement map notification message here
		Ptr<MapNotifyMsg> mapNotify = Create<MapNotifyMsg>();
		mapNotify->SetRecordCount(msg->GetRecordCount());
		mapNotify->SetNonce(msg->GetNonce());
		mapNotify->SetAuthDataLen(msg->GetAuthDataLen());
		mapNotify->setAuthData(msg->getAuthData());
		mapNotify->setKeyId(msg->getKeyId());
		mapNotify->SetRecord(msg->GetRecord());
		return mapNotify;
	}

	void
	MapServerDdt::Send(Ptr<Packet> p)
	{
		NS_LOG_FUNCTION(this);
		NS_ASSERT(m_event.IsExpired());
		m_socket->Send(p);
	}

	void
	MapServerDdt::HandleRead(Ptr<Socket> socket)
	{
	}

	void
	MapServerDdt::PopulateDatabase(Ptr<MapRegisterMsg> msg)
	{
		// TODO: if map server has received a map register for a same EID prefix,
		// replace by new one or combine them??
		// For multihoming case, combine RLOC for the same RLOC
		// For LISP-MN, replace them
		NS_LOG_DEBUG("Get a Map Register Message! Try to decode it...");
		std::stringstream ss;
		Ptr<MapReplyRecord> record = msg->GetRecord();
		Ptr<EndpointId> eid;
		Ptr<Locators> locators = record->GetLocators();
		if (record->GetEidAfi() == LispControlMsg::IP)
		{
			ss << "/" << unsigned(record->GetEidMaskLength());
			Ipv4Mask mask = Ipv4Mask(ss.str().c_str());
			NS_LOG_DEBUG(
				"Decoded EID prefix length: /" << unsigned(record->GetEidMaskLength()));
			eid = Create<EndpointId>(record->GetEidPrefix(), mask);
			Ptr<MapEntryImpl> mapEntry = Create<MapEntryImpl>();
			mapEntry->SetLocators(locators);
			mapEntry->SetEidPrefix(eid);
			/**
			 * IMPORTANT: For map server, when it receives a map register message, it
			 * populates the database and cache at the same time. Why we should populate
			 * cache also? Because in LISP-MN, map-server is risking to forward map request
			 * to a LISP-MN, whose RLOC is not globally routable. The aforementioned map request
			 * message need to be encapsulated to be delivered to LISP-MN. In other hand, map
			 * server need to do cache lookup when calling LispOutput(in LispOverIpv4Impl).
			 */
			m_mapTablesv4->SetEntry(record->GetEidPrefix(), mask, mapEntry,
									MapTables::IN_DATABASE);
			m_mapTablesv4->SetEntry(record->GetEidPrefix(), mask, mapEntry,
									MapTables::IN_CACHE);
			NS_LOG_DEBUG("MS's Map Table Content:" << *m_mapTablesv4);
		}
		else if (record->GetEidAfi() == LispControlMsg::IPV6)
		{
			ss << "/" << unsigned(record->GetEidMaskLength());

			eid = Create<EndpointId>(record->GetEidPrefix(),
									 Ipv6Prefix(ss.str().c_str()));
			Ipv6Prefix prefix = Ipv6Prefix(ss.str().c_str());
			eid = Create<EndpointId>(record->GetEidPrefix(), prefix);
			Ptr<MapEntryImpl> mapEntry = Create<MapEntryImpl>();
			mapEntry->SetLocators(locators);
			mapEntry->SetEidPrefix(eid);
			m_mapTablesv6->SetEntry(record->GetEidPrefix(), prefix, mapEntry,
									MapTables::IN_DATABASE);
			m_mapTablesv6->SetEntry(record->GetEidPrefix(), prefix, mapEntry,
									MapTables::IN_CACHE);
		}
	}

	void
	MapServerDdt::HandleReadFromClient(Ptr<Socket> socket)
	{
		NS_LOG_FUNCTION(this);
		Ptr<Packet> packet;
		Address from;

		while ((packet = socket->RecvFrom(from)))
		{
			uint8_t buf[packet->GetSize()];
			packet->CopyData(buf, packet->GetSize());

			// TODO: one day modify buf[0] >> 4 as msg_type
			uint8_t msg_type = (buf[0] >> 4);
			NS_LOG_DEBUG("Decoded Message type: " << unsigned(msg_type));
			if (msg_type == static_cast<uint8_t>(MapRegisterMsg::GetMsgType()))
			{
				Ptr<MapRegisterMsg> msg = MapRegisterMsg::Deserialize(buf);
				MapServerDdt::PopulateDatabase(msg);
				// check if map register needs a map notification message...
				if (msg->GetM() == 1)
				{
					// Actually, the following code is almost the same with that using to determine map-request's RLOC
					// how to refactor these two blocs of code?
					Ptr<MapNotifyMsg> mapNotifyMsg = GenerateMapNotifyMsg(msg);
					Ptr<MapReplyRecord> record = msg->GetRecord();
					Ptr<MapEntry> entry;
					if (record->GetEidAfi() == LispControlMsg::IP)
					{
						entry = m_mapTablesv4->DatabaseLookup(
							record->GetEidPrefix());
					}
					else if (record->GetEidAfi() == LispControlMsg::IPV6)
					{
						entry = m_mapTablesv6->DatabaseLookup(
							record->GetEidPrefix());
					}
					NS_ASSERT_MSG(
						entry != 0,
						"Impossible!!!Map Server should be alaways find the RLOC to send Map Notify");
					Ptr<Locator> locator =
						entry->GetLocators()->SelectFirsValidRloc();
					NS_LOG_DEBUG(
						"Send Map Notify message to ETR " << Ipv4Address::ConvertFrom(locator->GetRlocAddress()));
					MapResolver::ConnectToPeerAddress(locator->GetRlocAddress(),
													  m_peerPort, m_socket);
					Ptr<Packet> reqPacket = Create<Packet>(buf,
														   packet->GetSize());

					MapResolver::ConnectToPeerAddress(locator->GetRlocAddress(),
													  m_peerPort, m_socket);
					uint8_t buf[256];
					mapNotifyMsg->Serialize(buf);
					Ptr<Packet> packet = Create<Packet>(buf, 256);
					// Don't forget to make socket connect to the xTR which send map register message.
					Send(packet);
					NS_LOG_DEBUG(
						"Map Register message M bit is 1=> A Map notify message has been sent back");
				}
			}
			else if (msg_type == static_cast<uint8_t>(MapRequestMsg::GetMsgType()))
			{
				NS_LOG_DEBUG("Receive Map request on Map-server!");
				Ptr<MapRequestMsg> requestMsg = MapRequestMsg::Deserialize(buf);
				Ptr<MapRequestRecord> record = requestMsg->GetMapRequestRecord();
				Ptr<MapEntry> entry;
				if (record->GetAfi() == LispControlMsg::IP)
				{
					entry = m_mapTablesv4->DatabaseLookup(record->GetEidPrefix());
				}
				else if (record->GetAfi() == LispControlMsg::IPV6)
				{
					entry = m_mapTablesv6->DatabaseLookup(record->GetEidPrefix());
				}
				if (entry == 0)
				{
					NS_LOG_DEBUG("Send Negative Map-Reply");
					MapResolver::ConnectToPeerAddress(
						requestMsg->GetItrRlocAddrIp(), m_peerPort, m_socket);

					Ptr<MapReplyMsg> mapReply = GenerateNegMapReply(requestMsg);
					uint8_t newBuf[256];
					mapReply->Serialize(newBuf);
					Ptr<Packet> reactedPacket = Create<Packet>(newBuf, 256);
					Simulator::Schedule(Seconds(m_searchTimeVariable->GetValue()), &MapServerDdt::Send, this, reactedPacket);
				}
				else
				{
					Ptr<Locator> locator = entry->GetLocators()->SelectFirsValidRloc();
					NS_LOG_DEBUG("Forward Map-Request to ETR " << Ipv4Address::ConvertFrom(locator->GetRlocAddress()));

					Ptr<Packet> reqPacket = Create<Packet>(buf,
														   packet->GetSize());
					Simulator::Schedule(Seconds(m_searchTimeVariable->GetValue()), &MapServerDdt::ForwardRequest, this, locator->GetRlocAddress(), reqPacket);
				}
			}
			else if (msg_type == static_cast<uint8_t>(InfoRequestMsg::GetMsgType()))
			{
				// socket->GetPeerName (addr);

				// Get Global address and port of peer
				InetSocketAddress iaddr = InetSocketAddress::ConvertFrom(from);
				NS_LOG_DEBUG("Receive InfoRequest Msg from " << iaddr.GetIpv4() << ":" << iaddr.GetPort());

				/*
				 Get own locator through MapTables :
				 Assumption: there is a unique eid space and a unique locator in database, or several
				 eid space but all under the same RLOC
				*/
				Ptr<LispOverIpv4> lispOverIpv4 = GetNode()->GetObject<LispOverIpv4>();
				Ptr<MapTables> mapTables = lispOverIpv4->GetMapTablesV4();
				std::list<Ptr<MapEntry>> entryList;
				mapTables->GetMapEntryList(MapTables::IN_DATABASE, entryList);
				Ptr<MapEntry> mapEntry = entryList.front();
				Address maddr = mapEntry->RlocSelection()->GetRlocAddress();

				// Alternative: (might be useful in another case)
				/*
				Address eid = mapEntry->GetEidPrefix ()-> GetEidAddress ();
				Ptr<Locator> destLoc = Create<Locator> (iaddr.GetIpv4 ());
				Ptr<Locator> loc = lispOverIpv4->SelectSourceRloc(eid, destLoc);
				Address maddr = loc->GetRlocAddress ();
				*/

				// Generate InfoReplyMsg
				Ptr<InfoRequestMsg> msg = InfoRequestMsg::Deserialize(buf);
				Ptr<InfoRequestMsg> infoReply = GenerateInfoReplyMsg(msg, iaddr.GetPort(), iaddr.GetIpv4(), maddr);

				uint8_t BUF_SIZE = 16 + infoReply->GetAuthDataLen() + 20 + infoReply->GetNatLcaf()->GetLength();
				uint8_t *newBuf = new uint8_t[BUF_SIZE];
				infoReply->Serialize(newBuf);
				Ptr<Packet> reactedPacket = Create<Packet>(newBuf, BUF_SIZE);
				socket->SendTo(reactedPacket, 0, from);
			}
			else
			{
				NS_LOG_ERROR(
					"Unknown Control Message Type!!! Program should be stopped...");
			}
		}
	}

	void
	MapServerDdt::ForwardRequest(Address peerAddress, Ptr<Packet> p)
	{

		MapResolver::ConnectToPeerAddress(peerAddress,
										  m_peerPort, m_socket);
		Send(p);
	}

	Ptr<MapReplyMsg>
	MapServerDdt::GenerateNegMapReply(Ptr<MapRequestMsg> requestMsg)
	{
		Ptr<MapReplyMsg> mapReply = Create<MapReplyMsg>();
		mapReply->SetNonce(requestMsg->GetNonce());
		mapReply->SetRecordCount(1);

		// Record (with no locators)
		Ptr<MapReplyRecord> replyRecord = Create<MapReplyRecord>();
		replyRecord->SetRecordTtl(MapReplyRecord::m_defaultRecordTtl);
		replyRecord->SetAct(MapReplyRecord::NoAction);
		replyRecord->SetA(1);
		replyRecord->SetMapVersionNumber(0);										  // No map version number
		replyRecord->SetEidPrefix(requestMsg->GetMapRequestRecord()->GetEidPrefix()); // Also set eid-prefix AFI
		replyRecord->SetEidMaskLength(32);
		Ptr<Locators> locators;
		replyRecord->SetLocators(locators);

		mapReply->SetRecord(replyRecord);

		return mapReply;
	}

	Ptr<InfoRequestMsg>
	MapServerDdt::GenerateInfoReplyMsg(Ptr<InfoRequestMsg> msg, uint16_t etrUdpPort, Address etrAddress, Address msAddress)
	{

		Ptr<InfoRequestMsg> infoReply = Create<InfoRequestMsg>();
		// All fields same as received InfoRequest
		infoReply->SetR(0x01);
		infoReply->SetNonce(msg->GetNonce());
		infoReply->SetAuthDataLen(msg->GetAuthDataLen());
		infoReply->SetAuthData(msg->GetAuthData());
		infoReply->SetKeyId(msg->GetKeyId());
		infoReply->SetTtl(msg->GetTtl());
		infoReply->SetEidMaskLength(msg->GetEidMaskLength());
		infoReply->SetEidPrefixAfi(msg->GetEidPrefixAfi());
		infoReply->SetEidPrefix(msg->GetEidPrefix());

		// Generate NatLcaf part
		Ptr<NatLcaf> natLcaf = Create<NatLcaf>();
		if (GetRtrAddress().IsInvalid())
			natLcaf->SetRtrRlocAddress(Ipv4Address("0.0.0.0"));
		else
			natLcaf->SetRtrRlocAddress(GetRtrAddress());
		natLcaf->SetGlobalEtrRlocAddress(etrAddress);
		natLcaf->SetEtrUdpPort(etrUdpPort);
		natLcaf->SetMsRlocAddress(msAddress);
		natLcaf->SetPrivateEtrRlocAddress(Ipv4Address("0.0.0.0"));
		natLcaf->ComputeLength();

		infoReply->SetNatLcaf(natLcaf);
		return infoReply;
	}

} /* namespace ns3 */
