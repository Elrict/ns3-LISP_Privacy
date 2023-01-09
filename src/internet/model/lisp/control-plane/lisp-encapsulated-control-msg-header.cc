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
#include "lisp-encapsulated-control-msg-header.h"

namespace ns3
{
	NS_LOG_COMPONENT_DEFINE ("LispEncapsulatedControlMsgHeader");

	NS_OBJECT_ENSURE_REGISTERED (LispEncapsulatedControlMsgHeader);

  LispEncapsulatedControlMsgHeader::LispEncapsulatedControlMsgHeader (){}

  LispEncapsulatedControlMsgHeader::~LispEncapsulatedControlMsgHeader (){}

  TypeId LispEncapsulatedControlMsgHeader::GetTypeId (void)
	{
  	static TypeId tid = TypeId ("ns3::LispEncapsulatedControlMsgHeader")
    	.SetParent<Header> ()
    	.SetGroupName ("Lisp")
    	.AddConstructor<LispEncapsulatedControlMsgHeader> ();
  	return tid;
	}

	TypeId LispEncapsulatedControlMsgHeader::GetInstanceTypeId (void) const
	{
  	return GetTypeId ();
	}

  uint8_t 
	LispEncapsulatedControlMsgHeader::GetR(void) {
		return m_R;
	}

	void 
	LispEncapsulatedControlMsgHeader::SetR(uint8_t r) {
		m_R = r;
	}

	uint8_t 
	LispEncapsulatedControlMsgHeader::GetS(void) {
		return m_S;
	}

	void 
	LispEncapsulatedControlMsgHeader::SetS(uint8_t s) {
		m_S = s;
	}

	uint8_t 
	LispEncapsulatedControlMsgHeader::GetN(void) {
		return m_N;
	}

	void 
	LispEncapsulatedControlMsgHeader::SetN(uint8_t n) {
		m_N = n;
	}

	void LispEncapsulatedControlMsgHeader::Print (std::ostream& os) const
	{
  		os << "S " << unsigned(m_S) << " "
     << "R " << unsigned(m_R) << " "
     << "N " << unsigned(m_N) << " ";
	}

	uint32_t LispEncapsulatedControlMsgHeader::GetSerializedSize () const
	{
  	return 4; // 4 Bytes
	}

	/*
	+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |Type=8 |S|R|N|              Reserved                           |
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	*/
	void 
	LispEncapsulatedControlMsgHeader::Serialize(Buffer::Iterator start) const{
		Buffer::Iterator i = start;

		uint8_t type = static_cast<uint8_t>(LispControlMsg::MAP_ENCAP_CONTROL_MSG) << 4;
		uint8_t TSRN = (type) | (m_S << 3) | (m_R << 2) | (m_N << 1);
		i.WriteU8 (TSRN);
		
		i.WriteU8 (0x00); // Reserved
		i.WriteHtonU16 (0x00); //Reserved
	}

	uint32_t 
	LispEncapsulatedControlMsgHeader::Deserialize (Buffer::Iterator start){

		Buffer::Iterator i = start;
  	uint32_t TSRN = i.ReadU8 ();

  	m_S = TSRN >> 3;
  	m_R = TSRN >> 2;
  	m_N = TSRN >> 1;
  	
		return GetSerializedSize ();
	}

	LispControlMsg::LispControlMsgType LispEncapsulatedControlMsgHeader::GetMsgType(void) {
		return LispControlMsg::MAP_ENCAP_CONTROL_MSG;
	}

} /* namespace ns3 */
