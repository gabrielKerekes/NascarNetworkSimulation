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
#include "ns3/gnuplot.h"

#include <iostream>
#include <fstream>
#include <vector>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WirelessAnimationExample");

int sentPackets = 0;
int receivedPackets = 0;

uint32_t nWifiActiveNodes;
uint32_t nObstacles;
NodeContainer allNodes;
NodeContainer activeNodes;
NodeContainer passiveNodes;
TypeId tid;
Ipv4InterfaceContainer staInterfaces;

void ReceivePacket(Ptr<Socket> socket) {
    while (socket->Recv()) {
        NS_LOG_UNCOND("Received one packet!");
        receivedPackets++;
        std::cout << "Packet received from id" << socket->GetNode()->GetId()  << std::endl;
    }
}

static void GenerateTraffic(Ptr<Socket> socket, uint32_t pktSize,
        uint32_t pktCount, Time pktInterval) {

    
    if (pktCount > 0)
    {
        socket->Send (Create<Packet> (pktSize));
        sentPackets++;
        std::cout << "Packet sent from id" << socket->GetNode()->GetId() << std::endl;
        Simulator::Schedule (pktInterval, &GenerateTraffic, 
                socket, pktSize,pktCount-1, pktInterval);
    }
    else
    {
        socket->Close ();
    }
    
//    int r = rand() % nWifi;
//    int r2 = rand() % nWifi;
//    
//    Ptr<Socket> source = Socket::CreateSocket(allNodes.Get(r), tid);
//    InetSocketAddress remote = InetSocketAddress(staInterfaces.GetAddress(r2, 0), 80);
//    source->Connect(remote);
//    
//    if (pktCount > 0) {
//        source->Send(Create<Packet> (pktSize));
//        std::cout << "Packet sent from id" << source->GetNode()->GetId() << std::endl;
//        Simulator::Schedule(pktInterval, &GenerateTraffic,
//                socket, pktSize, pktCount - 1, pktInterval);
//    }
//    else {
//        source->Close();
//    }
    
}

int main(int argc, char *argv[]) 
{
    nWifiActiveNodes = 12;
    nObstacles = 8;
    double rss = -80; // -dBm
    uint32_t packetSize = 1000; // bytes
    uint32_t numPackets = 50;
    double interval = 1; // seconds
    
    //SeedManager::SetSeed (10); // nastavit raz, neodporuca sa menit
    SeedManager::SetRun (1);   // pre zarucenie nezavislosti je lepsi
    
    //srand(time(0));
    
//    Gnuplot graf("nascarThroughput.png");
//    graf.SetTerminal("png");
//    graf.SetTitle("Through");
//    graf.SetLegend("Pocet bodov [m]","Priepustnost[Mbit/s]");
//    graf.AppendExtra("set xrange[0:10]");
//    Gnuplot2dDataset data;
//    data.SetStyle (Gnuplot2dDataset::LINES_POINTS);
    
    CommandLine cmd;
    cmd.AddValue("nWifi", "Number of Cars", nWifiActiveNodes);
    cmd.AddValue("nObsacles", "Number of Obstacles", nObstacles);
    cmd.Parse(argc, argv);
    
    activeNodes.Create(nWifiActiveNodes);
    
    passiveNodes.Create(nObstacles);
    
    allNodes.Add(activeNodes);
    allNodes.Add(passiveNodes);
    
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
    //wifiChannel.AddPropagationLoss ("ns3::FriisPropagationLossModel");
    wifiChannel.AddPropagationLoss("ns3::FixedRssLossModel", "Rss", DoubleValue(rss));
    wifiPhy.SetChannel(wifiChannel.Create());
    
    // Add a mac and disable rate control
    NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default ();
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
            "DataMode", StringValue(phyMode),
            "ControlMode", StringValue(phyMode));
    // Set it to adhoc mode
    wifiMac.SetType("ns3::AdhocWifiMac");
    NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, activeNodes);
    
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
    
    mobility.Install(activeNodes);
    
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
    mobility.Install (passiveNodes);
    
    // Internet
    
    InternetStackHelper stack;
    stack.Install(allNodes);
    
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    staInterfaces = address.Assign(devices);
    
    
    tid = TypeId::LookupByName("ns3::UdpSocketFactory");
    

    Ptr<Socket> recvSink = Socket::CreateSocket(activeNodes.Get(0), tid);
    InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), 80);
    recvSink->Bind(local);
    recvSink->SetRecvCallback(MakeCallback(&ReceivePacket));
    
    Ptr<Socket> recvSink2 = Socket::CreateSocket(activeNodes.Get(1), tid);
    InetSocketAddress local2 = InetSocketAddress(Ipv4Address::GetAny(), 80);
    recvSink2->Bind(local2);
    recvSink2->SetRecvCallback(MakeCallback(&ReceivePacket));
   

    Ptr<Socket>  source = Socket::CreateSocket(activeNodes.Get(nWifiActiveNodes - 1), tid);
    InetSocketAddress remote = InetSocketAddress(staInterfaces.GetAddress(0, 0), 80);
    source->Connect(remote);
    
    Ptr<Socket>  source2 = Socket::CreateSocket(activeNodes.Get(nWifiActiveNodes - 2), tid);
    InetSocketAddress remote2 = InetSocketAddress(staInterfaces.GetAddress(1, 0), 80); // important to set different remote address
    source2->Connect(remote2);
  
    
    // Tracing off?
    wifiPhy.EnablePcap ("wifi-simple-adhoc", devices);
    
    double duration = 60.0;
    
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();
    Time interPacketInterval = Seconds (interval);
    
    Simulator::ScheduleWithContext (source->GetNode ()->GetId (),
        Seconds (0.0),
        &GenerateTraffic, 
        source,
        packetSize,
        numPackets, // number of packets
        interPacketInterval);
    
    Simulator::ScheduleWithContext (source2->GetNode ()->GetId (),
        Seconds (0.0),
        &GenerateTraffic, 
        source2,
        packetSize,
        numPackets, // number of packets
        interPacketInterval);
    

    Simulator::Stop(Seconds(duration));
    
    // NetAnim
    AnimationInterface anim("nascar.xml"); // Mandatory
    
    for (uint32_t i = 0; i < nWifiActiveNodes; i++) {
        //anim.UpdateNodeDescription(wifiStaNodes.Get(i), "CAR"); // Optional
        if (i % 4 == 0) {
            uint32_t carImgIdGreen = anim.AddResource("/home/student/Documents/ns3/ns-3.24.1/greencar.png");
            anim.UpdateNodeImage(i, carImgIdGreen);
        } else if (i % 4 == 1) {
            uint32_t carImgIdPurple = anim.AddResource("/home/student/Documents/ns3/ns-3.24.1/purplecar.png");
            anim.UpdateNodeImage(i, carImgIdPurple);
        } else if (i % 4 == 2) {
            uint32_t carImgIdBlue = anim.AddResource("/home/student/Documents/ns3/ns-3.24.1/bluecar.png");
            anim.UpdateNodeImage(i, carImgIdBlue);
        } else {
            uint32_t carImgIdOrange = anim.AddResource("/home/student/Documents/ns3/ns-3.24.1/orangecar.png");
            anim.UpdateNodeImage(i, carImgIdOrange);
        }
        //anim.UpdateNodeColor(wifiStaNodes.Get(i), 255, 0, 0); // Optional
        anim.UpdateNodeSize(i, 6, 1); 
    }
    for (uint32_t i = nWifiActiveNodes; i < nWifiActiveNodes + nObstacles; i++) {
        //anim.UpdateNodeDescription(obstacleNodes.Get(i), "Obstacle"); // Optional
        anim.UpdateNodeColor(allNodes.Get(i), 0, 0, 0); // Optional black
        anim.UpdateNodeSize(i, 1, 1);
    }
    
    //g: set special name for node 0
    //anim.UpdateNodeDescription(activeNodes.Get(0), "Master"); // Optional
    //anim.UpdateNodeColor(wifiStaNodes.Get(0), 0, 123, 123); // Optional
    
    UdpServerHelper udpServer (9);
    
    ApplicationContainer clientContainer = udpServer.Install(activeNodes.Get(0));
    clientContainer.Start(Seconds(0.0));
    clientContainer.Stop(Seconds(duration));
    
    UdpEchoClientHelper echoClient(staInterfaces.GetAddress(0), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(1));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    
    UdpEchoClientHelper echoClient2(staInterfaces.GetAddress(1), 9);
    echoClient2.SetAttribute("MaxPackets", UintegerValue(1));
    echoClient2.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient2.SetAttribute("PacketSize", UintegerValue(1024));
    
    ApplicationContainer clientApps = echoClient.Install(activeNodes.Get(0));
    clientApps.Start(Seconds(0.0));
    clientApps.Stop(Seconds(duration));    
    
    // Not sure if needed 2nd times, because it works without it as well
    ApplicationContainer clientApps2 = echoClient2.Install(activeNodes.Get(1));
    clientApps2.Start(Seconds(0.0));
    clientApps2.Stop(Seconds(duration));  
   
    
    anim.EnablePacketMetadata(); // Optional
    anim.EnableIpv4RouteTracking("routingtable-wireless.xml", Seconds(0), Seconds(5), Seconds(0.25)); //Optional
    anim.EnableWifiMacCounters(Seconds(0), Seconds(duration)); //Optional
    anim.EnableWifiPhyCounters(Seconds(0), Seconds(duration)); //Optional
    
    Simulator::Run();
    Simulator::Destroy();
    
    double throughput = 0;
    //UDP
    uint32_t totalPacketsThrough = DynamicCast<UdpServer> (clientContainer.Get (0))->GetReceived ();
    throughput = totalPacketsThrough * packetSize * 8 / (duration * 1000000.0); //Mbit/s
    //data.Add(distance,throughput);
          
    
    std::cout << "SendPackets -> " << sentPackets << std::endl;
    std::cout << "ReceivedPackets -> " << receivedPackets << std::endl;
    
    std::cout << throughput << " Mbit/s" << std::endl;
    
    return 0;
}