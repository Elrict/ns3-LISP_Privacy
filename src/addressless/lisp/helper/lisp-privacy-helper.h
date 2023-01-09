
#ifndef INTERNET_STACK_WITH_LISP_PRIV_HELPER_H_
#define INTERNET_STACK_WITH_LISP_PRIV_HELPER_H_

#include <string>

//#include <ns3/internet-stack-helper.h>
#include "ns3/node.h"
#include "ns3/lisp-helper.h"
#include "ns3/node-container.h"
#include "ns3/ptr.h"
#include "ns3/boolean.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/map-tables.h"
#include "ns3/lisp-protocol.h"
#include <set>

namespace ns3 {
// TODO Maybe ADD PCAP classes ?
class LispPrivacyHelper : public LispHelper
{
public:
  // TODO Also install mapping system (control plane)
  LispPrivacyHelper (void);
  virtual
  ~LispPrivacyHelper (void);

  // NB. Must be called only once the address of routers are assigned
  void SetNatting(bool value);
  void SetReverseNatting(bool value);

  void SetRlocCheck(bool value);
  void SetEidCheck(bool value);
  void SetServiceAddr(Ipv4Address addr);
  void SetServiceInterface(Ptr<Ipv4Interface> iface);
  void SetRlocInterface(Ptr<Ipv4Interface> iface);
  void SetRlocRedir(bool value);
  void SetAttribute (std::string name, const AttributeValue &value);
  void Install (Ptr<Node> node);

private:
  ObjectFactory m_ipv4factory;
  ObjectFactory m_ipv6factory;
  void CreateAndAggregateLispVersion (Ptr<Node> node,  TypeId typeId);
};

} /* namespace ns3 */

#endif /* INTERNET_STACK_WITH_LISP_HELPER_H_ */
