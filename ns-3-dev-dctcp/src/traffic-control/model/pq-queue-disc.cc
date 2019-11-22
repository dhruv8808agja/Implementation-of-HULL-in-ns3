#include "ns3/log.h"
#include "ns3/enum.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"
#include "ns3/simulator.h"
#include "ns3/abort.h"
#include "pq-queue-disc.h"
#include "ns3/drop-tail-queue.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("PhantomQueueDisc");

NS_OBJECT_ENSURE_REGISTERED (PhantomQueueDisc);

TypeId PhantomQueueDisc::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::PhantomQueueDisc")
    .SetParent<QueueDisc> ()
    .SetGroupName("TrafficControl")
    .AddConstructor<PhantomQueueDisc> ()
    .AddAttribute ("MaxSize",
                   "The maximum number of packets accepted by this queue disc",
                   QueueSizeValue (QueueSize ("25p")),
                   MakeQueueSizeAccessor (&QueueDisc::SetMaxSize,
                                          &QueueDisc::GetMaxSize),
                   MakeQueueSizeChecker ())
    .AddAttribute ("LinkBandwidth", 
                   "The PQ link bandwidth",
                   DataRateValue (DataRate ("1.5Mbps")),
                   MakeDataRateAccessor (&PhantomQueueDisc::m_linkBandwidth),
                   MakeDataRateChecker ())
    .AddAttribute ("LinkDelay", 
                   "The PQ link delay",
                   TimeValue (MilliSeconds (20)),
                   MakeTimeAccessor (&PhantomQueueDisc::m_linkDelay),
                   MakeTimeChecker ())
    .AddAttribute ("DrainRateFraction",
                   "fraction of total bandwidth set as drain rate",
                   DoubleValue (0.95),
                   MakeDoubleAccessor (&PhantomQueueDisc::m_drain_rate_fraction),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("MarkingthreShold",
                   "Marking threshold of phantom Queue",
                   DoubleValue (6000),
                   MakeDoubleAccessor (&PhantomQueueDisc::m_marking_threshold),
                   MakeDoubleChecker<double> ())
      ;
  return tid;
}

PhantomQueueDisc::PhantomQueueDisc () :
  QueueDisc (QueueDiscSizePolicy::SINGLE_INTERNAL_QUEUE)
{
  NS_LOG_FUNCTION (this);
}

PhantomQueueDisc::~PhantomQueueDisc ()
{
  NS_LOG_FUNCTION (this);
}

void
PhantomQueueDisc::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
  QueueDisc::DoDispose ();
}



bool
PhantomQueueDisc::DoEnqueue (Ptr<QueueDiscItem> item)
{
  NS_LOG_FUNCTION (this << item);
  uint32_t nQueued = GetInternalQueue (0)->GetCurrentSize ().GetValue ();

  if(item->GetSize()+nQueued>GetInternalQueue (0)->GetMaxSize().GetValue())
  {
    DropBeforeEnqueue (item, FORCED_DROP);
    return false;
  }

  Time now=Simulator::Now();

  m_vq=m_vq-((now-m_lastSet).GetSeconds()*m_drain_rate.GetBitRate()/8);

  if(m_vq<0)
  {
    m_vq=0;
  }

  m_vq+=item->GetSize();

  if(m_vq>m_marking_threshold)
  {
    Mark (item, FORCED_MARK);
  }

  m_lastSet=now;

  bool retval = GetInternalQueue (0)->Enqueue (item);
  NS_LOG_LOGIC ("Number packets " << GetInternalQueue (0)->GetNPackets ());
  NS_LOG_LOGIC ("Number bytes " << GetInternalQueue (0)->GetNBytes ());

  return retval;
}


void
PhantomQueueDisc::InitializeParams (void)
{
  NS_LOG_FUNCTION (this);
  NS_LOG_INFO ("Initializing PQ params.");
  m_drain_rate=m_linkBandwidth.GetBitRate()*(m_drain_rate_fraction);
  //m_marking_threshold=6000;
  m_vq=0;
  m_lastSet=Simulator::Now ();
  m_idle = 1;
  m_idleTime = NanoSeconds (0);
  NS_LOG_DEBUG ("\tm_delay " << m_linkDelay.GetSeconds () 
                             << "; vq " << m_vq
                             <<"; marking_thresh "<<m_marking_threshold
                             <<"; drain_rate"<< m_drain_rate<<std::endl);
}


Ptr<QueueDiscItem>
PhantomQueueDisc::DoDequeue (void)
{
  NS_LOG_FUNCTION (this);

  if (GetInternalQueue (0)->IsEmpty ())
    {
      NS_LOG_LOGIC ("Queue empty");
      m_idle = 1;
      m_idleTime = Simulator::Now ();
      return 0;
    }
  else
    {
      m_idle = 0;
      Ptr<QueueDiscItem> item = GetInternalQueue (0)->Dequeue ();
      NS_LOG_LOGIC ("Popped " << item);
      NS_LOG_LOGIC ("Number packets " << GetInternalQueue (0)->GetNPackets ());
      NS_LOG_LOGIC ("Number bytes " << GetInternalQueue (0)->GetNBytes ());
      return item;
    }
}

Ptr<const QueueDiscItem>
PhantomQueueDisc::DoPeek (void)
{
  NS_LOG_FUNCTION (this);

  if (GetInternalQueue (0)->IsEmpty ())
    {
      NS_LOG_LOGIC ("Queue empty");
      return 0;
    }

  Ptr<const QueueDiscItem> item = GetInternalQueue (0)->Peek ();
  NS_LOG_LOGIC ("Number packets " << GetInternalQueue (0)->GetNPackets ());
  NS_LOG_LOGIC ("Number bytes " << GetInternalQueue (0)->GetNBytes ());
  return item;
}

bool
PhantomQueueDisc::CheckConfig (void)
{
  NS_LOG_FUNCTION (this);
  if (GetNQueueDiscClasses () > 0)
    {
      NS_LOG_ERROR ("PhantomQueueDisc cannot have classes");
      return false;
    }

  if (GetNPacketFilters () > 0)
    {
      NS_LOG_ERROR ("PhantomQueueDisc cannot have packet filters");
      return false;
    }

  if (GetNInternalQueues () == 0)
    {
      // add a DropTail queue
      AddInternalQueue (CreateObjectWithAttributes<DropTailQueue<QueueDiscItem> >
                          ("MaxSize", QueueSizeValue (GetMaxSize ())));
    }

  if (GetNInternalQueues () != 1)
    {
      NS_LOG_ERROR ("PhantomQueueDisc needs 1 internal queue");
      return false;
    }

  return true;
}

} // namespace ns3
