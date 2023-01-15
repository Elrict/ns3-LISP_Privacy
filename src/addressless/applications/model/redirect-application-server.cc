#include "ns3/address.h"
#include "ns3/address-utils.h"
#include "ns3/log.h"
#include "ns3/inet-socket-address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/node.h"
#include "ns3/socket.h"
#include "ns3/udp-socket.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/packet.h"
#include "ns3/trace-source-accessor.h"
#include "redirect-header.h"
#include "ns3/udp-socket-factory.h"
#include "redirect-application-server.h"

namespace ns3
{

  NS_LOG_COMPONENT_DEFINE("RedirectApplicationServer");

  NS_OBJECT_ENSURE_REGISTERED(RedirectApplicationServer);

  TypeId
  RedirectApplicationServer::GetTypeId(void)
  {
    static TypeId tid = TypeId("ns3::RedirectApplicationServer")
                            .SetParent<Application>()
                            .SetGroupName("Applications")
                            .AddConstructor<RedirectApplicationServer>()
                            .AddAttribute("Local", "The interface of the server",
                                          PointerValue(),
                                          MakePointerAccessor(&RedirectApplicationServer::m_interface),
                                          MakePointerChecker<Ipv4Interface>())
                            .AddAttribute("Protocol",
                                          "The type id of the protocol to use for the rx socket.",
                                          TypeIdValue(UdpSocketFactory::GetTypeId()),
                                          MakeTypeIdAccessor(&RedirectApplicationServer::m_tid),
                                          MakeTypeIdChecker())
                            .AddAttribute("CheckAddress",
                                          "Activate or not the address check privacy feature.",
                                          BooleanValue(false),
                                          MakeBooleanAccessor(&RedirectApplicationServer::m_check),
                                          MakeBooleanChecker())
                            .AddAttribute(
                                "HashVariable",
                                "Hash processing time",
                                StringValue("ns3::ConstantRandomVariable[Constant=0.00004]"),
                                MakePointerAccessor(&RedirectApplicationServer::m_hashTime),
                                MakePointerChecker<RandomVariableStream>())
                            .AddTraceSource("Rx",
                                            "A packet has been received",
                                            MakeTraceSourceAccessor(&RedirectApplicationServer::m_rxTrace),
                                            "ns3::Packet::AddressTracedCallback")
                            .AddTraceSource("ConnectionEstablished",
                                            "Final Packet has been received",
                                            MakeTraceSourceAccessor(&RedirectApplicationServer::m_connectionEstablished),
                                            "ns3::TracedValueCallback::Int32");
    return tid;
  }

  RedirectApplicationServer::RedirectApplicationServer()
  {
    NS_LOG_FUNCTION(this);
    m_totalRx = 0;
    m_clients_waiting = 0;
  }

  RedirectApplicationServer::~RedirectApplicationServer()
  {
    NS_LOG_FUNCTION(this);
  }

  uint64_t RedirectApplicationServer::GetTotalRx() const
  {
    NS_LOG_FUNCTION(this);
    return m_totalRx;
  }

  std::list<Ptr<Socket>>
  RedirectApplicationServer::GetListeningSockets(void) const
  {
    NS_LOG_FUNCTION(this);
    return m_listeningSockets;
  }

  std::list<Ptr<Socket>>
  RedirectApplicationServer::GetAcceptedSockets(void) const
  {
    NS_LOG_FUNCTION(this);
    return m_socketList;
  }

  void RedirectApplicationServer::DoDispose(void)
  {
    NS_LOG_FUNCTION(this);
    m_listeningSockets.clear();
    m_socketList.clear();

    // chain up
    Application::DoDispose();
  }

  // Application Methods
  void RedirectApplicationServer::StartApplication() // Called at time specified by Start
  {
    NS_LOG_FUNCTION(this);
    Ptr<Socket> socket;
    NS_LOG_DEBUG("RedirectApplicationServer::Number of Addresses available: " << m_interface->GetNAddresses());

    for (size_t i = 0; i < m_interface->GetNAddresses(); i++)
    {
      socket = Socket::CreateSocket(GetNode(), m_tid);
      InetSocketAddress addr = InetSocketAddress(m_interface->GetAddress(i).GetLocal(), 50000);
      NS_LOG_DEBUG("RedirectApplicationServer::Socket created on address: " << m_interface->GetAddress(i).GetLocal());
      if (socket->Bind(addr) == -1)
      {
        NS_FATAL_ERROR("Failed to bind socket");
      }
      socket->Listen();
      socket->ShutdownSend();

      socket->SetRecvCallback(MakeCallback(&RedirectApplicationServer::HandleRead, this));
      socket->SetAcceptCallback(
          MakeCallback(&RedirectApplicationServer::HandleNewConnection, this),
          MakeCallback(&RedirectApplicationServer::HandleAccept, this));
      socket->SetCloseCallbacks(
          MakeCallback(&RedirectApplicationServer::HandlePeerClose, this),
          MakeCallback(&RedirectApplicationServer::HandlePeerError, this));
      m_listeningSockets.push_back(socket);
    }
  }

  void RedirectApplicationServer::StopApplication() // Called at time specified by Stop
  {
    NS_LOG_FUNCTION(this);
    while (!m_socketList.empty()) // these are accepted sockets, close them
    {
      Ptr<Socket> acceptedSocket = m_socketList.front();
      m_socketList.pop_front();
      acceptedSocket->Close();
    }
    while (!m_listeningSockets.empty())
    {
      Ptr<Socket> socket = m_listeningSockets.front();
      m_listeningSockets.pop_front();
      socket->Close();
      socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
    }
  }

  void RedirectApplicationServer::HandleRead(Ptr<Socket> socket)
  {
    NS_LOG_FUNCTION(this << socket);
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom(from)))
    {
      if (packet->GetSize() == 0)
      { // EOF
        break;
      }
      m_totalRx += packet->GetSize();
      m_rxTrace(packet, from);
    }
  }

  void RedirectApplicationServer::HandlePeerClose(Ptr<Socket> socket)
  {
    NS_LOG_FUNCTION(this << socket);
  }

  void RedirectApplicationServer::HandlePeerError(Ptr<Socket> socket)
  {
    NS_LOG_FUNCTION(this << socket);
  }


  bool RedirectApplicationServer::CheckAddress(Ptr<Socket> s, const Address &from)
  {
    Hasher hs;
    std::ostringstream ss;

    Address addre;
    InetSocketAddress socketAddress = InetSocketAddress(10);
    s->GetSockName(addre);
    socketAddress = InetSocketAddress::ConvertFrom(addre);

    InetSocketAddress peerAddress = InetSocketAddress::ConvertFrom(from);
    peerAddress.GetIpv4().Print(ss);

    int index = hs.GetHash32(ss.str()) % (m_interface->GetNAddresses());
    bool reject = m_interface->GetAddress(index).GetLocal().Get() == socketAddress.GetIpv4().Get();
    if (!reject)
    {
      std::cout << "Server::Check Address didn't match..." << std::endl;
    }
    return reject;
  }
  
  bool RedirectApplicationServer::HandleNewConnection(Ptr<Socket> s, const Address &from)
  {
    if (m_check)
      return CheckAddress(s, from);

    return true;
  }
  void RedirectApplicationServer::HandleAccept(Ptr<Socket> s, const Address &from)
  {
    NS_LOG_FUNCTION(this << s << from);
    s->SetRecvCallback(MakeCallback(&RedirectApplicationServer::HandleRead, this));
    m_socketList.push_back(s);
    m_clients_waiting = m_clients_waiting + 1;
    Simulator::Schedule(m_check ? m_clients_waiting * Seconds(m_hashTime->GetValue()) : Seconds(0), &RedirectApplicationServer::HandleClientConnectionTimer, this, m_socketList.size());
  }

  void RedirectApplicationServer::HandleClientConnectionTimer(int id){
    m_connectionEstablished(id);
    m_clients_waiting = m_clients_waiting - 1;
  }

} // Namespace ns3
