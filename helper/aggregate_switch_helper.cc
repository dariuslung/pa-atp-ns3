#include "aggregate_switch_helper.h"

#include "ns3/aggregate_switch.h"
#include "ns3/uinteger.h"

namespace ns3
{
AggregateSwitchHelper::AggregateSwitchHelper(uint16_t port)
    : ApplicationHelper(AggregateSwitch::GetTypeId())
{
    SetAttribute("Port", UintegerValue(port));
}

AggregateSwitchHelper::AggregateSwitchHelper(uint16_t port, const Address& ip, uint16_t dst_port)
    : ApplicationHelper(AggregateSwitch::GetTypeId())
{
    SetAttribute("Port", UintegerValue(port));
    SetAttribute("RemoteAddress", AddressValue(ip));
    SetAttribute("RemotePort", UintegerValue(dst_port));
}

} // namespace ns3
