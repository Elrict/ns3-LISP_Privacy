#ifndef RedirectApplicationClient_H
#define RedirectApplicationClient_H

#include "ns3/address.h"
#include "ns3/application.h"
#include "ns3/event-id.h"
#include "ns3/ptr.h"
#include "ns3/data-rate.h"
#include "ns3/traced-callback.h"

namespace ns3
{

  class Address;
  class RandomVariableStream;
  class Socket;

  class RedirectApplicationClient : public Application
  {
  public:
    /**
     * \brief Get the type ID.
     * \return the object TypeId
     */
    static TypeId GetTypeId(void);

    RedirectApplicationClient();

    virtual ~RedirectApplicationClient();

    /**
     * \brief Set the total number of bytes to send.
     *
     * Once these bytes are sent, no packet is sent again, even in on state.
     * The value zero means that there is no limit.
     *
     * \param maxBytes the total number of bytes to send
     */
    void SetMaxBytes(uint64_t maxBytes);

    /**
     * \brief Return a pointer to associated socket.
     * \return pointer to associated socket
     */
    Ptr<Socket> GetSocket(void) const;

  protected:
    virtual void DoDispose(void);

  private:
    // inherited from Application base class.
    virtual void StartApplication(void); // Called at time specified by Start
    virtual void StopApplication(void);  // Called at time specified by Stop

    // Event handlers
    /**
     * \brief Send a packet
     */
    void SendPacket(Ptr<Socket> socket);

    void ReceivedDataCallback(Ptr<Socket> socket);
    Ptr<Socket> SetupSocket(Address addr);

    Ptr<Socket> m_entranceSocket; //!< Associated socket
    Ptr<Socket> m_serverSocket;   //!< Associated socket

    Address m_entrance; //!< Peer address
    Address m_local;
    uint32_t m_pktSize;  //!< Size of packets
    uint64_t m_maxBytes; //!< Limit total number of bytes sent
    TypeId m_tid;        //!< Type of the socket used

    /// Traced Callback: transmitted packets.
    TracedCallback<Ptr<const Packet>> m_txTrace;

  private:
    void ContactServer(Ptr<Packet> packet);

    void ConnectionSucceeded(Ptr<Socket> socket);
    /**
     * \brief Handle a Connection Failed event
     * \param socket the not connected socket
     */
    void ConnectionFailed(Ptr<Socket> socket);
    Ptr<RandomVariableStream> m_changeTime;
  };

} // namespace ns3

#endif /* ONOFF_APPLICATION_H */
