#ifndef REDIREC_APP_SERVER_H
#define REDIREC_APP_SERVER_H

#include "ns3/address.h"
#include "ns3/application.h"
#include "ns3/event-id.h"
#include "ns3/ptr.h"
#include "ns3/data-rate.h"
#include "ns3/traced-callback.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"

namespace ns3
{

  class Address;
  class Socket;
  class Packet;

  /**
   * \ingroup applications
   * \defgroup RedirectApplicationServer RedirectApplicationServer
   *
   * This application was written to complement OnOffApplication, but it
   * is more general so a RedirectApplicationServer name was selected.  Functionally it is
   * important to use in multicast situations, so that reception of the layer-2
   * multicast frames of interest are enabled, but it is also useful for
   * unicast as an example of how you can write something simple to receive
   * packets at the application layer.  Also, if an IP stack generates
   * ICMP Port Unreachable errors, receiving applications will be needed.
   */

  /**
   * \ingroup RedirectApplicationServer
   *
   * \brief Receive and consume traffic generated to an IP address and port
   *
   * This application was written to complement OnOffApplication, but it
   * is more general so a RedirectApplicationServer name was selected.  Functionally it is
   * important to use in multicast situations, so that reception of the layer-2
   * multicast frames of interest are enabled, but it is also useful for
   * unicast as an example of how you can write something simple to receive
   * packets at the application layer.  Also, if an IP stack generates
   * ICMP Port Unreachable errors, receiving applications will be needed.
   *
   * The constructor specifies the Address (IP address and port) and the
   * transport protocol to use.   A virtual Receive () method is installed
   * as a callback on the receiving socket.  By default, when logging is
   * enabled, it prints out the size of packets and their address.
   * A tracing source to Receive() is also available.
   */
  class RedirectApplicationServer : public Application
  {
  public:
    /**
     * \brief Get the type ID.
     * \return the object TypeId
     */
    static TypeId GetTypeId(void);
    RedirectApplicationServer();

    virtual ~RedirectApplicationServer();

    /**
     * \return the total bytes received in this sink app
     */
    uint64_t GetTotalRx() const;

    /**
     * \return pointer to listening socket
     */
    std::list<Ptr<Socket>> GetListeningSockets(void) const;

    /**
     * \return list of pointers to accepted sockets
     */
    std::list<Ptr<Socket>> GetAcceptedSockets(void) const;

  protected:
    virtual void DoDispose(void);

  private:
    // inherited from Application base class.
    virtual void StartApplication(void); // Called at time specified by Start
    virtual void StopApplication(void);  // Called at time specified by Stop
    bool CheckAddress(Ptr<Socket> s, const Address &from);

    /**
     * \brief Handle a packet received by the application
     * \param socket the receiving socket
     */
    void HandleRead(Ptr<Socket> socket);
    /**
     * \brief Handle an incoming connection
     * \param socket the incoming connection socket
     * \param from the address the connection is from
     */
    void HandleAccept(Ptr<Socket> socket, const Address &from);
    /**
     * \brief Handle an connection close
     * \param socket the connected socket
     */
    void HandlePeerClose(Ptr<Socket> socket);
    /**
     * \brief Handle an connection error
     * \param socket the connected socket
     */
    void HandlePeerError(Ptr<Socket> socket);

    bool HandleNewConnection(Ptr<Socket> socket, const Address &from);
    void HandleClientConnectionTimer(int id);


    // In the case of TCP, each socket accept returns a new socket, so the
    // listening socket is stored separately from the accepted sockets
    Ptr<Ipv4Interface> m_interface; //!< Listening socket
    std::list<Ptr<Socket>> m_listeningSockets;
    std::list<Ptr<Socket>> m_socketList; //!< the accepted sockets
    bool m_check = false;

    uint64_t m_totalRx; //!< Total bytes received
    TypeId m_tid;       //!< Protocol TypeId

    /// Traced Callback: received packets, source address.
    TracedCallback<Ptr<const Packet>, const Address &> m_rxTrace;
    TracedCallback<int32_t> m_connectionEstablished;
      Ptr<RandomVariableStream> m_hashTime;

    int m_clients_waiting;

  };

} // namespace ns3

#endif /* PACKET_SINK_H */
