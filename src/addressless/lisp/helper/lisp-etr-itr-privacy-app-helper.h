
#ifndef SRC_INTERNET_HELPER_LISP_HELPER_LISP_ETR_ITR_REDIR_APP_HELPER_H_
#define SRC_INTERNET_HELPER_LISP_HELPER_LISP_ETR_ITR_REDIR_APP_HELPER_H_

#include <stdint.h>
#include "ns3/lisp-etr-itr-privacy-application.h"
#include "ns3/application-container.h"
#include "ns3/names.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"

namespace ns3
{
class Address;
class LispEtrItrPrivacyAppHelper 
{
public:
  LispEtrItrPrivacyAppHelper ();
  virtual
  ~LispEtrItrPrivacyAppHelper ();
  void SetTypeId(TypeId tid);
  void
  SetAttribute (std::string name, const AttributeValue &value);
  void SetServer(Ptr<Ipv4Interface> servAddr);
  void SetInterface(Ptr<Ipv4Interface> iface);
  void SetRlocRedir(bool value);
  void SetFastRedir(bool value);

  
  ApplicationContainer Install (Ptr<Node> node) const;

  void AddMapServerAddress (Address mapServerAddress);
  void AddMapResolverRlocs (Ptr<Locator> locator);
  void SetMapResolverRlocs (std::list<Ptr<Locator> > locator);


  private:
  Ptr<Application> InstallPriv(Ptr<Node> node) const;
  std::list<Ptr<Locator> > m_mapResolverRlocs;
  Address m_eidServer;
  std::list<Address> m_mapServerAddresses;
  ObjectFactory m_factory; //!<Object factory

};

} /* namespace ns3 */

#endif /* SRC_INTERNET_HELPER_LISP_HELPER_LISP_ETR_ITR_APP_HELPER_H_ */
