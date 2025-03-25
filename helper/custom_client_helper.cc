#include "custom_client_helper.h"

#include "ns3/custom_client.h"
#include "ns3/uinteger.h"

namespace ns3
{

CustomClientHelper::CustomClientHelper(const Address& address, uint16_t port)
    : ApplicationHelper(CustomClient::GetTypeId())
{
    SetAttribute("RemoteAddress", AddressValue(address));
    SetAttribute("RemotePort", UintegerValue(port));
}

CustomClientHelper::CustomClientHelper(const Address& address)
    : ApplicationHelper(CustomClient::GetTypeId())
{
    SetAttribute("RemoteAddress", AddressValue(address));
}

void
CustomClientHelper::SetFill(Ptr<Application> app, const std::string& fill)
{
    app->GetObject<CustomClient>()->SetFill(fill);
}

void
CustomClientHelper::SetFill(Ptr<Application> app, uint8_t fill, uint32_t dataLength)
{
    app->GetObject<CustomClient>()->SetFill(fill, dataLength);
}

void
CustomClientHelper::SetFill(Ptr<Application> app,
                             uint8_t* fill,
                             uint32_t fillLength,
                             uint32_t dataLength)
{
    app->GetObject<CustomClient>()->SetFill(fill, fillLength, dataLength);
}

} // namespace ns3
