#include "lisp-privacy-helper.h"
#include <iostream>
#include <string>
#include <fstream>
#include <algorithm>

#include "ns3/log.h"
#include "ns3/assert.h"
#include "ns3/fatal-error.h"
#include "ns3/fatal-impl.h"
#include "ns3/ipv4.h"
#include "ns3/ipv6.h"
#include "ns3/lisp-over-ipv4-impl.h"
#include "ns3/lisp-over-ipv4-impl-redir.h"
#include "ns3/node.h"
#include "ns3/node-container.h"
#include "ns3/object-factory.h"
#include "ns3/ptr.h"
#include "ns3/lisp-over-ipv6-impl.h"
#include "ns3/map-tables.h"
#include "ns3/simple-map-tables.h"
#include "ns3/lisp-protocol.h"

using std::cout;

namespace ns3
{

  NS_LOG_COMPONENT_DEFINE("LispPrivacyHelper");

  LispPrivacyHelper::LispPrivacyHelper(void)
  {
  }

  LispPrivacyHelper::~LispPrivacyHelper()
  {
    // TODO Auto-generated destructor stub
  }
  void LispPrivacyHelper::Install(Ptr<Node> node)
  {
    NS_LOG_FUNCTION(this);

    // neither ipv4 nor ipv6
    if (node->GetObject<Ipv4>() == 0 && node->GetObject<Ipv6>() == 0)
    {
      NS_FATAL_ERROR("LispPrivacyHelper::Install (): Aggregating LISP"
                     "to a node with no existing ipv6 or ipv4 object");
      return;
    }
    if (node->GetObject<Ipv4>() == 0)
    {
      NS_LOG_WARN("LispPrivacyHelper::Install (): Aggregating LISPv4"
                  "to a node with no existing ipv4 object");
    }
    else
    {
      NS_LOG_WARN("LispPrivacyHelper::Install: Aggregeting LISP ipv4");
      CreateAndAggregateLispVersion(node, LispOverIpv4ImplRedir::GetTypeId());
    }

    if (node->GetObject<Ipv6>() == 0)
    {
      NS_LOG_WARN("LispPrivacyHelper::Install (): Aggregating LISPv6"
                  "to a node with no existing ipv6 object");
    }
    else
    {
      CreateAndAggregateLispVersion(node, LispOverIpv6Impl::GetTypeId());
    }
    Ptr<LispOverIp> lisp = node->GetObject<LispOverIp>();
    lisp->SetRlocsList(m_rlocsList);

    /* PxTRs */
    lisp->SetPetrAddress(m_petrAddress);
    if (std::find(m_pitrs.begin(), m_pitrs.end(), node->GetId()) != m_pitrs.end())
    {
      lisp->SetPitr(true);
    }
    if (std::find(m_petrs.begin(), m_petrs.end(), node->GetId()) != m_petrs.end())
    {
      lisp->SetPetr(true);
    }
    /* RTRs */
    if (std::find(m_rtrs.begin(), m_rtrs.end(), node->GetId()) != m_rtrs.end())
    {
      lisp->SetRtr(true);
    }

    // now we can open the mapping socket
    lisp->OpenLispMappingSocket();
    /**
     * After trying and comparing, it is better to create mapTablesv4 and mapTablesv6
     * within lisp-helper. For normal xTRs, it makes no sense, because LispPrivacyHelper::InstallMapTables()
     * iterates the NodeContainer and assign the corresponding map Tables to lispOverIpv4. The two
     * smart pointers will be replace.
     * However, for LISP-MN, the following two instructions are important: because program
     * 's configuration XML file has no entries for LISP-MN. Thus, LISP-MN has no map tables,
     * which will lead to segmentation fault for packet copy-forwarding.
     * We make sure once lispOverIpv4 is created, mapTables are always accessible.
     */
    Ptr<MapTables> mapTablesv4 = Create<SimpleMapTables>();
    Ptr<MapTables> mapTablesv6 = Create<SimpleMapTables>();
    lisp->SetMapTablesIpv4(mapTablesv4);
    lisp->SetMapTablesIpv6(mapTablesv6);
    /**
     * IMPORTANT: DO NOT FORGET TO CREATE LispStatistics for lispOverIpv4 object.
     * Otherwise the statistics work in LispOverIpv4ImplRedir::LispOutput will encounter
     * segmentation fault (cause these two statistics-related object is not created!)
     *
     * At the first glance, you would say it is wrong to set a statistics object for ipv6,
     * for a ipv4 lisp. However, note that ipv4 and ipv4 can be used mixly. for example
     * inner header is ipv6, but the withdrew may be ipv4.
     */
    Ptr<LispStatistics> statisticsForV4 = Create<LispStatistics>();
    Ptr<LispStatistics> statisticsForV6 = Create<LispStatistics>();
    lisp->SetLispStatistics(statisticsForV4, statisticsForV6);
  }

  void
  LispPrivacyHelper::SetAttribute(std::string name, const AttributeValue &value)
  {
    m_ipv4factory.Set(name, value);
  }
  void LispPrivacyHelper::SetNatting(bool value)
  {
    SetAttribute("Nating", BooleanValue(value));
  }
  void LispPrivacyHelper::SetReverseNatting(bool value)
  {
    SetAttribute("ReverseNating", BooleanValue(value));
  }
  void LispPrivacyHelper::SetRlocInterface(Ptr<Ipv4Interface> iface)
  {
    SetAttribute("RlocInterface", PointerValue(iface));
  }
  void LispPrivacyHelper::SetRlocRedir(bool value)
  {
    SetAttribute("RlocRedir", BooleanValue(value));
  }
  void LispPrivacyHelper::SetRlocCheck(bool value)
  {
    SetAttribute("CheckRloc", BooleanValue(value));
  }
  void LispPrivacyHelper::SetEidCheck(bool value)
  {
    SetAttribute("CheckEid", BooleanValue(value));
  }
  void LispPrivacyHelper::SetServiceAddr(Ipv4Address addr)
  {
    SetAttribute("ServiceAddress", Ipv4AddressValue(addr));
  }
  void LispPrivacyHelper::SetServiceInterface(Ptr<Ipv4Interface> iface)
  {
    SetAttribute("ServiceInterface", PointerValue(iface));
  }

  void LispPrivacyHelper::CreateAndAggregateLispVersion(Ptr<Node> node, TypeId typeId)
  {
    NS_LOG_FUNCTION(this << typeId);
    Ptr<Object> lispProtocol;

    if (typeId == LispOverIpv6Impl::GetTypeId())
    {
      m_ipv6factory.SetTypeId(typeId);
      lispProtocol = m_ipv6factory.Create<Object>();
    }
    else
    {
      m_ipv4factory.SetTypeId(typeId);
      lispProtocol = m_ipv4factory.Create<Object>();
    }
    NS_ASSERT(typeId == LispOverIpv6Impl::GetTypeId() || typeId == LispOverIpv4ImplRedir::GetTypeId());

    node->AggregateObject(lispProtocol);
  }

} /* namespace ns3 */
