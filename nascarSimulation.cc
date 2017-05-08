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
NodeContainer allNodes;
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
    int r = rand() % nWifi;
    int r2 = rand() % nWifi;
    
    Ptr<Socket> source = Socket::CreateSocket(allNodes.Get(r), tid);
    InetSocketAddress remote = InetSocketAddress(staInterfaces.GetAddress(r2, 0), 80);
    source->Connect(remote);
    
    if (pktCount > 0) {
        source->Send(Create<Packet> (pktSize));
        Simulator::Schedule(pktInterval, &GenerateTraffic,
                socket, pktSize, pktCount - 1, pktInterval);
    }
    
    source->Close();
}

int main(int argc, char *argv[]) 
{
    nWifi = 2;
    double rss = -80; // -dBm
    uint32_t packetSize = 1000; // bytes
    uint32_t numPackets = 1;
    double interval = 1.0; // seconds
    
    SeedManager::SetSeed (10); // nastavit raz, neodporuca sa menit
    SeedManager::SetRun (1);   // pre zarucenie nezavislosti je lepsi

    CommandLine cmd;
    cmd.AddValue("nWifi", "Number of wifi STA devices", nWifi);
    cmd.Parse(argc, argv);

    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(nWifi);
    allNodes.Add(wifiStaNodes);
    
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
    pos.Set("X", StringValue("50.0"));
    pos.Set("Y", StringValue("50.0"));
    pos.Set("Rho", StringValue("ns3::UniformRandomVariable[Min=0|Max=50]"));

    Ptr<PositionAllocator> taPositionAlloc = pos.Create ()->GetObject<PositionAllocator> ();
    
    // Mobility

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::RandomDiscPositionAllocator",
            "X", StringValue("50.0"),
            "Y", StringValue("50.0"),
            "Rho", StringValue("ns3::UniformRandomVariable[Min=0|Max=50]"));
    
    mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
            "Speed", StringValue("ns3::UniformRandomVariable[Min=10|Max=20]"),
            "Pause", StringValue("ns3::ConstantRandomVariable[Constant=0]"),
            "PositionAllocator", PointerValue(taPositionAlloc));

//    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
//            "Bounds", RectangleValue(Rectangle(30, 70, 30, 70)),
//            "Distance", DoubleValue(30),
//            "Speed", StringValue("ns3::UniformRandomVariable[Min=5|Max=20]"));
    
    mobility.Install(wifiStaNodes);

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
    // Tracing
    wifiPhy.EnablePcap ("wifi-simple-adhoc", devices);

    double duration = 120.0;
    
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();
    Simulator::Stop(Seconds(duration));

    AnimationInterface anim("nascar-animation.xml"); // Mandatory
    for (uint32_t i = 0; i < wifiStaNodes.GetN(); ++i) {
        anim.UpdateNodeDescription(wifiStaNodes.Get(i), std::to_string(i)); // Optional
        anim.UpdateNodeColor(wifiStaNodes.Get(i), 255, 0, 0); // Optional
    }

    //g: set special name for node 0
//    anim.UpdateNodeDescription(wifiStaNodes.Get(0), "Boss"); // Optional
//    anim.UpdateNodeColor(wifiStaNodes.Get(0), 0, 123, 123); // Optional

    UdpEchoClientHelper echoClient(staInterfaces.GetAddress(1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(1));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    
    ApplicationContainer clientApps = echoClient.Install(allNodes.Get(0));
    clientApps.Start(Seconds(0.0));
    clientApps.Stop(Seconds(duration));    

    anim.EnablePacketMetadata(); // Optional
    anim.EnableIpv4RouteTracking("routingtable-wireless.xml", Seconds(0), Seconds(5), Seconds(0.25)); //Optional
    anim.EnableWifiMacCounters(Seconds(0), Seconds(duration)); //Optional
    anim.EnableWifiPhyCounters(Seconds(0), Seconds(duration)); //Optional
    
    Time interPacketInterval = Seconds (interval);
    
    Simulator::ScheduleWithContext (source->GetNode ()->GetId (),
                                  Seconds (1.0), &GenerateTraffic, 
                                  source, packetSize, 69871, interPacketInterval);
  
    Simulator::Run();
    Simulator::Destroy();
    
    return 0;
}