#include "topology.hpp"
#include "ns3/simulation.hpp"
using json = nlohmann::json;
namespace ns3
{
  NS_LOG_COMPONENT_DEFINE("LISPTopology");
  NS_OBJECT_ENSURE_REGISTERED(LISPTopology);
  json IPTopology::m_data;

  LISPTopology::LISPTopology(/* args */) : IPTopology()
  {
  }

  LISPTopology::~LISPTopology() {}

  void LISPTopology::SetTiming()
  {
    m_mapServerHelper.SetAttribute("SearchTimeVariable", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));
    m_mapResolverHelper.SetAttribute("SearchTimeVariable", StringValue("ns3::ConstantRandomVariable[Constant=0.1]"));
    m_lispPrivacyXtrHelper.SetAttribute("HashVariable", StringValue("ns3::ConstantRandomVariable[Constant=0.00004]"));
    m_lispPrivacyXtrHelper.SetAttribute("ProxyMapReply", BooleanValue(true));
   // data plane lisp no need to hash/encrypt delay becase control plane can save the value
    // hash variable server
    // co change time client
    // m_lispHelper.SetAttribute("HashVariable", StringValue("ns3::ConstantRandomVariable[Constant=0.0015]"));
  }
  void LISPTopology::AddNode(std::string name, double x, double y)
  {
    Ptr<LISPNode> node = CreateObject<LISPNode>(this);
    AnimationInterface::SetConstantPosition(node, x, y);
    this->nodesByName.insert({name, node});
    this->stack.Install(node);
  }

  void LISPTopology::SetMapServer(std::string name, unsigned int interfaceIndex)
  {
    Ptr<LISPNode> node = DynamicCast<LISPNode, IPNode>(this->GetNode(name));
    node->SetMapTables();
    this->SetLispCapable(node);
    m_lispPrivacyXtrHelper.AddMapServerAddress(node->GetAddress("router"));
    m_mapResolverHelper.SetMapServerAddress(node->GetAddress("router"));
    ApplicationContainer application = m_mapServerHelper.Install(node);
    application.Start(Seconds(0.0));
    application.Stop(Seconds(200.0));
  }
  void LISPTopology::SetMapResolver(std::string name, unsigned int interfaceIndex)
  {
    Ptr<IPNode> node = this->GetNode(name);
    SetLispCapable(node);

    Ptr<Locator> rloc = Create<Locator>(node->GetAddress("router"));
    m_lispPrivacyXtrHelper.AddMapResolverRlocs(rloc);

    ApplicationContainer application = m_mapResolverHelper.Install(node);
    application.Start(Seconds(0.0));
    application.Stop(Seconds(200.0));
  }

  void LISPTopology::AddRlocs(std::string lispNode, std::string destNode)
  {
    Ptr<IPNode> lispn = this->GetNode(lispNode);
    Ptr<Ipv4Interface> toInterface = lispn->GetInterface(destNode);
    for (size_t i = 0; i < toInterface->GetNAddresses(); i++)
    {
      m_lispHelper.AddRlocToSet(toInterface->GetAddress(i).GetLocal());
    }
    DynamicCast<LISPNode, IPNode>(lispn)->SetRlocInterface(toInterface);
  }
  void LISPTopology::SetLispCapable(Ptr<IPNode> node)
  {
    m_lispHelper.Install(node);
    m_lispHelper.InstallMapTables(NodeContainer((Ptr<Node>)(node)));
  }

  void LISPTopology::ReportMapDelay(int id, double time)
  {
    int x = (id - m_clients) - 2 >= 0 ? (id - m_clients) - 2 : 0;
    m_data["Clients"][x]["MapDelay"].push_back(time);
  }

  void LISPTopology::SetXtr(std::string name)
  {
    Ptr<LISPNode> node = DynamicCast<LISPNode, IPNode>(this->GetNode(name));
    node->SetMapTables();
    SetLispCapable(node);

    ApplicationContainer application;
    m_lispPrivacyXtrHelper.SetAttribute("xTRId", UintegerValue(node->GetId()));
    application = m_lispPrivacyXtrHelper.Install(node);
    Callback<void, int, double> c = MakeCallback(&LISPTopology::ReportMapDelay, this);
    application.Get(0)->TraceConnectWithoutContext("MappingDelay", c);
    application.Start(Seconds(0.0));
    application.Stop(Seconds(200.0));
  }

}