
#include "../simulation.hpp"

namespace ns3
{

    void Simulation::IPBase()
    {
        m_srvAddr = 10;
        m_srvCheck = true;
        m_entrance = "Entrance";
        m_destination = std::make_pair<std::string, std::string>("Entrance", "xTRs"); 
          
    }
}
