#ifndef SRC_INTERNET_HELPER_LISP_HELPER_MAP_SERVER_PRIVACY_HELPER_H_
#define SRC_INTERNET_HELPER_LISP_HELPER_MAP_SERVER_PRIVACY_HELPER_H_

#include <stdint.h>
#include "ns3/application-container.h"
#include "ns3/node-container.h"
#include "ns3/object-factory.h"
#include "ns3/lisp-protocol.h"

namespace ns3
{

class MapServerPrivacyDdtHelper
{

public:
  MapServerPrivacyDdtHelper ();
  virtual
  ~MapServerPrivacyDdtHelper ();

  /**
   * Record an attribute to be set in each Application after it is is created.
   *
   * \param name the name of the attribute to set
   * \param value the value of the attribute to set
   */
  void
  SetAttribute (std::string name, const AttributeValue &value);

  /**
   * Create a UdpEchoServerApplication on the specified Node.
   *
   * \param node The node on which to create the Application.  The node is
   *             specified by a Ptr<Node>.
   *
   * \returns An ApplicationContainer holding the Application created,
   */
  ApplicationContainer
  Install (Ptr<Node> node) const;

    /**
     * Create a UdpEchoServerApplication on specified node
     *
     * \param nodeName The node on which to create the application.  The node
     *                 is specified by a node name previously registered with
     *                 the Object Name Service.
     *
     * \returns An ApplicationContainer holding the Application created.
     */
    ApplicationContainer
    Install (std::string nodeName) const;

    /**
     * \param c The nodes on which to create the Applications.  The nodes
     *          are specified by a NodeContainer.
     *
     * Create one udp echo server application on each of the Nodes in the
     * NodeContainer.
     *
     * \returns The applications created, one Application per Node in the
     *          NodeContainer.
     */
    ApplicationContainer
    Install (NodeContainer c) const;

    void SetRtrAddress(Address rtr);
    Address GetRtrAddress (void);

  private:
    /**
     * Install an ns3::UdpEchoServer on the node configured with all the
     * attributes set with SetAttribute.
     *
     * \param node The node on which an UdpEchoServer will be installed.
     * \returns Ptr to the application installed.
     */
  Ptr<Application> InstallPriv (Ptr<Node> node) const;

  ObjectFactory m_factory; //!<Object factory
  Address m_rtrAddress; // Address of the configured RTR
};

} /* namespace ns3 */

#endif /* SRC_INTERNET_HELPER_LISP_HELPER_MAP_SERVER_HELPER_H_ */
