#include "ns3/map-resolver-privacy-helper.h"
//#include "ns3/map-resolver-client.h"
#include "ns3/map-resolver-privacy-ddt.h"
#include "ns3/uinteger.h"
#include "ns3/names.h"
#include "ns3/lisp-protocol.h"
#include "ns3/lisp-over-ip.h"

namespace ns3
{

MapResolverPrivacyDdtHelper::MapResolverPrivacyDdtHelper ()
{
  m_factory.SetTypeId (MapResolverPrivacyDdt::GetTypeId ());
}

MapResolverPrivacyDdtHelper::~MapResolverPrivacyDdtHelper ()
{
}

void MapResolverPrivacyDdtHelper::SetAttribute (std::string name, const AttributeValue &value)
{
  m_factory.Set (name, value);
}

ApplicationContainer
MapResolverPrivacyDdtHelper::Install (Ptr<Node> node) const
{
  return ApplicationContainer (InstallPriv (node));
}

ApplicationContainer
MapResolverPrivacyDdtHelper::Install (std::string nodeName) const
{
  Ptr<Node> node = Names::Find<Node> (nodeName);
  return ApplicationContainer (InstallPriv (node));
}

ApplicationContainer
MapResolverPrivacyDdtHelper::Install (NodeContainer c) const
{
  ApplicationContainer apps;
    for (NodeContainer::Iterator i = c.Begin (); i != c.End (); ++i)
      {
        apps.Add (InstallPriv (*i));
      }

    return apps;
}

Ptr<Application> MapResolverPrivacyDdtHelper::InstallPriv (Ptr<Node> node) const
{
  Ptr<LispOverIp> lisp = node->GetObject<LispOverIp>();
  NS_ASSERT_MSG(lisp!=0, "a MR must have one LispOverIp object! It is a lisp-speaking device!");
  Ptr<MapResolverPrivacyDdt> app = m_factory.Create<MapResolverPrivacyDdt> ();
  app->SetMapServerAddress (m_mapServerAddress);
  node->AddApplication (app);
  return app;
}

void MapResolverPrivacyDdtHelper::AddDdtRootRloc (Ptr<Locator> locator)
{
  m_ddtRootRlocs.push_back (locator);
}

void MapResolverPrivacyDdtHelper::SetDdtRootRlocs (std::list<Ptr<Locator> > locators)
{
  m_ddtRootRlocs = locators;
}

void MapResolverPrivacyDdtHelper::SetMapServerAddress (Address mapServerAddress)
{
  m_mapServerAddress = mapServerAddress;
}

} /* namespace ns3 */
