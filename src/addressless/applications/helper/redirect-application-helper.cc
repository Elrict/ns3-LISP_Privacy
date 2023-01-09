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
#include "ns3/on-off-helper.h"
#include "ns3/inet-socket-address.h"
#include "ns3/packet-socket-address.h"
#include "ns3/string.h"
#include "ns3/data-rate.h"
#include "ns3/uinteger.h"
#include "ns3/names.h"
#include "ns3/random-variable-stream.h"
#include "ns3/redirect-application-helper.h"

namespace ns3 {

RedirectApplicationClientHelper::RedirectApplicationClientHelper (std::string protocol, Address address, Address local)
{
  m_factory.SetTypeId ("ns3::RedirectApplicationClient");
  m_factory.Set ("Protocol", StringValue (protocol));
  m_factory.Set ("Remote", AddressValue (address));
  m_factory.Set ("Local", AddressValue (local));

}

void 
RedirectApplicationClientHelper::SetAttribute (std::string name, const AttributeValue &value)
{
  m_factory.Set (name, value);
}

ApplicationContainer
RedirectApplicationClientHelper::Install (Ptr<Node> node) const
{
  return ApplicationContainer (InstallPriv (node));
}

ApplicationContainer
RedirectApplicationClientHelper::Install (std::string nodeName) const
{
  Ptr<Node> node = Names::Find<Node> (nodeName);
  return ApplicationContainer (InstallPriv (node));
}

ApplicationContainer
RedirectApplicationClientHelper::Install (NodeContainer c) const
{
  ApplicationContainer apps;
  for (NodeContainer::Iterator i = c.Begin (); i != c.End (); ++i)
    {
      apps.Add (InstallPriv (*i));
    }

  return apps;
}

Ptr<Application>
RedirectApplicationClientHelper::InstallPriv (Ptr<Node> node) const
{
  Ptr<Application> app = m_factory.Create<Application> ();
  node->AddApplication (app);

  return app;
}
void 
RedirectApplicationClientHelper::SetConstantRate (DataRate dataRate, uint32_t packetSize)
{
  m_factory.Set ("PacketSize", UintegerValue (packetSize));
}

/* ============================================================== */
RedirectApplicationEntranceHelper::RedirectApplicationEntranceHelper (Address address, Ptr<Ipv4Interface> mainInterface)
{
  m_factory.SetTypeId ("ns3::RedirectApplicationEntrance");
  m_factory.Set ("Local", AddressValue (address));
  m_factory.Set("MainServer", PointerValue(mainInterface));
}

void 
RedirectApplicationEntranceHelper::SetAttribute (std::string name, const AttributeValue &value)
{
  m_factory.Set (name, value);
}

ApplicationContainer
RedirectApplicationEntranceHelper::Install (Ptr<Node> node) const
{
  return ApplicationContainer (InstallPriv (node));
}

ApplicationContainer
RedirectApplicationEntranceHelper::Install (std::string nodeName) const
{
  Ptr<Node> node = Names::Find<Node> (nodeName);
  return ApplicationContainer (InstallPriv (node));
}

ApplicationContainer
RedirectApplicationEntranceHelper::Install (NodeContainer c) const
{
  ApplicationContainer apps;
  for (NodeContainer::Iterator i = c.Begin (); i != c.End (); ++i)
    {
      apps.Add (InstallPriv (*i));
    }

  return apps;
}

Ptr<Application>
RedirectApplicationEntranceHelper::InstallPriv (Ptr<Node> node) const
{
  Ptr<Application> app = m_factory.Create<Application> ();
  node->AddApplication (app);

  return app;
}
void 
RedirectApplicationEntranceHelper::SetConstantRate (DataRate dataRate, uint32_t packetSize)
{
  m_factory.Set ("PacketSize", UintegerValue (packetSize));
}

/* ============================================================== */

RedirectApplicationServerHelper::RedirectApplicationServerHelper (Ptr<Ipv4Interface> interface)
{
  m_factory.SetTypeId ("ns3::RedirectApplicationServer");
  m_factory.Set ("Protocol", StringValue ("ns3::TcpSocketFactory"));
  m_factory.Set("Local", PointerValue(interface));

}
void 
RedirectApplicationServerHelper::SetAttribute (std::string name, const AttributeValue &value)
{
  m_factory.Set (name, value);
}

void 
RedirectApplicationServerHelper::SetChecking ()
{
  m_factory.Set ("CheckAddress", BooleanValue(true));
}

ApplicationContainer
RedirectApplicationServerHelper::Install (Ptr<Node> node) const
{
  return ApplicationContainer (InstallPriv (node));
}

ApplicationContainer
RedirectApplicationServerHelper::Install (std::string nodeName) const
{
  Ptr<Node> node = Names::Find<Node> (nodeName);
  return ApplicationContainer (InstallPriv (node));
}

ApplicationContainer
RedirectApplicationServerHelper::Install (NodeContainer c) const
{
  ApplicationContainer apps;
  for (NodeContainer::Iterator i = c.Begin (); i != c.End (); ++i)
    {
      apps.Add (InstallPriv (*i));
    }

  return apps;
}

Ptr<Application>
RedirectApplicationServerHelper::InstallPriv (Ptr<Node> node) const
{
  Ptr<Application> app = m_factory.Create<Application> ();
  node->AddApplication (app);

  return app;
}

} // namespace ns3
