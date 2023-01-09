/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2008 INRIA
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
#ifndef RedirectApplicationHelper_H
#define RedirectApplicationHelper_H

#include <stdint.h>
#include <string>
#include "ns3/object-factory.h"
#include "ns3/address.h"
#include "ns3/attribute.h"
#include "ns3/net-device.h"
#include "ns3/node-container.h"
#include "ns3/application-container.h"
#include "ns3/redirect-application-client.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
namespace ns3 {

class DataRate;

/**
 * \ingroup onoff
 * \brief A helper to make it easier to instantiate an ns3::OnOffApplication 
 * on a set of nodes.
 */
class RedirectApplicationClientHelper{
public:

  RedirectApplicationClientHelper (std::string protocol, Address address, Address local);
  void SetAttribute (std::string name, const AttributeValue &value);
  void SetConstantRate (DataRate dataRate, uint32_t packetSize = 512);

  ApplicationContainer Install (NodeContainer c) const;
  ApplicationContainer Install (Ptr<Node> node) const;
  ApplicationContainer Install (std::string nodeName) const;

private:

  Ptr<Application> InstallPriv (Ptr<Node> node) const;
  ObjectFactory m_factory; //!< Object factory.
};

class RedirectApplicationEntranceHelper{
public:

  RedirectApplicationEntranceHelper (Address address, Ptr<Ipv4Interface> mainAddress);
  void SetAttribute (std::string name, const AttributeValue &value);
  void SetConstantRate (DataRate dataRate, uint32_t packetSize = 512);

  ApplicationContainer Install (NodeContainer c) const;
  ApplicationContainer Install (Ptr<Node> node) const;
  ApplicationContainer Install (std::string nodeName) const;
private:

  Ptr<Application> InstallPriv (Ptr<Node> node) const;
  ObjectFactory m_factory; //!< Object factory.
};


class RedirectApplicationServerHelper{
public:

  RedirectApplicationServerHelper (Ptr<Ipv4Interface> mainInterface);
  void SetAttribute (std::string name, const AttributeValue &value);
  void SetChecking ();

  ApplicationContainer Install (NodeContainer c) const;
  ApplicationContainer Install (Ptr<Node> node) const;
  ApplicationContainer Install (std::string nodeName) const;
private:

  Ptr<Application> InstallPriv (Ptr<Node> node) const;
  ObjectFactory m_factory; //!< Object factory.
};

} // namespace ns3

#endif /* ON_OFF_HELPER_H */

