/*
 * lisp-protocol.cc
 *
 *  Created on: 28 janv. 2016
 *      Author: lionel
 */

#include "lisp-over-ipv4-impl.h"
#include "lisp-over-ipv6-impl.h"
#include <ns3/packet.h>
#include "ns3/address.h"
#include "ns3/assert.h"
#include "ns3/ipv4-address.h"
#include "ns3/ipv4-header.h"
#include "ns3/log.h"
#include "ns3/udp-header.h"
#include "ns3/udp-l4-protocol.h"
#include "lisp-header.h"
#include "ns3/ptr.h"
#include "simple-map-tables.h"
#include <ns3/ipv4-l3-protocol.h>
#include "rloc-metrics.h"
#include "lisp-over-ip.h"
#include "lisp-mapping-socket.h"

namespace ns3
{

  NS_LOG_COMPONENT_DEFINE("LispOverIpv4Impl");

  NS_OBJECT_ENSURE_REGISTERED(LispOverIpv4Impl);

  TypeId
  LispOverIpv4Impl::GetTypeId()
  {
    static TypeId tid = TypeId("ns3::LispOverIpv4Impl")
                            .SetParent<LispOverIpv4>()
                            .SetGroupName("Lisp")
                            .AddConstructor<LispOverIpv4Impl>();
    return tid;
  }

  LispOverIpv4Impl::LispOverIpv4Impl()
  {
    NS_LOG_FUNCTION(this);
    // Init MapTables
  }

  LispOverIpv4Impl::~LispOverIpv4Impl()
  {
    NS_LOG_FUNCTION(this);
  }

  // TODO Remember to manage error cases
  // TODO add argument to say packet must be dropped
  // Packets coming from upper layer or from an end-host
  // If false, we drop packet in IP, if true OK.
  void LispOverIpv4Impl::LispOutput(Ptr<Packet> packet, Ipv4Header const &innerHeader,
                                    Ptr<const MapEntry> localMapping,
                                    Ptr<const MapEntry> remoteMapping,
                                    Ptr<Ipv4Route> lispRoute,
                                    LispOverIp::EcmEncapsulation ecm)
  {

    NS_LOG_FUNCTION(this);
    Ptr<Locator> destLocator = 0;
    Ptr<Locator> srcLocator = 0;
    uint16_t udpSrcPort = 0;
    uint16_t udpDstPort = 0;

    // headers
    Ipv4Header outerHeader;
    uint16_t udpLength = 0;

    NS_ASSERT(localMapping != 0);

    NS_LOG_LOGIC("Check remote mapping existence");
    if (remoteMapping == 0)
    {
      // TODO drop packet
      m_statisticsForIpv4->IncCacheMissPackets();
      m_statisticsForIpv4->IncOutputDropPackets();
      m_statisticsForIpv4->IncOutputPackets();
      NS_LOG_WARN("No remote mapping for destination EID. Drop");
      return;
    }

    // NB. ns3 does not compute checksum by default

    // Select a destination Rloc if no drop packet

    if (remoteMapping->IsNegative())
    {
      // Packet destined to non-LISP site -> Encapsulate towards PETR if PETR configured
      Address petrAddress = GetPetrAddress();
      if (!petrAddress.IsInvalid())
      {
        NS_LOG_DEBUG("PETR configured");
        destLocator = Create<Locator>(petrAddress);
        Ptr<RlocMetrics> rlocMetrics = Create<RlocMetrics>();
        rlocMetrics->SetPriority(200);
        rlocMetrics->SetWeight(0);
        rlocMetrics->SetMtu(1500);
        rlocMetrics->SetUp(true);
        rlocMetrics->SetIsLocalIf(true);
        if (Ipv4Address::IsMatchingType(petrAddress))
          rlocMetrics->SetLocAfi(RlocMetrics::IPv4);
        else if (Ipv6Address::IsMatchingType(petrAddress))
          rlocMetrics->SetLocAfi(RlocMetrics::IPv6);
        else
          NS_LOG_ERROR("Unknown AFI");

        destLocator->SetRlocMetrics(rlocMetrics);
        // TODO set metric
      }
      else // If no PETR configured -> Drop packet
      {
        NS_LOG_DEBUG("No PETR configured");
        destLocator = 0;
      }
    }
    else
    {
      destLocator = SelectDestinationRloc(remoteMapping);
      NS_LOG_DEBUG("Destination RLOC address: " << Ipv4Address::ConvertFrom(destLocator->GetRlocAddress()));
    }

    if (destLocator == 0)
    {
      // TODO add stats and drop packet
      m_statisticsForIpv4->IncNoValidRloc();
      m_statisticsForIpv4->IncOutputDropPackets();
      m_statisticsForIpv4->IncOutputPackets();
      NS_LOG_WARN("No valid destination locator for eid " << innerHeader.GetDestination() << ". Drop!");
      return;
    }

    // Check size for destRloc MTU if needed
    if (destLocator->GetRlocMetrics()->GetMtu() &&
        (packet->GetSize() + LispHeader().GetSerializedSize() +
         Ipv4Header().GetSerializedSize() * 2 +
         UdpHeader().GetSerializedSize()) >
            destLocator->GetRlocMetrics()->GetMtu())
    {
      // drop packet
      // TODO send ICMP message
      int size = (packet->GetSize() + LispHeader().GetSerializedSize() +
                  Ipv4Header().GetSerializedSize() +
                  UdpHeader().GetSerializedSize());
      m_statisticsForIpv4->IncNoValidMtuPackets();
      m_statisticsForIpv4->IncOutputDropPackets();
      m_statisticsForIpv4->IncOutputPackets();
      NS_LOG_DEBUG("MTU DEST " << destLocator->GetRlocMetrics()->GetMtu() << " Packet size " << size);
      NS_LOG_ERROR("[LISP_OUTPUT] Drop! MTU check failed for destination RLOC.");
      return;
    }

    // Get Outgoing interface thanks to RouteOutput of m_routingProtocol
    // If the following 2 checks are not OK, drop
    // check if the 2 Rloc (DEST and SRC) are IPvX
    /*
     * We check the list of the Rlocs and select the one which
     * respects the following constraints:
     * - Has the same AF
     * - Is a local address
     * - Has a valid priority (i.e. less than 255)
     * - Is the address of the outgoing interface
     */
    srcLocator = SelectSourceRloc(static_cast<Address>(innerHeader.GetSource()), destLocator);

    if (srcLocator == 0)
    {
      // drop packet
      m_statisticsForIpv4->IncNoValidRloc();
      m_statisticsForIpv4->IncOutputDropPackets();
      m_statisticsForIpv4->IncOutputPackets();
      NS_LOG_ERROR("[LISP_OUTPUT] Drop! No valid source .");
      return;
    }

    // if MTU set, check MTU for the srcRloc too
    if (srcLocator->GetRlocMetrics()->GetMtu() &&
        (packet->GetSize() + LispHeader().GetSerializedSize() +
         Ipv4Header().GetSerializedSize() * 2 +
         UdpHeader().GetSerializedSize()) >
            srcLocator->GetRlocMetrics()->GetMtu())
    {
      // drop packet
      m_statisticsForIpv4->IncNoValidMtuPackets();
      m_statisticsForIpv4->IncOutputDropPackets();
      m_statisticsForIpv4->IncOutputPackets();
      NS_LOG_DEBUG("MTU SRC " << srcLocator->GetRlocMetrics()->GetMtu());
      NS_LOG_ERROR("[LISP_OUTPUT] Drop! MTU check failed for source RLOC.");
      return;
    }

    // add inner header before encapsulating whole packet
    packet->AddHeader(innerHeader);

    /* ------------------------------
     *    ECM Encapsulation
     * ------------------------------ */
    // If we have a MapRegister that must be ECM encapsulated, source port must
    // be set to 4341 (LISP data port) to initialize state in NAT.
    if (ecm == LispOverIp::ECM_XTR || ecm == LispOverIp::ECM_RTR)
    {
      NS_LOG_DEBUG("ECM encapsulation");
      udpSrcPort = LispOverIp::LISP_DATA_PORT;
      udpDstPort = LispOverIp::LISP_SIG_PORT;
      packet = PrependEcmHeader(packet, ecm);
      udpLength = innerHeader.GetPayloadSize() + innerHeader.GetSerializedSize() + LispEncapsulatedControlMsgHeader().GetSerializedSize();
    }
    /* ------------------------------
     *    LISP Encapsulation
     * ------------------------------ */
    else
    { // add LISP header to the packet (with inner IP header)
      NS_LOG_DEBUG("LISP data header encapsulation");

      /* === RTR === */
      if (IsRtr())
      {
        /* Case 1: destination is registered EID */
        Ptr<MapEntry> mapEntryDest = m_mapTablesIpv4->CacheLookup(innerHeader.GetDestination());
        Ptr<MapEntry> mapEntrySrc = m_mapTablesIpv4->CacheLookup(innerHeader.GetSource());
        if (mapEntryDest != 0 && mapEntryDest->IsNatedEntry())
        {
          NS_LOG_DEBUG("Encapsulation requires Translated NAT addresses (RTR)");

          /* Must encapsulate Data packets to NATed device */
          udpSrcPort = LispOverIp::LISP_SIG_PORT;          // Correspond to NAT state
          udpDstPort = remoteMapping->GetTranslatedPort(); // Correspond to NAT state
          // Wireshark cannot read such a packet because LISP DATA PORT isn't used, but that's fine
        }
        /* Case 2: source is registered EID */
        else if (mapEntrySrc != 0 && mapEntrySrc->IsNatedEntry())
        {
          // compute src port based on inner header(Follow algo get_lisp_srcport)
          NS_LOG_DEBUG("Encapsulation doesn't require Translated NAT addresses (RTR)");
          udpSrcPort = LispOverIp::GetLispSrcPort(packet);
          udpDstPort = LispOverIp::LISP_DATA_PORT;
        }
        else
          NS_LOG_ERROR("Neither source nor destination is a registered EID of RTR");
      }
      /* === xTR === */
      else
      {
        // compute src port based on inner header(Follow algo get_lisp_srcport)
        udpSrcPort = LispOverIp::GetLispSrcPort(packet);
        udpDstPort = LispOverIp::LISP_DATA_PORT;
      }

      packet = PrependLispHeader(packet, localMapping, remoteMapping, srcLocator, destLocator);
      // NB. In ns3 payloadSize is the size of payload without ip header.
      udpLength = innerHeader.GetPayloadSize() + innerHeader.GetSerializedSize() + LispHeader().GetSerializedSize();
    }
    if (!packet)
    {
      m_statisticsForIpv4->IncNoEnoughSpace();
      m_statisticsForIpv4->IncOutputDropPackets();
      m_statisticsForIpv4->IncOutputPackets();
      NS_LOG_ERROR("[LISP_OUTPUT] Drop! Not enough buffer space for packet.");
      return;
    }

    /*
     * We perform the encapsulation by adding the UDP then the outer IP header
     * according to the source locator Address Family (AF) -- If src ipv4
     * add ipv4 header, ipv6 if not.
     */
    if (Ipv4Address::IsMatchingType(srcLocator->GetRlocAddress()))
    {

      // NS_LOG_DEBUG ("Building outer header");
      //  outer Ipv4 Header
      outerHeader.SetPayloadSize(udpLength + 8); // 8 is size of UDP header
      outerHeader.SetTtl(innerHeader.GetTtl());  // copy inner TTL to outer header
      outerHeader.SetTos(0);                     // Default TOS
      outerHeader.SetDontFragment();             // set don't fragment bit
      outerHeader.SetSource(Ipv4Address::ConvertFrom(srcLocator->GetRlocAddress()));
      outerHeader.SetDestination(Ipv4Address::ConvertFrom(destLocator->GetRlocAddress()));
      outerHeader.SetProtocol(UdpL4Protocol::PROT_NUMBER); // set udp protocol

      // we just add the Lisp header and the UDP header
      NS_LOG_LOGIC("Encapsulating packet");
      packet = LispEncapsulate(packet, udpLength, udpSrcPort, udpDstPort);
      NS_ASSERT(m_statisticsForIpv4 != 0);
      m_statisticsForIpv4->IncOutputPackets();
      // finally we have a packet that we can re-inject in IP
      Ptr<Ipv4L3Protocol> ipv4 = GetNode()->GetObject<Ipv4L3Protocol>();
      NS_LOG_LOGIC("Re-injecting packet in IPV4");

      if (GetPitr() && ecm != LispOverIp::ECM_XTR && ecm != LispOverIp::ECM_RTR) // For data packets only
      {
        /* --- Artificial delay for PxTRs introduced --- */
        double rtt = m_rttVariable->GetValue();
        double stretch = m_pxtrStretchVariable->GetValue() + 1;
        Simulator::Schedule(Seconds(rtt * stretch), &Ipv4L3Protocol::SendWithHeader, ipv4, packet, outerHeader, lispRoute);
      }
      else if (IsRtr()) // For data packets, and ECM encapsulation
      {
        Simulator::Schedule(Seconds(m_rtrVariable->GetValue()), &Ipv4L3Protocol::SendWithHeader, ipv4, packet, outerHeader, lispRoute);
      }
      else
        ipv4->SendWithHeader(packet, outerHeader, lispRoute);
    }
    else if (Ipv6Address::IsMatchingType(srcLocator->GetRlocAddress()))
    {
      // TODO Implement IPv6
      LispOverIpv6Impl().LispEncapsulate(packet, udpLength, udpSrcPort, udpDstPort);
      // TODO Use Ipv6->Send()
      m_statisticsForIpv6->IncOutputDifAfPackets();
    }
    else
    {
      // error
      // should never happen --- drop packet
      return;
    }
  }

  // Packets coming from a possible Rloc to enter the AS
  void LispOverIpv4Impl::LispInput(Ptr<Packet> packet, Ipv4Header const &outerHeader, bool lisp)
  {
    UdpHeader udpHeader;
    LispHeader lispHeader;
    LispEncapsulatedControlMsgHeader ecmHeader;
    Ipv4Header innerIpv4Header;
    Ipv6Header innerIpv6Header;
    bool isMappingForPacket;

    m_statisticsForIpv4->IncInputPacket();

    // make basic check on the size
    if (packet->GetSize() < (udpHeader.GetSerializedSize() + outerHeader.GetSerializedSize()))
    {

      NS_LOG_ERROR("[LISP_INPUT] Drop! Packet size smaller that headers size.");
      m_statisticsForIpv4->IncBadSizePackets();
      return;
    }

    // Remove the UDP headers
    // NB: the IP header has already been removed and checked
    packet->RemoveHeader(udpHeader);
    NS_LOG_DEBUG("UDP header removed: " << udpHeader);
    if (lisp)
      // get the LISP header
      packet->RemoveHeader(lispHeader);
    else
      // get the ECM header
      packet->RemoveHeader(ecmHeader);
    NS_LOG_LOGIC("Lisp header removed: " << lispHeader);

    packet->EnableChecking();
    PacketMetadata::Item item;
    PacketMetadata::ItemIterator metadataIterator = packet->BeginItem();

    /*
     * We know that the first header is an ip header
     * check the mappings for the src and dest EIDs
     * check in the case of Ipv6 and Ipv4 addresses (lisp_check_ip_mappings)
     *
     * IMPORTANT: But it is also possible that the first header is an ip header
     * of maq request. Lionel's implementation has not considered that the inner header
     * is a lisp data plan message?
     */
    while (metadataIterator.HasNext())
    {
      item = metadataIterator.Next();
      if (!(item.tid.GetName().compare(Ipv4Header::GetTypeId().GetName())))
      {
        NS_LOG_DEBUG("Before checking metadata");

        if (GetPetr() || IsRtr()) // PETR or RTR case -> No need to check (decapsulate everything)
          isMappingForPacket = true;
        else
        {
          NS_LOG_DEBUG("Classic xTR");
          isMappingForPacket = LispOverIp::m_mapTablesIpv4->IsMapForReceivedPacket(
              packet,
              lispHeader,
              static_cast<Address>(outerHeader.GetSource()),
              static_cast<Address>(outerHeader.GetDestination()));
        }

        NS_LOG_DEBUG("Check passed");
        bool isControlPlanMsg = false;
        // if inner UDP of innerIpv4header is at port 4342. we should
        // use ipv4->Receive() to send this packet to itself.
        // that's why whether isMappingForPacket is true, we also retrieve @ip and port
        Ptr<Node> node = GetNode();
        packet->RemoveHeader(innerIpv4Header);

        if (innerIpv4Header.GetProtocol() == UdpL4Protocol::PROT_NUMBER)
        {
          NS_LOG_DEBUG("Next Header of Inner IP is still UDP, let's see the port");
          packet->RemoveHeader(udpHeader);
          if (udpHeader.GetDestinationPort() == LispOverIp::LISP_SIG_PORT)
          {
            NS_LOG_DEBUG("UDP on port:" << unsigned(udpHeader.GetDestinationPort()) << "->LISP Control Plan Message!");
            isControlPlanMsg = true;
          }
          // Add UDP&IP header to the packet
          packet->AddHeader(udpHeader);
        }
        innerIpv4Header.SetTtl(outerHeader.GetTtl());
        Address from = static_cast<Address>(innerIpv4Header.GetSource());
        Address to = static_cast<Address>(innerIpv4Header.GetDestination());
        innerIpv4Header.EnableChecksum();

        packet->AddHeader(innerIpv4Header);

        // Either find mapping for inner ip header or find the inner message is control message
        // use ip receive procedure to forward packet
        if (isMappingForPacket or isControlPlanMsg)
        {
          Ptr<Ipv4L3Protocol> ipv4 = (node->GetObject<Ipv4L3Protocol>());
          // put it back in ip Receive ()
          // Attention: it's the method LispOverIpv4::RecordReceiveParams that
          // retrieve m_currentDevice values!!! This method is called in
          // Ipv4L3Protocol::Received() method!
          ipv4->Receive(m_currentDevice, packet, m_ipProtocol, from, to, m_currentPacketType);
          NS_LOG_DEBUG("Re-inject the packet in receive to forward it to: " << innerIpv4Header.GetDestination());
        }
        else
        {
          // TODO drop and log
          NS_LOG_ERROR("Mapping check failed during local deliver! Attention if this is cause by double encapsulation!");
        }
        return;
      }
      // if inner header is ipv6
      else if (!(item.tid.GetName().compare(Ipv6Header::GetTypeId().GetName())))
      {
        m_statisticsForIpv6->IncInputDifAfPackets();
        isMappingForPacket = LispOverIp::m_mapTablesIpv6->IsMapForReceivedPacket(packet, lispHeader, static_cast<Address>(outerHeader.GetSource()), static_cast<Address>(outerHeader.GetDestination()));
        if (isMappingForPacket)
        {
          // remove inner ipheader
          // TODO do the same for Ipv6
        }
        else
        {
          // TODO drop and log
        }
        return;
      }
      else
      {
        // should not happen -- report error
        NS_LOG_ERROR("[LISP_INPUT] Drop! Unrecognized inner AF");

        m_statisticsForIpv4->IncBadSizePackets();
        return;
      }
    }
  }
  // ns3-privacy addition
  void LispOverIpv4Impl::BufferPacket(Ptr<Packet> packet,
                                      Ipv4Address source,
                                      Ipv4Address destination,
                                      uint8_t protocol,
                                      Ptr<Ipv4Route> route)
  {
    m_bufferedPackets.push_back({
        packet,
        source,
        destination,
        protocol,
        route,
    });
  }
  LispOverIpv4::MapStatus LispOverIpv4Impl::IsMapForEncapsulation(Ipv4Header const &innerHeader, Ptr<MapEntry> &srcMapEntry, Ptr<MapEntry> &destMapEntry, Ipv4Mask mask)
  {
    // mask == mask of the output iface
    NS_LOG_FUNCTION(this << innerHeader << mask);
    NS_LOG_DEBUG("Enter IsMapForEncapsulation");
    // Check if the source address is already defined
    if (innerHeader.GetSource().IsEqual(Ipv4Address::GetAny()))
    {
      return LispOverIpv4::No_Mapping;
    }
    /**
     * If both srcMapEntry and dstMapEntry are 0, we know we are dealing
     * with a classic data packet.
     * Therefore, we must check if the LISP device is registered to the MDS
     * before sending any encapsulated packet
     */
    if (srcMapEntry == 0 && destMapEntry == 0 && !IsRegistered())
      return LispOverIpv4::Not_Registered;

    if (srcMapEntry == 0)
    {
      // Check if the prefix of the source address is in the db
      srcMapEntry = LispOverIp::DatabaseLookup(static_cast<Address>(innerHeader.GetSource()));
    }

    // also check if it is the same address range (mask)
    if (srcMapEntry == 0)
    {
      NS_LOG_DEBUG("[MapForEncap] Source map entry does not exist");
      Ptr<MapTables> mapTable = LispOverIp::GetMapTablesV4();
      // NS_LOG_DEBUG ("Current Database content: \n"<<*mapTable);
      return LispOverIpv4::No_Mapping;
    }
    NS_LOG_DEBUG("found srcMapEntry: \n"
                 << srcMapEntry->GetLocators()->Print());
    if (srcMapEntry->IsNegative())
    {
      NS_LOG_DEBUG("[MapForEncap] Source map entry is a Negative mapping");
      return LispOverIpv4::No_Mapping;
    }

    if (mask.IsMatch(innerHeader.GetSource(), innerHeader.GetDestination()))
    {
      NS_LOG_DEBUG("[MapForEncap] No encap needed. Addresses matches in their mask!");
      return LispOverIpv4::No_Need_Encap;
    }

    if (destMapEntry == 0)
    {
      destMapEntry = LispOverIp::CacheLookup(static_cast<Address>(innerHeader.GetDestination()));
      // Supposed to return RTR RLOC if device is NATed
    }

    // Cache miss !
    if (destMapEntry == 0)
    {
      NS_LOG_DEBUG("[MapForEncap] SOURCE map entry exists but DEST map entry does not !");
      NS_LOG_DEBUG("MapTables content:" << *m_mapTablesIpv4);
      Ptr<LispOverIp> lisp = GetNode()->GetObject<LispOverIp>();
      // NS_LOG_INFO("XXX:"<<*lisp->GetMapTablesV4());
      MappingSocketMsgHeader sockMsgHdr;
      sockMsgHdr.SetMapAddresses((int)sockMsgHdr.GetMapAddresses() | static_cast<int>(LispMappingSocket::MAPA_EID));
      sockMsgHdr.SetMapType(static_cast<uint8_t>(LispMappingSocket::MAPM_MISS));
      sockMsgHdr.SetMapVersion(LispMappingSocket::MAPM_VERSION);

      Ptr<MappingSocketMsg> mapSockMsg = Create<MappingSocketMsg>();
      mapSockMsg->SetEndPoint(Create<EndpointId>(static_cast<Address>(innerHeader.GetDestination())));
      mapSockMsg->SetLocators(0);
      mapSockMsg->SetEIDSource(innerHeader.GetSource());

      NS_LOG_DEBUG("[MapForEncap] EID not found " << innerHeader.GetDestination());
      uint8_t buf[64];
      mapSockMsg->Serialize(buf);
      Ptr<Packet> packet = Create<Packet>(buf, 100);
      NS_LOG_DEBUG("Send Notification to all LISP apps");
      SendNotifyMessage(static_cast<uint8_t>(LispMappingSocket::MAPM_MISS), packet, sockMsgHdr, 0);
      return LispOverIpv4::No_Mapping;
    }
    else if (destMapEntry->IsNegative())
    {
      NS_LOG_DEBUG("Negative Map Entry " << innerHeader.GetDestination());
      /*
       * A mapping exists but it is a negative one. We treat it as
       * if no mapping was found.
       */
      return LispOverIpv4::Mapping_Exist;
    }

    return LispOverIpv4::Mapping_Exist;
  }

  /*
   * Check if there exists a local mapping
   * for the source EID.
   */
  bool LispOverIpv4Impl::NeedEncapsulation(Ipv4Header const &ipHeader, Ipv4Mask mask)
  {
    NS_LOG_FUNCTION(this << " outer header: " << ipHeader);

    // check if the src address is defined
    if (ipHeader.GetSource() == Ipv4Address::GetAny())
    {
      return false;
    }
    // PITR case -> Encapsulate all traffic
    //  RTR case -> Relay for other EID spaces
    if (GetPitr() || IsRtr())
      return true;

    // Use dblookup to find the list of rlocs (if mapping exists)
    Ptr<MapEntry> eidMapEntry = LispOverIp::DatabaseLookup(static_cast<Address>(ipHeader.GetSource()));
    // check if it has the same mask as the dest EID, if yes no need
    // to encap, if no encap
    if (eidMapEntry)
    {
      // Use check if addresses match in their mask
      if (mask.IsMatch(ipHeader.GetSource(), ipHeader.GetDestination()))
      {
        NS_LOG_DEBUG("Why no mapping entry found for EID:" << eidMapEntry->GetEidPrefix()->GetEidAddress());
        return false;
      }
      NS_LOG_DEBUG("The found EID-RLOC mapping is: EID(" << eidMapEntry->GetEidPrefix()->GetEidAddress() << ") --> RLOC\n(" << eidMapEntry->GetLocators()->Print() << ")");
      return true;
    }

    return false;
  }

  /*
   * Check if the received packet
   * OK !!
   */
  bool LispOverIpv4Impl::NeedDecapsulation(Ptr<const Packet> packet, Ipv4Header const &ipHeader, uint16_t lispPort)
  {
    NS_LOG_FUNCTION(this << " outer header: " << ipHeader);
    NS_ASSERT(packet != 0);

    Ptr<Packet> p = packet->Copy();
    UdpHeader udpHeader;
    LispHeader lispHeader;
    // Exclude too small packets (malformed)
    if (packet->GetSize() < (udpHeader.GetSerializedSize() + ipHeader.GetSerializedSize() + lispHeader.GetSerializedSize()))
    {
      NS_LOG_DEBUG("Packet size not good");
      return false;
    }

    if (ipHeader.GetProtocol() == UdpL4Protocol::PROT_NUMBER)
    {
      NS_LOG_DEBUG("Protocol beneath IP is really UDP");
      p->RemoveHeader(udpHeader);

      /* Accept all DATA packets (packets with UDP dest 4342) */
      if (lispPort == LispOverIp::LISP_DATA_PORT)
      {
        if (udpHeader.GetDestinationPort() == lispPort) // LispOverIp::LISP_DATA_PORT or LispOverIp::LISP_SIG_PORT
        {
          NS_LOG_DEBUG("Data packet => Needs decapsulation");
          return true;
        }
      }
      /* Accept only CONTROL packets if they are ECM encapsulated.
         Control packets that are not ECM encapsulated musn't be decapsulated, obviously */
      else
      {
        uint8_t buf[p->GetSize()];
        p->CopyData(buf, p->GetSize());
        uint8_t msg_type = (buf[0] >> 4);
        if (msg_type == static_cast<uint8_t>(LispEncapsulatedControlMsgHeader::GetMsgType()))
        {
          NS_LOG_DEBUG("ECM encapsulated Control packet => Needs decapsulation");
          return true;
        }
        else
        {
          NS_LOG_DEBUG("Control packet => Doesn't need decapsulation");
          return false;
        }
      }
    }
    return false;
  }

  bool
  LispOverIpv4Impl::IsMapNotifyForNatedXtr(Ptr<Packet> packet, Ipv4Header const &ipHeader, Ptr<MapEntry> &mapEntry)
  {

    UdpHeader udpHeader;
    packet->RemoveHeader(udpHeader);

    uint8_t buf[packet->GetSize()];
    packet->CopyData(buf, packet->GetSize());
    uint8_t msg_type = buf[0] >> 4;

    packet->AddHeader(udpHeader);

    if (msg_type == static_cast<uint8_t>(LispControlMsg::MAP_NOTIFY))
    {
      Ptr<MapNotifyMsg> mapNotify = MapNotifyMsg::Deserialize(buf);

      Address eid = mapNotify->GetRecord()->GetEidPrefix();
      NS_LOG_DEBUG("MapNotify for EID: " << eid);

      /* Check if MapNotify is for RTR or for NATed device */
      mapEntry = m_mapTablesIpv4->CacheLookup(eid);
      if (mapEntry != 0)
      {              // Cache hit -> MapNotify must be Data encapsulated
        return true; // TODO: maybe check that this mapEntry is a NATED mapEntry (with additional fields in entry)
      }
      /* /!\ Must first check cache, otherwise, MapNotify will be delivered locally */
      mapEntry = m_mapTablesIpv4->DatabaseLookup(eid);
      if (mapEntry != 0) // Database hit -> MapNotify is for local delivery
        return false;

      return false;
    }
    return false;
  }

  bool
  LispOverIpv4Impl::IsMapRequestForNatedXtr(Ptr<Packet> packet, Ipv4Header const &ipHeader, Ptr<MapEntry> &mapEntry)
  {
    UdpHeader udpHeader;
    packet->RemoveHeader(udpHeader);

    uint8_t buf[packet->GetSize()];
    packet->CopyData(buf, packet->GetSize());
    uint8_t msg_type = buf[0] >> 4;

    packet->AddHeader(udpHeader);

    if (msg_type == static_cast<uint8_t>(LispControlMsg::MAP_REQUEST))
    {
      Ptr<MapRequestMsg> mapRequest = MapRequestMsg::Deserialize(buf);

      if (mapRequest->GetS() == 1) // SMR or SMR-invoqued MapRequest
        return false;

      Address eid = mapRequest->GetMapRequestRecord()->GetEidPrefix();
      NS_LOG_DEBUG("MapNotify for EID: " << eid);

      /* Check if MapNotify is for RTR or for NATed device */
      mapEntry = m_mapTablesIpv4->CacheLookup(eid);
      if (mapEntry != 0)
      {              // Cache hit -> MapNotify must be Data encapsulated
        return true; // TODO: maybe check that this mapEntry is a NATED mapEntry (with additional fields in entry)
      }
      /* /!\ Must first check cache, otherwise, MapNotify will be delivered locally */
      mapEntry = m_mapTablesIpv4->DatabaseLookup(eid);
      if (mapEntry != 0) // Database hit -> MapNotify is for local delivery
        return false;

      return false;
    }
    return false;
  }

  // called at the end of lisp_output
  Ptr<Packet> LispOverIpv4Impl::LispEncapsulate(Ptr<Packet> packet, uint16_t udpLength, uint16_t udpSrcPort, uint16_t udpDstPort)
  {
    // add UDP header with src ports + dest port (4341 for Data or 4342 for Control)
    UdpHeader udpHeader;
    // UDP header
    udpHeader.SetDestinationPort(udpDstPort);
    udpHeader.SetSourcePort(udpSrcPort);
    udpHeader.ForceChecksum(0); // set checksum as 0 (RFC 6830)
    // the ip payload size include data + header sizes
    udpHeader.ForcePayloadSize(udpLength);

    // Finally put the headers (UDP the Ipv4)
    packet->AddHeader(udpHeader);

    return packet;
  }

  void
  LispOverIpv4Impl::ChangeItrRloc(Ptr<Packet> &packet, Address address)
  {
    /* Remove UDP header */
    UdpHeader udpHeader;
    packet->RemoveHeader(udpHeader);

    uint8_t buf[packet->GetSize()];
    packet->CopyData(buf, packet->GetSize());
    Ptr<MapRequestMsg> msg = MapRequestMsg::Deserialize(buf);

    msg->SetItrRlocAddrIp(address);

    /*uint8_t BUF_SIZE = 16 + msg->GetAuthDataLen() + 16
          + 12 * msg->GetRecord()->GetLocatorCount();*/
    uint8_t *newbuf = new uint8_t[64];
    msg->Serialize(newbuf);
    Ptr<Packet> p = Create<Packet>(newbuf, 64);
    packet = p;

    /* Add back UDP header */
    packet->AddHeader(udpHeader);
  }

  bool
  LispOverIpv4Impl::IsMapRegister(Ptr<Packet> packet)
  {
    /* Remove UDP header */
    UdpHeader udpHeader;
    packet->RemoveHeader(udpHeader);
    /* Remove ECM header */
    LispEncapsulatedControlMsgHeader ecmHeader;
    packet->RemoveHeader(ecmHeader);
    /* Remove Inner IpHeader */
    Ipv4Header innerHeader;
    packet->RemoveHeader(innerHeader);
    /* Remove inner UDP header */
    UdpHeader innerUdpHeader;
    packet->RemoveHeader(innerUdpHeader);
    /* We arrive at LISP msg */
    uint8_t buf[packet->GetSize()];
    packet->CopyData(buf, packet->GetSize());
    uint8_t msg_type = (buf[0] >> 4);

    return msg_type == static_cast<uint8_t>(MapRegisterMsg::GetMsgType());
  }

  void
  LispOverIpv4Impl::SetNatedEntry(Ptr<Packet> packet, Ipv4Header const &outerHeader)
  {

    Ptr<MapEntry> mapEntryCache = Create<MapEntryImpl>();
    Ptr<MapEntry> mapEntryDB = Create<MapEntryImpl>();

    /* For cache entry: Locator is the translated global address */
    Ptr<Locators> locators = Create<LocatorsImpl>();
    Ptr<Locator> locator = Create<Locator>(outerHeader.GetSource());
    locators->InsertLocator(locator);
    mapEntryCache->SetLocators(locators);

    /* RTR address */
    mapEntryCache->SetRtrRloc(Create<Locator>(outerHeader.GetDestination()));

    UdpHeader udpHeader;
    packet->RemoveHeader(udpHeader);

    /* Translated global port */
    mapEntryCache->SetTranslatedPort(udpHeader.GetSourcePort());

    /* Remove ECM header */
    LispEncapsulatedControlMsgHeader ecmHeader;
    packet->RemoveHeader(ecmHeader);
    /* Remove Inner IpHeader */
    Ipv4Header innerHeader;
    packet->RemoveHeader(innerHeader);

    /* Local (NATed) RLOC address of xTR */
    mapEntryCache->SetXtrLloc(Create<Locator>(innerHeader.GetSource()));

    /* Remove inner UDP header */
    UdpHeader innerUdpHeader;
    packet->RemoveHeader(innerUdpHeader);

    /* We arrive at MapRegister msg */
    uint8_t buf[packet->GetSize()];
    packet->CopyData(buf, packet->GetSize());
    Ptr<MapRegisterMsg> msg = MapRegisterMsg::Deserialize(buf);
    std::stringstream ss;
    Ptr<MapReplyRecord> record = msg->GetRecord();
    Ptr<EndpointId> eid;
    ss << "/" << unsigned(record->GetEidMaskLength());
    Ipv4Mask mask = Ipv4Mask(ss.str().c_str());
    eid = Create<EndpointId>(record->GetEidPrefix(), mask);
    mapEntryCache->SetEidPrefix(eid);
    mapEntryDB->SetEidPrefix(eid);
    /* For DB entry: Locator is the RTR locator */
    mapEntryDB->SetLocators(record->GetLocators());

    /* Set Entry in cache */
    m_mapTablesIpv4->SetEntry(record->GetEidPrefix(), mask, mapEntryCache, MapTables::IN_CACHE);

    /* Set Entry in database */
    m_mapTablesIpv4->SetEntry(record->GetEidPrefix(), mask, mapEntryDB, MapTables::IN_DATABASE);

    /* When RTR receives an ECM encapsulated MapRegister for the EID MN, it adds:
     * - in cache: the entry (EID -> NAT translated address) => Forward data packets to NATed device
     * - in database: the entry (EID -> RTR RLOC) => Answer MapRequests in behalf of NATed device
     *
     * The entry in the cache is enough to forward data packets (addressed to EID) towards
     * the NATed device.
     * However, there is no entry in the cache to forward control packets (addressed to LRLOC)
     * towards the NATed device.
     * Therefore, we manually add such an entry (LRLOC -> NAT translated address) into the cache.
     */
    Ptr<MapEntry> mapEntryCacheControl = Create<MapEntryImpl>();
    mapEntryCacheControl->SetLocators(locators);
    mapEntryCacheControl->SetRtrRloc(Create<Locator>(outerHeader.GetDestination()));
    mapEntryCacheControl->SetTranslatedPort(udpHeader.GetSourcePort());
    mapEntryCacheControl->SetXtrLloc(Create<Locator>(innerHeader.GetSource()));
    Ptr<EndpointId> eidControl = Create<EndpointId>(innerHeader.GetSource(), Ipv4Mask("/32"));
    mapEntryCacheControl->SetEidPrefix(eidControl);
    m_mapTablesIpv4->SetEntry(eidControl->GetEidAddress(), Ipv4Mask("/32"), mapEntryCacheControl, MapTables::IN_CACHE);
  }

} /* namespace ns3 */
