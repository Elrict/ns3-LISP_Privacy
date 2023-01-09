

#include "../simulation.hpp"

namespace ns3
{
    void Simulation::LISPNat()
    {
        m_eidPriv = SetBool(m_eidPriv);

        m_srvAddr = 10;
        m_lispNatting = true;
        m_entrance = "";
    }

}