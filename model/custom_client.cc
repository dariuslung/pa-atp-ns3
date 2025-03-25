#include "custom_client.h"

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
#include "ns3/trace-source-accessor.h"
#include "ns3/uinteger.h"
#include "ns3/udp-socket.h"

#include <sstream>
#include <algorithm>
#include "ns3/my_utils.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("CustomClientApplication");

NS_OBJECT_ENSURE_REGISTERED(CustomClient);

TypeId
CustomClient::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::CustomClient")
            .SetParent<Application>()
            .SetGroupName("Applications")
            .AddConstructor<CustomClient>()
            .AddAttribute("MaxPackets",
                          "The maximum number of packets the application will send (zero means infinite)",
                          UintegerValue(100),
                          MakeUintegerAccessor(&CustomClient::m_count),
                          MakeUintegerChecker<uint32_t>())
            .AddAttribute("Interval",
                          "The time to wait between packets",
                          TimeValue(Seconds(1.0)),
                          MakeTimeAccessor(&CustomClient::m_interval),
                          MakeTimeChecker())
            .AddAttribute("Port",
                          "Port for receiving packets",
                          UintegerValue(1),
                          MakeUintegerAccessor(&CustomClient::m_port),
                          MakeUintegerChecker<uint16_t>())
            .AddAttribute("RemoteAddress",
                          "The destination Address of the outbound packets",
                          AddressValue(),
                          MakeAddressAccessor(&CustomClient::m_peerAddr),
                          MakeAddressChecker())
            .AddAttribute("RemotePort",
                          "The destination port of the outbound packets",
                          UintegerValue(0),
                          MakeUintegerAccessor(&CustomClient::m_peerPort),
                          MakeUintegerChecker<uint16_t>())
            .AddAttribute("Tos",
                          "The Type of Service used to send IPv4 packets. "
                          "All 8 bits of the TOS byte are set (including ECN bits).",
                          UintegerValue(0),
                          MakeUintegerAccessor(&CustomClient::m_tos),
                          MakeUintegerChecker<uint8_t>())
            .AddAttribute("PacketSize",
                          "Size of echo data in outbound packets",
                          UintegerValue(100),
                          MakeUintegerAccessor(&CustomClient::SetDataSize, &CustomClient::GetDataSize),
                          MakeUintegerChecker<uint32_t>())
            .AddAttribute("JobId",
                          "Job ID number",
                          UintegerValue(0),
                          MakeUintegerAccessor(&CustomClient::m_jobId),
                          MakeUintegerChecker<uint16_t>())
            .AddAttribute("PartId",
                          "Part ID number",
                          UintegerValue(0),
                          MakeUintegerAccessor(&CustomClient::m_partId),
                          MakeUintegerChecker<uint16_t>())
            .AddTraceSource("Tx",
                            "A new packet is created and is sent",
                            MakeTraceSourceAccessor(&CustomClient::m_txTrace),
                            "ns3::Packet::TracedCallback")
            .AddTraceSource("Rx",
                            "A packet has been received",
                            MakeTraceSourceAccessor(&CustomClient::m_rxTrace),
                            "ns3::Packet::TracedCallback")
            .AddTraceSource("TxWithAddresses",
                            "A new packet is created and is sent",
                            MakeTraceSourceAccessor(&CustomClient::m_txTraceWithAddresses),
                            "ns3::Packet::TwoAddressTracedCallback")
            .AddTraceSource("RxWithAddresses",
                            "A packet has been received",
                            MakeTraceSourceAccessor(&CustomClient::m_rxTraceWithAddresses),
                            "ns3::Packet::TwoAddressTracedCallback");
    return tid;
}

CustomClient::CustomClient()
{
    NS_LOG_FUNCTION(this);
    m_sent = 0;
    m_socket = nullptr;
    m_sendEvent = EventId();
    m_data = nullptr;
    m_dataSize = 0;
    m_lastAACK = 0;
    m_lastGACK = 0;
    m_AWD = 15;
    m_CWD = 5;
}

CustomClient::~CustomClient()
{
    NS_LOG_FUNCTION(this);
    m_socket = nullptr;

    delete[] m_data;
    m_data = nullptr;
    m_dataSize = 0;
}

void
CustomClient::SetRemote(Address ip, uint16_t port)
{
    NS_LOG_FUNCTION(this << ip << port);
    m_peerAddr = ip;
    m_peerPort = port;
}

void
CustomClient::SetRemote(Address addr)
{
    NS_LOG_FUNCTION(this << addr);
    m_peerAddr = addr;
}

void
CustomClient::StartApplication()
{
    NS_LOG_FUNCTION(this);

    if (!m_socket)
    {
        TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
        m_socket = Socket::CreateSocket(GetNode(), tid);
        InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), m_port);
        NS_ABORT_MSG_IF(m_peerAddr.IsInvalid(), "'RemoteAddress' attribute not properly set");
        if (Ipv4Address::IsMatchingType(m_peerAddr))
        {
            if (m_socket->Bind(local) == -1)
            {
                NS_FATAL_ERROR("Failed to bind socket");
            }
            m_socket->SetIpTos(m_tos); // Affects only IPv4 sockets.
            m_socket->Connect(
                InetSocketAddress(Ipv4Address::ConvertFrom(m_peerAddr), m_peerPort));
        }
        else if (InetSocketAddress::IsMatchingType(m_peerAddr))
        {
            if (m_socket->Bind(local) == -1)
            {
                NS_FATAL_ERROR("Failed to bind socket");
            }
            m_socket->SetIpTos(m_tos); // Affects only IPv4 sockets.
            m_socket->Connect(m_peerAddr);
        }
        else
        {
            NS_ASSERT_MSG(false, "Incompatible address type: " << m_peerAddr);
        }
    }

    m_socket->SetRecvCallback(MakeCallback(&CustomClient::HandleRead, this));
    m_socket->SetAllowBroadcast(true);
    if (m_sent < m_count) {
        ScheduleTransmit(Seconds(0.));
    }
}

void
CustomClient::StopApplication()
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
CustomClient::SetDataSize(uint32_t dataSize)
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
CustomClient::GetDataSize() const
{
    NS_LOG_FUNCTION(this);
    return m_size;
}

void
CustomClient::SetFill(std::string fill)
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
CustomClient::SetFill(uint8_t fill, uint32_t dataSize)
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
CustomClient::SetFill(uint8_t* fill, uint32_t fillSize, uint32_t dataSize)
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
CustomClient::ScheduleTransmit(Time dt)
{
    NS_LOG_FUNCTION(this << dt);
    m_sendEvent = Simulator::Schedule(dt, &CustomClient::Send, this);
}

void
CustomClient::Send()
{
    NS_LOG_FUNCTION(this);

    NS_ASSERT(m_sendEvent.IsExpired());

    // Format : jobId,partId,gradientId
    std::stringstream ss;
    ss << m_jobId << ',' << m_partId << ',' << m_sent;
    SetFill(ss.str()); // Set payload to m_sent because counting packets as test
    Ptr<Packet> p;
    if (m_dataSize)
    {
        //
        // If m_dataSize is non-zero, we have a data buffer of the same size that we
        // are expected to copy and send.  This state of affairs is created if one of
        // the Fill functions is called.  In this case, m_size must have been set
        // to agree with m_dataSize
        //
        NS_ASSERT_MSG(m_dataSize == m_size,
                      "CustomClient::Send(): m_size and m_dataSize inconsistent");
        NS_ASSERT_MSG(m_data, "CustomClient::Send(): m_dataSize but no m_data");
        // Create packet with data
        p = Create<Packet>(m_data, m_dataSize);
    }
    else
    {
        //
        // If m_dataSize is zero, the client has indicated that it doesn't care
        // about the data itself either by specifying the data size by setting
        // the corresponding attribute or by not calling a SetFill function.  In
        // this case, we don't worry about it either.  But we do allow m_size
        // to have a value different from the (zero) m_dataSize.
        //
        p = Create<Packet>(m_size);
    }
    Address localAddress;
    m_socket->GetSockName(localAddress);
    // call to the trace sinks before the packet is actually sent,
    // so that tags added to the packet can be sent as well
    m_txTrace(p);
    if (Ipv4Address::IsMatchingType(m_peerAddr))
    {
        m_txTraceWithAddresses(
            p,
            localAddress,
            InetSocketAddress(Ipv4Address::ConvertFrom(m_peerAddr), m_peerPort));
    }
    m_socket->Send(p);
    ++m_sent;

    if (Ipv4Address::IsMatchingType(m_peerAddr))
    {
        NS_LOG_INFO(Simulator::Now().As(Time::S) << " worker ( " << m_jobId << ',' << m_partId << " ) sent " << m_size
                    << " bytes ( " << Ipv4Address::ConvertFrom(m_peerAddr)
                    << " port " << m_peerPort << " )");
    }
    else if (InetSocketAddress::IsMatchingType(m_peerAddr))
    {
        NS_LOG_INFO(
            Simulator::Now().As(Time::S) << " worker ( " << m_jobId << ',' << m_partId << " ) sent " << m_size << " bytes ( "
            << InetSocketAddress::ConvertFrom(m_peerAddr).GetIpv4() << " port "
            << InetSocketAddress::ConvertFrom(m_peerAddr).GetPort() << " )");
    }

    // if (m_sent < m_count)
    // {
    //     ScheduleTransmit(m_interval);
    // }
}

void
CustomClient::HandleRead(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);
    Ptr<Packet> packet;
    Address from;
    Address localAddress;
    while ((packet = socket->RecvFrom(from)))
    {
        uint8_t read_buffer[1000];
        packet->CopyData(read_buffer, packet->GetSize()); // Copy data from packet
        std::string read_data(read_buffer, read_buffer+packet->GetSize()); // Read data
        // if (InetSocketAddress::IsMatchingType(from))
        // {
        //     NS_LOG_INFO(Simulator::Now().As(Time::S) << " client received "
        //                 << packet->GetSize() << " bytes ( "
        //                 << InetSocketAddress::ConvertFrom(from).GetIpv4() << " port "
        //                 << InetSocketAddress::ConvertFrom(from).GetPort() << " )");
        // }
        NS_LOG_INFO(Simulator::Now().As(Time::S) << " worker ( " 
                    << m_jobId << ',' << m_partId << " ) received : " << read_data);
        socket->GetSockName(localAddress);
        m_rxTrace(packet);
        m_rxTraceWithAddresses(packet, from, localAddress);
        std::vector<std::string> pktAck = split_string(read_data, (char *)",");
        if (pktAck[0] == "GACK") {
            m_lastGACK = stoi(pktAck[1]);
            m_sent = m_lastGACK + 1;
            uint32_t boundary = std::min(m_lastGACK + m_CWD, m_lastAACK + m_AWD);
            if (m_sent < m_count && m_sent <= boundary) {   // m_sent = next to send, so within boundary is ok
                ScheduleTransmit(m_interval);

            }
        }


    }
}

} // Namespace ns3
