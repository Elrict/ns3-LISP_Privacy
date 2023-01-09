

#include "../simulation.hpp"

namespace ns3
{
    void Simulation::LISPNat()
    {
        m_eidPriv = SetBool(m_eidPriv);

        m_srvAddr = 10;
        m_natting = true;
        m_entrance = "";
        m_destination = std::make_pair<std::string, std::string>("xTRs", "Server");
    }

}