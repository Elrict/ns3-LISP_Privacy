#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "Topology.cc"

#include "ns3/netanim-module.h"
using namespace ns3;
NS_LOG_COMPONENT_DEFINE ("SimpleLisp");

/* Topology:  map_resolver    map_server
 *                      \      /
 *                       \    /
 *           xTR0 <----> router <-----> xTR1
 *            /        (non-LISP)           \
 *           /                               \
 *         host0                            host1
 *      (non-LISP)                        (non-LISP)
 */

struct Main
{
  int main (int argumentCount, char const *argv[])
  {
    LogComponentEnable ("SimpleLisp", LOG_LEVEL_INFO);
    PacketMetadata::Enable ();

    Ptr<LispTopology> topology = Create<LispTopology>();
    topology->NewNode ("host0", 10, 15);
    topology->GetNode ("host0")->ConnectTo (topology->NewNode ("xTR0", 15, 15));
    topology->GetNode ("xTR0")->ConnectTo (topology->NewNode ("router", 20, 15), IS_RLOC);
    topology->GetNode ("router")->ConnectTo (topology->NewNode ("xTR1", 25, 15), IS_RLOC);
    topology->GetNode ("xTR1")->ConnectTo (topology->NewNode ("host1", 30, 20));

    topology->GetNode ("router")->ConnectTo (topology->NewNode ("map_resolver", 20, 20), IS_RLOC);
    topology->GetNode ("router")->ConnectTo (topology->NewNode ("map_server", 20, 10), IS_RLOC);

    topology->PopulateRoutingTables ();

    topology->GetNode ("map_server")->SetMapServer (Seconds (0.0), Seconds (20.0));
    topology->GetNode ("map_resolver")->SetMapResolver (Seconds (0.0), Seconds (20.0));
    topology->GetNode ("xTR0")->SetXtr (Seconds (1.0), Seconds (20.0));
    topology->GetNode ("xTR1")->SetXtr (Seconds (1.0), Seconds (20.0));

    InstallTcpApplication (topology->GetNode ("host0"), topology->GetNode ("host1"));

    topology->SetPcapFilesPrefix ("simple_lisp");
    topology->EnableAllPcapFiles ();

    AnimationInterface anim ("simplelisp.xml");
    anim.EnablePacketMetadata (true);
    Simulator::Run ();
    Simulator::Destroy ();

    return 0;
  }


  void InstallTcpApplication (Ptr<Ipv4Topology::Ipv4TopologyNode> source, Ptr<Ipv4Topology::Ipv4TopologyNode> destination)
  {
    uint16_t port = 50000;
    this->InstallTcpReceiver (destination, port);
    this->InstallTcpSender (source, destination, port);
  }

  void InstallTcpReceiver (Ptr<Ipv4Topology::Ipv4TopologyNode> node, uint16_t port)
  {
    InetSocketAddress tcp_address = InetSocketAddress (node->GetAddress (), port);
    PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", tcp_address);
    ApplicationContainer sinkApp = sinkHelper.Install (node->GetNode ());
    sinkApp.Start (Seconds (1.0));
    sinkApp.Stop (Seconds (20.0));
  }

  void InstallTcpSender (Ptr<Ipv4Topology::Ipv4TopologyNode> node,
                         Ptr<Ipv4Topology::Ipv4TopologyNode> destination, uint16_t port)
  {
    InetSocketAddress tcp_dest_address = InetSocketAddress (destination->GetAddress (), port);
    OnOffHelper sender_helper ("ns3::TcpSocketFactory", tcp_dest_address);
    sender_helper.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
    sender_helper.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
    sender_helper.SetAttribute ("DataRate", DataRateValue (DataRate ("50b/s")));
    sender_helper.SetAttribute ("PacketSize", UintegerValue (10));
    sender_helper.SetAttribute ("MaxBytes", UintegerValue (1000));

    ApplicationContainer sender_app = sender_helper.Install (node->GetNode ());
    sender_app.Start (Seconds (4.0));
    sender_app.Stop (Seconds (20.0));
  }
};

int main (int argc, char const *argv[])
{
  Main main;
  main.main (argc, argv);
}
