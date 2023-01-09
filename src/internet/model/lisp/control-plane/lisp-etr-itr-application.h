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
#ifndef SRC_INTERNET_MODEL_LISP_CONTROL_PLANE_LISP_ETR_ITR_APPLICATION_H_
#define SRC_INTERNET_MODEL_LISP_CONTROL_PLANE_LISP_ETR_ITR_APPLICATION_H_

#include "ns3/application.h"
#include "ns3/event-id.h"
#include "ns3/ptr.h"
#include "ns3/traced-callback.h"
#include "ns3/map-tables.h"
#include "ns3/lisp-over-ip.h"
#include "ns3/lisp-protocol.h"
#include "ns3/lisp-over-ipv4.h"
#include "ns3/map-request-msg.h"
#include "ns3/map-reply-msg.h"
#include "ns3/info-request-msg.h"
#include "ns3/map-register-msg.h"
#include "ns3/mapping-socket-msg-header.h"
#include "ns3/mapping-socket-msg.h"
#include "ns3/locators-impl.h"
#include "ns3/string.h"


namespace ns3
{

class Socket;
class Packet;
class LispMappingSocket;

class LispEtrItrApplication : public Application
{

public:

  static TypeId
  GetTypeId (void);

  LispEtrItrApplication ();
  virtual
  ~LispEtrItrApplication ();

  static Ptr<RandomVariableStream> GetRttModel (void);

  void SetMapServerAddresses (std::list<Address> mapServerAddress);
  //TODO: implement this getter. useful for DHCP
  std::list<Address> GetMapServerAddresses (std::list<Address> mapServerAddress);
  void SetMapTables (Ptr<MapTables> mapTablesV4, Ptr<MapTables> mapTablesV6);
  void AddMapResolverLoc (Ptr<Locator> locator);
  //TODO: implement this getter. useful for DHCP
  std::list<Ptr<Locator> > GetMapResolverRLocs (Ptr<Locator> locator);
  static const uint8_t MAX_REQUEST_NB; //!< maximum number of requests pending in list

  std::list<Ptr<MapRequestMsg>> GetMapRequestMsgList();

  /**
    By default, rtr is set to false, meaning that the locators in the message, will
    be those of the LISP device.
    If rtr is set to True, this means that all locators will be replaced with the
    locator of an RTR.
  */
  void SendMapRegisters(bool rtr = false);

  void SendInfoRequest(void);

  /**
   * \brief send SMR(i.e. Map Request Message with S bit set as 1) to all other xTRs
   * contacted recently. In RFC6830, xTR will send SMR to all xTRs contacted in the last minutes.
   * As a first step, we send SMR to all xTRs in the cache.
   */
  void SendSmrMsg(void);

  /**
   * \brief After reception of a SMR by a xTR, send an invoked-SMR(i.e. Map Request Message with S and s bit set as 1)
   * to the aforementioned xTR. This take the received SMR as the only input method
   * and returns nothing.
   */
  void SendInvokedSmrMsg(Ptr<MapRequestMsg> smr);


  /**
   * \brief Send a map request message
   */
  virtual void SendMapRequest(Ptr<MapRequestMsg> mapRequestMsg);

//protected:
  /**
   * \brief Schedule the next packet transmission
   * \param dt time interval between packets.
   */
  void ScheduleTransmit (Time dt);

  void SendToLisp (Ptr<Packet> packet);

  void Send (Ptr<Packet> packet);

  /**
   * \brief Handle a packet reception.
   *
   * This function is called by lower layers.
   *
   * \param socket the socket the packet was received to.
   */
  void HandleRead (Ptr<Socket> socket);

  void HandleReadControlMsg (Ptr<Socket> socket);
  virtual void HandleControlMsg(Ptr<Packet> packet, Address from);

  void HandleMapSockRead (Ptr<Socket> lispMappingSocket);

  bool IsInRequestList (Ptr<EndpointId> eid) const;
  bool IsInRequestCounter (Ptr<EndpointId> eid) const;
  uint8_t GetRequestCount(Ptr<EndpointId> eid);

  void AddInMapReqList (Ptr<EndpointId> eid, Ptr<MapRequestMsg> reqMsg);

  void DeleteFromMapReqList (Ptr<EndpointId> eid);

  Ptr<InfoRequestMsg> GenerateInfoRequest(Ptr<MapEntry> mapEntry);
  /**
    By default, rtr is set to false, meaning that the locators in the message, will
    be those of the LISP device.
    If rtr is set to True, this means that all locators will be replaced with the
    locator of an RTR.
  */
  virtual Ptr<MapRegisterMsg> GenerateMapRegister(Ptr<MapEntry> mapEntry, bool rtr = false);
  Ptr<MapRequestMsg> GenerateMapRequest(Ptr<EndpointId> eid, Address host = Address(Ipv4Address()));
  virtual Ptr<MapReplyMsg> GenerateMapReply(Ptr<MapRequestMsg> msg);
  Ptr<MappingSocketMsg> GenerateMapSocketAddMsgBody(Ptr<MapReplyMsg> replyMsg);
  MappingSocketMsgHeader GenerateMapSocketAddMsgHeader(Ptr<MapReplyMsg> replyMsg);
  Ptr<MapReplyMsg> GenerateMapReply4ChangedMapping (Ptr<MapRequestMsg> requestMsg);
  Ptr<MappingSocketMsg> GenerateMapSocketAddMsgBodyForRtr(Address rtrAddress);
  MappingSocketMsgHeader GenerateMapSocketAddMsgHeaderForRtr(void);

  Address GetLocalAddress (Address address);

	Ptr<EndpointId> GetLispMnEid();

protected:

  virtual void DoDispose (void);

  virtual void StartApplication (void);

  virtual void StopApplication (void);

  bool m_requestSent;
  bool m_recvIvkSmr;
  EventId m_resendSmrEvent;                //!< Message refresh event
  typedef std::map<Ptr<EndpointId>, Ptr<MapRequestMsg>, MapTables::CompareEndpointId> RequestPendingList_t;
  RequestPendingList_t m_requestList;
  typedef std::map<Ptr<EndpointId>, uint8_t, MapTables::CompareEndpointId> RequestPendingCounter;
  RequestPendingCounter m_requestCounter;
  // each etr is configure with the address of the map
  // server it must register to
  std::list<Address> m_mapServerAddress;
  // a map table containing the prefixes it is responsible for
  Ptr<MapTables> m_mapTablesV4;
  Ptr<MapTables> m_mapTablesV6;
  std::list<Ptr<Locator> > m_mapResolverRlocs;
  std::list<Ptr<Locator> > m_rtrRlocs;
  // Save SMR before that xTR find the RLOC of the LISP-MN node
  // Upon reception of map reply, check immediately whether can send the saved
  // message. We use simple list data structure. Of course, we can use more
  // efficiency data structure in the future.
  std::list<Ptr<MapRequestMsg>> m_mapReqMsg;

  std::set<Address> m_remoteItrCache; //!< Records all (P)ITRs that send MapRequests to LISP device

  Ptr<RandomVariableStream> m_rttVariable; //!< RV representing the distribution of RTTs between xTRs

  Ptr<Socket> m_lispMappingSocket; //Socket for communication with dataplane
  Ptr<Socket> m_socket; // emeline: Socket for communication with MS (MapRegister) and MR (MapRequest)
  Ptr<Socket> m_lispCtlMsgRcvSocket; // Socket to receive Control messages (MapRequest/MapReply)
  Ptr<Socket> m_lispCtlMsgRcvSocket6; // Socket to receive Control messages (MapRequest/MapReply)

  uint32_t m_sent;
  uint32_t m_count;
  Time m_interval;//!< Packet inter-send time
  Address m_lispProtoAddress;
  EventId m_event;
  uint16_t m_peerPort; // Port of MS
  uint32_t m_seed;

  /// Callbacks for tracing the MapRegister Tx events
  TracedCallback<Ptr<const Packet> > m_mapRegisterTxTrace;
  /// Callbacks for tracing the MapNotify Rx events
  TracedCallback<Ptr<const Packet> > m_mapNotifyRxTrace;

};

} /* namespace ns3 */

#endif /* SRC_INTERNET_MODEL_LISP_CONTROL_PLANE_LISP_ETR_ITR_APPLICATION_H_ */
