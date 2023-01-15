#ifndef RedirectApplicationEntrance_H
#define RedirectApplicationEntrance_H

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
  class RandomVariableStream;
  class Socket;

  class RedirectApplicationEntrance : public Application
  {
  public:
    static TypeId GetTypeId(void);
    Ptr<Packet> GeneratePacket(Address peer);
    RedirectApplicationEntrance();
    virtual ~RedirectApplicationEntrance();
    void SetMaxBytes(uint64_t maxBytes);
    Ptr<Socket> GetSocket(void) const;

  protected:
    virtual void DoDispose(void);
    Ipv4Address GenerateAddress(Address peer);

  private:
    // inherited from Application base class.
    virtual void StartApplication(void); // Called at time specified by Start
    virtual void StopApplication(void);  // Called at time specified by Stop

    // Event handlers
    void SendPacket(Ptr<Socket> coSocket, Address peer);

    void ReceivedDataCallback(Ptr<Socket> coSocket);
    bool ConnectionRequestCallback(Ptr<Socket> coSocket, const Address &address);
    void NewConnectionCreatedCallback(Ptr<Socket> coSocket, const Address &address);

    Ptr<Socket> m_listenSocket; //!< Associated socket
    Address m_localAddress;     //!< Peer address
    Ptr<Ipv4Interface> m_serverInterface;
    bool m_connected;           //!< True if connected
    DataRate m_cbrRate;         //!< Rate that data is generated
    DataRate m_cbrRateFailSafe; //!< Rate that data is generated (check copy)
    uint32_t m_pktSize;         //!< Size of packets
    uint32_t m_residualBits;    //!< Number of generated, but not sent, bits
    uint64_t m_maxBytes;        //!< Limit total number of bytes sent
    uint64_t m_totBytes;        //!< Total bytes sent so far
    EventId m_startStopEvent;   //!< Event id for next start or stop event
    EventId m_sendEvent;        //!< Event id of pending "send packet" event
    /// Traced Callback: transmitted packets.
    TracedCallback<Ptr<const Packet>> m_txTrace;

  private:
    void ConnectionSucceeded(Ptr<Socket> coSocket);
    /**
     * \brief Handle a Connection Failed event
     * \param socket the not connected socket
     */
    void ConnectionFailed(Ptr<Socket> coSocket);
    Ptr<RandomVariableStream> m_hashTime;
    TypeId m_tid; //!< Protocol TypeId
    int m_clients_waiting;
  };

} // namespace ns3

#endif /* ONOFF_APPLICATION_H */
