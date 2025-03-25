#include "aggregate_switch.h"

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

#include "ns3/my_utils.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("AggregateSwitchApplication");

NS_OBJECT_ENSURE_REGISTERED(AggregateSwitch);

TypeId
AggregateSwitch::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::AggregateSwitch")
            .SetParent<Application>()
            .SetGroupName("Applications")
            .AddConstructor<AggregateSwitch>()
            .AddAttribute("Port",
                          "Port to listen for incoming packets.",
                          UintegerValue(1),
                          MakeUintegerAccessor(&AggregateSwitch::m_port),
                          MakeUintegerChecker<uint16_t>())
            .AddAttribute("Tos",
                          "The Type of Service used to send IPv4 packets. "
                          "All 8 bits of the TOS byte are set (including ECN bits).",
                          UintegerValue(0),
                          MakeUintegerAccessor(&AggregateSwitch::m_tos),
                          MakeUintegerChecker<uint8_t>())
            .AddAttribute("MaxParts",
                          "Maximum number of parts for one pktGradient",
                          UintegerValue(1),
                          MakeUintegerAccessor(&AggregateSwitch::m_maxParts),
                          MakeUintegerChecker<uint8_t>())
            .AddAttribute("RemoteAddress",
                          "The destination Address of the outbound packets",
                          AddressValue(),
                          MakeAddressAccessor(&AggregateSwitch::m_peerAddr),
                          MakeAddressChecker())
            .AddAttribute("RemotePort",
                          "The destination port of the outbound packets",
                          UintegerValue(0),
                          MakeUintegerAccessor(&AggregateSwitch::m_peerPort),
                          MakeUintegerChecker<uint16_t>())
            .AddTraceSource("Rx",
                            "A packet has been received",
                            MakeTraceSourceAccessor(&AggregateSwitch::m_rxTrace),
                            "ns3::Packet::TracedCallback")
            .AddTraceSource("Fw",
                            "A packet has been forwarded",
                            MakeTraceSourceAccessor(&AggregateSwitch::m_fwTrace),
                            "ns3::Packet::TracedCallback")
            .AddTraceSource("RxWithAddresses",
                            "A packet has been received",
                            MakeTraceSourceAccessor(&AggregateSwitch::m_rxTraceWithAddresses),
                            "ns3::Packet::TwoAddressTracedCallback")
            .AddTraceSource("FwWithAddresses",
                            "A packet has been forwarded",
                            MakeTraceSourceAccessor(&AggregateSwitch::m_fwTraceWithAddresses),
                            "ns3::Packet::TwoAddressTracedCallback");

    return tid;
}

AggregateSwitch::AggregateSwitch()
{
    NS_LOG_FUNCTION(this);
    m_dataSize = 0;
    m_data = nullptr;
    m_sent = 0;
    m_buffer.clear();
}

AggregateSwitch::~AggregateSwitch()
{
    NS_LOG_FUNCTION(this);
    m_socket = nullptr;
    m_socket6 = nullptr;

    delete[] m_data;
    m_data = nullptr;
    m_dataSize = 0;
}

void
AggregateSwitch::SetRemote(Address ip, uint16_t port)
{
    NS_LOG_FUNCTION(this << ip << port);
    m_peerAddr = ip;
    m_peerPort = port;
}

void
AggregateSwitch::SetRemote(Address addr)
{
    NS_LOG_FUNCTION(this << addr);
    m_peerAddr = addr;
}

void
AggregateSwitch::StartApplication()
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

    if (!m_socket6)
    {
        TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
        m_socket6 = Socket::CreateSocket(GetNode(), tid);
        Inet6SocketAddress local6 = Inet6SocketAddress(Ipv6Address::GetAny(), m_port);
        if (m_socket6->Bind(local6) == -1)
        {
            NS_FATAL_ERROR("Failed to bind socket");
        }
        if (addressUtils::IsMulticast(local6))
        {
            Ptr<UdpSocket> udpSocket = DynamicCast<UdpSocket>(m_socket6);
            if (udpSocket)
            {
                // equivalent to setsockopt (MCAST_JOIN_GROUP)
                udpSocket->MulticastJoinGroup(0, local6);
            }
            else
            {
                NS_FATAL_ERROR("Error: Failed to join multicast group");
            }
        }
    }

    m_socket->SetIpTos(m_tos); // Affects only IPv4 sockets.
    m_socket->SetRecvCallback(MakeCallback(&AggregateSwitch::HandleRead, this));
    m_socket->SetAllowBroadcast(true);
}

void
AggregateSwitch::StopApplication()
{
    NS_LOG_FUNCTION(this);

    if (m_socket)
    {
        m_socket->Close();
        m_socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
    }
    if (m_socket6)
    {
        m_socket6->Close();
        m_socket6->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
    }
}

void
AggregateSwitch::SetFill(std::string fill)
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
AggregateSwitch::SendResult()
{
    NS_LOG_FUNCTION(this);
    Ptr<Packet> p;
    if (m_dataSize)
    {
        //
        // If m_dataSize is non-zero, we have a data m_buffer of the same size that we
        // are expected to copy and send.  This state of affairs is created if one of
        // the Fill functions is called.  In this case, m_size must have been set
        // to agree with m_dataSize
        //
        NS_ASSERT_MSG(m_dataSize == m_size,
                      "AggregateSwitch::Send(): m_size and m_dataSize inconsistent");
        NS_ASSERT_MSG(m_data, "AggregateSwitch::Send(): m_dataSize but no m_data");
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
    m_fwTrace(p);
    if (Ipv4Address::IsMatchingType(m_peerAddr))
    {
        m_fwTraceWithAddresses(
            p,
            localAddress,
            InetSocketAddress(Ipv4Address::ConvertFrom(m_peerAddr), m_peerPort));
    }
    Address dstAddress = InetSocketAddress(Ipv4Address::ConvertFrom(m_peerAddr), m_peerPort);
    m_socket->SendTo(p, 0, dstAddress);
    ++m_sent;

    if (Ipv4Address::IsMatchingType(m_peerAddr))
    {
        NS_LOG_INFO(Simulator::Now().As(Time::S) << " switch sent result ( " 
                    << Ipv4Address::ConvertFrom(m_peerAddr)
                    << " port " << m_peerPort << " )");
    }
    else if (InetSocketAddress::IsMatchingType(m_peerAddr))
    {
        NS_LOG_INFO(
            Simulator::Now().As(Time::S) << " switch sent result ( "
            << InetSocketAddress::ConvertFrom(m_peerAddr).GetIpv4() << " port "
            << InetSocketAddress::ConvertFrom(m_peerAddr).GetPort() << " )");
    }
}

void
AggregateSwitch::HandleRead(Ptr<Socket> socket)
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

        // Log packet data
        uint8_t read_buffer[1000];
        packet->CopyData(read_buffer, packet->GetSize());
        std::string read_data(read_buffer, read_buffer+packet->GetSize());
        NS_LOG_INFO(Simulator::Now().As(Time::S) << " switch received : " << read_data);
        std::vector<std::string> pktGradient = split_string(read_data, (char *)",");

        // Broadcast to workers
        if (pktGradient[0] == "AACK") {
            Address dstAddress = InetSocketAddress(Ipv4Address("255.255.255.255"), m_peerPort);
            socket->SendTo(packet, 0, from);
            return;
        }

        // GACK
        SetFill("GACK," + pktGradient[2]);
        Ptr<Packet> pktGACK;
        pktGACK = Create<Packet>(m_data, m_dataSize);
        socket->SendTo(pktGACK, 0, from);

        if (InetSocketAddress::IsMatchingType(from))
        {
            NS_LOG_INFO(Simulator::Now().As(Time::S) << " switch sent GACK ( "
                        << InetSocketAddress::ConvertFrom(from).GetIpv4() << " port "
                        << InetSocketAddress::ConvertFrom(from).GetPort() << " )");
        }
        
        // Buffer
        if (m_buffer.size() >= m_bufferSize) {
            NS_LOG_INFO(Simulator::Now().As(Time::S) << " buffer overflow");
            // IMPLEMENT FORWARDING
            return;
        }
        std::string key = pktGradient[0] + ',' + pktGradient[2];
        uint16_t part = std::stoi(pktGradient[1]);
        auto ret = m_buffer.insert(std::pair<std::string, std::set<uint16_t>>(key, std::set<uint16_t>({part})));
        if (ret.second == false) {      // Already exists in m_buffer
            auto retSet = m_buffer[key].insert(part);       // Insert new part
            if (retSet.second == false) {       // Duplicate part
                NS_LOG_INFO(Simulator::Now().As(Time::S) << " ERROR: part duplicate found");
            }
        }
        if (m_buffer[key].size() == m_maxParts) {       // Check if all parts present, perform aggregation
            std::string result = "RESULT," + pktGradient[0] + ',' + pktGradient[2];
            SetFill(result);
            SendResult();
            m_buffer.erase(key);
        }
    }
}

} // Namespace ns3
