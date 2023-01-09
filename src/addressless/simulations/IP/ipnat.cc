
#include "../simulation.hpp"

namespace ns3
{

    void Simulation::IPNat()
    {
        m_natting = true;
        m_entrance = "";
        m_destination = std::make_pair<std::string, std::string>("xTRs", "Server");
          
    }
}
