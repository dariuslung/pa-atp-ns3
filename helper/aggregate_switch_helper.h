#ifndef AGGREGATE_SWITCH_HELPER_H
#define AGGREGATE_SWITCH_HELPER_H

#include <ns3/application-helper.h>

#include <stdint.h>

namespace ns3
{

/**
 * \ingroup udpecho
 * \brief Create a server application which waits for input UDP packets
 *        and sends them back to the original sender.
 */
class AggregateSwitchHelper : public ApplicationHelper
{
  public:
    /**
     * Create AggregateSwitchHelper which will make life easier for people trying
     * to set up simulations with echos.
     *
     * \param port The port the server will wait on for incoming packets
     */
    AggregateSwitchHelper(uint16_t port);
    AggregateSwitchHelper(uint16_t port, const Address& ip, uint16_t dst_port);
};


} // namespace ns3

#endif /* AGGREGATE_SWITCH_HELPER_H */
