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
#ifndef SRC_INTERNET_MODEL_LISP_CONTROL_PLANE_INFO_REQUEST_MSG_H_
#define SRC_INTERNET_MODEL_LISP_CONTROL_PLANE_INFO_REQUEST_MSG_H_

#include "lisp-control-msg.h"
#include "nat-lcaf.h"

namespace ns3 {

class InfoRequestMsg: public LispControlMsg {
public:
	static const LispControlMsgType msgType;
	InfoRequestMsg();
	virtual
	~InfoRequestMsg();

	uint8_t GetR(void);
	void SetR(uint8_t r);

	void SetNonce(uint64_t nonce);
	uint64_t GetNonce(void);

	void SetKeyId(uint16_t keyId);
	uint16_t GetKeyId(void);

	void SetAuthDataLen(uint16_t authDataLen);
	uint16_t GetAuthDataLen(void);

	void SetAuthData(uint32_t authData);
	uint32_t GetAuthData(void);

	void SetTtl (uint32_t ttl);
  uint32_t GetTtl (void);

  void SetEidMaskLength (uint8_t eidMaskLength);
  uint8_t GetEidMaskLength (void);

  void SetEidPrefixAfi (LispControlMsg::AddressFamily afi);
  LispControlMsg::AddressFamily GetEidPrefixAfi (void);

  void SetEidPrefix (Address eidPrefix);
  Address GetEidPrefix (void);

  void SetAfi (uint16_t afi);
  uint16_t GetAfi (void);

  void SetNatLcaf(Ptr<NatLcaf> natLcaf);
  Ptr<NatLcaf> GetNatLcaf(void);

	void Serialize(uint8_t *buf);
	static Ptr<InfoRequestMsg> Deserialize(uint8_t *buf);

	static LispControlMsg::LispControlMsgType GetMsgType(void);
	

	uint8_t m_R;
	uint32_t reserved; //This field MUST be set to 0 on transmit and MUST be ignored on receipt
	uint64_t m_nonce;
	//A configured ID to find the configured Message Authentication Code (MAC) algo and key value
	//used for the authentication function.
	uint16_t m_keyID;
	uint16_t m_authDataLen; //Authentication Data Length
	uint32_t m_authData;	//Authentication Data
	uint32_t m_ttl;
	uint8_t m_eidMaskLen;
	LispControlMsg::AddressFamily m_eidPrefixAfi;
	Address m_eidPrefix;
	uint16_t m_afi;

	Ptr<NatLcaf> m_natLcaf; // When this is an InfoReply msg
};

} /* namespace ns3 */

#endif /* SRC_INTERNET_MODEL_LISP_CONTROL_PLANE_INFO_REQUEST_MSG_H_ */
