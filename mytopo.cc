/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"

#include <vector>
#include "ns3/aggregate_switch_helper.h"
#include "ns3/aggregate_switch.h"
#include "ns3/custom_client_helper.h"
#include "ns3/custom_client.h"
#include "ns3/parameter_server_helper.h"
#include "ns3/parameter_server.h"

// Default Network Topology
//
//        10.1.1.0
//  n0 -------------- n1
//     point-to-point
//

// Dumbbell Topology
//
//  n0 ---
//       |
//      s1 ---- s2
//       |
//  n1 ---
//

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("CustomScript");

int
main(int argc, char* argv[])
{
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("CustomClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("AggregateSwitchApplication", LOG_LEVEL_INFO);
    LogComponentEnable ("ParameterServerApplication", LOG_LEVEL_INFO);

    // Nodes
    NodeContainer leftWingNodes;
    leftWingNodes.Create(3);
    NodeContainer rightWingNodes;
    rightWingNodes.Create(1);
    NodeContainer bottleneckNodes;
    bottleneckNodes.Create(2);

    // PointToPoint links
    NetDeviceContainer leftWingDevices;
    NetDeviceContainer rightWingDevices;
    NetDeviceContainer leftSwitchDevices;
    NetDeviceContainer rightSwitchDevices;
    NetDeviceContainer bottleneckDevices;   // Bottleneck switches left and right

    PointToPointHelper ptp1;
    ptp1.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    ptp1.SetChannelAttribute("Delay", StringValue("2ms"));

    PointToPointHelper ptp2;
    ptp2.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    ptp2.SetChannelAttribute("Delay", StringValue("4ms"));

    // Install ptp links for left wing and left switch
    NetDeviceContainer cl1 = ptp1.Install(leftWingNodes.Get(0), bottleneckNodes.Get(0));
    leftWingDevices.Add(cl1.Get(0));
    leftSwitchDevices.Add(cl1.Get(1));

    NetDeviceContainer cl2 = ptp2.Install(leftWingNodes.Get(1), bottleneckNodes.Get(0));
    leftWingDevices.Add(cl2.Get(0));
    leftSwitchDevices.Add(cl2.Get(1));

    NetDeviceContainer cl3 = ptp2.Install(leftWingNodes.Get(2), bottleneckNodes.Get(0));
    leftWingDevices.Add(cl3.Get(0));
    leftSwitchDevices.Add(cl3.Get(1));
    
    // Install ptp links for right wing and right switch
    NetDeviceContainer cr1 = ptp1.Install(rightWingNodes.Get(0), bottleneckNodes.Get(1));
    rightWingDevices.Add(cr1.Get(0));
    rightSwitchDevices.Add(cr1.Get(1));

    // Install ptp link for bottleneck
    bottleneckDevices.Add(ptp2.Install(bottleneckNodes.Get(0), bottleneckNodes.Get(1)));

    // Internet Stack
    InternetStackHelper stack;
    stack.Install(leftWingNodes);
    stack.Install(rightWingNodes);
    stack.Install(bottleneckNodes);

    // IPv4 Address
    Ipv4InterfaceContainer leftWingIfc;
    Ipv4InterfaceContainer rightWingIfc;
    Ipv4InterfaceContainer leftSwitchIfc;
    Ipv4InterfaceContainer rightSwitchIfc;
    Ipv4InterfaceContainer bottleneckIfc;
    Ipv4AddressHelper address;

    // Assign IP to bottleneck
    address.SetBase("10.1.1.0", "255.255.255.0"); // or use NetMask("/24")
    NetDeviceContainer ndcSwitch;
    ndcSwitch.Add(bottleneckDevices.Get(0));
    ndcSwitch.Add(bottleneckDevices.Get(1));
    bottleneckIfc = address.Assign(NetDeviceContainer(ndcSwitch));
    address.NewNetwork();

    // Assign IP to left wing
    address.SetBase("10.2.1.0", "255.255.255.0");
    for (uint32_t i = 0; i < leftWingNodes.GetN(); ++i) {
        NetDeviceContainer ndc;
        ndc.Add(leftWingDevices.Get(i));
        ndc.Add(leftSwitchDevices.Get(i));
        Ipv4InterfaceContainer ifc = address.Assign(ndc);
        leftWingIfc.Add(ifc.Get(0));
        leftSwitchIfc.Add(ifc.Get(1));
        address.NewNetwork();
    }

    // Assign IP to right wing
    address.SetBase("10.3.1.0", "255.255.255.0");
    for (uint32_t i = 0; i < rightWingNodes.GetN(); ++i) {
        NetDeviceContainer ndc;
        ndc.Add(rightWingDevices.Get(i));
        ndc.Add(rightSwitchDevices.Get(i));
        Ipv4InterfaceContainer ifc = address.Assign(ndc);
        rightWingIfc.Add(ifc.Get(0));
        rightSwitchIfc.Add(ifc.Get(1));
        address.NewNetwork();
    }

    const Address leftSwitchAddr = bottleneckIfc.GetAddress(0);
    const Address rightSwitchAddr = bottleneckIfc.GetAddress(1);

    // Port numbers
    uint16_t inPort = 9;

    // Right INA Switch
    uint16_t maxParts = 3;
    AggregateSwitchHelper aggregateSwitch(inPort, rightWingIfc.GetAddress(0), inPort); // params : open port, dst address, dst port
    aggregateSwitch.SetAttribute("MaxParts", UintegerValue(maxParts));

    ApplicationContainer switchApp = aggregateSwitch.Install(bottleneckNodes.Get(1)); // Install right switch
    switchApp.Start(Seconds(0.0));
    switchApp.Stop(Seconds(10.0));

    // Job 1 Part 1
    uint16_t workerID = 0;
    CustomClientHelper cc0(rightSwitchAddr, inPort); // params : dst address, dst port
    cc0.SetAttribute("Port", UintegerValue(inPort));
    cc0.SetAttribute("MaxPackets", UintegerValue(2));
    cc0.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    cc0.SetAttribute("PacketSize", UintegerValue(1024));
    cc0.SetAttribute("JobId", UintegerValue(1));
    cc0.SetAttribute("PartId", UintegerValue(0));

    ApplicationContainer cApp0 = cc0.Install(leftWingNodes.Get(workerID));
    cApp0.Start(Seconds(1.0));
    cApp0.Stop(Seconds(10.0));

    // Job 1 Part 2
    workerID = 1;
    CustomClientHelper cc1(rightSwitchAddr, inPort); // params : dst address, dst port
    cc1.SetAttribute("Port", UintegerValue(inPort));
    cc1.SetAttribute("MaxPackets", UintegerValue(2));
    cc1.SetAttribute("Interval", TimeValue(Seconds(1.5)));
    cc1.SetAttribute("PacketSize", UintegerValue(1024));
    cc1.SetAttribute("JobId", UintegerValue(1));
    cc1.SetAttribute("PartId", UintegerValue(1));

    ApplicationContainer cApp1 = cc1.Install(leftWingNodes.Get(workerID));
    cApp1.Start(Seconds(1.0));
    cApp1.Stop(Seconds(10.0));

    // Job 1 Part 3
    workerID = 2;
    CustomClientHelper cc2(rightSwitchAddr, inPort); // params : dst address, dst port
    cc2.SetAttribute("Port", UintegerValue(inPort));
    cc2.SetAttribute("MaxPackets", UintegerValue(2));
    cc2.SetAttribute("Interval", TimeValue(Seconds(1.5)));
    cc2.SetAttribute("PacketSize", UintegerValue(1024));
    cc2.SetAttribute("JobId", UintegerValue(1));
    cc2.SetAttribute("PartId", UintegerValue(2));

    ApplicationContainer cApp2 = cc2.Install(leftWingNodes.Get(workerID));
    cApp2.Start(Seconds(1.0));
    cApp2.Stop(Seconds(10.0));

    // PS Job 1
    uint16_t psID = 0;
    ParameterServerHelper ps0(inPort); // open port
    ps0.SetAttribute("MaxPackets", UintegerValue(0));
    ps0.SetAttribute("RemotePort", UintegerValue(inPort));

    ApplicationContainer psApp0 = ps0.Install(rightWingNodes.Get(psID));
    psApp0.Start(Seconds(0.0));
    psApp0.Stop(Seconds(10.0));

    // Routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Simulator
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}
