
#include "topology.hpp"
namespace ns3
{
  NS_LOG_COMPONENT_DEFINE("IPNode");
  NS_OBJECT_ENSURE_REGISTERED(IPNode);

  IPNode::IPNode() : Node()
  {
  }
  IPNode::~IPNode()
  {
  }

  Ipv4Address IPNode::GetAddress(std::string neighbor)
  {
    Ptr<Ipv4Interface> iface = this->GetInterface(neighbor);
    return iface->GetAddress(0).GetLocal();
  }
  Ptr<Ipv4Interface> IPNode::GetInterface(std::string neighbor)
  {
    return this->GetObject<Ipv4>()->GetObject<Ipv4L3Protocol>()->GetInterface(m_linkByNeighbor[neighbor]);
  }
  uint32_t IPNode::GetInterfaceIndex(std::string neighbor)
  {
    return m_linkByNeighbor[neighbor];
  }

  void IPNode::AddInterface(uint32_t interface, std::string neighbor)
  {
    m_linkByNeighbor[neighbor] = interface;
  }
  void IPNode::AddAddress(std::string neighbor, Ipv4Address addr)
  {
    GetInterface(neighbor)->AddAddress(Ipv4InterfaceAddress(addr, Ipv4Mask("/24")));
  }

}