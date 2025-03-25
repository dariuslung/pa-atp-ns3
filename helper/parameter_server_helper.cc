#include "parameter_server_helper.h"

#include "ns3/parameter_server.h"
#include "ns3/uinteger.h"

namespace ns3
{

ParameterServerHelper::ParameterServerHelper(uint16_t port)
    : ApplicationHelper(ParameterServer::GetTypeId())
{
    SetAttribute("Port", UintegerValue(port));
}

void
ParameterServerHelper::SetFill(Ptr<Application> app, const std::string& fill)
{
    app->GetObject<ParameterServer>()->SetFill(fill);
}

void
ParameterServerHelper::SetFill(Ptr<Application> app, uint8_t fill, uint32_t dataLength)
{
    app->GetObject<ParameterServer>()->SetFill(fill, dataLength);
}

void
ParameterServerHelper::SetFill(Ptr<Application> app,
                             uint8_t* fill,
                             uint32_t fillLength,
                             uint32_t dataLength)
{
    app->GetObject<ParameterServer>()->SetFill(fill, fillLength, dataLength);
}

} // namespace ns3
