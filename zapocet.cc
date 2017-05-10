/*
 * Vytvorte simulaciu pre prostredie Netanim
 * Vytvorte graf zavislosti Priepustnosti od vzdialenosti
 */


#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/olsr-helper.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/gnuplot.h"

#include <iostream>
#include <fstream>
#include <vector>
#include <string>

using namespace ns3;

void ReceivePacket (Ptr<Socket> socket)
{
    while (socket->Recv ())
    { NS_LOG_UNCOND ("Received one packet!");  }
}

static void GenerateTraffic (Ptr<Socket> socket, uint32_t pktSize, uint32_t pktCount, Time pktInterval ){
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


int main (int argc, char *argv[])
{
    uint32_t sinkNode = 0;
    uint32_t sourceNode = 8;
    
    // disable fragmentation for frames below 2200 bytes
    Config::SetDefault ("ns3::WifiRemoteStationManager::FragmentationThreshold", StringValue ("2200"));
    // turn off RTS/CTS for frames below 2200 bytes
    Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue ("2200"));
    // Fix non-unicast data rate to be the same as that of unicast
    Config::SetDefault ("ns3::WifiRemoteStationManager::NonUnicastMode", 
            StringValue ("DsssRate1Mbps"));
    
    Gnuplot graf("graf.svg");
    graf.SetTerminal("svg");
    graf.SetTitle("Graf");
    graf.SetLegend("Vzdialenost [m]","Priepustnost[Mbit/s]");
    Gnuplot2dDataset data;
    data.SetTitle ("strata udajov");
    data.SetStyle (Gnuplot2dDataset::LINES_POINTS);
    
    std::cout << "Distance" << "\t\t" << "Throughput" << '\n';
    for (uint32_t d = 0; d<=800;d=d+50) {
        
        NodeContainer c;
        c.Create (9);
        
        // The below set of helpers will help us to put together the wifi NICs we want
        WifiHelper wifi;
        
        YansWifiPhyHelper wifiPhy =  YansWifiPhyHelper::Default ();
        // set it to zero; otherwise, gain will be added
        wifiPhy.Set ("RxGain", DoubleValue (-10) ); 
        // ns-3 supports RadioTap and Prism tracing extensions for 802.11b
        wifiPhy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO); 
        
        YansWifiChannelHelper wifiChannel;
        wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
        wifiChannel.AddPropagationLoss ("ns3::FriisPropagationLossModel");
        wifiPhy.SetChannel (wifiChannel.Create ());
        
        // Add a non-QoS upper mac, and disable rate control
        NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default ();
        wifi.SetStandard (WIFI_PHY_STANDARD_80211b);
        wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                "DataMode",StringValue ("DsssRate1Mbps"),
                "ControlMode",StringValue ("DsssRate1Mbps"));
        // Set it to adhoc mode
        wifiMac.SetType ("ns3::AdhocWifiMac");
        NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, c);
        
        MobilityHelper mobility;
        mobility.SetPositionAllocator ("ns3::GridPositionAllocator",//vzdialenost
                "MinX", DoubleValue (0.0),
                "MinY", DoubleValue (0.0),
                "DeltaX", DoubleValue (d),
                "DeltaY", DoubleValue (d),
                "GridWidth", UintegerValue (3),
                "LayoutType", StringValue ("RowFirst"));
        mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
        mobility.Install (c);
        
        // Enable OLSR
        OlsrHelper olsr;
        Ipv4StaticRoutingHelper staticRouting;
        
        Ipv4ListRoutingHelper list;
        list.Add (staticRouting, 0);
        list.Add (olsr, 10);
        
        InternetStackHelper internet;
        internet.SetRoutingHelper (list); // has effect on the next Install ()
        internet.Install (c);
        
        Ipv4AddressHelper ipv4;
//        NS_LOG_INFO ("Assign IP Addresses.");
        ipv4.SetBase ("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer i = ipv4.Assign (devices);
        
        TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
        Ptr<Socket> recvSink = Socket::CreateSocket (c.Get (sinkNode), tid);
        InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), 80);
        recvSink->Bind (local);
        recvSink->SetRecvCallback (MakeCallback (&ReceivePacket));
        
        Ptr<Socket> source = Socket::CreateSocket (c.Get (sourceNode), tid);
        InetSocketAddress remote = InetSocketAddress (i.GetAddress (sinkNode, 0), 80);
        source->Connect (remote);
        
        
        // Give OLSR time to converge-- 30 seconds perhaps
        Simulator::Schedule (Seconds (30.0),
                &GenerateTraffic,
                source,
                1000,//Velkost balika
                500,// Pocet balikov
                Seconds (0.005));//interval medzi balikmi);
        
        // Output what we are doing
        
        FlowMonitorHelper flowmon;
        Ptr<FlowMonitor> monitor = flowmon.InstallAll ();
        
        Simulator::Stop (Seconds (40.0));
        
        // NetAnim
        //    AnimationInterface anim ("animation.xml");
        //      // pre vsetky uzly nastavte
        //      for (uint32_t i = 0; i < c.GetN (); ++i){
        //          anim.UpdateNodeDescription (c.Get (i), "STA"); 
        //          anim.UpdateNodeColor (c.Get (i), 255, 0, 0);}
        //      anim.EnablePacketMetadata ();
        
        
        Simulator::Run ();
        
        // FlowMonitor
        monitor->CheckForLostPackets ();
        Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
        FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats ();
        double throughput = 0;
        for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
        {
            // first 2 FlowIds are for ECHO apps, we don't want to display them
            //
            // Duration for throughput measurement is 9.0 seconds, since 
            //   StartTime of the OnOffApplication is at about "second 1"
            // and 
            //   Simulator::Stops at "second 10".
            //if (i->first > 2)
            //  {
            Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
            std::cout << "Flow " << i->first - 2 << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
            std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
            std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
            std::cout << "  TxOffered:  " << i->second.txBytes * 8.0 / 10 / 1000 / 1000  << " Mbps\n";
            std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
            std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
            std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / 10 / 1000 / 1000  << " Mbps\n";
            throughput = i->second.rxBytes * 8.0 / 10 / 1000 / 1000;
            //  }
        }
        data.Add(d,throughput);
        
        Simulator::Destroy ();
        std::cout << d << "\t\t\t" << throughput << " Mbit/s" << std::endl;
        
    }
    
    graf.AddDataset (data);
    std::ofstream plotFile ("graf.plt");
    graf.GenerateOutput (plotFile);
    plotFile.close ();
    
    return 0;
}


