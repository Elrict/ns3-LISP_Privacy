/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2019 University of Liege
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
 * Author: Emeline Marechal <emeline.marechal1@gmail.com>
 */
#ifndef SRC_INTERNET_MODEL_LISP_CONTROL_PLANE_NAT_LCAF_H_
#define SRC_INTERNET_MODEL_LISP_CONTROL_PLANE_NAT_LCAF_H_

#include "lisp-control-msg.h"
#include "ns3/lisp-over-ip.h"

namespace ns3 {

class NatLcaf: public SimpleRefCount<NatLcaf> 
{
public:

	NatLcaf();
	virtual
	~NatLcaf();

	uint16_t GetLength(void);

	void SetMsUdpPort(uint16_t msUdpPort);
	uint16_t GetMsUdpPort(void);

	void SetEtrUdpPort(uint16_t etrUdpPort);
	uint16_t GetEtrUdpPort(void);

	void SetGlobalEtrRlocAddress (Address address);
  Address GetGlobalEtrRlocAddress (void);

  void SetMsRlocAddress (Address address);
  Address DetMsRlocAddress (void);

  void SetPrivateEtrRlocAddress (Address address);
  Address GetPrivateEtrRlocAddress (void);

  void SetRtrRlocAddress (Address address);
  Address GetRtrRlocAddress (void);

  void ComputeLength(void);

	void Serialize(uint8_t *buf);
	static Ptr<NatLcaf> Deserialize(uint8_t *buf);

	static const uint16_t NAT_LCAF_LENGTH = 20; //Limitation (only one RLOC)

private:
	uint16_t m_afi; //Set to 16387
	uint8_t m_rsvd1; //these 8-bit fields are reserved for future use 
	uint8_t m_rsvd2; //and MUST be transmitted as 0 and ignored on receipt.
	uint8_t m_flags; //for future definition and use.  For now, set to zero on transmission and ignored on receipt.
	uint8_t m_type; // Type 7: NAT-Traversal
	uint16_t m_length; //Length in bytes starting and including the byte after this Length field.

	uint16_t m_msUdpPort; //Set to 4342
	uint16_t m_etrUdpPort; //Global translated port of NATed LISP device

	Address m_globalEtrRlocAddress; //Global translated address of NATed LISP device
	Address m_msRlocAddress; // Address of MS
	Address m_privateEtrRlocAddress; //Inserted by NATed LISP device (?)

	// Limitation: only one RTR RLOC can be transmitted. Other addresses will be ignored.
	Address m_rtrRlocAddress;

};

} /* namespace ns3 */

#endif /* SRC_INTERNET_MODEL_LISP_CONTROL_PLANE_NAT_LCAF_H_ */
