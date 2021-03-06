/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
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
 *
 * Author: Vikas Pushkar (Adapted from third.cc)
 */


#include "ns3/core-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/netanim-module.h"
#include "ns3/basic-energy-source.h"
#include "ns3/simple-device-energy-model.h"
#include "ns3/config-store-module.h"

#include <iostream>
#include <fstream>
#include <vector>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WirelessAnimationExample");

uint32_t nWifi;
uint32_t nObstacles;
NodeContainer allNodes;
NodeContainer passiveNodes;
TypeId tid;
Ipv4InterfaceContainer staInterfaces;

void ReceivePacket(Ptr<Socket> socket) {
    while (socket->Recv()) {
        NS_LOG_UNCOND("Received one packet!");
    }
}

static void GenerateTraffic(Ptr<Socket> socket, uint32_t pktSize,
        uint32_t pktCount, Time pktInterval) {
    // todo: num of nodes    
    //    int r = rand() % nWifi;
    //    int r2 = rand() % nWifi;
    //    
    //    Ptr<Socket> source = Socket::CreateSocket(allNodes.Get(r), tid);
    //    InetSocketAddress remote = InetSocketAddress(staInterfaces.GetAddress(r2, 0), 80);
    //    source->Connect(remote);
    //    
    if (pktCount > 0)
    {
        socket->Send (Create<Packet> (pktSize));
        Simulator::Schedule (pktInterval, &GenerateTraffic, 
                socket, pktSize,pktCount-1, pktInterval);
    }
    else
    {
        socket->Close ();
    }
}

int main(int argc, char *argv[]) 
{
    nWifi = 20;
    nObstacles = 5;
    double rss = -80; // -dBm
    uint32_t packetSize = 1000; // bytes
    uint32_t numPackets = 50;
    double interval = 0.1; // seconds
    
    srand(time(0));
    
    CommandLine cmd;
    cmd.AddValue("nWifi", "Number of Cars", nWifi);
    cmd.AddValue("nObsacles", "Number of Obstacles", nObstacles);
    cmd.Parse(argc, argv);
    
    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(nWifi);
    allNodes.Add(wifiStaNodes);
    
    NodeContainer obstacleNodes;
    obstacleNodes.Create(nObstacles);
    passiveNodes.Add(obstacleNodes);
    
    std::string phyMode ("DsssRate1Mbps");
    
    WifiHelper wifi;
    wifi.SetStandard (WIFI_PHY_STANDARD_80211b);
    
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    // This is one parameter that matters when using FixedRssLossModel
    // set it to zero; otherwise, gain will be added
    wifiPhy.Set("RxGain", DoubleValue(0));
    // ns-3 supports RadioTap and Prism tracing extensions for 802.11b
    wifiPhy.SetPcapDataLinkType(YansWifiPhyHelper::DLT_IEEE802_11_RADIO);
    
    YansWifiChannelHelper wifiChannel;
    wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    // The below FixedRssLossModel will cause the rss to be fixed regardless
    // of the distance between the two stations, and the transmit power
    wifiChannel.AddPropagationLoss("ns3::FixedRssLossModel", "Rss", DoubleValue(rss));
    wifiPhy.SetChannel(wifiChannel.Create());
    
    // Add a mac and disable rate control
    WifiMacHelper wifiMac;
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
            "DataMode", StringValue(phyMode),
            "ControlMode", StringValue(phyMode));
    // Set it to adhoc mode
    wifiMac.SetType("ns3::AdhocWifiMac");
    NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, wifiStaNodes);
    
    ObjectFactory pos;
    pos.SetTypeId("ns3::RandomDiscPositionAllocator");
    pos.Set("X", StringValue("25.0"));
    pos.Set("Y", StringValue("25.0"));
    pos.Set("Rho", StringValue("ns3::UniformRandomVariable[Min=0|Max=20]"));
    
    Ptr<PositionAllocator> taPositionAlloc = pos.Create ()->GetObject<PositionAllocator> ();
    
    // Mobility Cars
    
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::RandomDiscPositionAllocator",
            "X", StringValue("25.0"),
            "Y", StringValue("25.0"),
            "Rho", StringValue("ns3::UniformRandomVariable[Min=0|Max=20]"));
    
    mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
            "Speed", StringValue("ns3::UniformRandomVariable[Min=10|Max=20]"),
            "Pause", StringValue("ns3::ConstantRandomVariable[Constant=0]"),
            "PositionAllocator", PointerValue(taPositionAlloc));
    
    mobility.Install(wifiStaNodes);
    
    // Mobility Obstacles
    
    MobilityHelper mo;
    mo.SetPositionAllocator ("ns3::GridPositionAllocator",//vzdialenost
            "MinX", DoubleValue (0.0),
            "MinY", DoubleValue (0.0),
            "DeltaX", DoubleValue (10),
            "DeltaY", DoubleValue (10),
            "GridWidth", UintegerValue (3),
            "LayoutType", StringValue ("RowFirst"));
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (obstacleNodes);
    
    // Internet
    
    InternetStackHelper stack;
    stack.Install(allNodes);
    
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    staInterfaces = address.Assign(devices);
    
    tid = TypeId::LookupByName("ns3::UdpSocketFactory");
    Ptr<Socket> recvSink = Socket::CreateSocket(allNodes.Get(0), tid);
    InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), 80);
    recvSink->Bind(local);
    recvSink->SetRecvCallback(MakeCallback(&ReceivePacket));
    
    Ptr<Socket> source = Socket::CreateSocket(allNodes.Get(nWifi - 1), tid);
    InetSocketAddress remote = InetSocketAddress(staInterfaces.GetAddress(0, 0), 80);
    source->Connect(remote);
    
    // Tracing off
    //    wifiPhy.EnablePcap ("wifi-simple-adhoc", devices);
    
    double duration = 10.0;
    
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();
    Time interPacketInterval = Seconds (interval);
    
    Simulator::ScheduleWithContext (source->GetNode ()->GetId (),
            Seconds (0.0),
            &GenerateTraffic, 
            source,
            packetSize,
            numPackets, // number of packets
            interPacketInterval);
    
    Simulator::Stop(Seconds(duration));
    
    // NetAnim
    AnimationInterface anim("nascar-animation.xml"); // Mandatory
    uint32_t carImgId = anim.AddResource("/home/student/Documents/ns3/ns-3.26/car.png");
    for (uint32_t i = 0; i < wifiStaNodes.GetN(); ++i) {
        anim.UpdateNodeDescription(wifiStaNodes.Get(i), "CAR"); // Optional
        anim.UpdateNodeColor(wifiStaNodes.Get(i), 255, 0, 0); // Optional
        anim.UpdateNodeImage(i, carImgId);
        anim.UpdateNodeSize(i, 3, 0.5);
    }
    for (uint32_t i = 0; i < obstacleNodes.GetN(); ++i) {
        anim.UpdateNodeDescription(obstacleNodes.Get(i), "Obstacle"); // Optional
        anim.UpdateNodeColor(obstacleNodes.Get(i), 0, 255, 0); // Optional
    }
    
    //g: set special name for node 0
    anim.UpdateNodeDescription(wifiStaNodes.Get(0), "Master"); // Optional
    anim.UpdateNodeColor(wifiStaNodes.Get(0), 0, 123, 123); // Optional
    
    UdpEchoClientHelper echoClient(staInterfaces.GetAddress(1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(1));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    
    ApplicationContainer clientApps = echoClient.Install(allNodes.Get(0));
    clientApps.Start(Seconds(0.0));
    clientApps.Stop(Seconds(duration));    
    
    anim.EnablePacketMetadata(); // Optional
    //    anim.EnableIpv4RouteTracking("routingtable-wireless.xml", Seconds(0), Seconds(5), Seconds(0.25)); //Optional
    //    anim.EnableWifiMacCounters(Seconds(0), Seconds(duration)); //Optional
    //    anim.EnableWifiPhyCounters(Seconds(0), Seconds(duration)); //Optional
    
    Simulator::Run();
    Simulator::Destroy();
    
    return 0;
}