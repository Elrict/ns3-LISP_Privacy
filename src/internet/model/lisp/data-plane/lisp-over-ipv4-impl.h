/*
 * lisp-protocol.h
 *
 *  Created on: 28 janv. 2016
 *      Author: lionel
 */

#ifndef LISP_OVER_IPV4_IMPL_H_
#define LISP_OVER_IPV4_IMPL_H_

#include <ns3/node.h>
#include "ns3/packet.h"
#include "ns3/ptr.h"
#include "lisp-over-ipv4.h"
#include "lisp-protocol.h"
#include "map-tables.h"
#include "ns3/map-notify-msg.h"

namespace ns3
{

class LispOverIpv4Impl : public LispOverIpv4
{

public:
  static TypeId GetTypeId (void);
  LispOverIpv4Impl ();
  virtual ~LispOverIpv4Impl ();

  /**
   * LISP encapsulation.
   * If ecm is True, special case of ECM-encapsulated packet 
   */
  void LispOutput (Ptr<Packet> packet, Ipv4Header const &innerHeader,
                   Ptr<const MapEntry> localMapping,
                   Ptr<const MapEntry> remoteMapping,
                   Ptr<Ipv4Route> lispRoute,
                   LispOverIp::EcmEncapsulation ecm);

  /**
   * if lisp is true, we have a LISP data header inside packet.
   * Otherwise, we have an ECM header inside packet.
   */
  void LispInput (Ptr<Packet> packet, Ipv4Header const &outerHeader, bool lisp);

  void BufferPacket(Ptr<Packet> packet,
                       Ipv4Address source,
                       Ipv4Address destination,
                       uint8_t protocol,
                       Ptr<Ipv4Route> route);

  LispOverIpv4::MapStatus IsMapForEncapsulation (Ipv4Header const &innerHeader, Ptr<MapEntry> &srcMapEntry, Ptr<MapEntry> &destMapEntry, Ipv4Mask mask);

  /**
   *
   */
  bool NeedEncapsulation (Ipv4Header const &ipHeader, Ipv4Mask mask);

  /**
   * Accepts all Data packets.
   * Accepts Control Packets that are ECM encapsulated.
   */
  bool NeedDecapsulation (Ptr<const Packet> packet, Ipv4Header const &ipHeader, uint16_t lispPort);

  /**
   * Check in database and in cache for whom is the MapNotify destined
   */
  bool IsMapNotifyForNatedXtr(Ptr<Packet> packet, Ipv4Header const &ipHeader, Ptr<MapEntry>  &mapEntry);

  /**
   * Check in database and in cache for whom is the MapRequest destined
   */
  bool IsMapRequestForNatedXtr(Ptr<Packet> packet, Ipv4Header const &ipHeader, Ptr<MapEntry>  &mapEntry);


  /**
   *
   */
  Ptr<Packet> LispEncapsulate (Ptr<Packet> packet, uint16_t udpLength,
                        uint16_t udpSrcPort, uint16_t udpDstPort);

  /* Changes the ITR RLOC inside a MapRegisterMsg to 'address'
   */
  void ChangeItrRloc (Ptr<Packet> &packet, Address address);

  /**
   * Checks whether inner msg is a MapRegister
   */
  bool IsMapRegister (Ptr<Packet> packet);

  /**
   * Method for use only by RTRs.
   * It records information about the NATed prefix:
   *  - In cache: to be able to forward packets through NAT to the destination xTR
   *  - In database: to be able to answer MapRequests in name of the NATed xTR.
   */
  void SetNatedEntry (Ptr<Packet> packet, Ipv4Header const &outerHeader);

};

} /* namespace ns3 */

#endif /*LISP_OVER_IPV4_IMPL_H_ */
