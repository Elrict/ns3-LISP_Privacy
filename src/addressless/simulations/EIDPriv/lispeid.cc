
#include "../simulation.hpp"

namespace ns3
{
    NS_LOG_COMPONENT_DEFINE("LISPEid");
    /* Topology:
    *
    *            xTRc <----> router <-----> xTRs ---- Main service Module
    *            /                         
    *           /                          
    *         host                   
    */
    void Simulation::LISPEid()
    {
        m_eidPriv = SetBool(m_eidPriv);
        m_xtrEidCheck = true;
        m_srvAddr = 10;
        m_entrance = "xTRs";
        m_destination = std::make_pair<std::string, std::string>("xTRs", "router");
        
    }
}