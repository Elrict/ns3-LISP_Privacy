#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/addressless-module.h"
#include "ns3/flow-monitor-helper.h"
using json = nlohmann::json;

using namespace ns3;
enum simu
{
  IP = 1,
  IPNAT = 7,
  LISPNAIVE = 2,
  LISPEID = 3,
  LISPRLOC = 4,
  LISPFAST = 5,
  LISPPASSIVE = 6,
  LISPNATv2 = 8,
  LISPBI = 9,
  IPVANILLA = 10,
  LISPVANILLA = 11

};
std::map<std::string, uint8_t> mapInput = {
    {"IPBASE", IP},
    {"IPVANILLA", IPVANILLA},
    {"LISPVANILLA", LISPVANILLA},
    {"IPNAT", IPNAT},
    {"LISPNAIVE", LISPNAIVE},
    {"LISPEID", LISPEID},
    {"LISPRLOC", LISPRLOC},
    {"LISPFAST", LISPFAST},
    {"LISPPASSIVE", LISPPASSIVE},
    {"LISPBI", LISPBI}};

NS_LOG_COMPONENT_DEFINE("Addressless");

std::vector<std::string> parseArgs(std::string input)
{
  std::string delimiter = "-";
  size_t pos = 0;
  std::vector<std::string> res;
  while ((pos = input.find(delimiter)) != std::string::npos)
  {
    res.push_back(input.substr(0, pos));
    input.erase(0, pos + delimiter.length()); /* erase() function store the current positon and move to next token. */
  }
  res.push_back(input);

  return res;
}

int main(int argc, char *argv[])
{
  CommandLine cmd;
  std::string simuChoice;
  std::string protocol;
  double delay = 0;
  int nbrClients = 1;
  cmd.AddValue("SIMU", "IP topology with Entrance and Main modules.", simuChoice);
  cmd.AddValue("Clients", "Number of clients", nbrClients);
  cmd.AddValue("Protocol", "Transport layer Protocol to be used: TCP or UDP", protocol);
  cmd.AddValue("ClientInterval", "Interval between subsequent clients connections", delay);
  cmd.Parse(argc, argv);
  Simulation simu;
  simu.m_nbrclients = nbrClients;
  simu.m_timeBtwClients = delay;
  PacketMetadata::Enable();
  Packet::EnablePrinting();

  for (std::string str : parseArgs(simuChoice))
  {
    switch (mapInput[str])
    {
    case IPVANILLA:
      simu.IPVanilla();
      break;
    case LISPVANILLA:
      simu.LISPVanilla();
      break;
    case IP:
      simu.IPBase();
      break;
    case IPNAT:
      simu.IPNat();
      break;
    case LISPNAIVE:
      simu.LISPNaive();
      break;
    case LISPEID:
      simu.LISPEid();
      break;
    case LISPRLOC:
      simu.LISPRlocRedir();
      break;
    case LISPFAST:
      simu.LISPFastRedir();
      break;
    case LISPPASSIVE:
      simu.LISPNat();
      break;
    case LISPBI:
      simu.LISPBidirectionnel();
      break;
    default:
      NS_FATAL_ERROR("No model chosen.");
      break;
    }
  }

  simu.m_topology->m_protocol = (protocol == "TCP") ? "ns3::TcpSocketFactory" : "ns3::UdpSocketFactory";
  simu.Setup();
  
  Simulator::Run();
  Simulator::Destroy();

  std::ofstream out;

  // std::ios::app is the open mode "append" meaning
  // new data will be written to the end of the file.
  if (IPTopology::m_connectionTimes.size() < (size_t)nbrClients)
  {
    std::cerr << " Not good: " << IPTopology::m_connectionTimes.size() << "<" << (size_t)nbrClients << std::endl;
  }
  else
  {

    std::string filename = "plots/json/" + simuChoice + "_" + protocol + ".json";

    json j;
    std::ifstream file(filename);
    file.ignore(std::numeric_limits<std::streamsize>::max());
    std::streamsize length = file.gcount();
    file.clear(); //  Since ignore will have set eof.
    file.seekg(0, std::ios_base::beg);
    if(length > 10)
    {
      file >> j;
    }
    file.close();
    j[std::to_string((double)(simu.m_timeBtwClients * 1000))] = IPTopology::m_data;
    std::ofstream o(filename, std::ofstream::trunc);
    o << std::setw(1) << j << std::endl;

  }

  return 0;
}
