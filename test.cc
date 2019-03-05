#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/config.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/on-off-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/yans-wifi-channel.h"



NS_LOG_COMPONENT_DEFINE("wifi_test");

using namespace ns3;

Ptr<PacketSink> sink;                         /* Pointer to the packet sink application */
Ptr<FlowMonitor> monitor;

uint64_t lastTotalRx = 0;                     /* The value of the last total received bytes */

void CalculateThroughput ()
{
  Time now = Simulator::Now ();                                         /* Return the simulator's virtual time. */
  double cur = (sink->GetTotalRx () - lastTotalRx) * (double) 8 / 1e5;     /* Convert Application RX Packets to MBits. */
  std::cout << now.GetSeconds () << "s: \t" << cur << " Mbit/s" << std::endl;
  lastTotalRx = sink->GetTotalRx ();
  Simulator::Schedule (MilliSeconds (100), &CalculateThroughput);
}

int main(int argc, char *argv[])
{
    //---initilization-------//
    uint32_t payloadSize = 1448;    //udp -> 1472 && tcp -> 1448
    std::string dataRate = "200Mbps";
    std::string tcpVariant = "TcpNewReno";
    std::string phyRate = "VhtMcs9";
//    double snr = 25;
    double rss = -54;
    double simulationTime = 10;
    double activationTime = 1.0;
    bool pcapTracing = true;
    int no_of_stations = 1;
    int i;
    bool channelBonding = true;
    bool shortGuardInterval = true;


    //std::cout<<"Enter the no of stations:\n";
    //std::cin>>no_of_stations;

    //-----No fragmentation & no rts/cts-----//
    Config::SetDefault("ns3::WifiRemoteStationManager::FragmentationThreshold", StringValue("999999"));
    Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue("999999"));

    //-----Set tcpVariant---//
    Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpWestwood::GetTypeId ()));
    // the default protocol type in ns3::TcpWestwood is WESTWOOD
    Config::SetDefault ("ns3::TcpWestwood::ProtocolType", EnumValue (TcpWestwood::WESTWOODPLUS));


    WifiMacHelper wifiMac;
    WifiHelper wifiHelper;
    wifiHelper.SetStandard(WIFI_PHY_STANDARD_80211ac);

    //----Set up Legacy Channel----//
    YansWifiChannelHelper wifiChannel;
    wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
    wifiChannel.AddPropagationLoss ("ns3::FixedRssLossModel", "Rss", DoubleValue (rss));

    /* Setup Physical Layer */
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
    wifiPhy.SetChannel (wifiChannel.Create ());
    wifiPhy.Set ("TxPowerStart", DoubleValue (10.0));
    wifiPhy.Set ("TxPowerEnd", DoubleValue (10.0));
    wifiPhy.Set ("TxPowerLevels", UintegerValue (1));
    wifiPhy.Set ("TxGain", DoubleValue (0));
    wifiPhy.Set ("RxGain", DoubleValue (0));
    wifiPhy.Set ("RxNoiseFigure", DoubleValue (10));
    wifiPhy.Set ("CcaMode1Threshold", DoubleValue (-79));
    wifiPhy.Set ("EnergyDetectionThreshold", DoubleValue (-79 + 3));
    wifiPhy.SetErrorRateModel ("ns3::YansErrorRateModel");

    std::ostringstream oss;
    oss << "VhtMcs" << 9;
    wifiHelper.SetRemoteStationManager ("ns3::ConstantRateWifiManager","DataMode", StringValue (oss.str()),"ControlMode", StringValue (oss.str()));

    //-------MIMO setup--------------//
    //uint8_t nStreams = 1 + (32 / 8); //number of MIMO streams(no of mimo setups 0-31 -> 32)

    wifiPhy.Set("Antennas", UintegerValue (4));
    wifiPhy.Set("MaxSupportedTxSpatialStreams", UintegerValue (1));
    wifiPhy.Set("MaxSupportedRxSpatialStreams", UintegerValue (1));
    wifiPhy.Set("ShortGuardEnabled", BooleanValue(true));
//    wifiPhy.Set("TxAntennas", UintegerValue(4));
//    wifiPhy.Set("RxAntennas", UintegerValue(4));

    //----NODE creation--------//
    NodeContainer networkNodes;
    networkNodes.Create (no_of_stations + 1);
    Ptr<Node> apWifiNode = networkNodes.Get (no_of_stations);
    Ptr<Node> staWifiNode[no_of_stations];
    i = 0;
    while(i < no_of_stations)
    {
      staWifiNode[i] = networkNodes.Get (i);
      i++;
    }

    //-------------Configure AP---------------//
    Ssid ssid = Ssid ("network");
    wifiMac.SetType ("ns3::ApWifiMac", "Ssid", SsidValue (ssid));
    NetDeviceContainer apDevice;
    apDevice = wifiHelper.Install (wifiPhy, wifiMac, apWifiNode);

    //------Configure STA--------//
    wifiMac.SetType ("ns3::StaWifiMac","Ssid", SsidValue (ssid), "ActiveProbing", BooleanValue(true));
    NetDeviceContainer staDevices[no_of_stations];
    i = 0;
    while(i < no_of_stations)
    {
      staDevices[i] = wifiHelper.Install (wifiPhy, wifiMac, staWifiNode[i]);
      i++;
    }

    //------channel boanding setup---------//
    if (channelBonding)
      {
        Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ChannelWidth", UintegerValue (40));
      }

    //------------short guard interval setup-----------//
    Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/HtConfiguration/ShortGuardIntervalSupported", BooleanValue (shortGuardInterval));


    //------------Mobility model-------//
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
    positionAlloc->Add (Vector (0.0, 0.0, 0.0));
    positionAlloc->Add (Vector (0.0, 0.0, 0.0));
    mobility.SetPositionAllocator (positionAlloc);
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (apWifiNode);
    i = 0;
    while(i < no_of_stations)
    {
      mobility.Install (staWifiNode[i]);
      i++;
    }

    /* Internet stack */
    InternetStackHelper stack;
    stack.Install (networkNodes);

    Ipv4AddressHelper address;
    address.SetBase ("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface;
    apInterface = address.Assign (apDevice);
    Ipv4InterfaceContainer staInterface[no_of_stations];
    i = 0;
    while(i < no_of_stations)
    {
      staInterface[i] = address.Assign (staDevices[i]);
      i++;
    }

    /* Populate routing table */
    Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

    /* Install TCP Receiver on the access point */
    PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), 9));
    ApplicationContainer sinkApp = sinkHelper.Install (apWifiNode);
    sink = StaticCast<PacketSink> (sinkApp.Get (0));

    /* Install TCP Transmitter on the station */
    OnOffHelper server ("ns3::TcpSocketFactory", (InetSocketAddress (apInterface.GetAddress (0), 9)));
    server.SetAttribute ("PacketSize", UintegerValue (payloadSize));
    server.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
    server.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
    server.SetAttribute ("DataRate", DataRateValue (DataRate (dataRate)));

    ApplicationContainer serverApp[no_of_stations];
    i = 0;
    while(i < no_of_stations)
    {
      serverApp[i] = server.Install (staWifiNode[i]);
      i++;
    }
    /* Start Applications */
    sinkApp.Start (Seconds (0.0));

    i = 0;
    while(i < no_of_stations)
    {
      serverApp[i].Start (Seconds (activationTime));
      std::cout<<Seconds(activationTime)<<"Device:\t"<<i + 1<<"\n";
//      activationTime++;
      i++;
    }
    Simulator::Schedule (Seconds (1.0), &CalculateThroughput);

    //PCAP Tracing activation
    if (pcapTracing)
        {
          for(i = 0; i<no_of_stations; i++)
          {
            wifiPhy.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO);
            wifiPhy.EnablePcap ("AccessPoint", apDevice);
            wifiPhy.EnablePcap ("Station", staDevices[i]);
          }
        }

    // Flow monitor initilization
    FlowMonitorHelper flowHelper;
    monitor = flowHelper.InstallAll();

    double totaltxpacket = 0, totalrxpacket = 0, lostpackets;
    double totalthr = 0.0, delay_total=0.0, plr = 0.0, totaldelay = 0.0, pl = 0.0;

    /* Start Simulation */
    Simulator::Stop (Seconds (simulationTime));
    Simulator::Run ();
    Simulator::Destroy ();

    double averageThroughput = ((sink->GetTotalRx () * 8) / (1e6  * simulationTime));
    std::cout << "\nAverage throughput: " << averageThroughput << " Mbit/s" << std::endl;

    monitor->SerializeToXmlFile("NameOfFile.xml", true, true);
    monitor->CheckForLostPackets();

    Ptr<Ipv4FlowClassifier>classifier = DynamicCast<Ipv4FlowClassifier>(flowHelper.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats>stats = monitor->GetFlowStats();
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
      Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
      if(i->first)
      {
              totaldelay += i->second.delaySum.GetSeconds();
              totaltxpacket = totaltxpacket + i ->second.txPackets;
              totalrxpacket = totalrxpacket + i ->second.rxPackets;
              totalthr += i ->second.rxPackets * payloadSize * 8.0 / (simulationTime * 1e6);

        std::cout << "\nFlow"<<i->first - 2<<" ("<<t.sourceAddress<<"->"<<t.destinationAddress<<")\n";
        std::cout<<"Tx Bytes: " <<i->second.txBytes <<"\n";
        std::cout<<"Rx Bytes: " <<i->second.rxBytes <<"\n";
        std::cout<<"Throughput: " <<i->second.rxBytes * 8.0 /( simulationTime *1e6) <<"MBit/s \n";
      }
    }
    if(totalrxpacket!=0)
    {
      delay_total = ((double) totaldelay / (double) totalrxpacket) * 1000;
    }

    std::cout<<"Delay: "<<delay_total<<std::endl;

    lostpackets = totaltxpacket - totalrxpacket;

    std::cout<<"Lost packets = "<<lostpackets<<" "<<"Total TX packets = "<<totaltxpacket<<" Total RX packets = "<<totalrxpacket<<std::endl;

    if (totaltxpacket != 0)
    {

    pl = (double) (lostpackets / totaltxpacket);
    plr = pl * 100.0;

    }
    std::cout<<"PLR: "<<plr<<std::endl;
    //std::cout<<sink->GetTotalRx();

    return 0;

}
