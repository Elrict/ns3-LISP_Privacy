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
#ifndef SRC_APPLICATIONS_MODEL_REDIRECT_APP_MSG_H_
#define SRC_APPLICATIONS_MODEL_REDIRECT_APP_MSG_H_

#include "ns3/ptr.h"
namespace ns3 {

class RedirectAppMsg {
public:
	static const RedirectAppMsg msgType;
	RedirectAppMsg();
	virtual
	~RedirectAppMsg();

    /* Get Set each attribute */

    /* Serialize / Deserialize*/ 
	void Serialize(uint8_t *buf);
	static Ptr<RedirectAppMsg> Deserialize(uint8_t *buf);

    enum RedirectAppMsgType {
        MAP_REQUEST = 0x01,
        MAP_REPLY = 0x02,
        MAP_REGISTER = 0x03,
        MAP_NOTIFY = 0x04,
        MAP_REFERRAL = 0x06,
        INFO_REQUEST = 0x07,
        MAP_ENCAP_CONTROL_MSG = 0x08,
    };
    /* attributes */
	
};

} /* namespace ns3 */

#endif 