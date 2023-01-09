#include "../simulation.hpp"

namespace ns3
{
    /** Topology:
     *
     *            xTRc <----> router <-----> xTRs ---- Main service Module
     *            /
     *           /
     *         host
     **/
    void Simulation::LISPFastRedir()
    {
        m_eidPriv = SetBool(m_eidPriv);
        m_xtrEidCheck = true;
        m_fastRedir = true;
        m_srvAddr = 10;
        m_entrance = "";
        m_destination = std::make_pair<std::string, std::string>("xTRs", "Server");
    }

}