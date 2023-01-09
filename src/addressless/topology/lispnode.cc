
#include "topology.hpp"
namespace ns3
{
    NS_LOG_COMPONENT_DEFINE("LISPNode");
    NS_OBJECT_ENSURE_REGISTERED(LISPNode);
    LISPNode::LISPNode(LISPTopology *_topology) : IPNode()
    {
        m_topo = _topology;
        m_rlocs = nullptr;
    }

    LISPNode::~LISPNode() {}

    void LISPNode::SetMapTables()
    {
        for (auto iter : m_linkByNeighbor)
        {
            /* TODO
            if(iter.first=="router" && mapsize !=1){
                continue;
            }
            */
            // have to add router otherwise map server ends up with an empty map table and it crashes.
            this->AddEntryToMapTables(this->GetAddress(iter.first));
        }

            m_topo->m_lispHelper.SetMapTablesForEtr(Address(m_rlocs->GetAddress(0).GetLocal()), m_ipv4MapTables, m_ipv6MapTables);
    }
    void LISPNode::SetRlocInterface(Ptr<Ipv4Interface> rlocs)
    {
        m_rlocs = rlocs;
    }
    void LISPNode::AddEntryToMapTables(Ipv4Address eidAddress)
    {
        this->InitializeMapTables();

        Ipv4Mask eidMask = Ipv4Mask("255.255.255.0");
        auto location = MapTables::IN_DATABASE;

        uint8_t priority = 200;
        uint8_t weight = 30;
        bool reachability = true;
        if (Ipv4Address::IsMatchingType(eidAddress))
        {
            if (m_rlocs != nullptr)
            {
                for (size_t i = 0; i < m_rlocs->GetNAddresses(); i++)
                {
                    m_ipv4MapTables->InsertLocator(eidAddress, eidMask, m_rlocs->GetAddress(i).GetLocal(), priority, weight,
                                                   location, reachability);
                }
            }
        }
        else if (Ipv6Address::IsMatchingType(eidAddress))
        {
            // todo
        }
    }
    void LISPNode::InitializeMapTables()
    {
        if (m_ipv4MapTables == nullptr)
        {
            m_ipv4MapTables = Create<SimpleMapTables>();
        }
        if (m_ipv6MapTables == nullptr)
        {
            m_ipv6MapTables = Create<SimpleMapTables>();
        }
    }

}