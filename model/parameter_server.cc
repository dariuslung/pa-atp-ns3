#include "parameter_server.h"

#include "ns3/address-utils.h"
#include "ns3/inet-socket-address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/ipv4-address.h"
#include "ns3/ipv6-address.h"
#include "ns3/log.h"
#include "ns3/nstime.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/socket.h"
#include "ns3/udp-socket.h"
#include "ns3/uinteger.h"

#include <sstream>
#include <algorithm>
#include "ns3/my_utils.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("ParameterServerApplication");

NS_OBJECT_ENSURE_REGISTERED(ParameterServer);

TypeId
ParameterServer::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::ParameterServer")
            .SetParent<Application>()
            .SetGroupName("Applications")
            .AddConstructor<ParameterServer>()
            .AddAttribute("Port",
                          "Port to listen for incoming packets.",
                          UintegerValue(1),
                          MakeUintegerAccessor(&ParameterServer::m_port),
                          MakeUintegerChecker<uint16_t>())
            .AddAttribute("MaxPackets",
                          "The maximum number of packets the application will send (zero means infinite)",
                          UintegerValue(100),
                          MakeUintegerAccessor(&ParameterServer::m_count),
                          MakeUintegerChecker<uint32_t>())
            .AddAttribute("Interval",
                          "The time to wait between packets",
                          TimeValue(Seconds(1.0)),
                          MakeTimeAccessor(&ParameterServer::m_interval),
                          MakeTimeChecker())
            .AddAttribute("RemoteAddress",
                          "The destination Address of the outbound packets",
                          AddressValue(),
                          MakeAddressAccessor(&ParameterServer::m_peerAddr),
                          MakeAddressChecker())
            .AddAttribute("RemotePort",
                          "The destination port of the outbound packets",
                          UintegerValue(0),
                          MakeUintegerAccessor(&ParameterServer::m_peerPort),
                          MakeUintegerChecker<uint16_t>())
            .AddAttribute("Tos",
                          "The Type of Service used to send IPv4 packets. "
                          "All 8 bits of the TOS byte are set (including ECN bits).",
                          UintegerValue(0),
                          MakeUintegerAccessor(&ParameterServer::m_tos),
                          MakeUintegerChecker<uint8_t>())
            .AddAttribute("PacketSize",
                          "Size of echo data in outbound packets",
                          UintegerValue(100),
                          MakeUintegerAccessor(&ParameterServer::SetDataSize, &ParameterServer::GetDataSize),
                          MakeUintegerChecker<uint32_t>())
            .AddTraceSource("Tx",
                            "A new packet is created and is sent",
                            MakeTraceSourceAccessor(&ParameterServer::m_txTrace),
                            "ns3::Packet::TracedCallback")
            .AddTraceSource("Rx",
                            "A packet has been received",
                            MakeTraceSourceAccessor(&ParameterServer::m_rxTrace),
                            "ns3::Packet::TracedCallback")
            .AddTraceSource("TxWithAddresses",
                            "A new packet is created and is sent",
                            MakeTraceSourceAccessor(&ParameterServer::m_txTraceWithAddresses),
                            "ns3::Packet::TwoAddressTracedCallback")
            .AddTraceSource("RxWithAddresses",
                            "A packet has been received",
                            MakeTraceSourceAccessor(&ParameterServer::m_rxTraceWithAddresses),
                            "ns3::Packet::TwoAddressTracedCallback");
    return tid;
}

ParameterServer::ParameterServer()
{
    NS_LOG_FUNCTION(this);
    m_sent = 0;
    m_socket = nullptr;
    m_sendEvent = EventId();
    m_data = nullptr;
    m_dataSize = 0;
    m_gradientCount = 0;
}

ParameterServer::~ParameterServer()
{
    NS_LOG_FUNCTION(this);
    m_socket = nullptr;

    delete[] m_data;
    m_data = nullptr;
    m_dataSize = 0;
}

void
ParameterServer::SetRemote(Address ip, uint16_t port)
{
    NS_LOG_FUNCTION(this << ip << port);
    m_peerAddr = ip;
    m_peerPort = port;
}

void
ParameterServer::SetRemote(Address addr)
{
    NS_LOG_FUNCTION(this << addr);
    m_peerAddr = addr;
}

void
ParameterServer::StartApplication()
{
    NS_LOG_FUNCTION(this);

    if (!m_socket)
    {
        TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
        m_socket = Socket::CreateSocket(GetNode(), tid);
        InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), m_port);
        if (m_socket->Bind(local) == -1)
        {
            NS_FATAL_ERROR("Failed to bind socket");
        }
        if (addressUtils::IsMulticast(m_local))
        {
            Ptr<UdpSocket> udpSocket = DynamicCast<UdpSocket>(m_socket);
            if (udpSocket)
            {
                // equivalent to setsockopt (MCAST_JOIN_GROUP)
                udpSocket->MulticastJoinGroup(0, m_local);
            }
            else
            {
                NS_FATAL_ERROR("Error: Failed to join multicast group");
            }
        }
    }

    m_socket->SetIpTos(m_tos); // Affects only IPv4 sockets.
    m_socket->SetRecvCallback(MakeCallback(&ParameterServer::HandleRead, this));
    m_socket->SetAllowBroadcast(true);
}

void
ParameterServer::StopApplication()
{
    NS_LOG_FUNCTION(this);

    if (m_socket)
    {
        m_socket->Close();
        m_socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
        m_socket = nullptr;
    }

    Simulator::Cancel(m_sendEvent);
}

void
ParameterServer::SetDataSize(uint32_t dataSize)
{
    NS_LOG_FUNCTION(this << dataSize);

    //
    // If the client is setting the echo packet data size this way, we infer
    // that she doesn't care about the contents of the packet at all, so
    // neither will we.
    //
    delete[] m_data;
    m_data = nullptr;
    m_dataSize = 0;
    m_size = dataSize;
}

uint32_t
ParameterServer::GetDataSize() const
{
    NS_LOG_FUNCTION(this);
    return m_size;
}

void
ParameterServer::SetFill(std::string fill)
{
    NS_LOG_FUNCTION(this << fill);

    uint32_t dataSize = fill.size() + 1;

    if (dataSize != m_dataSize)
    {
        delete[] m_data;
        m_data = new uint8_t[dataSize];
        m_dataSize = dataSize;
    }

    memcpy(m_data, fill.c_str(), dataSize);

    //
    // Overwrite packet size attribute.
    //
    m_size = dataSize;
}

void
ParameterServer::SetFill(uint8_t fill, uint32_t dataSize)
{
    NS_LOG_FUNCTION(this << fill << dataSize);
    if (dataSize != m_dataSize)
    {
        delete[] m_data;
        m_data = new uint8_t[dataSize];
        m_dataSize = dataSize;
    }

    memset(m_data, fill, dataSize);

    //
    // Overwrite packet size attribute.
    //
    m_size = dataSize;
}

void
ParameterServer::SetFill(uint8_t* fill, uint32_t fillSize, uint32_t dataSize)
{
    NS_LOG_FUNCTION(this << fill << fillSize << dataSize);
    if (dataSize != m_dataSize)
    {
        delete[] m_data;
        m_data = new uint8_t[dataSize];
        m_dataSize = dataSize;
    }

    if (fillSize >= dataSize)
    {
        memcpy(m_data, fill, dataSize);
        m_size = dataSize;
        return;
    }

    //
    // Do all but the final fill.
    //
    uint32_t filled = 0;
    while (filled + fillSize < dataSize)
    {
        memcpy(&m_data[filled], fill, fillSize);
        filled += fillSize;
    }

    //
    // Last fill may be partial
    //
    memcpy(&m_data[filled], fill, dataSize - filled);

    //
    // Overwrite packet size attribute.
    //
    m_size = dataSize;
}

void
ParameterServer::HandleRead(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);
    Ptr<Packet> packet;
    Address from;
    Address localAddress;
    while ((packet = socket->RecvFrom(from)))
    {
        socket->GetSockName(localAddress);
        m_rxTrace(packet);
        m_rxTraceWithAddresses(packet, from, localAddress);

        uint8_t read_buffer[1000];
        packet->CopyData(read_buffer, packet->GetSize()); // Copy data from packet
        std::string read_data(read_buffer, read_buffer+packet->GetSize()); // Read data
        NS_LOG_INFO(Simulator::Now().As(Time::S) << " PS received : " << read_data);
        std::vector<std::string> pktGradient = split_string(read_data, (char *)",");
        
        // Broadcast to workers
        if (pktGradient[0] == "AACK") {
            return;
        }

        m_gradientCount++;
        // IMPLEMENT BROADCAST AACK
        SetFill("AACK," + pktGradient[1] + ',' + pktGradient[2]);
        Ptr<Packet> pktAACK;
        pktAACK = Create<Packet>(m_data, m_dataSize);
        Address dstAddress = InetSocketAddress(Ipv4Address("255.255.255.255"), m_peerPort);
        socket->SendTo(pktAACK, 0, dstAddress);
        if (InetSocketAddress::IsMatchingType(dstAddress))
        {
            NS_LOG_INFO(Simulator::Now().As(Time::S) << " PS sent AACK ( "
                        << InetSocketAddress::ConvertFrom(dstAddress).GetIpv4() << " port "
                        << InetSocketAddress::ConvertFrom(dstAddress).GetPort() << " )");
        }
    }
}

} // Namespace ns3
