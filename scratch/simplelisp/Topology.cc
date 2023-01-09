#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/lisp-helper.h"
#include <iostream>

#include "ns3/netanim-module.h"
using namespace ns3;

class Topology : public Object
{
public:
  class TopologyNode : public Object
  {
  protected:
    Topology* topology;
    Ptr<Node> node;
    std::string name;
    std::vector<NetDeviceContainer> linksDevices;

  public:
    TopologyNode (std::string _name, Topology* _topology)
    {
      this->node = CreateObject<Node>();
      this->topology = _topology;
      this->name = _name;
      this->topology->AddNode (this);
    }

    virtual ~TopologyNode ()
    {
      this->topology = nullptr;
    }

    virtual Ptr<TopologyNode> ConnectTo (Ptr<TopologyNode> nodeTo)
    {
      NodeContainer linkNodes = NodeContainer (this->node, nodeTo->node);
      NetDeviceContainer networkDevices = topology->linkHelper.Install (linkNodes);
      this->linksDevices.push_back (networkDevices);
      nodeTo->linksDevices.push_back (networkDevices);
      return this;
    }

    Ptr<Node> GetNode ()
    {
      return node;
    }
    std::string GetName ()
    {
      return name;
    }
  };

protected:
  NodeContainer nodes;
  std::map<std::string, Ptr<TopologyNode> > nodesByName;
  PointToPointHelper linkHelper;
  std::string pcapFilesPrefix = "";

public:
  Topology ()
  {
    this->SetupLinks ("100Mbps", "10ms");
  }

  void SetupLinks (std::string dataRate, std::string delay)
  {
    this->linkHelper.SetDeviceAttribute ("DataRate", StringValue (dataRate));
    this->linkHelper.SetChannelAttribute ("Delay", StringValue (delay));
  }

  Ptr<TopologyNode> NewNode (std::string name, double x, double y)
  {
    Ptr<TopologyNode> node = Create<TopologyNode> (name, this);
    return node;
  }

  Ptr<TopologyNode> GetNode (std::string name)
  {
    auto iterator = this->nodesByName.find (name);
    if (iterator == this->nodesByName.cend ())
      {
        throw new std::logic_error ("Unknown node.");
      }
    return (*iterator).second;
  }

  void SetPcapFilesPrefix (std::string prefix)
  {
    this->pcapFilesPrefix = prefix;
  }

  void EnableAllPcapFiles ()
  {
    this->linkHelper.EnablePcapAll (this->pcapFilesPrefix);
  }

private:
  virtual void AddNode (Ptr<TopologyNode> node)
  {
    this->nodes.Add (node->GetNode ());
    this->nodesByName.insert ({node->GetName (), node});
    return;
  }
};

class Ipv4Topology : public Topology
{
public:
  class Ipv4TopologyNode : public Topology::TopologyNode
  {
  protected:
    std::vector<Ipv4Address> ipv4Address;
    Ipv4Topology* ipv4Topology;

  public:
    Ipv4TopologyNode (std::string _name, Ipv4Topology* _topology) : TopologyNode (_name, _topology)
    {
      this->ipv4Topology = _topology;
      _topology->internetStackHelper.Install (this->node);
    }

    virtual ~Ipv4TopologyNode ()
    {
      this->ipv4Topology = nullptr;
    }

    // Not thread safe
    virtual Ptr<TopologyNode> ConnectTo (Ptr<TopologyNode> nodeTo)
    {
      this->TopologyNode::ConnectTo (nodeTo);
      Ptr<Ipv4TopologyNode> ipv4NodeTo = DynamicCast<Ipv4TopologyNode, TopologyNode> (nodeTo);
      if (ipv4NodeTo == nullptr)
        {
          return this;
        }
      NetDeviceContainer linkDevices = *(this->linksDevices.rbegin ());
      Ipv4InterfaceContainer interfaces = this->ipv4Topology->assignIPs (linkDevices);
      this->ipv4Address.push_back (interfaces.GetAddress (0));
      ipv4NodeTo->ipv4Address.push_back (interfaces.GetAddress (1));
      return this;
    }

    Ipv4Address GetAddress (unsigned int interfaceIndex = 0)
    {
      return this->ipv4Address.at (interfaceIndex);
    }
  };

protected:
  InternetStackHelper internetStackHelper;
  Ipv4AddressHelper ipv4AddressHelper;

public:
  Ipv4Topology () : Topology ()
  {
    this->SetupIPAddressBase ("10.0.0.0", "255.255.255.0");
  }

  Ipv4InterfaceContainer assignIPs (NetDeviceContainer devices, bool incrementNetwork = true)
  {
    Ipv4InterfaceContainer interfaces = this->ipv4AddressHelper.Assign (devices);
    if (incrementNetwork)
      {
        this->ipv4AddressHelper.NewNetwork ();
      }
    return interfaces;
  }

  void SetupIPAddressBase (const char* network, const char* mask, const char* base = "0.0.0.1")
  {
    this->ipv4AddressHelper.SetBase (network, mask, base);
  }

  Ptr<Ipv4TopologyNode> NewNode (std::string name, double x, double y)
  {
    Ptr<Ipv4TopologyNode> node = Create<Ipv4TopologyNode> (name, this);
    return node;
  }

  void PopulateRoutingTables ()
  {
    Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
  }

  Ptr<Ipv4TopologyNode> GetNode (std::string name)
  {
    auto iterator = this->nodesByName.find (name);
    if (iterator == this->nodesByName.cend ())
      {
        throw new std::logic_error ("Unknown node.");
      }
    return DynamicCast<Ipv4TopologyNode, TopologyNode> ((*iterator).second);
  }
};

#define IS_RLOC true
#define IS_EID false

class LispTopology : public Ipv4Topology
{
public:
  class LispTopologyNode : public Ipv4Topology::Ipv4TopologyNode
  {
  protected:
    LispTopology* lispTopology;
    ApplicationContainer application;
    Address rloc;
    Ptr<SimpleMapTables> ipv4MapTables;
    Ptr<SimpleMapTables> ipv6MapTables;

  public:
    LispTopologyNode (std::string _name, LispTopology* _topology) : Ipv4TopologyNode (_name, _topology)
    {
      this->lispTopology = _topology;
    }

    virtual ~LispTopologyNode ()
    {
      this->lispTopology = nullptr;
    }

    // Not thread safe
    virtual Ptr<TopologyNode> ConnectTo (Ptr<TopologyNode> nodeTo)
    {
      this->ConnectTo (nodeTo, IS_EID);
      return this;
    }

    // done
    Ptr<TopologyNode> ConnectTo (Ptr<TopologyNode> nodeTo, bool is_rlocs)
    {
      this->Ipv4TopologyNode::ConnectTo (nodeTo);
      Ptr<LispTopologyNode> lispNodeTo = DynamicCast<LispTopologyNode, TopologyNode> (nodeTo);
      if (lispNodeTo == nullptr)
        {
          return this;
        }
      if (is_rlocs)
        {
          Ipv4Address thisAddress = *(this->ipv4Address.rbegin ());
          Ipv4Address nodeToAddress = *(lispNodeTo->ipv4Address.rbegin ());
          this->lispTopology->lispHelper.AddRlocToSet (thisAddress);
          this->rloc = thisAddress;
          this->lispTopology->lispHelper.AddRlocToSet (nodeToAddress);
          lispNodeTo->rloc = nodeToAddress;
        }
      return this;
    }
    // Done
    void SetMapServer (Time start, Time stop, unsigned int interfaceIndex = 0)
    {
      this->SetMapTables ();
      this->SetLispCapable ();

      Address address = this->ipv4Address.at (interfaceIndex);
      this->lispTopology->lispXtrHelper.AddMapServerAddress (address);
      this->lispTopology->mapResolverHelper.SetMapServerAddress (address);

      this->application = this->lispTopology->mapServerHelper.Install (this->node);
      this->application.Start (start);
      this->application.Stop (stop);
    }
    // Done
    void SetMapResolver (Time start, Time stop, unsigned int interfaceIndex = 0)
    {
      this->SetLispCapable ();

      Address address = this->ipv4Address.at (interfaceIndex);
      Ptr<Locator> rloc = Create<Locator> (address);
      this->lispTopology->lispXtrHelper.AddMapResolverRlocs (rloc);

      this->application = this->lispTopology->mapResolverHelper.Install (this->node);
      this->application.Start (start);
      this->application.Stop (stop);
    }
    // DONE
    void SetXtr (Time start, Time stop)
    {
      this->SetMapTables ();
      this->SetLispCapable ();
      this->application = this->lispTopology->lispXtrHelper.Install (this->node);
      this->application.Start (start);
      this->application.Stop (stop);
    }
    // Done
    void AddEntryToMapTables (Ipv4Address eidAddress)
    {
      this->InitializeMapTables ();

      Ipv4Mask eidMask = Ipv4Mask ("255.255.255.0");
      auto location = MapTables::IN_DATABASE;

      Ipv4Address rlocAddress = Ipv4Address::ConvertFrom (this->rloc);
      uint8_t priority = 200;
      uint8_t weight = 30;
      bool reachability = true;
      if (Ipv4Address::IsMatchingType (eidAddress))
        {
          ipv4MapTables->InsertLocator (eidAddress, eidMask, rlocAddress, priority, weight,
                                        location, reachability);
        }
      else if (Ipv6Address::IsMatchingType (eidAddress))
        {
          ipv6MapTables->InsertLocator (eidAddress, eidMask, rlocAddress, priority, weight,
                                        location, reachability);
        }
    }

private:
    // Done
    void SetLispCapable ()
    {
      this->lispTopology->lispHelper.Install (this->node);
      this->lispTopology->lispHelper.InstallMapTables (this->node);
    }
    // Done
    void SetMapTables ()
    {
      for (auto iter = this->ipv4Address.cbegin (); iter < this->ipv4Address.cend (); iter++)
        {
          Ipv4Address eidAddress = *iter;
          this->AddEntryToMapTables (eidAddress);
        }
      
     

      this->lispTopology->lispHelper.SetMapTablesForEtr (this->rloc, this->ipv4MapTables, this->ipv6MapTables);
    }
    // Done
    void InitializeMapTables ()
    {
      if (this->ipv4MapTables == nullptr)
        {
          this->ipv4MapTables = Create<SimpleMapTables> ();
        }
      if (this->ipv6MapTables == nullptr)
        {
          this->ipv6MapTables = Create<SimpleMapTables> ();
        }
    }
  };

protected:
  LispHelper lispHelper;
  LispEtrItrAppHelper lispXtrHelper;
  MapServerDdtHelper mapServerHelper;
  MapResolverDdtHelper mapResolverHelper;

public:
  LispTopology () : Ipv4Topology ()
  {
  }

  Ptr<LispTopologyNode> NewNode (std::string name, double x, double y)
  {
    Ptr<LispTopologyNode> node = Create<LispTopologyNode> (name, this);
    AnimationInterface::SetConstantPosition (node->GetNode(), x, y);
    return node;
  }

  Ptr<LispTopologyNode> GetNode (std::string name)
  {
    auto iterator = this->nodesByName.find (name);
    if (iterator == this->nodesByName.cend ())
      {
        throw new std::logic_error ("Unknown node.");
      }
    return DynamicCast<LispTopologyNode, TopologyNode> ((*iterator).second);
  }
};
