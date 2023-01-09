#ifndef SRC_INTERNET_MODEL_LISP_CONTROL_PLANE_LISP_ETR_ITR_REDIR_APPLICATION_H_
#define SRC_INTERNET_MODEL_LISP_CONTROL_PLANE_LISP_ETR_ITR_REDIR_APPLICATION_H_

#include "ns3/lisp-etr-itr-application.h"
#include "ns3/socket.h"
#include "ns3/socket-factory.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/uinteger.h"
namespace ns3
{
    class LispEtrItrPrivacyApplication : public LispEtrItrApplication
    {
    public:
        static TypeId
        GetTypeId(void);
        // Will be used to implement Redir fired upon map request
        LispEtrItrPrivacyApplication();
        ~LispEtrItrPrivacyApplication();
        Ptr<MapReplyMsg> GenerateMapReply(Ptr<MapRequestMsg> requestMsg);
        Ipv4Address GenerateAddress(Ipv4Address source);
        Ptr<MapRegisterMsg> GenerateMapRegister(Ptr<MapEntry> mapEntry, bool rtr);

    protected:
        void SendMapRequest(Ptr<MapRequestMsg> mapReqMsg);

        void SendTo(Ptr<Packet> packet, Address to);
        void FastRedir(Ptr<MapRequestMsg> requestMsg);
        void HandleControlMsg(Ptr<Packet> packet, Address from);

        Ptr<Ipv4Interface> m_serverInterface;
        Ptr<Ipv4Interface> m_rlocInterface;
        bool m_fastRedir;
        bool m_rlocRedir;
        int m_hashDone = 0;
        Ptr<RandomVariableStream> m_hashTime;
        std::string m_protocol;
        TracedCallback<int, double> m_mapDelay;
        uint16_t m_id;
        int m_clientswaiting = 0;
        std::map<uint32_t, bool> m_privacyDone;
        bool m_proxy;
        std::map<uint32_t, Time> m_mapReqSent;
    };
}
#endif
