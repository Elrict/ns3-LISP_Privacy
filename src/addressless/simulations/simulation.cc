#include "simulation.hpp"

namespace ns3
{
    Simulation::Simulation()
    {
        m_topology = Create<LISPTopology>();
    }
    void Simulation::Setup()
    {
        BuildBaseTopology();
        // Lisp context
        if (m_eidPriv || m_rlocPriv)
        {
            BuildLispTopology();
            m_topology->SetTiming();
            SetLispPlane();
        }

        Ipv4GlobalRoutingHelper::PopulateRoutingTables();
        m_topology->SetupAnim();
        InstallApplications();
    }
    void Simulation::BuildBaseTopology()
    {
        m_topology->m_clients = m_nbrclients;
        m_totaly = (m_nbrclients)*5;
        m_middle = m_totaly / 2;
        m_topology->AddRouter("xTRs", m_totaly + 5, m_middle);
        m_topology->AddHost("Server", m_totaly + 10, m_middle);
        m_topology->AddRouter("router", m_totaly, m_middle);

        for (int i = 0; i < m_nbrclients; i++)
        {
            m_topology->AddHost("Client" + std::to_string(i), (m_totaly / 2) - 10, m_middle + (m_totaly / 2) - (i * 5));
        }

        ConnectClients();

        m_topology->Connect("xTRs", "Server", 1, m_srvAddr);
        m_topology->Connect("router", "xTRs", 1, m_xTRsAddr);

        if (m_entrance == "Entrance")
        {
            m_topology->AddHost(m_entrance, m_totaly + 10, m_middle - 10);
            m_topology->Connect("xTRs", m_entrance);
        }
    }
    void Simulation::ConnectClients()
    {
        if (m_xtrperclient)
        {
            for (int i = 0; i < m_nbrclients; i++)
            {
                std::string str = std::to_string(i);
                m_topology->AddRouter("xTRc" + str, m_totaly / 2, m_middle + (m_totaly / 2) - (i * 5));
                m_topology->Connect("Client" + str, "xTRc" + str, 1, 1);
            }
            for (int i = 0; i < m_nbrclients; i++)
            {
                std::string str = std::to_string(i);
                m_topology->Connect("router", "xTRc" + str, 1, 1);
            }
        }
        else
        {
            m_topology->AddRouter("xTRc", m_totaly / 2, m_middle);
            for (int i = 0; i < m_nbrclients; i++)
            {
                m_topology->Connect("Client" + std::to_string(i), "xTRc", 1, m_bidirectionnel ? m_srvAddr : 1);
            }
            m_topology->Connect("router", "xTRc", 1, 1);
        }
    }
    void Simulation::BuildLispTopology()
    {
        m_topology->AddHost("map_resolver", m_totaly, m_middle + 10);
        m_topology->AddHost("map_server", m_totaly, m_middle - 10);
        m_topology->Connect("router", "map_resolver");
        m_topology->Connect("router", "map_server", 1, 1);

        for (int i = 0; i < m_nbrclients; i++)
        {
            m_topology->AddRlocs("xTRc" + std::to_string(i), "router");
        }

        m_topology->AddRlocs("xTRs", "router");
        m_topology->AddRlocs("map_resolver", "router");
        m_topology->AddRlocs("map_server", "router");
    }
    void Simulation::SetLispPlane()
    {
        m_topology->SetMapServer("map_server");
        m_topology->SetMapResolver("map_resolver");

        SetClientXtr();
        SetServerXtr();
    }
    void Simulation::SetClientXtr()
    {
        for (int i = 0; i < m_nbrclients; i++)
        {
            m_topology->SetXtr("xTRc" + std::to_string(i));
        }
    }
    void Simulation::SetServerXtr()
    {
        m_topology->m_lispPrivacyXtrHelper.SetServer(m_topology->GetNode("Server")->GetInterface("xTRs"));
        m_topology->m_lispPrivacyXtrHelper.SetInterface(m_topology->GetNode("xTRs")->GetInterface("router"));
        m_topology->m_lispHelper.SetServiceAddr(m_topology->GetNode("Server")->GetAddress("xTRs"));
        m_topology->m_lispHelper.SetServiceInterface(m_topology->GetNode("Server")->GetInterface("xTRs"));
        m_topology->m_lispHelper.SetRlocInterface(m_topology->GetNode("xTRs")->GetInterface("router"));
        m_topology->m_lispPrivacyXtrHelper.SetRlocRedir(m_rlocRedir);
        m_topology->m_lispPrivacyXtrHelper.SetFastRedir(m_fastRedir);

        m_topology->m_lispHelper.SetReverseNatting(m_lispNatting);
        m_topology->m_lispHelper.SetRlocCheck(m_xtrRlocCheck);
        m_topology->m_lispHelper.SetRlocRedir(m_rlocRedir);
        m_topology->m_lispHelper.SetEidCheck(m_xtrEidCheck);
        m_topology->m_lispPrivacyXtrHelper.SetAttribute("Protocol", StringValue(m_topology->m_protocol));

        m_topology->SetXtr("xTRs");
    }
    void Simulation::InstallApplications()
    {
        if (m_natting)
            m_topology->InstallNating(m_natted.first, m_natted.second);

        m_topology->InstallServerModule("Server", m_srvCheck);

        if (m_entrance != "")
            m_topology->InstallEntranceModule(m_entrance, "Server");

        for (int i = 0; i < m_nbrclients; i++)
        {
            m_topology->InstallTcpSender("Client" + std::to_string(i), m_topology->GetNode("Client" + std::to_string(i))->GetAddress("xTRc" + std::to_string(i)),m_topology->GetNode(m_destination.first)->GetAddress(m_destination.second), m_timeBtwClients*(i));
        }
    }
    bool Simulation::SetBool(bool input)
    {
        NS_ASSERT_MSG(!input, "You cannot choose multiple Eid/rloc privacy at the same time.");
        return (!input);
    }
}
