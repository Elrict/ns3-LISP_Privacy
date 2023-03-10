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
#ifndef LISP_OVER_IPV6_IMPL_H_
#define LISP_OVER_IPV6_IMPL_H_

#include "ns3/packet.h"
#include "ns3/ptr.h"
#include "lisp-over-ipv6.h"
#include "lisp-over-ipv4.h"
#include "lisp-protocol.h"

namespace ns3
{

class LispOverIpv6Impl : public LispOverIpv6
{
public:
  static TypeId GetTypeId (void);
  LispOverIpv6Impl ();
  virtual
  ~LispOverIpv6Impl ();

  Ptr<Packet>
  LispEncapsulate (Ptr<Packet> packet, uint16_t udpLength, uint16_t udpSrcPort, uint16_t udpDstPort);

  LispOverIpv4::MapStatus IsMapForEncapsulation (Ptr<const Packet> p, Ptr<MapEntry> srcMapEntry, Ptr<MapEntry> destMapEntry) const;
};

} /* namespace ns3 */

#endif /* LISP_OVER_IPV6_IMPL_H_ */
