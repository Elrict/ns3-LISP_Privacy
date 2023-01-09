#include "ns3/lisp-over-ipv4-impl-redir.h"
#include "ns3/lisp-over-ipv6-impl.h"
#include <ns3/packet.h>
#include "ns3/address.h"
#include "ns3/assert.h"
#include "ns3/ipv4-address.h"
#include "ns3/ipv4-header.h"
#include "ns3/log.h"
#include "ns3/udp-header.h"
#include "ns3/udp-l4-protocol.h"
#include "ns3/lisp-header.h"
#include "ns3/ptr.h"
#include "ns3/simple-map-tables.h"
#include <ns3/ipv4-l3-protocol.h>
#include "ns3/rloc-metrics.h"
#include "ns3/lisp-over-ip.h"
#include "ns3/lisp-mapping-socket.h"
#include "ns3/boolean.h"

namespace ns3
{

  NS_LOG_COMPONENT_DEFINE("LispOverIpv4ImplRedir");

  NS_OBJECT_ENSURE_REGISTERED(LispOverIpv4ImplRedir);

  TypeId
  LispOverIpv4ImplRedir::GetTypeId()
  {
    static TypeId tid = TypeId("ns3::LispOverIpv4ImplRedir")
                            .SetParent<LispOverIpv4>()
                            .SetGroupName("Lisp")
                            .AddAttribute("ServiceAddress", "The address of the service.",
                                          Ipv4AddressValue(),
                                          MakeIpv4AddressAccessor(&LispOverIpv4ImplRedir::m_srvAddr),
                                          MakeIpv4AddressChecker())
                            .AddAttribute("ServiceInterface", "The interface of the server",
                                          PointerValue(),
                                          MakePointerAccessor(&LispOverIpv4ImplRedir::m_srvInterface),
                                          MakePointerChecker<Ipv4Interface>())
                            .AddAttribute("CheckRloc",
                                          "Activate or not the address check privacy feature.",
                                          BooleanValue(false),
                                          MakeBooleanAccessor(&LispOverIpv4ImplRedir::m_rlocCheck),
                                          MakeBooleanChecker())
                            .AddAttribute("CheckEid",
                                          "Activate or not the address check privacy feature.",
                                          BooleanValue(false),
                                          MakeBooleanAccessor(&LispOverIpv4ImplRedir::m_eidCheck),
                                          MakeBooleanChecker())
                            .AddAttribute("RlocRedir",
                                          "Activate or not the rloc redirection privacy feature.",
                                          BooleanValue(false),
                                          MakeBooleanAccessor(&LispOverIpv4ImplRedir::m_rlocRedir),
                                          MakeBooleanChecker())
                            .AddAttribute("RlocInterface", "The interface of the xtr",
                                          PointerValue(),
                                          MakePointerAccessor(&LispOverIpv4ImplRedir::m_rlocInterface),
                                          MakePointerChecker<Ipv4Interface>())
                            .AddAttribute("ReverseNating",
                                          "Activate or not the natting privacy feature.",
                                          BooleanValue(false),
                                          MakeBooleanAccessor(&LispOverIpv4ImplRedir::m_passive),
                                          MakeBooleanChecker())
                            .AddAttribute("Nating",
                                          "Activate or not the natting privacy feature.",
                                          BooleanValue(false),
                                          MakeBooleanAccessor(&LispOverIpv4ImplRedir::m_natting),
                                          MakeBooleanChecker())
                            .AddAttribute(
                                "HashVariable",
                                "Hash processing time",
                                StringValue("ns3::ConstantRandomVariable[Constant=0.00004]"),
                                MakePointerAccessor(&LispOverIpv4ImplRedir::m_hashTime),
                                MakePointerChecker<RandomVariableStream>())
                            // TODO: use this to have united data plen
                            .AddConstructor<LispOverIpv4ImplRedir>();
    return tid;
  }

  LispOverIpv4ImplRedir::LispOverIpv4ImplRedir()
  {
    NS_LOG_FUNCTION(this);
    // Init MapTables
  }

  LispOverIpv4ImplRedir::~LispOverIpv4ImplRedir()
  {
    NS_LOG_FUNCTION(this);
  }

  bool LispOverIpv4ImplRedir::CheckEid(Ipv4Address source, Ipv4Address destination)
  {
    Hasher hs;
    std::ostringstream ss;

    source.Print(ss);

    int index = hs.GetHash32(ss.str()) % (m_srvInterface->GetNAddresses());
    bool reject = m_srvInterface->GetAddress(index).GetLocal().Get() == destination.Get();
    if (!reject)
    {
      std::cout << "xtrs::Check Eid didn't match..." << std::endl;
    }
    return reject;
  }

  bool LispOverIpv4ImplRedir::CheckRloc(Ipv4Address rloc, Ipv4Address source)
  {
    if (rloc.Get() == Ipv4Address("10.1.1.1").Get())
      return true;

    Ipv4Address addr = GenerateAddress(source.CombineMask(Ipv4Mask("/24")), m_rlocInterface);
    if (addr.Get() != rloc.Get())
    {
      std::cout << "* RLOCREDIR: Dest Address received: " << rloc << std::endl;
      std::cout << "source: " << source << std::endl;
      std::cout << "* RLOCREDIR: Dest Address wanted: " << addr << std::endl;
    }
    NS_ASSERT(addr.Get() == rloc.Get());
    return addr.Get() == rloc.Get();
  }

  Ipv4Address LispOverIpv4ImplRedir::GenerateAddress(Address peer, Ptr<Ipv4Interface> iface)
  {
    Hasher hs;
    std::ostringstream ss;
    Ipv4Address::ConvertFrom(peer).Print(ss);

    int index = hs.GetHash32(ss.str()) % (iface->GetNAddresses());
    Ipv4Address addr = iface->GetAddress(index).GetLocal();
    return addr;
  }

  int LispOverIpv4ImplRedir::FindAddress(Ipv4Address addr)
  {
    for (size_t i = 0; i < m_srvInterface->GetNAddresses(); i++)
    {

      if (addr.Get() == m_srvInterface->GetAddress(i).GetLocal().Get())
      {
        return i;
      }
    }
    return -1;
  }

  void LispOverIpv4ImplRedir::LispOutput(Ptr<Packet> packet, Ipv4Header const &innerHeader,
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
    if (m_passive)
    {
      Ipv4Header x = innerHeader;
      x.SetSource(m_srvAddr);
      packet->AddHeader(x);
    }
    else
    {
      packet->AddHeader(innerHeader);
    }

    // add inner header before encapsulating whole packet

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
      if (m_rlocRedir)
      {
        outerHeader.SetSource(GenerateAddress(innerHeader.GetDestination(), m_rlocInterface));
      }
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
  void LispOverIpv4ImplRedir::LispInput(Ptr<Packet> packet, Ipv4Header const &outerHeader, bool lisp)
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
        if (isControlPlanMsg)
        {
          Ptr<Ipv4L3Protocol> ipv4 = (node->GetObject<Ipv4L3Protocol>());
          // put it back in ip Receive ()
          // Attention: it's the method LispOverIpv4::RecordReceiveParams that
          // retrieve m_currentDevice values!!! This method is called in
          // Ipv4L3Protocol::Received() method!

          ipv4->Receive(m_currentDevice, packet, m_ipProtocol, from, to, m_currentPacketType);
          NS_LOG_DEBUG("Re-inject the packet in receive to forward it to: " << innerIpv4Header.GetDestination());
        }
        else if (isMappingForPacket)
        {
          int checks_done = 0;

          // ns3 privacy addition
          if (m_eidCheck)
          {
            // We eidcheck only the addresses that are destined to one of the address of the server.
            // Thus no check if destined to the xtr.
            if (!(innerIpv4Header.GetDestination().Get() == GetNode()->GetObject<Ipv4>()->GetObject<Ipv4L3Protocol>()->GetInterface(2)->GetAddress(0).GetLocal().Get()))
            {
              int index = FindAddress(innerIpv4Header.GetDestination());

              // Thus no check if not destined to server.
              if (index != -1)
              {
                if (!CheckEid(innerIpv4Header.GetSource(), innerIpv4Header.GetDestination()))
                {
                  std::cout << "XTR::Failed eid address check in the xtr." << std::endl;
                  return;
                }
                // checks_done += 1;
              }
            }
          }
          else if (m_passive)
          {
            if (Ipv4Address::ConvertFrom(to) == m_srvAddr)
            {
              uint32_t ip = Ipv4Address::ConvertFrom(from).Get();
              to = static_cast<Address>(GenerateAddress(from, m_srvInterface));
              packet->RemoveHeader(innerIpv4Header);
              innerIpv4Header.SetDestination(Ipv4Address::ConvertFrom(to));
              packet->AddHeader(innerIpv4Header);

              if(m_privacyDone.find(ip) == m_privacyDone.end()){
                m_privacyDone[ip] = true;
                checks_done += 1;

              }
            }
            else if (FindAddress(Ipv4Address::ConvertFrom(to)) != -1)
            {
              // if a packet uses one of the server prefix address as destination address directly
              // drop;
              return;
            }
          }
          else if (m_natting)
          {
            std::cout << m_srvAddr << std::endl;
          }
          if (m_rlocCheck)
          {
            if (!CheckRloc(outerHeader.GetDestination(), innerIpv4Header.GetSource()))
            {
              std::cout << "XTR::Failed rloc address check in the xtr." << std::endl;
              return;
            }
            // checks_done += 1;
          }

          m_clientswaiting +=1;

          Simulator::Schedule((Seconds(m_hashTime->GetValue() * checks_done)*m_clientswaiting), &LispOverIpv4ImplRedir::DelayedReceive, this, m_currentDevice, packet, m_ipProtocol, from, to, m_currentPacketType);

          //*****
          NS_LOG_DEBUG("Re-inject the packet in receive to forward it to: " << innerIpv4Header.GetDestination());
        }
        else
        {
          this->m_mapTablesIpv4->Print(std::cout);
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

  void LispOverIpv4ImplRedir::DelayedReceive(Ptr<NetDevice> device, Ptr<const Packet> p, uint16_t protocol, const Address &from, const Address &to, NetDevice::PacketType packetType)
  {
    Ptr<Ipv4L3Protocol> ipv4 = (GetNode()->GetObject<Ipv4L3Protocol>());
    ipv4->Receive(device, p, protocol, from, to, packetType);
    m_clientswaiting -=1;
  }
} /* namespace ns3 */
