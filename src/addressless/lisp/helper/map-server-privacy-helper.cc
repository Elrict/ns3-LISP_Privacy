#include "map-server-privacy-helper.h"
#include "ns3/map-server-privacy-ddt.h"
#include "ns3/uinteger.h"
#include "ns3/names.h"
#include "ns3/lisp-protocol.h"
#include "ns3/lisp-over-ip.h"

namespace ns3
{

MapServerPrivacyDdtHelper::MapServerPrivacyDdtHelper ()
{
  m_factory.SetTypeId (MapServerPrivacyDdt::GetTypeId ());
}

  MapServerPrivacyDdtHelper::~MapServerPrivacyDdtHelper ()
{

}

void
MapServerPrivacyDdtHelper::SetAttribute (std::string name, const AttributeValue &value)
{
  m_factory.Set (name, value);
}

ApplicationContainer
MapServerPrivacyDdtHelper::Install (Ptr<Node> node) const
{
  /**
   * We should make sure that map server always has a lispoveripv4 or lispoveripv6 object
   * Because MapServerPrivacyDdt should be a lisp-speaking equipment. For example, in LISP-MN
   * map server need to send encapsulated control message.
   */
  NS_ASSERT(node->GetObject<LispOverIp>() != 0);
  return ApplicationContainer (InstallPriv (node));
}

ApplicationContainer
MapServerPrivacyDdtHelper::Install (std::string nodeName) const
{
  Ptr<Node> node = Names::Find<Node> (nodeName);
  NS_ASSERT(node->GetObject<LispOverIp>() != 0);
  return ApplicationContainer (InstallPriv (node));
}

ApplicationContainer
MapServerPrivacyDdtHelper::Install (NodeContainer c) const
{
  ApplicationContainer apps;
  for (NodeContainer::Iterator i = c.Begin (); i != c.End (); ++i)
    {
      apps.Add (InstallPriv (*i));
    }

  return apps;
}

void 
MapServerPrivacyDdtHelper::SetRtrAddress(Address rtr){
  m_rtrAddress = rtr;
}

Address 
MapServerPrivacyDdtHelper::GetRtrAddress (void){
  return m_rtrAddress;
}

Ptr<Application> MapServerPrivacyDdtHelper::InstallPriv (Ptr<Node> node) const
{
  Ptr<LispOverIp> lisp = node->GetObject<LispOverIp>();
  NS_ASSERT_MSG(lisp!=0, "a MR must have one LispOverIp object! It is a lisp-speaking device!");
  Ptr<MapServerPrivacyDdt> app = m_factory.Create<MapServerPrivacyDdt> ();
  app->SetRtrAddress(m_rtrAddress);
  node->AddApplication (app);
  // We tell MapResolverDdt the pointer to MapTables saved in lispOverIp
  app->SetMapTables(lisp->GetMapTablesV4(), lisp->GetMapTablesV6());
  return app;
}

} /* namespace ns3 */
