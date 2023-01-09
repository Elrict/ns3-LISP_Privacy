

#include "../simulation.hpp"
/* Simulation: Rloc redirection
 *    Topology:
 *
 *               xTRc <----> router <-----> xTRs ---- Main service  Module
 *               /
 *              /
 *             host
 *
 * In this simulation, A part of the LISP control plane is rewritten to
 *      - Send on a map reply a specific rloc computed by hashing.
 *      - check when decapsulating a packet that the rloc used is the right one.
 *
 * Questions:
 *      - 1 check for each packet??
 *
 * Code:
 *      - [lisp/control-plane/lisp-etr-itr-privacy-application]
 *
 */
namespace ns3
{
    void Simulation::LISPRlocRedir()
    {
        m_rlocPriv = SetBool(m_rlocPriv);

        m_xtrRlocCheck = true;
        m_xTRsAddr = 10;
        m_rlocRedir = true;
        m_entrance = "";
    }

}