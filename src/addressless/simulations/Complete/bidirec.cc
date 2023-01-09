

#include "../simulation.hpp"

namespace ns3
{
    void Simulation::LISPBidirectionnel()
    {
        m_eidPriv = SetBool(m_eidPriv);
        m_rlocPriv = SetBool(m_rlocPriv);

        m_bidirectionnel = true;
        m_srvAddr = 10;
        m_xTRsAddr = 10;
        m_lispNatting = true;
        m_rlocRedir = true;
        m_entrance = "";
        m_xtrRlocCheck = true;
    }

}