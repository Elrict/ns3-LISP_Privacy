/*
 * internet-stack-with-lisp-helper.h
 *
 *  Created on: 30 janv. 2016
 *      Author: lionel
 */

#ifndef INTERNET_STACK_WITH_LISP_HELPER_H_
#define INTERNET_STACK_WITH_LISP_HELPER_H_

#include <string>

//#include <ns3/internet-stack-helper.h>
#include "ns3/node.h"
#include "ns3/node-container.h"
#include "ns3/ptr.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/map-tables.h"
#include "ns3/lisp-protocol.h"
#include <set>

namespace ns3 {
// TODO Maybe ADD PCAP classes ?
class LispHelper
{
public:
  // TODO Also install mapping system (control plane)
  LispHelper (void);
  virtual
  ~LispHelper (void);

  // NB. Must be called only once the address of routers are assigned
  void BuildMapTables (std::string localMapTablesConfigFilePath);
  void BuildMapTables2 (std::string localMapTablesConfigFilePath);
  void SetMapTablesForEtr (Address etrRloc, Ptr<MapTables> ipv4MapTables, Ptr<MapTables> ipv6MapTables);
  void BuildRlocsSet (std::string rlocsListFilePath);
  void AddRlocToSet (Address rloc);
  void SetPetrAddress (Address petrAddress);
  void SetPitrs (NodeContainer c);
  void SetPetrs (NodeContainer c);
  void SetRtrs (NodeContainer c);
  void InstallMapTables (NodeContainer c);
  void Install (Ptr<Node> node) const;
  void Install (NodeContainer c) const;
  void InstallAll (void) const;

protected:
  void CreateAndAggregateLispVersion (Ptr<Node> node, const std::string typeId) const;
  MapTables::MapEntryLocation GetLocation (int locationInt);
  std::vector<std::string> Split (std::string str);
  std::map<Address, Ptr<MapTables> > m_mapTablesIpv4;
  std::map<Address, Ptr<MapTables> > m_mapTablesIpv6;
  std::map<Address, Ptr<LispStatistics> > m_lispStatisticsMapForV4;
  std::map<Address, Ptr<LispStatistics> > m_lispStatisticsMapForV6;
  std::set<Address> m_rlocsList;
  Address m_petrAddress;
  std::vector<uint32_t> m_pitrs; //!< Vector containing identifier of all PETRs nodes
  std::vector<uint32_t> m_petrs; //!< Vector containing identifier of all PITRs nodes
  std::vector<uint32_t> m_rtrs; //!< Vector containing identifier of all RTRs nodes

};

} /* namespace ns3 */

#endif /* INTERNET_STACK_WITH_LISP_HELPER_H_ */
