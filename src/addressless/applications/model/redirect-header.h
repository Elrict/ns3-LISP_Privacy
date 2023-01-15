#ifndef REDIRECT_HEADER_H
#define REDIRECT_HEADER_H

#include "ns3/header.h"
#include "ns3/nstime.h"

namespace ns3 {
/**
 * \ingroup udpclientserver
 *
 * \brief Packet header for UDP client/server application.
 *
 * The header is made of a 32bits sequence number followed by
 * a 64bits time stamp.
 */
class RedirectHeader : public Header
{
public:
  RedirectHeader ();

  /**
   * \param redirectPacket redirect bit;
   */
  void SetRedirect (bool redirectPacket);
  /**
   * \return the redirect bit;
   */
  bool GetRedirect (void) const;
  static TypeId GetTypeId (void);
  virtual TypeId GetInstanceTypeId (void) const;
  virtual void Print (std::ostream &os) const;
  virtual uint32_t GetSerializedSize (void) const;
  virtual void Serialize (Buffer::Iterator start) const;
  virtual uint32_t Deserialize (Buffer::Iterator start);

private:
  bool m_redirect; //!< Sequence number
};

} // namespace ns3

#endif /* REDIRECT_HEADER_H */
