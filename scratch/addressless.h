#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/addressless-module.h"
#include "ns3/flow-monitor-helper.h"

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
    {"LISPBI", LISPBI}
};