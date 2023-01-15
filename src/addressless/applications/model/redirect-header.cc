#include "ns3/assert.h"
#include "ns3/log.h"
#include "ns3/header.h"
#include "ns3/simulator.h"
#include "redirect-header.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("RedirectHeader");

NS_OBJECT_ENSURE_REGISTERED (RedirectHeader);

RedirectHeader::RedirectHeader ()
  : m_redirect(false)
{
  NS_LOG_FUNCTION (this);
}

void
RedirectHeader::SetRedirect(bool redirectPacket)
{
  NS_LOG_FUNCTION (this << redirectPacket);
  m_redirect = redirectPacket;
}

bool 
RedirectHeader::GetRedirect (void) const
{

  NS_LOG_FUNCTION (this);
  return m_redirect;

}

TypeId
RedirectHeader::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::RedirectHeader")
    .SetParent<Header> ()
    .SetGroupName("Applications")
    .AddConstructor<RedirectHeader> ()
  ;
  return tid;
}

TypeId
RedirectHeader::GetInstanceTypeId (void) const
{
  return GetTypeId ();
}
void
RedirectHeader::Print (std::ostream &os) const
{
  NS_LOG_FUNCTION (this << &os);
  os << "(redirect=" << m_redirect << ")";
}
uint32_t
RedirectHeader::GetSerializedSize (void) const
{
  NS_LOG_FUNCTION (this);
  return 1;
}

void
RedirectHeader::Serialize (Buffer::Iterator start) const
{
  NS_LOG_FUNCTION (this << &start);
  Buffer::Iterator i = start;
  i.WriteU8(m_redirect);
}
uint32_t
RedirectHeader::Deserialize (Buffer::Iterator start)
{
  NS_LOG_FUNCTION (this << &start);
  Buffer::Iterator i = start;
  m_redirect = i.ReadU8 ();
  return GetSerializedSize ();
}

} // namespace ns3
