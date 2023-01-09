#ifndef ADDRESSLESS_SIMULATION_H
#define ADDRESSLESS_SIMULATION_H
#include "ns3/topology.hpp"
#include "ns3/simulator.h"
namespace ns3
{
    class Simulation : public Object
    {

    public:
            // SIMULATION PARAMETERS
        Ptr<LISPTopology> m_topology;
        int m_nbrclients = 1;
        double m_timeBtwClients = 0;

        Simulation();
        /**
         * Launch the naive lisp simulation.
         * This simulation is an immediate application of the IP addressless scheme to a
         * lisp environnement.
         *
         * \returns Nothing.
         */
        void LISPNaive();
        /**
         * Launch the eid lisp simulation.
         * This simulation uses the xtr as the entrance module, removing the need for an independant entrance module but
         * adding the cost of an application layer to the etr.
         *
         * \returns Nothing.
         */
        void LISPEid();
        /**
         * Launch the rloc lisp simulation.
         * This simulation uses the xtr to use specific rloc addresses according to the server and client ip.
         *
         * \returns Nothing.
         */
        void LISPRlocRedir();
        /**
         * Launch the rloc lisp simulation.
         * This simulation uses the xtr to use specific rloc addresses according to the server and client ip.
         *
         * \returns Nothing.
         */
        void LISPFastRedir();
        void IPVanilla();
        void LISPVanilla();
        void LISPBidirectionnel();
        void LISPNat();
        /**
         * Launch the classic ip simulation.
         * This simulation uses the architecture described in the IP addressless paper.
         *
         * \returns The topology set up.
         */
        void IPBase();
        void IPNat();
        /**
         * @brief Establish the appropriate simulation environnement.
         *
         *  After the parameters have been set for the simulation, the actual setup takes place.
         *
         * \returns Nothing.
         */
        void Setup();

    protected:
        void ConnectClients();

        /**
         * @brief Function that simplify the setup of basic ip topology.
         *              Topology:
         *
         *        host <---> xTRc <----> router <-----> xTRs <----> Main service Module (+ Entrance module)
         *
         * \returns Nothing.
         */
        void BuildBaseTopology();
        /**
         * @brief  Function that simplify the setup of basic lisp topology.
         *              Topology:
         *
         *                                  MR
         *                                 /
         *        host <---> xTRc <----> router <-----> xTRs <----> Main service Module (+ Entrance module)
         *                                /
         *                               MS
         * \returns Nothing.
         */
        void BuildLispTopology();
        /**
         * @brief Function that simplify the setup of basic lisp data and control plane.
         *
         * \returns Nothing.
         */
        void SetLispPlane();

        /**
         * @brief According to the simulation parameters, install the proper applications on the proper nodes.
         *
         */
        void InstallApplications();
        /**
         * Check and verifies that a boolean parameter is not already set before setting it.
         * \returns The boolean.
         */
        bool SetBool(bool input);
        void SetClientXtr();
        void SetServerXtr();



        std::string m_entrance = "Entrance"; // Name of the node handling the entrance module work.

        std::pair<std::string, std::string> m_destination = std::make_pair<std::string, std::string>("Server", "xTRs"); // Specify the IP address to which the initial connection attemp must be made.
        std::pair<std::string, std::string> m_natted = std::make_pair<std::string, std::string>("xTRs", "Server");      // Specify the elements on which to install the nat applications.

        int m_xTRsAddr = 1; // Specify the number of rloc addresses to allocate to the xTRs.
        int m_srvAddr = 1;  // Specify the number of IP addresses to allocate to the server node.
        bool m_srvCheck = false;
        bool m_xtrEidCheck = false;
        bool m_xtrRlocCheck = false;

        bool m_fastRedir = false;
        bool m_rlocRedir = false;
        bool m_natting = false;
        bool m_lispNatting = false;

        bool m_eidPriv = false;  // Make sure that only one eid priv technique is applied.
        bool m_rlocPriv = false; // Make sure that only one rloc priv technique is applied.

        bool m_bidirectionnel = false;

        bool m_xtrperclient = true;

        int m_totaly = 0;
        int m_middle = 0;
    };
    NS_OBJECT_ENSURE_REGISTERED(Simulation);
}

#endif /* THREE_GPP_HTTP_CLIENT_H */
