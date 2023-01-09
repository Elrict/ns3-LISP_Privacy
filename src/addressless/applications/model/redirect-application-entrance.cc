#include "ns3/log.h"
#include "ns3/address.h"
#include "ns3/inet-socket-address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/packet-socket-address.h"
#include "ns3/node.h"
#include "ns3/nstime.h"
#include "ns3/data-rate.h"
#include "ns3/random-variable-stream.h"
#include "ns3/socket.h"
#include "ns3/object-ptr-container.h"
#include <ns3/tcp-socket.h>
#include <ns3/tcp-socket-factory.h>
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/string.h"
#include "ns3/pointer.h"
#include "ns3/redirect-application-entrance.h"
#include "redirect-header.h"

namespace ns3
{

  NS_LOG_COMPONENT_DEFINE("RedirectApplicationEntrance");

  NS_OBJECT_ENSURE_REGISTERED(RedirectApplicationEntrance);

  TypeId
  RedirectApplicationEntrance::GetTypeId(void)
  {
    static TypeId tid = TypeId("ns3::RedirectApplicationEntrance")
                            .SetParent<Application>()
                            .SetGroupName("Applications")
                            .AddConstructor<RedirectApplicationEntrance>()
                            .AddAttribute("PacketSize", "The size of packets sent in on state",
                                          UintegerValue(512),
                                          MakeUintegerAccessor(&RedirectApplicationEntrance::m_pktSize),
                                          MakeUintegerChecker<uint32_t>(1))
                            .AddAttribute("Local", "The address of the socket",
                                          AddressValue(),
                                          MakeAddressAccessor(&RedirectApplicationEntrance::m_localAddress),
                                          MakeAddressChecker())
                            .AddAttribute("MainServer", "The interface of the server",
                                          PointerValue(),
                                          MakePointerAccessor(&RedirectApplicationEntrance::m_serverInterface),
                                          MakePointerChecker<Ipv4Interface>())
                            .AddAttribute(
                                "HashVariable",
                                "Hash processing time",
                                StringValue("ns3::ConstantRandomVariable[Constant=0.00004]"),
                                MakePointerAccessor(&RedirectApplicationEntrance::m_hashTime),
                                MakePointerChecker<RandomVariableStream>())
                            .AddAttribute("Protocol",
                                          "The type id of the protocol to use for the rx socket.",
                                          TypeIdValue(UdpSocketFactory::GetTypeId()),
                                          MakeTypeIdAccessor(&RedirectApplicationEntrance::m_tid),
                                          MakeTypeIdChecker())
                            .AddTraceSource("Tx", "A new packet is created and is sent",
                                            MakeTraceSourceAccessor(&RedirectApplicationEntrance::m_txTrace),
                                            "ns3::Packet::TracedCallback");
    return tid;
  }

  RedirectApplicationEntrance::RedirectApplicationEntrance()
      : m_listenSocket(0),
        m_connected(false),
        m_residualBits(0),
        m_totBytes(0),
        m_clients_waiting(0)
  {
    NS_LOG_FUNCTION(this);
  }

  RedirectApplicationEntrance::~RedirectApplicationEntrance()
  {
    NS_LOG_FUNCTION(this);
  }

  Ptr<Socket>
  RedirectApplicationEntrance::GetSocket(void) const
  {
    NS_LOG_FUNCTION(this);
    return m_listenSocket;
  }

  void
  RedirectApplicationEntrance::DoDispose(void)
  {
    NS_LOG_FUNCTION(this);

    m_listenSocket = 0;
    // chain up
    Application::DoDispose();
  }

  // Application Methods
  void RedirectApplicationEntrance::StartApplication() // Called at time specified by Start
  {
    NS_LOG_FUNCTION(this);

    // Create the socket if not already
    if (!m_listenSocket)
    {
      m_listenSocket = Socket::CreateSocket(GetNode(), m_tid);
      NS_LOG_FUNCTION(this << m_listenSocket);
      if (m_listenSocket->Bind(m_localAddress) == -1)
      {
        NS_FATAL_ERROR("Failed to bind socket");
      }

      m_listenSocket->Listen();
      m_listenSocket->SetAcceptCallback(MakeCallback(&RedirectApplicationEntrance::ConnectionRequestCallback,
                                                     this),
                                        MakeCallback(&RedirectApplicationEntrance::NewConnectionCreatedCallback,
                                                     this));

      m_listenSocket->SetRecvCallback(MakeCallback(&RedirectApplicationEntrance::ReceivedDataCallback, this));
    }
    m_cbrRateFailSafe = m_cbrRate;
  }
  void RedirectApplicationEntrance::StopApplication() // Called at time specified by Stop
  {
    NS_LOG_FUNCTION(this);

    if (m_listenSocket != 0)
    {
      m_listenSocket->Close();
    }
    else
    {
      NS_LOG_WARN("RedirectApplicationEntrance found null socket to close in StopApplication");
    }
  }

  Ptr<Packet> RedirectApplicationEntrance::GeneratePacket(Address peer)
  {
    Ipv4Address addr = GenerateAddress(peer);

    NS_LOG_DEBUG("RedirectApplicationEntrance::Chosen IP of the entrance module : " << addr);
    Address serverAddress = Address(addr);
    uint8_t *address = new uint8_t[4];
    serverAddress.CopyTo(address);

    RedirectHeader redirH;
    redirH.SetRedirect(true);
    Ptr<Packet> packet = Create<Packet>(address, 4);
    packet->AddHeader(redirH);
    return packet;
  }
  Ipv4Address RedirectApplicationEntrance::GenerateAddress(Address peer)
  {
    Hasher hs;

    std::ostringstream ss;
    InetSocketAddress iaddr = InetSocketAddress(10);

    iaddr = InetSocketAddress::ConvertFrom(peer);
    iaddr.GetIpv4().Print(ss);

    int index = hs.GetHash32(ss.str()) % (m_serverInterface->GetNAddresses());
    Ipv4Address addr = m_serverInterface->GetAddress(index).GetLocal();
    return addr;
  }
  void RedirectApplicationEntrance::SendPacket(Ptr<Socket> coSocket, Address peer)
  {
    NS_LOG_FUNCTION(this);

    Ptr<Packet> packet = GeneratePacket(peer);
    m_txTrace(packet);

    if (m_tid == UdpSocketFactory::GetTypeId())
    {
      coSocket->SendTo(packet, 0, peer);
    }
    else
    {
      coSocket->Send(packet);
    }
    m_totBytes += m_pktSize;
    m_residualBits = 0;
    m_clients_waiting = m_clients_waiting - 1;
  }
  /*******************************/
  //==        CALBACKS         ==//
  /*******************************/
  void RedirectApplicationEntrance::ReceivedDataCallback(Ptr<Socket> coSocket)
  {

    NS_LOG_FUNCTION(this << coSocket);
    Address from;
    Ptr<Packet> packet;
    m_clients_waiting = m_clients_waiting +1;
    while ((packet = coSocket->RecvFrom(from)))
    { 
      Simulator::Schedule(m_clients_waiting*Seconds(m_hashTime->GetValue()), &RedirectApplicationEntrance::SendPacket, this, coSocket, from);
    }
  }

  bool
  RedirectApplicationEntrance::ConnectionRequestCallback(Ptr<Socket> coSocket,
                                                         const Address &address)
  {
    NS_LOG_FUNCTION(this << coSocket << address);
    return true; // Unconditionally accept the connection request.
  }

  void
  RedirectApplicationEntrance::NewConnectionCreatedCallback(Ptr<Socket> coSocket,
                                                            const Address &address)
  {
    NS_LOG_FUNCTION(this << coSocket << address);
    coSocket->SetRecvCallback(MakeCallback(&RedirectApplicationEntrance::ReceivedDataCallback,
                                           this));
  }

} // Namespace ns3
