#ifndef ADDRESSLESS_TOPOLOGY_H
#define ADDRESSLESS_TOPOLOGY_H
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include <iostream>
#include "ns3/netanim-module.h"
#include "ns3/redirect-application-helper.h"
#include "ns3/lisp-etr-itr-privacy-app-helper.h"
#include "ns3/lisp-privacy-helper.h"
#include "ns3/map-server-privacy-helper.h"
#include "ns3/map-resolver-privacy-helper.h"
#include "ns3/json.hpp"
using json = nlohmann::json;
namespace ns3
{

  class IPNode : public Node
  {

  protected:
    std::map<std::string, uint32_t> m_linkByNeighbor; // Keeps track of neighbors and the interfaces connecting to them.

  public:
    IPNode(/* args */);
    ~IPNode();

    // Getters
    Ipv4Address GetAddress(std::string neighbor);
    Ptr<Ipv4Interface> GetInterface(std::string neighbor);
    uint32_t GetInterfaceIndex(std::string neighbor);

    /**
     * Setter, adds an interface to the map and its associated neighbor
     *
     * \param interface the index of the interface.
     * \param neighbor name of the neighbor.
     *
     * \returns Nothing.
     **/
    void AddInterface(uint32_t interface, std::string neighbor);

    /**
     * Manually adds an address to the interface associated with the neighbor.
     *
     * \param addr the address to add to the interface.
     * \param neighbor name of the neighbor.
     *
     * \returns Nothing.
     **/
    void AddAddress(std::string neighbor, Ipv4Address addr);
  };
  class IPTopology : public Object
  {
  protected:
    std::map<std::string, Ptr<IPNode>> nodesByName; // Keep tracks of all nodes in the topology
    PointToPointHelper linkHelper;                  // Helper that set up all the link layer
    InternetStackHelper stack;                      // Helper that sets up all the ip network layer
    Ipv4AddressHelper m_ipv4;                       // Helper that sets up all the addressing
    Ipv4NatHelper m_natHelper;
    AnimationInterface *anim; // Helper that takes care of all the animation output of the simulation.
    std::vector<std::string> hostNames;
    std::vector<std::string> routerNames;
    void ReportConnection(int32_t id);

    uint16_t m_port; // Port used by the redirection protocol.

  public:
    int m_clients;
    std::string m_protocol;
    static std::vector<double> m_connectionTimes;
    static json m_data;

    IPTopology();
    ~IPTopology();
    /**
     *  Adds a node to the topology.
     *
     * \param name The name of the node.
     * \param x the x coordinate to use in the resulting animation.
     * \param y the y coordinate to use in the resulting animation.
     *
     **/
    void AddHost(std::string name, double x, double y);
    /**
     *  Adds a router to the topology.
     *
     * \param name The name of the router.
     * \param x the x coordinate to use in the resulting animation.
     * \param y the y coordinate to use in the resulting animation.
     *
     * \returns A pointer to the node.
     **/
    void AddRouter(std::string name, double x, double y);
    /**
     *  Set up all the element of the animation, like nodes, routers, links,...
     * \returns Nothing.
     **/
    void SetupAnim();

    Ptr<IPNode> GetNode(std::string name);
    Ipv4Address GetTopAddress(Ptr<IPNode> node);

    void Connect(std::string nodeA, std::string nodeB, uint8_t nbAdrNodeA = 1, uint8_t nbAdrNodeB = 1, bool pcap = false);
    void InstallNating(std::string routerN, std::string nodeN);
    void InstallServerModule(std::string node, bool check);
    void InstallEntranceModule(std::string node, std::string mainServer);
    void InstallTcpSender(std::string node, Ipv4Address source, Ipv4Address destination, double startTime);

  protected:
    virtual void AddNode(std::string name, double x, double y);
  };
  class LISPTopology : public IPTopology
  {
  protected:
    void ReportMapDelay(int id, double time);
    // COntrol plane server & resolver
    MapServerPrivacyDdtHelper m_mapServerHelper;
    MapResolverPrivacyDdtHelper m_mapResolverHelper;
    std::map<std::string, std::vector<Ipv4Address>> m_nodesRlocs;

  public:
    // Control plane
    LispEtrItrPrivacyAppHelper m_lispPrivacyXtrHelper;

    // dataplane
    LispPrivacyHelper m_lispHelper;
    LISPTopology(/* args */);
    ~LISPTopology();
    void AddRlocs(std::string lispNode, std::string destNode);
    /**
     * Setup a node as an Xtr, installing its lisp stack and the itr-etr control plane application.
     *
     * \param name The node on which to create the Application.     *
     * \returns Nothing.
     */
    void SetXtr(std::string name);
    void SetMapServer(std::string name, unsigned int interfaceIndex = 0);
    void SetMapResolver(std::string name, unsigned int interfaceIndex = 0);

    void AddEntryToMapTables(Ipv4Address eidAddress);
    void SetTiming();

  private:
    void AddNode(std::string name, double x, double y);

    /**
     * Setup a node as lisp node by installing its lisp stack (dataplane).
     *
     * \param node The node on which to create the Application.
     * \param redir If true, the data plane installed will be the custom one used for redirection.
     *    - if True: See [src/internet/model/lisp/privacy/data-plane/lisp-over-ipv4-impl-redir]
     *    - if False: See [src/internet/model/lisp/data-plane/lisp-over-ipv4-impl]
     *
     * \returns Nothing.
     */
    void SetLispCapable(Ptr<IPNode> node);
    void SetMapTables();
    void InitializeMapTables();
  };

  class LISPNode : public IPNode
  {

  protected:
    // This interface and its associated addresses will be used as the rlocs for all the non-lisp nodes inside the AS.
    // I could make it more complicated and associate a subset of addresses of this interface to some intra-as subnet but it is not needed for these simulations.
    Ptr<Ipv4Interface> m_rlocs;

    LISPTopology *m_topo;
    Ptr<SimpleMapTables> m_ipv4MapTables;
    Ptr<SimpleMapTables> m_ipv6MapTables;

  public:
    LISPNode(LISPTopology *_topology);
    ~LISPNode();
    void SetMapTables();
    void SetRlocInterface(Ptr<Ipv4Interface> rlocs);
    void AddEntryToMapTables(Ipv4Address eidAddress);
    void InitializeMapTables();
  };

}
#endif /* THREE_GPP_HTTP_CLIENT_H */
