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
#ifndef SRC_INTERNET_MODEL_LISP_CONTROL_PLANE_LISP_ENCAPSULATED_CONTROL_MSG_HEADER_H_
#define SRC_INTERNET_MODEL_LISP_CONTROL_PLANE_LISP_ENCAPSULATED_CONTROL_MSG_HEADER_H_

#include "ns3/lisp-control-msg.h"

namespace ns3
{

// for map-requests
// Also for Registration of ETR when ETR is NATed
class LispEncapsulatedControlMsgHeader : public Header
{
public:

  /**
   * \brief Get the type identifier.
   * \return type identifier
   */
  static TypeId GetTypeId (void);

  /**
   * \brief Return the instance type identifier.
   * \return instance type ID
   */
  virtual TypeId GetInstanceTypeId (void) const;

  static const LispControlMsg::LispControlMsgType msgType;
  LispEncapsulatedControlMsgHeader ();
  virtual
  ~LispEncapsulatedControlMsgHeader ();

  uint8_t GetR(void);
	void SetR(uint8_t p);

	uint8_t GetS(void);
	void SetS(uint8_t s);

	uint8_t GetN(void);
	void SetN(uint8_t n);

  /**
   * \brief Print some informations about the LISP header.
   * \param os output stream
   * \return info about this LISP header
   */
  virtual void Print (std::ostream& os) const;

  uint32_t GetSerializedSize () const;
	/**
   * \brief Serialize the header.
   * \param start Buffer iterator
   */
  virtual void Serialize (Buffer::Iterator start) const;

  /**
   * \brief Deserialize the header.
   * \param start Buffer iterator
   * \return size of the header
   */
  virtual uint32_t Deserialize (Buffer::Iterator start);

	static LispControlMsg::LispControlMsgType GetMsgType(void);

private:
  uint8_t m_S;
  //uint8_t m_D;  ?

  /* The 6th bit in the ECM LISP header is allocated as the "R" bit.
   The R bit indicates that the encapsulated Map-Register is to be
   processed by an RTR (draft NAT traversal) */
  uint8_t m_R; 
  /*The 7th bit in the ECM header is allocated as
   the "N" bit.  The N bit indicates that this Map-Register is being relayed by an RTR.
   When an RTR relays the ECM-ed Map-Register to a Map-Server, the N bit must be set to 1.
   (draft NAT traversal) */
  uint8_t m_N;
  uint32_t m_reserved;
};

} /* namespace ns3 */

#endif /* SRC_INTERNET_MODEL_LISP_CONTROL_PLANE_LISP_ENCAPSULATED_CONTROL_MSG_HEADER_H_ */
