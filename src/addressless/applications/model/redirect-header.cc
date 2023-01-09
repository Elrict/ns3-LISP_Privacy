/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2009 INRIA
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
 * Author: Mathieu Lacage <mathieu.lacage@sophia.inria.fr>
 */
// TODO: CHange author

#include "ns3/assert.h"
#include "ns3/log.h"
#include "ns3/header.h"
#include "ns3/simulator.h"
#include "redirect-header.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("RedirectHeader");

NS_OBJECT_ENSURE_REGISTERED (RedirectHeader);

RedirectHeader::RedirectHeader ()
  : m_redirect(false)
{
  NS_LOG_FUNCTION (this);
}

void
RedirectHeader::SetRedirect(bool redirectPacket)
{
  NS_LOG_FUNCTION (this << redirectPacket);
  m_redirect = redirectPacket;
}

bool 
RedirectHeader::GetRedirect (void) const
{

  NS_LOG_FUNCTION (this);
  return m_redirect;

}

TypeId
RedirectHeader::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::RedirectHeader")
    .SetParent<Header> ()
    .SetGroupName("Applications")
    .AddConstructor<RedirectHeader> ()
  ;
  return tid;
}

TypeId
RedirectHeader::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}
void
RedirectHeader::Print (std::ostream &os) const
{
  NS_LOG_FUNCTION (this << &os);
  os << "(redirect=" << m_redirect << ")";
}
uint32_t
RedirectHeader::GetSerializedSize (void) const
{
  NS_LOG_FUNCTION (this);
  return 1;
}

void
RedirectHeader::Serialize (Buffer::Iterator start) const
{
  NS_LOG_FUNCTION (this << &start);
  Buffer::Iterator i = start;
  i.WriteU8(m_redirect);
}
uint32_t
RedirectHeader::Deserialize (Buffer::Iterator start)
{
  NS_LOG_FUNCTION (this << &start);
  Buffer::Iterator i = start;
  m_redirect = i.ReadU8 ();
  return GetSerializedSize ();
}

} // namespace ns3
