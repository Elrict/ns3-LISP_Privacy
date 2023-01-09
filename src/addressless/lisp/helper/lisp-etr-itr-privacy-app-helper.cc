#include "ns3/lisp-etr-itr-privacy-app-helper.h"

namespace ns3
{
    NS_LOG_COMPONENT_DEFINE("LispEtrItrPrivacyAppHelper");

    LispEtrItrPrivacyAppHelper::LispEtrItrPrivacyAppHelper()
    {
        m_factory.SetTypeId(LispEtrItrPrivacyApplication::GetTypeId());
    }
    LispEtrItrPrivacyAppHelper::~LispEtrItrPrivacyAppHelper()
    {
        // TODO Auto-generated destructor stub
    }

    void
    LispEtrItrPrivacyAppHelper::SetAttribute(std::string name, const AttributeValue &value)
    {
        m_factory.Set(name, value);
    }

    ApplicationContainer LispEtrItrPrivacyAppHelper::Install(Ptr<Node> node) const
    {
        return ApplicationContainer(InstallPriv(node));
    }

  void LispEtrItrPrivacyAppHelper::SetServer(Ptr<Ipv4Interface> servAddr)
    {
        SetAttribute("ServerInterface", PointerValue(servAddr));
    }
    void LispEtrItrPrivacyAppHelper::SetInterface(Ptr<Ipv4Interface> iface)
    {
        SetAttribute("RlocInterface", PointerValue(iface));
    }
        
    void LispEtrItrPrivacyAppHelper::SetFastRedir(bool value)
    {
        SetAttribute("FastRedir", BooleanValue(value));
    }
    void LispEtrItrPrivacyAppHelper::SetRlocRedir(bool value)
    {
        SetAttribute("RlocRedir", BooleanValue(value));
    }
    Ptr<Application> LispEtrItrPrivacyAppHelper::InstallPriv(Ptr<Node> node) const
    {
        Ptr<LispEtrItrPrivacyApplication> app = m_factory.Create<LispEtrItrPrivacyApplication>();
    
        Ptr<LispOverIp> lisp = node->GetObject<LispOverIp>();
        for (std::list<Ptr<Locator>>::const_iterator it = m_mapResolverRlocs.begin(); it != m_mapResolverRlocs.end(); it++)
        {
            app->AddMapResolverLoc(*it);
        }
        app->SetMapTables(lisp->GetMapTablesV4(), lisp->GetMapTablesV6());
        lisp->GetMapTablesV4()->SetxTRApp(app);
        NS_LOG_DEBUG("xTR address set to simple map table:" << lisp->GetMapTablesV4()->GetxTRApp() << " It should be:" << app);
        lisp->GetMapTablesV6()->SetxTRApp(app);
        app->SetMapServerAddresses(m_mapServerAddresses);
        node->AddApplication(app);
        return app;
    }
    void LispEtrItrPrivacyAppHelper::AddMapServerAddress(Address mapServerAddress)
    {
        m_mapServerAddresses.push_front(mapServerAddress);
    }

    void LispEtrItrPrivacyAppHelper::AddMapResolverRlocs(Ptr<Locator> locator)
    {
        m_mapResolverRlocs.push_back(locator);
    }

}