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

#include "nat-lcaf.h"

namespace ns3 {
NS_LOG_COMPONENT_DEFINE("NatLcaf");


NatLcaf::NatLcaf() {
	m_afi = 16387;
	m_rsvd1 = 0x00;
	m_rsvd2 = 0x00;
	m_flags = 0x00;
	m_type = 7;
	m_msUdpPort = LispOverIp::LISP_SIG_PORT;
}

NatLcaf::~NatLcaf() {

}

uint16_t 
NatLcaf::GetLength(void){
	return m_length;
}

void 
NatLcaf::SetMsUdpPort(uint16_t msUdpPort){
	m_msUdpPort = msUdpPort;
}
uint16_t 
NatLcaf::GetMsUdpPort(void){
	return m_msUdpPort;
}

void 
NatLcaf::SetEtrUdpPort(uint16_t etrUdpPort){
	m_etrUdpPort = etrUdpPort;
}
uint16_t 
NatLcaf::GetEtrUdpPort(void){
	return m_etrUdpPort;
}

void 
NatLcaf::SetGlobalEtrRlocAddress (Address address){
	m_globalEtrRlocAddress = address;
}
Address 
NatLcaf::GetGlobalEtrRlocAddress (void){
	return m_globalEtrRlocAddress;
}

void 
NatLcaf::SetMsRlocAddress (Address address){
	m_msRlocAddress = address;
}
Address 
NatLcaf::DetMsRlocAddress (void){
	return m_msRlocAddress;
}

void 
NatLcaf::SetPrivateEtrRlocAddress (Address address){
	m_privateEtrRlocAddress = address;
}
Address 
NatLcaf::GetPrivateEtrRlocAddress (void){
	return m_privateEtrRlocAddress;
}

void 
NatLcaf::SetRtrRlocAddress (Address address){
	m_rtrRlocAddress = address;
}
Address 
NatLcaf::GetRtrRlocAddress (void){
	return m_rtrRlocAddress;
}

void
NatLcaf::ComputeLength(void){
	// Computation of length depending on IPv4 or IPv6 addresses
	// Addresses
	std::vector<Address> addresses;
	addresses.push_back(m_globalEtrRlocAddress);
	addresses.push_back(m_msRlocAddress);
	addresses.push_back(m_privateEtrRlocAddress);
	addresses.push_back(m_rtrRlocAddress);

	uint16_t length = 0;
	for (std::vector<Address>::const_iterator it = addresses.begin(); it != addresses.end(); it++)
	{
		length += 2; //AFI
		if (Ipv4Address::IsMatchingType((*it)))
			length += 4;
		if (Ipv6Address::IsMatchingType((*it)))
			length += 16;
	}
	length += 4; //MS UDP Port Number and ETR UDP Port Number 
	m_length = length;
}


void NatLcaf::Serialize(uint8_t *buf) {
	uint8_t size = 0;

	// Afi
	int afi_size = 2;
	for (int i = 0; i < afi_size; i++) {
		buf[size + i] = (m_afi
				>> 8 * (afi_size - 1 - i)) & 0xff;
	}
	size += afi_size;

	// Rsvd1
	buf[size] = m_rsvd1;
	size +=1;
	//Flags
	buf[size] = m_flags;
	size+=1;
	// Type
	buf[size] = m_type;
	size+=1;
	// Rsvd2
	buf[size] = m_rsvd2;
	size+=1;

	int length_size = 2;
	// Length
	for (int i = 0; i < length_size; i++) {
		buf[size + i] = (m_length
				>> 8 * (length_size - 1 - i)) & 0xff;
	}
	size += length_size;

	//MS UDP port number
	int port_size = 2;
	for (int i = 0; i < port_size; i++) {
		buf[size + i] = (m_msUdpPort
				>> 8 * (port_size - 1 - i)) & 0xff;
	}
	size += port_size;
	//ETR UDP port number
	for (int i = 0; i < port_size; i++) {
		buf[size + i] = (m_etrUdpPort
				>> 8 * (port_size - 1 - i)) & 0xff;
	}
	size += port_size;

	// Addresses
	std::vector<Address> addresses;
	addresses.push_back(m_globalEtrRlocAddress);
	addresses.push_back(m_msRlocAddress);
	addresses.push_back(m_privateEtrRlocAddress);
	addresses.push_back(m_rtrRlocAddress);
	for (std::vector<Address>::const_iterator it = addresses.begin(); it != addresses.end(); it++)
	{
		if (Ipv4Address::IsMatchingType((*it))) {
			// We mainly focus IPv4 (01) and IPv6(02). EID-Prefix-AFI occupy two bytes.
			// So EID-Prefix-AFI occupyies tw
			buf[size] = 0x00;
			size += 1;
			buf[size] = static_cast<uint8_t>(LispControlMsg::IP);
			// Do not forget to increment variable size after put EID-Prefix-AFI into buffer
			size +=1;
			Ipv4Address::ConvertFrom((*it)).Serialize(buf + size);
			size += 4;
		} else if (Ipv6Address::IsMatchingType((*it))) {
			buf[size] = 0x00;
			size += 1;
			buf[size] = static_cast<uint8_t>(LispControlMsg::IPV6);
			size +=1;
			Ipv6Address::ConvertFrom((*it)).Serialize(buf + size);
			size += 16;
		}
	}
}

Ptr<NatLcaf> NatLcaf::Deserialize(uint8_t *buf) {

	//TODO
	Ptr<NatLcaf> record = Create<NatLcaf>();
	int size = 8;

	// Not set in NatLcaf because we consider it will always have expected values
	// AFI
	/*
	uint16_t afi = 0;
	int afi_size = 2;
	for (int i = 0; i < afi_size; i++) {
		afi <<= 8;
		afi |= buf[size + i];
	}
	size += afi_size;

	int rsvd1 = buf[size];
	size+=1;
	int flags = buf[size];
	size+=1;
	int type = buf[size];
	size+=1;
	int rsvd2 = buf[size];
	size+=1;

	// Length
	uint16_t length = 0; // Used to decode number of RLOCs
	int length_size = 2;
	for (int i = 0; i < length_size; i++) {
		length <<= 8;
		length |= buf[size + i];
	}
	size += afi_size;
	*/

	// MS UDP port Number
	uint16_t msUdpPort = 0;
	int msUdpPort_size = 2;
	for (int i = 0; i < msUdpPort_size; i++) {
		msUdpPort<<= 8;
		msUdpPort |= buf[size + i];
	}
	size += msUdpPort_size;
	record->SetMsUdpPort(msUdpPort);

	// ETR UDP port Number
	uint16_t etrUdpPort = 0;
	int etrUdpPort_size = 2;
	for (int i = 0; i < etrUdpPort_size; i++) {
		etrUdpPort<<= 8;
		etrUdpPort |= buf[size + i];
	}
	size += etrUdpPort_size;
	record->SetEtrUdpPort(etrUdpPort);

	std::vector<Address> addresses;

	for (int i=0; i< 4; i++) // Limitation: only decode one RTR RLOC
	{
		uint16_t prefix_afi = 0;
		prefix_afi |= buf[size];
		prefix_afi <<= 8;
		size += 1;
		prefix_afi |= buf[size];
		size += 1;

		if (prefix_afi == static_cast<uint16_t>(LispControlMsg::IP)) {
			addresses.push_back(static_cast<Address>(Ipv4Address::Deserialize(buf + size)));
			//NS_LOG_DEBUG("Decode EID Address: "<<Ipv4Address::ConvertFrom(static_cast<Address>(Ipv4Address::Deserialize(buf + size))));
			size += 4;
		} else if (prefix_afi == static_cast<uint16_t>(LispControlMsg::IPV6)) {
			addresses.push_back(
				static_cast<Address>(Ipv6Address::Deserialize(buf + size)));
			size += 16;
		}
		else{
			NS_LOG_ERROR("Unknown Address family Number:"<<prefix_afi);
		}

	}

	record->SetRtrRlocAddress(addresses[3]);
	record->SetPrivateEtrRlocAddress(addresses[2]);
	record->SetMsRlocAddress(addresses[1]);
	record->SetGlobalEtrRlocAddress(addresses[0]);
	
	return record;
}

/*
void MapReplyRecord::Print(std::ostream& os) {

	os.flush();
	os << "\nEid prefix afi " << unsigned(static_cast<int>(m_eidPrefixAfi))
			<< " " << "Mask Length " << unsigned(m_eidMaskLength);
	if (m_eidPrefixAfi == LispControlMsg::IP)
		os << "EID prefix " << Ipv4Address::ConvertFrom(m_eidPrefix) << " ";
	else if (m_eidPrefixAfi == LispControlMsg::IPV6)
		os << "EID prefix " << Ipv6Address::ConvertFrom(m_eidPrefix) << " ";
	if (m_locators)
		os << "Locators: " << m_locators->Print() << std::endl;
}*/
} /* namespace ns3 */
