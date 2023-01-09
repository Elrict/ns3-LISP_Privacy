#include "topology.hpp"
namespace ns3
{

  NS_LOG_COMPONENT_DEFINE("IPTopology");
  NS_OBJECT_ENSURE_REGISTERED(IPTopology);
  std::vector<double> IPTopology::m_connectionTimes;
  IPTopology::IPTopology()
  {
    this->linkHelper.SetDeviceAttribute("DataRate", StringValue("1000Mbps"));
    this->linkHelper.SetChannelAttribute("Delay", StringValue("5ms"));
    m_port = 50000;
    m_ipv4.SetBase("10.1.1.0", "255.255.255.0");
    
  }

  IPTopology::~IPTopology()
  {
  }
  void IPTopology::SetupAnim()
  {
    AnimationInterface *anim = new AnimationInterface("results/animation.xml");
    
    for (std::string n : this->hostNames)
    {
      anim->UpdateNodeDescription(this->nodesByName[n], n); // Optional
      anim->UpdateNodeColor(this->nodesByName[n], 255, 0, 0);
    }

    for (std::string n : this->routerNames)
    {
      anim->UpdateNodeDescription(this->nodesByName[n], n); // Optional
      anim->UpdateNodeColor(this->nodesByName[n], 120, 0, 0);
    }
    anim->EnablePacketMetadata(true);

    // Optional
  }
  void IPTopology::AddHost(std::string name, double x, double y)
  {
    this->hostNames.push_back(name);
    this->AddNode(name, x, y);
    return;
  }
  void IPTopology::AddRouter(std::string name, double x, double y)
  {
    this->routerNames.push_back(name);
    this->AddNode(name, x, y);
    return;
  }

  void IPTopology::AddNode(std::string name, double x, double y)
  {
    Ptr<IPNode> node = CreateObject<IPNode>();
    AnimationInterface::SetConstantPosition(node, x, y);
    this->nodesByName.insert({name, node});
    this->stack.Install(node);
    return ;
  }
  Ptr<IPNode> IPTopology::GetNode(std::string name)
  {
    auto iterator = this->nodesByName.find(name);
    if (iterator == this->nodesByName.cend())
    {
      throw new std::logic_error("Unknown node.");
      return nullptr;
    }
    return this->nodesByName[name];
  }

  void IPTopology::Connect(std::string nodeA, std::string nodeB, uint8_t nbAdrNodeA, uint8_t nbAdrNodeB, bool pcap)
  {
    NS_LOG_DEBUG("Connecting " << nodeA << " and " << nodeB);
    Ptr<IPNode> A = this->GetNode(nodeA);
    Ptr<IPNode> B = this->GetNode(nodeB);

    NodeContainer linkNodes = NodeContainer((Ptr<Node>)B, (Ptr<Node>)A);
    NetDeviceContainer networkDevices = this->linkHelper.Install(linkNodes);

    if (pcap)
      this->linkHelper.EnablePcap("results/"+nodeA, NodeContainer(A));

    Ipv4InterfaceContainer interfaces = m_ipv4.Assign(networkDevices);

    B->AddInterface(interfaces.Get(0).second, nodeA);
    for (size_t i = 1; i < nbAdrNodeB; i++)
    {
      B->AddAddress(nodeA, m_ipv4.NewAddress());
    }

    A->AddInterface(interfaces.Get(1).second, nodeB);
    for (size_t i = 1; i < nbAdrNodeA; i++)
    {
      A->AddAddress(nodeB, m_ipv4.NewAddress());
    }

    m_ipv4.NewNetwork();
  }
  void IPTopology::InstallNating(std::string routerN, std::string nodeN)
  {
    Ptr<IPNode> router = this->GetNode(routerN);
    // TODO: change
    Ptr<IPNode> node = this->GetNode(nodeN);
    Ptr<Ipv4Nat> nat = m_natHelper.Install(router);

    nat->SetInside(router->GetInterfaceIndex("Server"));
    nat->SetOutside(router->GetInterfaceIndex("router"));
    Ipv4StaticNatRule rule1(Ipv4Address("10.1.2.5"), Ipv4Address("10.1.2.2"));
    nat->AddStaticRule(rule1);

    Ipv4StaticNatRule rule2(router->GetAddress("router"), router->GetAddress("router"));
    nat->AddStaticRule(rule2);
    // nat->AddAddressPool(Ipv4Address("10.1.2.2"), Ipv4Mask("255.255.255.255"));
    // Ipv4DynamicNatRule rule(Ipv4Address("10.1.2.0"), Ipv4Mask("255.255.255.0"));
    // nat->AddDynamicRule(rule);
  }
  Ipv4Address IPTopology::GetTopAddress(Ptr<IPNode> node)
  {
    return node->GetAddress("xTRs") == "127.0.0.1" ? node->GetAddress("router") : node->GetAddress("xTRs");
  }
  void IPTopology::ReportConnection(int32_t id)
  {
    double time = Simulator::Now().GetMicroSeconds();
    m_data["Clients"][id]["ConnectionDelay"] = time - 1000000;
    m_connectionTimes.push_back(time - 1000000);
  }

  void IPTopology::InstallServerModule(std::string server, bool check)
  {
    RedirectApplicationServerHelper serverHelper(GetNode(server)->GetInterface("xTRs"));
    if (check)
      serverHelper.SetChecking();
    
    serverHelper.SetAttribute("Protocol", StringValue (m_protocol));
    ApplicationContainer serverApp = serverHelper.Install(GetNode(server));
    serverApp.Get(0)->TraceConnectWithoutContext("ConnectionEstablished", MakeCallback(&IPTopology::ReportConnection, this));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(200.0));
  }
  void IPTopology::InstallEntranceModule(std::string entrance, std::string server)
  {
    RedirectApplicationEntranceHelper receiver_helper(InetSocketAddress(GetTopAddress(GetNode(entrance)), m_port), GetNode(server)->GetInterface("xTRs"));
    receiver_helper.SetAttribute("Protocol", StringValue (m_protocol));
    ApplicationContainer recv_app = receiver_helper.Install(GetNode(entrance));
    recv_app.Start(Seconds(1.0));
    recv_app.Stop(Seconds(200.0));
  }
  void IPTopology::InstallTcpSender(std::string client, Ipv4Address source, Ipv4Address destination, double startTime)
  {
    RedirectApplicationClientHelper sender_helper(m_protocol, InetSocketAddress(destination, m_port), InetSocketAddress(source, 41000));
    sender_helper.SetAttribute("PacketSize", UintegerValue(10));
    sender_helper.SetAttribute("MaxBytes", UintegerValue(1000));
    ApplicationContainer sender_app = sender_helper.Install(GetNode(client));
    sender_app.Start(Seconds(1+startTime));
    sender_app.Stop(Seconds(200.0));
  }
}
