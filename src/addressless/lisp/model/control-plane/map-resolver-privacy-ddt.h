#ifndef SRC_INTERNET_MODEL_LISP_CONTROL_PLANE_MAP_RESOLVER_PRIVACY_DDT_H_
#define SRC_INTERNET_MODEL_LISP_CONTROL_PLANE_MAP_RESOLVER_PRIVACY_DDT_H_

#include "ns3/map-resolver.h"
#include "ns3/map-resolver-ddt.h"
#include "ns3/map-tables.h"
#include "ns3/lisp-control-msg.h"
#include "ns3/lisp-over-ip.h"
#include "ns3/map-referral-msg.h"
#include "ns3/map-request-msg.h"


namespace ns3
{

class MapResolverPrivacyDdt : public MapResolver
{
public:
  MapResolverPrivacyDdt ();
  virtual
  ~MapResolverPrivacyDdt ();

  static TypeId
  GetTypeId (void);

  void SetMapServerAddress (Address mapServer);

private:

  virtual void StartApplication (void);

  virtual void StopApplication (void);

  virtual void SendMapRequest (Ptr<MapRequestMsg> mapRequestMsg);

  virtual void HandleRead (Ptr<Socket> socket);

  virtual void HandleReadFromClient (Ptr<Socket> socket);

  // when ddt is used
  Ptr<Locators> rootDdtNodeRlocs;
  //
  Address m_mapServerAddress;
  Ptr<MapReferralCache> m_mapRefCache;
  Ptr<PendingRequestList> m_pendingReqList;
  std::map<uint32_t, bool> m_cachedMapping;
  std::map<uint32_t, Time> m_timeMapping;
  std::map<uint32_t, std::vector<uint32_t>> m_alreadyAsked;
  int m_requestWaiting;

};

} /* namespace ns3 */

#endif /* SRC_INTERNET_MODEL_LISP_CONTROL_PLANE_MAP_RESOLVER_DDT_H_ */
