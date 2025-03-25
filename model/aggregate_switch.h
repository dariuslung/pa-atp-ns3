#ifndef AGGREGATE_SWITCH_H
#define AGGREGATE_SWITCH_H

#include "ns3/address.h"
#include "ns3/application.h"
#include "ns3/event-id.h"
#include "ns3/ptr.h"
#include "ns3/traced-callback.h"

#include <map>
#include <set>

namespace ns3
{

class Socket;
class Packet;

/**
 * \ingroup applications
 * \defgroup udpecho UdpEcho
 */

/**
 * \ingroup udpecho
 * \brief A Udp Echo server
 *
 * Every packet received is sent back.
 */
class AggregateSwitch : public Application
{
  public:
    /**
     * \brief Get the type ID.
     * \return the object TypeId
     */
    static TypeId GetTypeId();
    AggregateSwitch();
    ~AggregateSwitch() override;

    //////////// CUSTOM ////////////
    void SetRemote(Address ip, uint16_t port);
    void SetRemote(Address addr);
    void SetFill(std::string fill);
    void SendResult();       // Send result to its destination
    ////////////////////////////////

  private:
    void StartApplication() override;
    void StopApplication() override;

    /**
     * \brief Handle a packet reception.
     *
     * This function is called by lower layers.
     *
     * \param socket the socket the packet was received to.
     */
    void HandleRead(Ptr<Socket> socket);

    uint16_t m_port;       //!< Port to listen for incoming packets.

    uint8_t m_tos;         //!< The packets Type of Service
    Ptr<Socket> m_socket;  //!< IPv4 Socket
    Ptr<Socket> m_socket6; //!< IPv6 Socket
    Address m_local;       //!< local multicast address
    Address m_peerAddr; //!< Remote peer address
    uint16_t m_peerPort;   //!< Remote peer port

    uint32_t m_size;
    uint32_t m_dataSize;
    uint8_t *m_data;       // Packet data
    uint32_t m_sent;       //!< Counter for sent packets

    uint16_t m_maxParts;
    uint16_t m_bufferSize = 10;
    std::map<std::string, std::set<uint16_t>> m_buffer;

    /// Callbacks for tracing the packet Fw events
    TracedCallback<Ptr<const Packet>> m_fwTrace;

    /// Callbacks for tracing the packet Rx events
    TracedCallback<Ptr<const Packet>> m_rxTrace;

    /// Callbacks for tracing the packet Fw events, includes source and destination addresses
    TracedCallback<Ptr<const Packet>, const Address&, const Address&> m_fwTraceWithAddresses;

    /// Callbacks for tracing the packet Rx events, includes source and destination addresses
    TracedCallback<Ptr<const Packet>, const Address&, const Address&> m_rxTraceWithAddresses;
};

} // namespace ns3

#endif /* UDP_ECHO_SERVER_H */
