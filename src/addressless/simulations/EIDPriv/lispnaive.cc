

#include "../simulation.hpp"

/* Simulation:  Naive lisp redirection
 *    Topology:
 *
 *               xTRc <----> router <-----> xTRs ---- Entrance Module
 *               /                           \
 *              /                             \
 *             host                            Server
 *
 * In this simulation, the classic addressless architecture is applied for the lisp topology.
 *
 */
namespace ns3
{
    NS_LOG_COMPONENT_DEFINE("LISPNaive");
    void Simulation::LISPNaive()
    {
        m_eidPriv = SetBool(m_eidPriv);
        m_srvAddr = 30;
        m_entrance = "Entrance";
        m_srvCheck = true;
        m_destination = std::make_pair<std::string, std::string>("Entrance", "xTRs");      

    }
}