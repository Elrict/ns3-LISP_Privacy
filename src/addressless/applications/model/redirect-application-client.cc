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
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/packet.h"
#include "ns3/boolean.h"
#include "ns3/uinteger.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/onoff-application.h"
#include "ns3/udp-socket-factory.h"
#include "redirect-header.h"
#include "ns3/string.h"
#include "ns3/pointer.h"
#include "ns3/redirect-application-client.h"

namespace ns3
{

  NS_LOG_COMPONENT_DEFINE("RedirectApplicationClient");

  NS_OBJECT_ENSURE_REGISTERED(RedirectApplicationClient);

  TypeId
  RedirectApplicationClient::GetTypeId(void)
  {
    static TypeId tid = TypeId("ns3::RedirectApplicationClient")
                            .SetParent<Application>()
                            .SetGroupName("Applications")
                            .AddConstructor<RedirectApplicationClient>()
                            .AddAttribute("PacketSize", "The size of packets sent in on state",
                                          UintegerValue(512),
                                          MakeUintegerAccessor(&RedirectApplicationClient::m_pktSize),
                                          MakeUintegerChecker<uint32_t>(1))
                            .AddAttribute("Remote", "The address of the destination",
                                          AddressValue(),
                                          MakeAddressAccessor(&RedirectApplicationClient::m_entrance),
                                          MakeAddressChecker())
                            .AddAttribute("Local", "The address of the socket",
                                          AddressValue(),
                                          MakeAddressAccessor(&RedirectApplicationClient::m_local),
                                          MakeAddressChecker())
                            .AddAttribute("MaxBytes",
                                          "The total number of bytes to send. Once these bytes are sent, "
                                          "no packet is sent again, even in on state. The value zero means "
                                          "that there is no limit.",
                                          UintegerValue(0),
                                          MakeUintegerAccessor(&RedirectApplicationClient::m_maxBytes),
                                          MakeUintegerChecker<uint64_t>())
                            .AddAttribute("Protocol", "The type of protocol to use. This should be "
                                                      "a subclass of ns3::SocketFactory",
                                          TypeIdValue(UdpSocketFactory::GetTypeId()),
                                          MakeTypeIdAccessor(&RedirectApplicationClient::m_tid),
                                          // This should check for SocketFactory as a parent
                                          MakeTypeIdChecker())
                            .AddAttribute(
                                "CoChangeTime",
                                "",
                                StringValue("ns3::ConstantRandomVariable[Constant=0.01]"),
                                MakePointerAccessor(&RedirectApplicationClient::m_changeTime),
                                MakePointerChecker<RandomVariableStream>())
                            .AddTraceSource("Tx", "A new packet is created and is sent",
                                            MakeTraceSourceAccessor(&RedirectApplicationClient::m_txTrace),
                                            "ns3::Packet::TracedCallback");

    return tid;
  }

  RedirectApplicationClient::RedirectApplicationClient()
      : m_entranceSocket(0),
        m_serverSocket(0)
  {
    NS_LOG_FUNCTION(this);
  }

  RedirectApplicationClient::~RedirectApplicationClient()
  {
    NS_LOG_FUNCTION(this);
  }

  void
  RedirectApplicationClient::SetMaxBytes(uint64_t maxBytes)
  {
    NS_LOG_FUNCTION(this << maxBytes);
    m_maxBytes = maxBytes;
  }

  Ptr<Socket>
  RedirectApplicationClient::GetSocket(void) const
  {
    NS_LOG_FUNCTION(this);
    return m_serverSocket;
  }

  void
  RedirectApplicationClient::DoDispose(void)
  {
    NS_LOG_FUNCTION(this);

    m_entranceSocket = 0;
    m_serverSocket = 0;
    // chain up
    Application::DoDispose();
  }

  // Application Methods
  void RedirectApplicationClient::StartApplication() // Called at time specified by Start
  {
    NS_LOG_FUNCTION(this);
    m_entranceSocket = SetupSocket(m_entrance);
    SendPacket(m_entranceSocket);
  }

  Ptr<Socket> RedirectApplicationClient::SetupSocket(Address addr)
  {
    Ptr<Socket> socket = Socket::CreateSocket(GetNode(), m_tid);
    if (socket->Bind(m_local) == -1)
    {
      NS_FATAL_ERROR("Failed to bind socket");
    }
    socket->Connect(addr);

    socket->SetConnectCallback(
        MakeCallback(&RedirectApplicationClient::ConnectionSucceeded, this),
        MakeCallback(&RedirectApplicationClient::ConnectionFailed, this));

    socket->SetRecvCallback(MakeCallback(&RedirectApplicationClient::ReceivedDataCallback, this));
    return socket;
  }
  void RedirectApplicationClient::ContactServer(Ptr<Packet> packet)
  {
    // Closing entrance socket, not needed anymore
    m_entranceSocket->Close();
    m_entranceSocket = 0;

    uint8_t *buffer = new uint8_t[packet->GetSize()];
    packet->CopyData(buffer, packet->GetSize());

    Address mainAddress = Address();
    mainAddress.CopyFrom(buffer, packet->GetSize());

    NS_LOG_DEBUG("RedirectApplicationClient::Received IP of the entrance module : " << Ipv4Address::ConvertFrom(mainAddress));
    // Using another port to avoid waiting for the entrance socket tobe done closing (tcp closing)
    m_local = InetSocketAddress(InetSocketAddress::ConvertFrom(m_local).GetIpv4(), InetSocketAddress::ConvertFrom(m_local).GetPort() + 1);
    m_serverSocket = SetupSocket(InetSocketAddress(Ipv4Address::ConvertFrom(mainAddress), 50000));
    
    SendPacket(m_serverSocket);

  }
  void RedirectApplicationClient::ReceivedDataCallback(Ptr<Socket> socket)
  {
    Address addre;
    socket->GetPeerName(addre);

    NS_LOG_FUNCTION(this << socket);
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom(from)))
    {
      if (packet->GetSize() > 0)
      {
        RedirectHeader redirH;
        packet->RemoveHeader(redirH);

        if (redirH.GetRedirect() == 1)
        {
          Simulator::Schedule(Seconds(m_changeTime->GetValue()), &RedirectApplicationClient::ContactServer, this, packet);
        }
        else
        {
          return;
        }
      }
    }
  }

  void RedirectApplicationClient::StopApplication() // Called at time specified by Stop
  {
    NS_LOG_FUNCTION(this);

    if (m_entranceSocket != 0)
    {
      NS_ASSERT(m_entranceSocket->Close() != -1);
    }
    if (m_serverSocket != 0)
    {
      NS_ASSERT(m_serverSocket->Close() != -1);
    }
  }

  void RedirectApplicationClient::SendPacket(Ptr<Socket> sock)
  {
    NS_LOG_FUNCTION(this);
    Ptr<Packet> packet = Create<Packet>(m_pktSize);
    m_txTrace(packet);
    sock->Send(packet);
  }

  void RedirectApplicationClient::ConnectionSucceeded(Ptr<Socket> socket)
  {
    NS_LOG_FUNCTION(this << socket);
  }

  void RedirectApplicationClient::ConnectionFailed(Ptr<Socket> socket)
  {
    NS_LOG_FUNCTION(this << socket);
  }

} // Namespace ns3
