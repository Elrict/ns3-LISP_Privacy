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
#ifndef SRC_INTERNET_MODEL_LISP_CONTROL_PLANE_MAP_SERVER_PRIVACY_DDT_H_
#define SRC_INTERNET_MODEL_LISP_CONTROL_PLANE_MAP_SERVER_PRIVACY_DDT_H_

#include "ns3/map-server.h"
#include "ns3/map-tables.h"
#include "ns3/lisp-control-msg.h"
#include "ns3/lisp-over-ip.h"
#include "ns3/map-request-msg.h"
#include "ns3/map-register-msg.h"
#include "ns3/info-request-msg.h"
#include "ns3/map-notify-msg.h"
#include "ns3/event-id.h"

namespace ns3
{

  class LispEtrItrApplication;

class MapServerPrivacyDdt : public MapServer
{
public:
  MapServerPrivacyDdt ();
  virtual
  ~MapServerPrivacyDdt ();

  static TypeId GetTypeId (void);

  /* This function returns a RandomVariableStream that models
   * the response time of the Mapping Distribution System
   */
  static Ptr<RandomVariableStream> GetMdsModel (void); 

  Ptr<MapTables> GetMapTablesV4();

  Ptr<MapTables> GetMapTablesV6();

  void SetMapTables (Ptr<MapTables> mapTablesV4, Ptr<MapTables> mapTablesV6);


private:
  virtual void StartApplication (void);

  virtual void StopApplication (void);

  void ForwardRequest(Address peerAddress, Ptr<Packet> p);

  virtual Ptr<MapNotifyMsg> GenerateMapNotifyMsg (Ptr<MapRegisterMsg> msg);

  Ipv4Address SelectDstRlocAddress(Ptr<LispControlMsg> msg);

  virtual void Send (Ptr<Packet> p);

  // Read responses on m_socket
  virtual void HandleRead (Ptr<Socket> socket);

  // Read Map register and Map request msg
  virtual void HandleReadFromClient (Ptr<Socket> socket);

  virtual void PopulateDatabase (Ptr<MapRegisterMsg> msg);

  virtual Ptr<MapReplyMsg> GenerateNegMapReply(Ptr<MapRequestMsg> requestMsg);
  virtual Ptr<InfoRequestMsg> GenerateInfoReplyMsg (Ptr<InfoRequestMsg> msg, uint16_t port, Address address, Address msAddress);

  Ptr<MapTables> m_mapTablesv4;
  Ptr<MapTables> m_mapTablesv6;

};


} /* namespace ns3 */

#endif /* SRC_INTERNET_MODEL_LISP_CONTROL_PLANE_MAP_SERVER_DDT_H_ */
