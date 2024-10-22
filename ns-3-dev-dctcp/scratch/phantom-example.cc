/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2016 NITK Surathkal
 *
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
 * Author: Sourabh Jain <sourabhjain560@outlook.com>
 *         Mohit P. Tahiliani <tahiliani@nitk.edu.in>
 */

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

// static void
// CwndChange (uint32_t oldCwnd, uint32_t newCwnd)
// {
//   NS_LOG_UNCOND (Simulator::Now ().GetSeconds () << "\t" << newCwnd);
//   std::cout<<Simulator::Now ().GetSeconds () << "\t" << newCwnd<<std::endl;
// }

int main (int argc, char *argv[])
{
  uint32_t    nLeaf = 6;
  uint32_t    maxPackets = 15;
  bool        modeBytes  = false;
  uint32_t    queueDiscLimitPackets = 25; //1000
  // double      minTh = 5;
  // double      maxTh = 15;
  uint32_t    pktSize = 1200;
  std::string appDataRate = "10Mbps";
  std::string queueDiscType = "RED";
  uint16_t port = 5001;
  std::string bottleNeckLinkBw = "10Mbps";
  std::string bottleNeckLinkDelay = "6us";

  LogComponentEnable ("PhantomQueueDisc", LOG_LEVEL_INFO);

  CommandLine cmd;
  cmd.AddValue ("nLeaf", "Number of left and right side leaf nodes", nLeaf);
  cmd.AddValue ("maxPackets","Max Packets allowed in the device queue", maxPackets);
  cmd.AddValue ("queueDiscLimitPackets","Max Packets allowed in the queue disc", queueDiscLimitPackets);
  cmd.AddValue ("queueDiscType", "Set Queue disc type to RED or FengAdaptive", queueDiscType);
  cmd.AddValue ("appPktSize", "Set OnOff App Packet Size", pktSize);
  cmd.AddValue ("appDataRate", "Set OnOff App DataRate", appDataRate);
  cmd.AddValue ("modeBytes", "Set Queue disc mode to Packets <false> or bytes <true>", modeBytes);
  
  // cmd.AddValue ("redMinTh", "RED queue minimum threshold", minTh);
  // cmd.AddValue ("redMaxTh", "RED queue maximum threshold", maxTh);
  cmd.Parse (argc,argv);

  // if ((queueDiscType != "RED") && (queueDiscType != "FengAdaptive"))
  //   {
  //     std::cout << "Invalid queue disc type: Use --queueDiscType=RED or --queueDiscType=FengAdaptive" << std::endl;
  //     exit (1);
  //   }

  Config::SetDefault ("ns3::OnOffApplication::PacketSize", UintegerValue (pktSize));
  Config::SetDefault ("ns3::OnOffApplication::DataRate", StringValue (appDataRate));

  Config::SetDefault ("ns3::QueueBase::MaxSize",
                      QueueSizeValue (QueueSize (QueueSizeUnit::PACKETS, maxPackets)));

  if (modeBytes)
    {
      Config::SetDefault ("ns3::PhantomQueueDisc::MaxSize",
                          QueueSizeValue (QueueSize (QueueSizeUnit::PACKETS, queueDiscLimitPackets)));
    }
  else
    {
      Config::SetDefault ("ns3::PhantomQueueDisc::MaxSize",
                          QueueSizeValue (QueueSize (QueueSizeUnit::BYTES, queueDiscLimitPackets * pktSize)));
      // minTh *= pktSize;
      // maxTh *= pktSize;
    }

//Setting TCPDctcp
  TypeId tcpTid;
  NS_ABORT_MSG_UNLESS (TypeId::LookupByNameFailSafe ("ns3::TcpDctcp", &tcpTid), "TypeId " << "ns3::TcpVegas" << " not found");
  Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue ("ns3::TcpVegas"));
//Setting TCPDctcp over  
  Config::SetDefault ("ns3::PhantomQueueDisc::LinkBandwidth", StringValue (bottleNeckLinkBw));
  Config::SetDefault ("ns3::PhantomQueueDisc::LinkDelay", StringValue (bottleNeckLinkDelay));
  Config::SetDefault ("ns3::PhantomQueueDisc::DrainRateFraction", DoubleValue(0.90));
  Config::SetDefault ("ns3::PhantomQueueDisc::MarkingthreShold", DoubleValue(6000));



  // Create the point-to-point link helpers
  PointToPointHelper bottleNeckLink;
  bottleNeckLink.SetDeviceAttribute  ("DataRate", StringValue (bottleNeckLinkBw));
  bottleNeckLink.SetChannelAttribute ("Delay", StringValue (bottleNeckLinkDelay));

  PointToPointHelper pointToPointLeaf;
  pointToPointLeaf.SetDeviceAttribute    ("DataRate", StringValue ("10Mbps"));
  pointToPointLeaf.SetChannelAttribute   ("Delay", StringValue ("6us"));

  PointToPointDumbbellHelper d (nLeaf, pointToPointLeaf,
                                nLeaf, pointToPointLeaf,
                                bottleNeckLink);

  // Install Stack
  InternetStackHelper stack;
  for (uint32_t i = 0; i < d.LeftCount (); ++i)
    {
      stack.Install (d.GetLeft (i));
    }
  for (uint32_t i = 0; i < d.RightCount (); ++i)
    {
      stack.Install (d.GetRight (i));
    }

  stack.Install (d.GetLeft ());
  stack.Install (d.GetRight ());
  TrafficControlHelper tchBottleneck;
  QueueDiscContainer queueDiscs;
  tchBottleneck.SetRootQueueDisc ("ns3::PhantomQueueDisc");
  //tchBottleneck.Install (d.GetLeft ()->GetDevice (0));
  queueDiscs = tchBottleneck.Install (d.GetRight ()->GetDevice (0));  

  // Assign IP Addresses
  d.AssignIpv4Addresses (Ipv4AddressHelper ("10.1.1.0", "255.255.255.0"),
                         Ipv4AddressHelper ("10.2.1.0", "255.255.255.0"),
                         Ipv4AddressHelper ("10.3.1.0", "255.255.255.0"));

  // Install on/off app on all right side nodes
  BulkSendHelper clientHelper ("ns3::TcpSocketFactory", Address ());
    clientHelper.SetAttribute ("MaxBytes", UintegerValue (0));

  //clientHelper.SetAttribute ("OnTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max=1.]"));
  //clientHelper.SetAttribute ("OffTime", StringValue ("ns3::UniformRandomVariable[Min=0.|Max=1.]"));
  Address sinkLocalAddress (InetSocketAddress (Ipv4Address::GetAny (), port));
  PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", sinkLocalAddress);
  ApplicationContainer sinkApps;
  for (uint32_t i = 0; i < d.LeftCount (); ++i)
    {
      sinkApps.Add (packetSinkHelper.Install (d.GetLeft (i)));
    }
  sinkApps.Start (Seconds (0.0));
  sinkApps.Stop (Seconds (12.0));



  ApplicationContainer clientApps;
  for (uint32_t i = 0; i < d.RightCount (); ++i)
    {
      // Create an on/off app sending packets to the left side
      AddressValue remoteAddress (InetSocketAddress (d.GetLeftIpv4Address (i), port));
      clientHelper.SetAttribute ("Remote", remoteAddress);
      clientApps.Add (clientHelper.Install (d.GetRight (i)));
    }
    //>>>>>>>>>>>>>>>>>>>>>
  // Ptr<Socket> ns3TcpSocket = Socket::CreateSocket (d.GetRight (1), TcpSocketFactory::GetTypeId ());
  // ns3TcpSocket->TraceConnectWithoutContext ("CongestionWindow", MakeCallback (&CwndChange));

  // Ptr<BulkSendApplication> app = CreateObject<BulkSendApplication> ();
  // app->Setup (ns3TcpSocket, InetSocketAddress (d.GetLeftIpv4Address (1), port), 1040, 1000, DataRate ("10Mbps"));
  
  // d.GetRight (1)->AddApplication (app);
  // app->SetStartTime (Seconds (1.));
  // app->SetStopTime (Seconds (11.));
  //>>>>>>>>>>>>>>>>>>>>>>>>
  clientApps.Start (Seconds (1.0)); // Start 1 second after sink
  clientApps.Stop (Seconds (11.0)); // Stop before the sink

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();


  // AsciiTraceHelper ascii;
  //     pointToPointLeaf.EnableAsciiAll (ascii.CreateFileStream ("./PhantomQ/leafTraces_1_node/leafNodeTraces.tr"));
  //     pointToPointLeaf.EnablePcapAll ("PhantomQ/leafTraces_1_node/leafNodeTr.tr", false);

  //     bottleNeckLink.EnableAsciiAll (ascii.CreateFileStream ("./PhantomQ/bottleneckTraces_1_node/bottleneckNodeTraces.tr"));
  //     bottleNeckLink.EnablePcapAll ("PhantomQ/bottleneckTraces_1_node/bottlenectNodeTr.tr", false);


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
