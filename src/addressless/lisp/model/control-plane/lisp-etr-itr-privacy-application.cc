
#include "ns3/lisp-etr-itr-privacy-application.h"
#include "ns3/redirect-header.h"
namespace ns3
{
	NS_LOG_COMPONENT_DEFINE("LispEtrItrPrivacyApplication");
	NS_OBJECT_ENSURE_REGISTERED(LispEtrItrPrivacyApplication);

	// Needed to be overriden in child class to have proper constructor.
	TypeId LispEtrItrPrivacyApplication::GetTypeId(void)
	{
		static TypeId tid = TypeId("ns3::LispEtrItrPrivacyApplication")
								.SetParent<Application>()
								.SetGroupName("Lisp")
								.AddConstructor<LispEtrItrPrivacyApplication>()
								.AddAttribute("xTRId",
											  "The id of this xTR",
											  UintegerValue(0),
											  MakeUintegerAccessor(&LispEtrItrPrivacyApplication::m_id),
											  MakeUintegerChecker<uint16_t>())
								.AddAttribute("PeerPort",
											  "The destination port of the packet",
											  UintegerValue(LispOverIp::LISP_SIG_PORT),
											  MakeUintegerAccessor(&LispEtrItrPrivacyApplication::m_peerPort),
											  MakeUintegerChecker<uint16_t>())
								.AddAttribute("FastRedir",
											  "Activate or not the fast redirection privacy feature.",
											  BooleanValue(false),
											  MakeBooleanAccessor(&LispEtrItrPrivacyApplication::m_fastRedir),
											  MakeBooleanChecker())
								.AddAttribute("RlocRedir",
											  "Activate or not the rloc redirection privacy feature.",
											  BooleanValue(false),
											  MakeBooleanAccessor(&LispEtrItrPrivacyApplication::m_rlocRedir),
											  MakeBooleanChecker())
								.AddAttribute("RlocInterface", "The interface of the xtr",
											  PointerValue(),
											  MakePointerAccessor(&LispEtrItrPrivacyApplication::m_rlocInterface),
											  MakePointerChecker<Ipv4Interface>())
								.AddAttribute("ServerInterface", "The interface of the server",
											  PointerValue(),
											  MakePointerAccessor(&LispEtrItrPrivacyApplication::m_serverInterface),
											  MakePointerChecker<Ipv4Interface>())
								.AddAttribute("Interval",
											  "The time to wait between packets", TimeValue(Seconds(3.0)),
											  MakeTimeAccessor(&LispEtrItrPrivacyApplication::m_interval),
											  MakeTimeChecker())
								.AddAttribute(
									"Protocol",
									"",
									StringValue("TCP"),
									MakeStringAccessor(&LispEtrItrPrivacyApplication::m_protocol),
									MakeStringChecker())
								.AddAttribute(
									"RttVariable",
									"The random variable representing the distribution of RTTs between xTRs)",
									StringValue("ns3::ConstantRandomVariable[Constant=0]"),
									MakePointerAccessor(&LispEtrItrPrivacyApplication::m_rttVariable),
									MakePointerChecker<RandomVariableStream>())
								.AddAttribute(
									"ProxyMapReply",
									"",
									BooleanValue(false),
									MakeBooleanAccessor(&LispEtrItrPrivacyApplication::m_proxy),
									MakeBooleanChecker())
								.AddAttribute(
									"HashVariable",
									"Hash processing time",
									StringValue("ns3::ConstantRandomVariable[Constant=0.00004]"),
									MakePointerAccessor(&LispEtrItrPrivacyApplication::m_hashTime),
									MakePointerChecker<RandomVariableStream>())
								.AddTraceSource("MapRegisterTx", "A MapRegister is sent by the LISP device",
												MakeTraceSourceAccessor(&LispEtrItrPrivacyApplication::m_mapRegisterTxTrace),
												"ns3::Packet::TracedCallback")
								.AddTraceSource("MapNotifyRx", "A MapNotify is received by the LISP device",
												MakeTraceSourceAccessor(&LispEtrItrPrivacyApplication::m_mapNotifyRxTrace),
												"ns3::Packet::TracedCallback")
								.AddTraceSource("MappingDelay",
												"Mapping Delay has been measured",
												MakeTraceSourceAccessor(&LispEtrItrPrivacyApplication::m_mapDelay),
												"ns3::AttributeObjectTest::NumericTracedCallback");
		return tid;
	}

	LispEtrItrPrivacyApplication::LispEtrItrPrivacyApplication() : LispEtrItrApplication()
	{
		NS_LOG_FUNCTION(this);
	}

	LispEtrItrPrivacyApplication::~LispEtrItrPrivacyApplication() {}

	Ipv4Address LispEtrItrPrivacyApplication::GenerateAddress(Ipv4Address source)
	{
		Hasher hs;
		std::ostringstream ss;
		source.CombineMask(Ipv4Mask("/24")).Print(ss);
		int index = hs.GetHash32(ss.str()) % (m_rlocInterface->GetNAddresses());
		NS_LOG_DEBUG("* RLOCREDIR::Address generated: " << m_rlocInterface->GetAddress(index).GetLocal());
		return m_rlocInterface->GetAddress(index).GetLocal();
	}

	void LispEtrItrPrivacyApplication::FastRedir(Ptr<MapRequestMsg> requestMsg)
	{
		Hasher hs;
		std::ostringstream ss;
		// ns3-privacy addition
		Ipv4Address::ConvertFrom(requestMsg->GetSourceEidAddr()).Print(ss);
		int index = hs.GetHash32(ss.str()) % (m_serverInterface->GetNAddresses());
		Ipv4Address addr = m_serverInterface->GetAddress(index).GetLocal();
		NS_LOG_DEBUG("* FASTREDIR::Address generated: " << addr);

		Address serverAddress = Address(addr);
		uint8_t *address = new uint8_t[4];
		serverAddress.CopyTo(address);

		RedirectHeader redirH;
		redirH.SetRedirect(true);

		Ptr<Packet> packet = Create<Packet>(address, 4);
		packet->AddHeader(redirH);

		if (m_protocol == "ns3::TcpSocketFactory")
		{
			TcpHeader tcp;
			tcp.SetSourcePort(50000);
			tcp.SetDestinationPort(41000);
			Ptr<TcpOptionTS> ts = Create<TcpOptionTS>();
			tcp.AppendOption(ts);
			tcp.SetAckNumber(SequenceNumber32(0));
			tcp.SetSequenceNumber(SequenceNumber32(0));
			packet->AddHeader(tcp);
		}
		else
		{
			UdpHeader udp;
			udp.SetSourcePort(50000);
			udp.SetDestinationPort(41000);
			packet->AddHeader(udp);
		}

		Ptr<Ipv4L3Protocol> ipv4 = GetNode()->GetObject<Ipv4L3Protocol>();
		Ptr<Ipv4Route> route = Create<Ipv4Route>();
		route->SetDestination(Ipv4Address::ConvertFrom(requestMsg->GetSourceEidAddr()));
		route->SetGateway("10.1.4.2");
		route->SetOutputDevice(GetNode()->GetDevice(0));
		route->SetSource(ipv4->GetInterface(1)->GetAddress(0).GetLocal());
		ipv4->Send(packet, ipv4->GetInterface(1)->GetAddress(0).GetLocal(), Ipv4Address::ConvertFrom(requestMsg->GetSourceEidAddr()), m_protocol == "ns3::TcpSocketFactory" ? 6 : 17, route);
		m_clientswaiting -= 1;
	}

	Ptr<MapReplyMsg>
	LispEtrItrPrivacyApplication::GenerateMapReply(
		Ptr<MapRequestMsg> requestMsg)
	{
		
		NS_LOG_FUNCTION(this);
		Ptr<MapReplyMsg> mapReply = Create<MapReplyMsg>(); // Smart pointer, default value is 0
		Ptr<MapRequestRecord> record = requestMsg->GetMapRequestRecord();
		Ptr<MapEntry> entry;
		if (record->GetAfi() == LispControlMsg::IP)
		{
			// TODO May be use mapping socket instead
			NS_LOG_DEBUG("Execute database look up for EID: " << Ipv4Address::ConvertFrom(record->GetEidPrefix()));
			entry = m_mapTablesV4->DatabaseLookup(record->GetEidPrefix());
		}
		else if (record->GetAfi() == LispControlMsg::IPV6)
		{
			NS_LOG_DEBUG("Execute database look up for EID: " << Ipv6Address::ConvertFrom(record->GetEidPrefix()));
			entry = m_mapTablesV6->DatabaseLookup(record->GetEidPrefix());
		}
		if (entry == 0)
		{
			// TODO: Should implement negative map-reply case.
			NS_LOG_DEBUG("Send Negative Map-Reply");
			// Now we simply return a 0 in case of negative map reply.
			return 0;
		}
		else
		{
			NS_LOG_DEBUG("Send Map-Reply to ITR");

			Ptr<MapReplyRecord> replyRecord = Create<MapReplyRecord>();

			mapReply->SetNonce(requestMsg->GetNonce());
			mapReply->SetRecordCount(1);
			replyRecord->SetAct(MapReplyRecord::NoAction);
			replyRecord->SetA(1);
			replyRecord->SetMapVersionNumber(entry->GetVersionNumber());
			replyRecord->SetRecordTtl(MapReplyRecord::m_defaultRecordTtl);
			replyRecord->SetEidPrefix(entry->GetEidPrefix()->GetEidAddress());

			if (entry->GetEidPrefix()->IsIpv4())
				replyRecord->SetEidMaskLength(
					entry->GetEidPrefix()->GetIpv4Mask().GetPrefixLength());
			else
				replyRecord->SetEidMaskLength(
					entry->GetEidPrefix()->GetIpv6Prefix().GetPrefixLength());

			
			/**/
			m_hashDone = 0;

			if (m_fastRedir)
			{
				m_clientswaiting += 1;
				Simulator::Schedule(m_clientswaiting * Seconds(m_hashTime->GetValue()), &LispEtrItrPrivacyApplication::FastRedir, this, requestMsg);
				replyRecord->SetLocators(entry->GetLocators());
			}
			else if (m_rlocRedir)
			{
				Ptr<Locators> rtrLocs = Create<LocatorsImpl>();
				rtrLocs->InsertLocator(Create<Locator>(GenerateAddress(Ipv4Address::ConvertFrom(requestMsg->GetSourceEidAddr()))));
				replyRecord->SetLocators(rtrLocs);
				uint32_t ip = Ipv4Address::ConvertFrom(requestMsg->GetSourceEidAddr()).Get();
				if (m_privacyDone.find(ip) == m_privacyDone.end())
				{
					m_clientswaiting += 1;
					m_privacyDone[ip] = true;
					m_hashDone += 1;
				}
			}
			else
				replyRecord->SetLocators(entry->GetLocators());

			/**/

			mapReply->SetRecord(replyRecord);
			NS_LOG_DEBUG(
				"MAP REPLY READY, Its content is as follows:\n"
				<< *mapReply);
		}
		return mapReply;
	}

	void LispEtrItrPrivacyApplication::HandleControlMsg(Ptr<Packet> packet, Address from)
	{
		uint8_t buf[packet->GetSize()];
		packet->CopyData(buf, packet->GetSize());
		// Do not forget to right shift first byte to obtain message type..
		uint8_t msg_type = buf[0] >> 4;
		if (msg_type == MapRequestMsg::GetMsgType())
		{
			Ptr<MapRequestMsg> requestMsg = MapRequestMsg::Deserialize(buf);
			Ptr<Packet> reactedPacket;

			/**
			 * After reception of a map request, the possible reactions:
			 * 1) A conventional map reply
			 * 2) A SMR-invoked MapRequest
			 * 3) A map reply to a SMR-invoked MapRequest
			 *
			 * No matter to react to a SMR or normal map request message, the destination RLOC is the same.
			 * Update: 2018-01-24, In case of reception of a SMR, perhaps it is better to send an
			 * invoked-SMR to map resolver/map server. Because in case of double encapsulation issue in
			 * LISP-MN, the first invoked-SMR will be surely failed.
			 *
			 */

			if (requestMsg->GetItrRlocAddrIp() != static_cast<Address>(Ipv4Address()))
				MapResolver::ConnectToPeerAddress(
					requestMsg->GetItrRlocAddrIp(), m_peerPort, m_socket);
			else if ((requestMsg->GetItrRlocAddrIpv6() != static_cast<Address>(Ipv6Address())))
				MapResolver::ConnectToPeerAddress(
					requestMsg->GetItrRlocAddrIpv6(), m_peerPort, m_socket);
			else
				NS_LOG_ERROR(
					"NO valid ITR address (neither ipv4 nor ipv6) to send back Map Message!!!");

			if (requestMsg->GetS() == 0)
			{

				/* If device is NATed, don't answer with a MapReply */
				Ptr<LispOverIpv4> lisp = m_node->GetObject<LispOverIpv4>();
				NS_LOG_DEBUG(
					"Receive one Map-Request on ETR from " << Ipv4Address::ConvertFrom(requestMsg->GetItrRlocAddrIp()) << ". Prepare a Map Reply Message.");

				if (!lisp->IsNated())
				{
					uint8_t newBuf[256];
					Ptr<MapReplyMsg> mapReply = this->GenerateMapReply(requestMsg);
					/**
					 * Update, 02-02-2018, Yue
					 * We never consider how to process the case NEGATIVE MAP Reply!
					 * Should checke whether mapReply is 0 before sending it.
					 */
					if (mapReply != 0)
					{
						mapReply->Serialize(newBuf);
						reactedPacket = Create<Packet>(newBuf, 256);
						Simulator::Schedule(m_hashDone * m_clientswaiting * Seconds(m_hashTime->GetValue()), &LispEtrItrPrivacyApplication::SendTo, this, reactedPacket, requestMsg->GetItrRlocAddrIp());
				
						// TODO: we should add check for the return value of Send method.
						//  Since it is possible that map reply has not been sent due to cache miss...
						NS_LOG_DEBUG(
							"Map Reply Sent to " << Ipv4Address::ConvertFrom(requestMsg->GetItrRlocAddrIp()));
					}
					else
					{
						NS_LOG_WARN("Negative map reply case! No message will be send to xTR:" << Ipv4Address::ConvertFrom(requestMsg->GetItrRlocAddrIp()));
					}
				}

				/*
				 * Record all RLOCs that send MapRequests for novel SMR procedure
				 */

				m_remoteItrCache.insert(requestMsg->GetItrRlocAddrIp());
			}
			else if (requestMsg->GetS() == 1 and requestMsg->GetS2() == 0)
			{
				/**
				 * case: reception of an SMR.
				 * In case of double encapsulation, it is possible that first transmission
				 * of invoked SMR after reception of SMR is lost, because the destination
				 * address of invoked-SMR is the LISP-MN, which cannot be found
				 * in the cache of the considered xTR!
				 * Thus, it is necessary to first check whether the destination @IP
				 * is a known RLOC by xTR. If not, go to send SMR procedure
				 * for LISP-MN's new Local RLOC
				 *
				 */
				Address rlocAddr = requestMsg->GetItrRlocAddrIp();
				NS_LOG_DEBUG(
					"Receive a SMR on ETR from " << Ipv4Address::ConvertFrom(rlocAddr) << ". Prepare an invoked Map Request Message.");
				if (m_node->GetObject<LispOverIpv4>()->IsLocatorInList(rlocAddr))
				{
					// If RLOC is in the list of lispOverIpv4 object => RLOC is really a RLOC of some xTR
					// Then we can send invoked-SMR to the xTR
					SendInvokedSmrMsg(requestMsg);
				}
				else
				{
					NS_LOG_DEBUG(
						"The destination IP address: "
						<< Ipv4Address::ConvertFrom(rlocAddr)
						<< " is not a known RLOC! In LISP-MN, it maybe an EID...Send Map request to query it");
					Ptr<EndpointId> eid = Create<EndpointId>(rlocAddr);
					// Generate a map request and send it...
					SendMapRequest(GenerateMapRequest(eid));
					// TODO:2018-01-26:
					//  Save the invoked-SMR message.
					//	Remember to add something in map reply process part.
					//  The saved invoked-SMR should be sent upon reception of map reply...
				}
			}
			else if (requestMsg->GetS() == 1 and requestMsg->GetS2() == 1)
			{
				/**
				 * case: reception of a SMR-invoked Map Request.
				 * Unlike a normal map reply, we don't care the EID-prefix conveyed in this message.
				 * We will put the changed mapping in map-reply
				 */
				/**
				 * TODO: how to know which mapping entry is changed in database?
				 * For LISP-MN, it is simple, since its database just has one mapping entry.
				 * This a normal Map Rely...
				 */
				NS_LOG_DEBUG(
					"Receive an SMR-invoked Request on ETR from " << Ipv4Address::ConvertFrom(requestMsg->GetItrRlocAddrIp()) << ". Prepare a Map Reply Message.");
				m_recvIvkSmr = true;
				// Given reception of SMR-invoked map request, remove the scheduled event.
				Simulator::Remove(m_resendSmrEvent);
				uint8_t newBuf[256];

				// Instead of response the queried EID-prefix, maReply conveys the content of database!
				// Ptr<MapReplyMsg> mapReply = LispEtrItrApplication::GenerateMapReply4ChangedMapping(requestMsg);
				/* Emeline: this code doesn't work because MapRequest/Reply can only carry one
				 * Mapping Record (due to implementation).
				 * Therefore: just answer with a normal MapReply */
				Ptr<MapReplyMsg> mapReply = this->GenerateMapReply(requestMsg);
				if (mapReply != 0)
				{
					mapReply->Serialize(newBuf);
					reactedPacket = Create<Packet>(newBuf, 256);

					/* --- Artificial delay for SMR procedure --- */
					Simulator::Schedule(Seconds((m_rttVariable->GetValue() / 2)), &LispEtrItrPrivacyApplication::SendTo, this, reactedPacket, requestMsg->GetItrRlocAddrIp());
					NS_LOG_DEBUG(
						"A Map Reply Message Sent to " << Ipv4Address::ConvertFrom(requestMsg->GetItrRlocAddrIp()) << " in response to an invoked SMR");
				}
				else
				{
					NS_LOG_WARN("Negative map reply case! No message will be send to xTR:" << Ipv4Address::ConvertFrom(requestMsg->GetItrRlocAddrIp()));
				}

				/**
				 * It's better to send map reply in response to invoked SMR to MR/MS according to
				 * RFC6830. Up to Yue to implement it.
				 */
			}
			else
			{
				NS_LOG_ERROR(
					"Unknown M bit value. Either 0 or 1. No other values!");
			}
		}
		else if (msg_type == static_cast<uint8_t>(MapReplyMsg::GetMsgType()))
		{

			NS_LOG_DEBUG(
				"Msg Type " << unsigned(msg_type) << ": GET a MAP REPLY");
			// Get Map Reply
			Ptr<MapReplyMsg> replyMsg = MapReplyMsg::Deserialize(buf);
			m_mapDelay(m_id, (Simulator::Now() - m_mapReqSent[Ipv4Address::ConvertFrom(replyMsg->GetRecord()->GetEidPrefix()).Get()]).GetMicroSeconds());

			Ptr<MappingSocketMsg> mapSockMsg = GenerateMapSocketAddMsgBody(
				replyMsg);
			MappingSocketMsgHeader mapSockHeader =
				GenerateMapSocketAddMsgHeader(replyMsg);
			NS_LOG_DEBUG("Mapping socket message created");
			NS_ASSERT_MSG(mapSockMsg != 0,
						  "Cannot create map socket message body !!! Please check why.");
			uint8_t buf[256];
			mapSockMsg->Serialize(buf);
			/**
			 * In my opinion, it is not necessary to create a packet with 256 bytes as payload.
			 * In fact, we just need to ensure that length of buffer is more than the length of mapSockMsg.
			 * Since this is just an exchange between LispOverIp(data plan) and LispEtrItrApplication(control plan)
			 * Both data plan and control plan are on the same node.
			 * It is like a socket communication between user and kernel space.
			 */
			Ptr<Packet> packet = Create<Packet>(buf, 256);
			packet->AddHeader(mapSockHeader);
			// Send to lispOverIp object so that it can insert the mapping entry in Cache.
			// ATTENTION: with SMR, before inserting one map entry, should first check its presence in Cache.
			// Now we apply a replacement strategy: if the EID-prefix already in Cache, replace it with the new
			// One.
			SendToLisp(packet);
			// Don't forget to remove Eid in pending list...
			DeleteFromMapReqList(mapSockMsg->GetEndPointId());
			/**
			 * After reception of map reply and insertion of received EID-RLOC mapping into cache,
			 * remember to check if the map request messages with received EID are present in m_mapReqMsg. If yes,
			 * remember to send them
			 * It is better to move this method to cache database.
			 */
			for (std::list<Ptr<MapRequestMsg>>::const_iterator it = m_mapReqMsg.begin(); it != m_mapReqMsg.end(); ++it)
			{
				//
			}
		}
		else if (msg_type == static_cast<uint8_t>(LispControlMsg::MAP_NOTIFY))
		{
			/**
			 * After reception of Map notify message, two possible reactions:
			 * 1) In traditional LISP, no reaction after Map registration procedure
			 * 2) In LISP-MN mode, if supporting SMR, a SMR message should be sent out!
			 * As a first step, we choose to send map request to all RLOCs in the cache.
			 * Maybe it's better to have a flag for mapTablesChanged. Only it is true,
			 * SMR procedure will be triggered. In addition, we also need a mechanism to
			 * quickly check whether database is changed (Map versioning???)
			 */
			NS_LOG_DEBUG("LISP device received MapNotify");
			/* --- Tracing --- */
			m_mapNotifyRxTrace(packet);

			/* --- Notifies DataPlane that LISP device is registered (allowed to send data packets)--- */
			Ptr<MappingSocketMsg> mapSockMsg = Create<MappingSocketMsg>();
			mapSockMsg->SetEndPoint(
				Create<EndpointId>(Ipv4Address("0.0.0.0"), Ipv4Mask("/32"))); // Don't care about this endpoint (won't be used)
			MappingSocketMsgHeader mapSockHeader;
			mapSockHeader.SetMapType(LispMappingSocket::MAPM_ISREGISTERED);
			mapSockHeader.SetMapRlocCount(0);
			mapSockHeader.SetMapVersioning(0);

			mapSockHeader.SetMapAddresses(static_cast<uint16_t>(LispMappingSocket::MAPA_EID));
			// MAPA_EID used to say that LISP device is registered.
			// MAPA_EIDMASK is used to say that LISP device is NOT registered.

			uint8_t buf[256];
			mapSockMsg->Serialize(buf);
			Ptr<Packet> packet = Create<Packet>(buf, 256);
			packet->AddHeader(mapSockHeader);
			SendToLisp(packet);

			/* The SMR procedure is now implemented on the RemoteItr Cache, instead of on the LISP Cache */
			if (!m_remoteItrCache.empty())
			{
				NS_LOG_DEBUG(
					"Receive a map notify message. xTR's Cache is not empty. Trigger SMR procedure for every entry in Cache...");
				/* --- Artificial delay for the SMR procedure--- */
				Simulator::Schedule(Seconds(m_rttVariable->GetValue() / 2), &LispEtrItrApplication::SendSmrMsg, this);
				/**
				 * After sendng SMR, don't forget to schedule a resend event for SMR
				 * Since for double encapsulation case, surely the first trial will be failed.
				 */
				m_resendSmrEvent = Simulator::Schedule(Seconds(2.0 + m_rttVariable->GetValue() / 2),
													   &LispEtrItrApplication::SendSmrMsg, this);
				m_recvIvkSmr = false;
			}
		}
		else
			NS_LOG_ERROR("Problem with packet!");
	}

	void LispEtrItrPrivacyApplication::SendTo(Ptr<Packet> packet, Address to)
	{

		if (m_rlocRedir && m_clientswaiting > 1)
		{
			m_clientswaiting -= 1;
		}
		MapResolver::ConnectToPeerAddress(to, m_peerPort, m_socket);
		NS_LOG_FUNCTION(this);

		NS_ASSERT(m_event.IsExpired());
		m_socket->Send(packet);
		++m_sent;
	}

	Ptr<MapRegisterMsg> LispEtrItrPrivacyApplication::GenerateMapRegister(
		Ptr<MapEntry> mapEntry, bool rtr)
	{
		Ptr<MapRegisterMsg> msg = Create<MapRegisterMsg>();
		Ptr<MapReplyRecord> record = Create<MapReplyRecord>();
		record->SetRecordTtl(static_cast<uint32_t>(0xffffffff));
		/**
		 * TODO: We consider M bit is set by default as 1
		 * so that Map Server will sends a Map Notify Message once upon reception
		 * of Map Register message.
		 */
		msg->SetM(1);
		msg->SetP(m_proxy && (!(m_fastRedir or m_rlocRedir)));
		msg->SetNonce(0); // Nonce is 0 for map register
		msg->setKeyId(static_cast<uint16_t>(0xface));
		msg->SetAuthDataLen(04); // Set
		record->SetEidPrefix(mapEntry->GetEidPrefix()->GetEidAddress());
		if (record->GetEidAfi() == LispControlMsg::IP)
			record->SetEidMaskLength(
				mapEntry->GetEidPrefix()->GetIpv4Mask().GetPrefixLength());
		else if (record->GetEidAfi() == LispControlMsg::IPV6)
			record->SetEidMaskLength(
				mapEntry->GetEidPrefix()->GetIpv6Prefix().GetPrefixLength());

		if (rtr)
		{ // Must replace all locators with RTR RLOC
			Ptr<Locators> rtrLocs = Create<LocatorsImpl>();
			rtrLocs->InsertLocator(m_rtrRlocs.front());
			record->SetLocators(rtrLocs);
		}
		else // Get list of Locator and save it into record
			record->SetLocators(mapEntry->GetLocators());

		record->SetMapVersionNumber(mapEntry->GetVersionNumber());
		msg->SetRecord(record);
		msg->SetRecordCount(1);
		return msg;
	}

	void LispEtrItrPrivacyApplication::SendMapRequest(Ptr<MapRequestMsg> mapReqMsg)
	{
		Ptr<MapRequestRecord> record = mapReqMsg->GetMapRequestRecord();
		uint32_t prefix = Ipv4Address::ConvertFrom(record->GetEidPrefix()).CombineMask("/24").Get();
		if (m_mapReqSent.find(prefix) != m_mapReqSent.end())
		{
			return;
		}
		m_mapReqSent[prefix] = Simulator::Now();
		// TODO: whehter here 64 bytes here is good...
		uint8_t bufMapReq[64];
		mapReqMsg->Serialize(bufMapReq);
		Ptr<Packet> packetMapReqMsg;
		packetMapReqMsg = Create<Packet>(bufMapReq, 64);
		MapResolver::ConnectToPeerAddress(
			m_mapResolverRlocs.front()->GetRlocAddress(),
			LispOverIp::LISP_SIG_PORT, m_socket);
		Send(packetMapReqMsg);
	}
}