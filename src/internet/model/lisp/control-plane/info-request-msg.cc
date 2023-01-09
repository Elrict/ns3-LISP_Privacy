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
#include "info-request-msg.h"

namespace ns3 {
NS_LOG_COMPONENT_DEFINE ("InfoRequestMsg");

InfoRequestMsg::InfoRequestMsg() {
	m_R = 0;
	reserved = 0;
	m_nonce = 0;
	m_ttl = 0;
	m_eidMaskLen = 0;
	m_eidPrefixAfi = LispControlMsg::IP;
	m_eidPrefix = static_cast<Address>(Ipv4Address());
	m_afi = 0;
}

InfoRequestMsg::~InfoRequestMsg() {
}

uint8_t 
InfoRequestMsg::GetR(void) {
	return m_R;
}

void 
InfoRequestMsg::SetR(uint8_t r) {
	m_R = r;
}

void
InfoRequestMsg::SetNonce(uint64_t nonce){
	m_nonce = nonce;
}

uint64_t 
InfoRequestMsg::GetNonce(void){
	return m_nonce;
}

uint16_t 
InfoRequestMsg::GetKeyId(){
	return m_keyID;
}

void 
InfoRequestMsg::SetKeyId(uint16_t keyId) {
	m_keyID = keyId;
}

void 
InfoRequestMsg::SetAuthDataLen(uint16_t authDataLen){
	m_authDataLen = authDataLen;
}

uint16_t 
InfoRequestMsg::GetAuthDataLen(void){
	return m_authDataLen;
}

uint32_t 
InfoRequestMsg::GetAuthData(){
	return m_authData;
}

void 
InfoRequestMsg::SetAuthData(uint32_t authData) {
	m_authData = authData;
}

void 
InfoRequestMsg::SetTtl (uint32_t ttl){
	m_ttl = ttl;
}

uint32_t 
InfoRequestMsg::GetTtl (void){
	return m_ttl;
}

void 
InfoRequestMsg::SetEidMaskLength(uint8_t eidMaskLength) {
	m_eidMaskLen = eidMaskLength;
}

uint8_t 
InfoRequestMsg::GetEidMaskLength(void) {
	return m_eidMaskLen;
}

void 
InfoRequestMsg::SetEidPrefixAfi (LispControlMsg::AddressFamily afi){
	m_eidPrefixAfi = afi;
}

LispControlMsg::AddressFamily 
InfoRequestMsg::GetEidPrefixAfi (void){
	return m_eidPrefixAfi;
}

void 
InfoRequestMsg::SetEidPrefix (Address eidPrefix){
	m_eidPrefix = eidPrefix;
	if (Ipv4Address::IsMatchingType(eidPrefix))
		m_eidPrefixAfi = LispControlMsg::IP;
	else
		m_eidPrefixAfi = LispControlMsg::IPV6;
}


Address 
InfoRequestMsg::GetEidPrefix (void){
	return m_eidPrefix;
}

void 
InfoRequestMsg::SetAfi (uint16_t afi){
	m_afi = afi;
}

uint16_t 
InfoRequestMsg::GetAfi (void){
	return m_afi;
}

void 
InfoRequestMsg::SetNatLcaf(Ptr<NatLcaf> natLcaf){
	m_natLcaf = natLcaf;
}

Ptr<NatLcaf> 
InfoRequestMsg::GetNatLcaf(void){
	return m_natLcaf;
}

void InfoRequestMsg::Serialize(uint8_t *buf) {
	//NS_LOG_FUNCTION(this);
	uint8_t type = static_cast<uint8_t>(LispControlMsg::INFO_REQUEST) << 4;
	buf[0] = (type) | (m_R << 3);
	buf[1] = 0x00; //reserved
	buf[2] = 0x00; //reserved
	buf[3] = 0x00; //reserved

	// nonce
	int nonce_size = 8;
	for(int i=0; i< nonce_size; i++){
		buf[4+i] = (m_nonce >> 8*(nonce_size-i-1)) & 0xffff;
	}

	// Key ID field
	buf[12] = (m_keyID >> 8 ) & 0xff;
	buf[13] = m_keyID & 0xff;

	// AuthDatalen
	int authen_len_size = 2;
	for (int i=0; i< authen_len_size;i++){
		buf[14+i] = (m_authDataLen >> 8*(authen_len_size - 1 -i)) & 0xff;
	}

	uint8_t size = 16;
	// Authentication data field
	buf[16] = 0xaa;
	buf[17] = 0xbb;
	buf[18] = 0xcc;
	buf[19] = 0xdd;

	size += m_authDataLen;

	// ttl
	int ttl_size = 4;
	for (int i = 0; i < ttl_size; i++) {
		buf[size + i] = (m_ttl
				>> 8 * (ttl_size - 1 - i)) & 0xff;
	}
	size += ttl_size;

	// reserved
	buf[size] = 0x00;
	size+=1;

	// eidMaskLength
	buf[size] = m_eidMaskLen;
	size+=1;

	// eidPrefixAfi and eidPrefix
	if (Ipv4Address::IsMatchingType(m_eidPrefix)) {
		// We mainly focus IPv4 (01) and IPv6(02). EID-Prefix-AFI occupy two bytes.
		// So EID-Prefix-AFI occupyies tw
		buf[size] = 0x00;
		size += 1;
		buf[size] = static_cast<uint8_t>(LispControlMsg::IP);
		// Do not forget to increment variable size after put EID-Prefix-AFI into buffer
		size +=1;
		Ipv4Address::ConvertFrom(m_eidPrefix).Serialize(buf + size);
		size += 4;
	} else if (Ipv6Address::IsMatchingType(m_eidPrefix)) {
		buf[size] = 0x00;
		size += 1;
		buf[size] = static_cast<uint8_t>(LispControlMsg::IPV6);
		size +=1;
		Ipv6Address::ConvertFrom(m_eidPrefix).Serialize(buf + size);
		size += 16;
	}

	if (m_natLcaf != 0){
		m_natLcaf->Serialize(buf + size);
	}
	else{
		// Afi
		int afi_size = 2;
		for (int i = 0; i < afi_size; i++) {
			buf[size + i] = (m_afi
				>> 8 * (afi_size - 1 - i)) & 0xff;
		}
		size += afi_size;

		//Nothing follows AFI
		buf[size] = 0x00;
		buf[size + 1] = 0x00;
	}

	

}

Ptr<InfoRequestMsg> 
InfoRequestMsg::Deserialize(uint8_t *buf) {

	Ptr<InfoRequestMsg> msg = Create<InfoRequestMsg>();

	// R
	msg->SetR((buf[0] >> 3) & 0x01);

	// Nonce
	uint64_t nonce = 0;
	int nonce_size = 8;

	for(int i=0; i< nonce_size; i++){
		nonce <<= 8;
		nonce |= buf[4+i];
	}
	msg->SetNonce(nonce);

	//Key ID
	uint8_t size = 12;
	int keyID_size = 2;
	uint16_t keyID = 0;
	for(int i=0; i< keyID_size; i++){
		keyID <<= 8;
		keyID |= buf[size+i];
	}
	msg->SetKeyId(keyID);
	size += keyID_size;

	// Authentication Data length
	int authDataLen_size = 2;
	uint16_t authDataLen = 0;
	for(int i=0; i< authDataLen_size; i++){
		authDataLen <<= 8;
		authDataLen |= buf[size+i];
	}
	msg->SetAuthDataLen(authDataLen);
	size += authDataLen_size;

	// Authentication Data
	uint32_t authData = 0;
	for(int i=0; i< authDataLen; i++){
		authData <<= 8;
		authData |= buf[size+i];
	}
	msg->SetAuthData(authData);
	size += authDataLen;

	//ttl
	int ttl_size = 4;
	uint32_t ttl = 0;
	for (int i = 0; i < ttl_size; i++) {
		ttl <<= 8;
		ttl |= buf[size + i];
	}
	msg->SetTtl(ttl);
	size += ttl_size;

	//eid mask length
	size += 1; // skip reserved field
	uint8_t eidMaskLen = 0;
	eidMaskLen |= buf[size];
	msg->SetEidMaskLength(eidMaskLen);
	size += 1;

	//eidPrefixAfi and eidPrefix
	// skip the first byte of EID-Prefix-AFI
	uint16_t eid_prefix_afi = 0;
	eid_prefix_afi |= buf[size];
	eid_prefix_afi <<= 8;
	size += 1;
	eid_prefix_afi |= buf[size];
	size += 1;
	if (eid_prefix_afi == static_cast<uint16_t>(LispControlMsg::IP)) {
		msg->SetEidPrefixAfi(LispControlMsg::IP);
		msg->SetEidPrefix(
				static_cast<Address>(Ipv4Address::Deserialize(buf + size)));
		//NS_LOG_DEBUG("Decode EID Address: "<<Ipv4Address::ConvertFrom(static_cast<Address>(Ipv4Address::Deserialize(buf + size))));
		size += 4;
	} else if (eid_prefix_afi == static_cast<uint16_t>(LispControlMsg::IPV6)) {
		msg->SetEidPrefixAfi(LispControlMsg::IPV6);
		msg->SetEidPrefix(
				static_cast<Address>(Ipv6Address::Deserialize(buf + size)));
		size += 16;
	}

	if (msg->GetR ()) {// InfoReply msg
		msg->SetNatLcaf(NatLcaf::Deserialize(buf + size));
	}
	else{
		//AFI
		int afi_size = 2;
		uint32_t afi = 0;
		for (int i = 0; i < afi_size; i++) {
			afi <<= 8;
			afi |= buf[size + i];
		}
		msg->SetAfi(afi);
		size += afi_size;
	}
	return msg;
}

LispControlMsg::LispControlMsgType InfoRequestMsg::GetMsgType(void) {
	return LispControlMsg::INFO_REQUEST;
}

} /* namespace ns3 */
