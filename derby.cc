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
#include "ns3/flow-monitor-module.h"
#include "ns3/gnuplot.h"

#include <iostream>
#include <fstream>
#include <vector>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WirelessAnimationExample");

void ReceivePacket(Ptr<Socket> socket) {
    while (socket->Recv()) {
        //NS_LOG_UNCOND("Received one packet!");
        //receivedPackets++;
        //std::cout << "Packet received from id" << socket->GetNode()->GetId()  << std::endl;
    }
}

static void GenerateTraffic(Ptr<Socket> socket, double pktSize,
        double pktCount, Time pktInterval) {

    
    if (pktCount > 0)
    {
        socket->Send (Create<Packet> (pktSize));
        //std::cout << "Packet sent from id" << socket->GetNode()->GetId() << std::endl;
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
    srand(time(0));
    
    int numOfTests = 10;
    double totalThroughputs[numOfTests], totalPacketsSents[numOfTests], totalPacketsReceiveds[numOfTests];
    for (int cycleNumber = 0; cycleNumber < numOfTests; cycleNumber++)
    {
        //double rss = -95.0; // -dBm   // -95 - vsetko prijme ... -96 - nic neprijme
        double packetSize = 1024; // bytes
        double numPackets = 1000;
        double interval = 0.1; // seconds
    
        int sentPackets = 0;
        int receivedPackets = 0;

        double duration = 120.0;

        int spawningMinDistance = 0;
        int spawningMaxDistance = 10;
        
        int minDistance = 0;  
        int maxDistance = 10;

        double nWifiActiveNodes = (cycleNumber+1)*5;
        double nObstacles = 8;
        NodeContainer allNodes;
        NodeContainer activeNodes;
        NodeContainer passiveNodes;
        TypeId tid;
        Ipv4InterfaceContainer staInterfaces;
    
    SeedManager::SetSeed (rand()); 
    // nastavit raz, neodporuca sa menit
    SeedManager::SetRun (1);   // pre zarucenie nezavislosti je lepsi
    
    
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
    
//    passiveNodes.Create(nObstacles);
    
    allNodes.Add(activeNodes);
//    allNodes.Add(passiveNodes);
    
    std::string phyMode ("DsssRate1Mbps");
    
    WifiHelper wifi;
    wifi.SetStandard (WIFI_PHY_STANDARD_80211b);
    
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    // This is one parameter that matters when using FixedRssLossModel
    // set it to zero; otherwise, gain will be added
    wifiPhy.Set("RxGain", DoubleValue (-10));
    // ns-3 supports RadioTap and Prism tracing extensions for 802.11b
    wifiPhy.SetPcapDataLinkType(YansWifiPhyHelper::DLT_IEEE802_11_RADIO);
    
    YansWifiChannelHelper wifiChannel;
    wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    wifiChannel.AddPropagationLoss ("ns3::FriisPropagationLossModel");
    //wifiChannel.AddPropagationLoss("ns3::FixedRssLossModel", "Rss", DoubleValue(rss));
  
    wifiPhy.SetChannel(wifiChannel.Create());
    
    // Add a mac and disable rate control
    //NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default ();
    WifiMacHelper wifiMac;
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
            "DataMode", StringValue(phyMode),
            "ControlMode", StringValue(phyMode));
    // Set it to adhoc mode
    wifiMac.SetType("ns3::AdhocWifiMac");
    NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, activeNodes);    
    
    // Mobility Cars
    
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::RandomDiscPositionAllocator",
            "X", StringValue(std::to_string(maxDistance)),
            "Y", StringValue(std::to_string(maxDistance)),
            "Rho", StringValue("ns3::UniformRandomVariable[Min="+ std::to_string(spawningMinDistance) +"|Max=" + std::to_string(spawningMaxDistance) + "]"));
    
    ObjectFactory pos;
    pos.SetTypeId("ns3::RandomDiscPositionAllocator");
    pos.Set("X", StringValue(std::to_string(maxDistance)));
    pos.Set("Y", StringValue(std::to_string(maxDistance)));
    pos.Set("Rho", StringValue("ns3::UniformRandomVariable[Min="+ std::to_string(minDistance) +"|Max=" + std::to_string(maxDistance) + "]"));
    
    Ptr<PositionAllocator> taPositionAlloc = pos.Create ()->GetObject<PositionAllocator> ();
    
    mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
            "Speed", StringValue("ns3::UniformRandomVariable[Min=50|Max=200]"),
            "Pause", StringValue("ns3::ConstantRandomVariable[Constant=0]"),
            "PositionAllocator", PointerValue(taPositionAlloc));
    
    mobility.Install(activeNodes);
    
    // Internet
    
    InternetStackHelper stack;
    stack.Install(allNodes);
    
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    staInterfaces = address.Assign(devices);    
    
    tid = TypeId::LookupByName("ns3::UdpSocketFactory");
    
    Ptr<Socket> recvSinks[activeNodes.GetN()];
    Ptr<Socket> sources[activeNodes.GetN()];
    for (int i = 0; i < activeNodes.GetN(); i++)
    {        
        recvSinks[i] = Socket::CreateSocket(activeNodes.Get(i), tid);
        InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), 80);
        recvSinks[i]->Bind(local);
        recvSinks[i]->SetRecvCallback(MakeCallback(&ReceivePacket));

        int r;
        while ((r = rand() % activeNodes.GetN()) == i);
        
        sources[i] = Socket::CreateSocket(activeNodes.Get(r), tid);
        InetSocketAddress remote = InetSocketAddress(staInterfaces.GetAddress(i, 0), 80);
        sources[i]->Connect(remote);
    } 
    
    // Tracing off?
    wifiPhy.EnablePcap ("wifi-simple-adhoc", devices);    
    
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();
    Time interPacketInterval = Seconds (interval);
    
    for (int i = 0; i < activeNodes.GetN(); i++)
    {            
        Simulator::ScheduleWithContext (sources[i]->GetNode ()->GetId (),
            Seconds (0.0),
            &GenerateTraffic, 
            sources[i],
            packetSize,
            numPackets/activeNodes.GetN(), // number of packets
            interPacketInterval);
    }
    
    // FlowMon
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll ();    

    Simulator::Stop(Seconds(duration));
    
    // NetAnim
    AnimationInterface anim("nascar-animation.xml"); // Mandatory
    
    uint32_t carImgId = anim.AddResource("/home/student/Documents/ns3/ns-3.26/car.png");
    for (uint32_t i = 0; i < nWifiActiveNodes; i++) {
        anim.UpdateNodeDescription(allNodes.Get(i), std::to_string(i)); // Optional
        anim.UpdateNodeColor(allNodes.Get(i), 255, 0, 0); // Optional
        anim.UpdateNodeImage(i, carImgId);
        anim.UpdateNodeSize(i, 3, 0.5);
        //anim.UpdateNodeColor(wifiStaNodes.Get(i), 255, 0, 0); // Optional
        anim.UpdateNodeSize(i, 6, 1); 
    }
    for (uint32_t i = nWifiActiveNodes; i < nWifiActiveNodes/* + nObstacles*/; i++) {
        //anim.UpdateNodeDescription(obstacleNodes.Get(i), "Obstacle"); // Optional
        anim.UpdateNodeColor(allNodes.Get(i), 0, 0, 0); // Optional black
        anim.UpdateNodeSize(i, 1, 1);
    }
    
    anim.EnablePacketMetadata(); // Optional
    anim.EnableIpv4RouteTracking("routingtable-wireless.xml", Seconds(0), Seconds(duration), Seconds(0.25)); //Optional
    anim.EnableWifiMacCounters(Seconds(0), Seconds(duration)); //Optional
    anim.EnableWifiPhyCounters(Seconds(0), Seconds(duration)); //Optional
    
    Simulator::Run();
    
    // FlowMonitor
    monitor->CheckForLostPackets ();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();
    double totalThroughput = 0;
    double totalPacketsSent = 0;
    double totalPacketsReceived = 0;
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
        double packetsSent = i->second.txPackets;
        double packetsReceived = i->second.rxPackets;
        double throughput = i->second.rxBytes * 8.0 / (duration * 1000000.0);
        
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
        std::cout << "Flow " << i->first - 2 << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Packets: " << packetsSent << "\n";
        std::cout << "  TxOffered:  " << i->second.txBytes * 8.0 / (duration * 1000000.0) << " Mbps\n";
        std::cout << "  Rx Packets: " << packetsReceived << "\n";
        std::cout << "  Throughput: " << throughput << " Mbps\n";
        
        totalThroughput += throughput;
        totalPacketsSent += packetsSent;
        totalPacketsReceived += packetsReceived;
    }   
    
    std::cout << "\n" << "Total throughput: " << totalThroughput << " Mbps\n";
    std::cout << "Total packets sent: " << totalPacketsSent << "\n";
    std::cout << "Total packets received: " << totalPacketsReceived << "\n";
    
    totalThroughputs[cycleNumber] = totalThroughput;
    totalPacketsSents[cycleNumber] = totalPacketsSent;
    totalPacketsReceiveds[cycleNumber] = totalPacketsReceived;
    
    Simulator::Destroy();
    }
    
    std::cout << "\n";
    for (int i = 0; i < numOfTests; i++)
    {
        std::cout << totalThroughputs[i] << "Mbps - " << totalPacketsReceiveds[i] << "/" << totalPacketsSents[i] << "\n";
    }
    
    return 0;
}
