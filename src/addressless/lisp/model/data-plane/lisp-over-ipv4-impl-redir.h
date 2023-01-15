#ifndef LISP_OVER_IPV4_IMPL_REDIR_H_
#define LISP_OVER_IPV4_IMPL_REDIR_H_

#include <ns3/node.h>
#include "ns3/packet.h"
#include "ns3/ptr.h"
#include "ns3/lisp-over-ipv4-impl.h"
#include "ns3/lisp-protocol.h"
#include "ns3/map-tables.h"
#include "ns3/ipv4-interface.h"
#include "ns3/map-notify-msg.h"

namespace ns3
{

  class LispOverIpv4ImplRedir : public LispOverIpv4Impl
  {

  public:
    static TypeId GetTypeId(void);
    LispOverIpv4ImplRedir();
    virtual ~LispOverIpv4ImplRedir();
    /**
     * if lisp is true, we have a LISP data header inside packet.
     * Otherwise, we have an ECM header inside packet.
     */
    void LispInput(Ptr<Packet> packet, Ipv4Header const &outerHeader, bool lisp);
    void LispOutput(Ptr<Packet> packet, Ipv4Header const &innerHeader,
                    Ptr<const MapEntry> localMapping,
                    Ptr<const MapEntry> remoteMapping,
                    Ptr<Ipv4Route> lispRoute,
                    LispOverIp::EcmEncapsulation ecm);

  private:
    int FindAddress(Ipv4Address addr);

    /**
     * @brief Perform a check of the destination rloc.
     *
     * @param rloc Destination rloc of the packet
     * @param source Source EID of the packet.
     * @param destination Destinaation EID of the packet.
     * @return true if the rloc generated from the two EIDs is the same as the rloc used.
     * @return false otherwise
     */
    bool CheckRloc(Ipv4Address rloc, Ipv4Address source);
    /**
     * @brief Perform a check of the destination eid.
     *
     * @param source Source EID of the packet.
     * @param destination Destinaation EID of the packet.
     * @return true if the eid generated from the source EID is the same as the eid used as destination.
     * @return false otherwise.
     */
    bool CheckEid(Ipv4Address source, Ipv4Address destination);

    /**
     * @brief Associate the peer address to one of the address of the server through hashing
     *
     * @param peer peer address.
     * @return Ipv4Address : the address that should be used by the peer to reach the server.
     */
    Ipv4Address GenerateAddress(Address peer, Ptr<Ipv4Interface> iface);
    void DelayedReceive(Ptr<NetDevice> device, Ptr<const Packet> p, uint16_t protocol, const Address &from,
                        const Address &to, NetDevice::PacketType packetType);

    std::map<uint32_t, uint32_t> natMap;
    Ipv4Address m_srvAddr;              // service public address.
    Ptr<Ipv4Interface> m_srvInterface;  // server interface.
    Ptr<Ipv4Interface> m_rlocInterface; // server interface.
    int m_clientswaiting = 0;

    bool m_eidCheck;  // should destination eid of the server be checked?
    bool m_rlocCheck; // should destination rloc be checked?
    bool m_rlocRedir;
    bool m_passive; // should natting be performed towards the server ?
    bool m_natting;        // should natting be performed towards the server ?

    std::map<uint32_t, bool> m_privacyDone;
    Ptr<RandomVariableStream> m_hashTime;
  };

} /* namespace ns3 */

#endif /*LISP_OVER_IPV4_IMPL_H_ */
