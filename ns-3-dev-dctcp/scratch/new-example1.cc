    #include "ns3/core-module.h"
    #include "ns3/network-module.h"
    #include "ns3/internet-module.h"
    #include "ns3/point-to-point-module.h"
    #include "ns3/applications-module.h"
    #include "ns3/point-to-point-layout-module.h"
    #include "ns3/traffic-control-module.h"

    #include <iostream>
    #include <iomanip>
    #include <map>

    using namespace ns3;
    //rtt calculation
   
    int main (int argc, char *argv[])
    {
    
    bool        modeBytes  = false;
    uint32_t    queueDiscLimitPackets = 25; //1000
    uint32_t    pktSize = 1200;
    std::string appDataRate = "10Mbps";
    uint16_t port = 5001;
    std::string bottleNeckLinkBw = "10Mbps";
    std::string bottleNeckLinkDelay = "6us";

    LogComponentEnable ("PhantomQueueDisc", LOG_LEVEL_INFO);

    CommandLine cmd;
    
    
    cmd.AddValue ("queueDiscLimitPackets","Max Packets allowed in the queue disc", queueDiscLimitPackets);
    cmd.AddValue ("appPktSize", "Set OnOff App Packet Size", pktSize);
    cmd.AddValue ("appDataRate", "Set OnOff App DataRate", appDataRate);
    cmd.AddValue ("modeBytes", "Set Queue disc mode to Packets <false> or bytes <true>", modeBytes);
    

    cmd.Parse (argc,argv);
    Config::SetDefault ("ns3::OnOffApplication::PacketSize", UintegerValue (pktSize));
    Config::SetDefault ("ns3::OnOffApplication::DataRate", StringValue (appDataRate));

    
    if (modeBytes)
        {
        Config::SetDefault ("ns3::PhantomQueueDisc::MaxSize",
                            QueueSizeValue (QueueSize (QueueSizeUnit::PACKETS, queueDiscLimitPackets)));
        }
    else
        {
        Config::SetDefault ("ns3::PhantomQueueDisc::MaxSize",
                            QueueSizeValue (QueueSize (QueueSizeUnit::BYTES, queueDiscLimitPackets * pktSize)));
        }


    //Setting TCPDctcp
    TypeId tcpTid;
    NS_ABORT_MSG_UNLESS (TypeId::LookupByNameFailSafe ("ns3::TcpDctcp", &tcpTid), "TypeId " << "ns3::TcpDctcp" << " not found");
    Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue ("ns3::TcpDctcp"));
    //Setting TCPDctcp over  
    Config::SetDefault ("ns3::PhantomQueueDisc::LinkBandwidth", StringValue (bottleNeckLinkBw));
    Config::SetDefault ("ns3::PhantomQueueDisc::LinkDelay", StringValue (bottleNeckLinkDelay));
    Config::SetDefault ("ns3::PhantomQueueDisc::DrainRateFraction", DoubleValue(0.95));
    Config::SetDefault ("ns3::PhantomQueueDisc::MarkingthreShold", DoubleValue(6000));

    // Create the point-to-point link helpers
    PointToPointHelper bottleNeckLink;
    bottleNeckLink.SetDeviceAttribute  ("DataRate", StringValue (bottleNeckLinkBw));
    bottleNeckLink.SetChannelAttribute ("Delay", StringValue (bottleNeckLinkDelay));

    PointToPointHelper pointToPointLeaf;
    pointToPointLeaf.SetDeviceAttribute    ("DataRate", StringValue ("10Mbps"));
    pointToPointLeaf.SetChannelAttribute   ("Delay", StringValue ("6us"));

    NodeContainer sender1;
    NodeContainer sender2;
    NodeContainer receiver;
    NodeContainer bottleneckNodes;

    bottleneckNodes.Create(2);
    sender1.Add(bottleneckNodes.Get(0));
    sender2.Add(bottleneckNodes.Get(0));
    sender1.Create(1);
    sender2.Create(1);
    receiver.Add(bottleneckNodes.Get(1));
    receiver.Create(1);

    NetDeviceContainer s1Dev = pointToPointLeaf.Install(sender1);
    NetDeviceContainer s2Dev = pointToPointLeaf.Install(sender2);
    NetDeviceContainer recDev = pointToPointLeaf.Install(receiver);
    NetDeviceContainer bottleneckDev = bottleNeckLink.Install(bottleneckNodes);

    InternetStackHelper stack;
    stack.InstallAll();


    TrafficControlHelper tchBottleneck;
    QueueDiscContainer queueDiscs;
    tchBottleneck.SetRootQueueDisc ("ns3::PhantomQueueDisc");
    tchBottleneck.Install(bottleneckDev.Get(1));
    queueDiscs = tchBottleneck.Install (bottleneckDev.Get(0));

    Ipv4AddressHelper address;
    address.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer s1Interfaces;
    s1Interfaces = address.Assign (s1Dev);  

    address.SetBase ("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer s2Interfaces;
    s2Interfaces = address.Assign(s2Dev);    

    address.SetBase ("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer recInterfaces;
    recInterfaces = address.Assign(recDev);

    address.SetBase ("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer bottleneckInterfaces;
    bottleneckInterfaces = address.Assign(bottleneckDev);


    BulkSendHelper clientHelper ("ns3::TcpSocketFactory", Address ());
    clientHelper.SetAttribute ("MaxBytes", UintegerValue (0));

 
    Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), port));
    PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", sinkLocalAddress);
    ApplicationContainer sinkApps;
    sinkApps.Add (packetSinkHelper.Install (receiver.Get(1)));
    
    sinkApps.Start (Seconds (0.0));
    sinkApps.Stop (Seconds (12.0));


    ApplicationContainer clientApps;
    AddressValue remoteAddress (InetSocketAddress (recInterfaces.GetAddress(1), port));
    clientHelper.SetAttribute ("Remote", remoteAddress);
    clientApps.Add(clientHelper.Install(sender1.Get(1)));
    clientApps.Add(clientHelper.Install(sender2.Get(1)));

    clientApps.Start (Seconds (1.0)); // Start 1 second after sink
    clientApps.Stop (Seconds (11.0)); // Stop before the sink

    Ipv4GlobalRoutingHelper::PopulateRoutingTables ();


    //rtt 
    bool tracing = true;
    if (tracing)
        {
            AsciiTraceHelper ascii;
       bottleNeckLink.EnableAsciiAll (ascii.CreateFileStream ("./PhantomQ/bottleneckNodeTraces.tr"));
       bottleNeckLink.EnablePcapAll ("./PhantomQ/pcap/bottlenectNodeTr.pcap", false);
        }
    //rtt over


    std::cout << "Running the simulation" << std::endl;
    Simulator::Run ();

    QueueDisc::Stats st = queueDiscs.Get (0)->GetStats ();

    // if (st.GetNDroppedPackets (PhantomQueueDisc::UNFORCED_DROP) == 0)
    //   {
    //     std::cout << "There should be some unforced drops" << std::endl;
    //     exit (1);
    //   }

    if (st.GetNDroppedPackets (QueueDisc::INTERNAL_QUEUE_DROP) != 0)
        {
        std::cout << "There should be zero drops due to queue full" << std::endl;
        exit (1);
        }

    std::cout << "*** Stats from the bottleneck queue disc ***" << std::endl;
    std::cout << st << std::endl;
    std::cout << "bottleNeckLinkBw : "<<bottleNeckLinkBw<<std::endl;
    std::cout << "Average Throughput : "<<(st.nTotalSentBytes/(10.0*1024*128))<<" Mbps"<<std::endl;
    std::cout << "Destroying the simulation" << std::endl;

    Simulator::Destroy ();
    return 0;

}