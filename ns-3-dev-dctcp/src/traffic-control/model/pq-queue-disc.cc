// /* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright © 2011 Marcos Talau
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
 * Author: Marcos Talau (talau@users.sourceforge.net)
 *
 * Thanks to: Duy Nguyen<duy@soe.ucsc.edu> by RED efforts in NS3
 *
 *
 * This file incorporates work covered by the following copyright and  
 * permission notice:  
 *
 * Copyright (c) 1990-1997 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor of the Laboratory may be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * PORT NOTE: This code was ported from ns-2 (queue/red.cc).  Almost all 
 * comments have also been ported from NS-2
 */

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
    .AddAttribute ("MeanPktSize",
                   "Average of packet size",
                   UintegerValue (500),
                   MakeUintegerAccessor (&PhantomQueueDisc::m_meanPktSize),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("IdlePktSize",
                   "Average packet size used during idle times. Used when m_cautions = 3",
                   UintegerValue (0),
                   MakeUintegerAccessor (&PhantomQueueDisc::m_idlePktSize),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("Wait",
                   "True for waiting between dropped packets",
                   BooleanValue (true),
                   MakeBooleanAccessor (&PhantomQueueDisc::m_isWait),
                   MakeBooleanChecker ())
    .AddAttribute ("Gentle",
                   "True to increases dropping probability slowly when average queue exceeds maxthresh",
                   BooleanValue (true),
                   MakeBooleanAccessor (&PhantomQueueDisc::m_isGentle),
                   MakeBooleanChecker ())
    .AddAttribute ("ARED",
                   "True to enable ARED",
                   BooleanValue (false),
                   MakeBooleanAccessor (&PhantomQueueDisc::m_isARED),
                   MakeBooleanChecker ())
    .AddAttribute ("AdaptMaxP",
                   "True to adapt m_curMaxP",
                   BooleanValue (false),
                   MakeBooleanAccessor (&PhantomQueueDisc::m_isAdaptMaxP),
                   MakeBooleanChecker ())
    .AddAttribute ("FengAdaptive",
                   "True to enable Feng's Adaptive RED",
                   BooleanValue (false),
                   MakeBooleanAccessor (&PhantomQueueDisc::m_isFengAdaptive),
                   MakeBooleanChecker ())
    .AddAttribute ("NLRED",
                   "True to enable Nonlinear RED",
                   BooleanValue (false),
                   MakeBooleanAccessor (&PhantomQueueDisc::m_isNonlinear),
                   MakeBooleanChecker ())
    .AddAttribute ("MinTh",
                   "Minimum average length threshold in packets/bytes",
                   DoubleValue (5),
                   MakeDoubleAccessor (&PhantomQueueDisc::m_minTh),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("MaxTh",
                   "Maximum average length threshold in packets/bytes",
                   DoubleValue (15),
                   MakeDoubleAccessor (&PhantomQueueDisc::m_maxTh),
                   MakeDoubleChecker<double> ())
    .AddAttribute ("MaxSize",
                   "The maximum number of packets accepted by this queue disc",
                   QueueSizeValue (QueueSize ("25p")),
                   MakeQueueSizeAccessor (&QueueDisc::SetMaxSize,
                                          &QueueDisc::GetMaxSize),
                   MakeQueueSizeChecker ())
    .AddAttribute ("QW",
                   "Queue weight related to the exponential weighted moving average (EWMA)",
                   DoubleValue (0.002),
                   MakeDoubleAccessor (&PhantomQueueDisc::m_qW),
                   MakeDoubleChecker <double> ())
    .AddAttribute ("LInterm",
                   "The maximum probability of dropping a packet",
                   DoubleValue (50),
                   MakeDoubleAccessor (&PhantomQueueDisc::m_lInterm),
                   MakeDoubleChecker <double> ())
    .AddAttribute ("TargetDelay",
                   "Target average queuing delay in ARED",
                   TimeValue (Seconds (0.005)),
                   MakeTimeAccessor (&PhantomQueueDisc::m_targetDelay),
                   MakeTimeChecker ())
    .AddAttribute ("Interval",
                   "Time interval to update m_curMaxP",
                   TimeValue (Seconds (0.5)),
                   MakeTimeAccessor (&PhantomQueueDisc::m_interval),
                   MakeTimeChecker ())
    .AddAttribute ("Top",
                   "Upper bound for m_curMaxP in ARED",
                   DoubleValue (0.5),
                   MakeDoubleAccessor (&PhantomQueueDisc::m_top),
                   MakeDoubleChecker <double> (0, 1))
    .AddAttribute ("Bottom",
                   "Lower bound for m_curMaxP in ARED",
                   DoubleValue (0.0),
                   MakeDoubleAccessor (&PhantomQueueDisc::m_bottom),
                   MakeDoubleChecker <double> (0, 1))
    .AddAttribute ("LastSet",
                   "Store the last time m_curMaxP was updated",
                   TimeValue (Seconds (0.0)),
                   MakeTimeAccessor (&PhantomQueueDisc::m_lastSet),
                   MakeTimeChecker ())
    .AddAttribute ("Rtt",
                   "Round Trip Time to be considered while automatically setting m_bottom",
                   TimeValue (Seconds (0.1)),
                   MakeTimeAccessor (&PhantomQueueDisc::m_rtt),
                   MakeTimeChecker ())
    .AddAttribute ("Ns1Compat",
                   "NS-1 compatibility",
                   BooleanValue (false),
                   MakeBooleanAccessor (&PhantomQueueDisc::m_isNs1Compat),
                   MakeBooleanChecker ())
    .AddAttribute ("LinkBandwidth", 
                   "The RED link bandwidth",
                   DataRateValue (DataRate ("1.5Mbps")),
                   MakeDataRateAccessor (&PhantomQueueDisc::m_linkBandwidth),
                   MakeDataRateChecker ())
    .AddAttribute ("LinkDelay", 
                   "The RED link delay",
                   TimeValue (MilliSeconds (20)),
                   MakeTimeAccessor (&PhantomQueueDisc::m_linkDelay),
                   MakeTimeChecker ())
    .AddAttribute ("UseEcn",
                   "True to use ECN (packets are marked instead of being dropped)",
                   BooleanValue (false),
                   MakeBooleanAccessor (&PhantomQueueDisc::m_useEcn),
                   MakeBooleanChecker ())
    .AddAttribute ("UseHardDrop",
                   "True to always drop packets above max threshold",
                   BooleanValue (true),
                   MakeBooleanAccessor (&PhantomQueueDisc::m_useHardDrop),
                   MakeBooleanChecker ())
  ;

  return tid;
}

PhantomQueueDisc::PhantomQueueDisc () :
  QueueDisc (QueueDiscSizePolicy::SINGLE_INTERNAL_QUEUE)
{
  NS_LOG_FUNCTION (this);
  m_uv = CreateObject<UniformRandomVariable> ();
}

PhantomQueueDisc::~PhantomQueueDisc ()
{
  NS_LOG_FUNCTION (this);
}

void
PhantomQueueDisc::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
  m_uv = 0;
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
  m_vq=m_vq-(now-m_lastSet).GetSeconds()*m_drain_rate.GetBitRate()/8;
  if(m_vq<0)
    m_vq=0;
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

/*
 * Note: if the link bandwidth changes in the course of the
 * simulation, the bandwidth-dependent RED parameters do not change.
 * This should be fixed, but it would require some extra parameters,
 * and didn't seem worth the trouble...
 */
void
PhantomQueueDisc::InitializeParams (void)
{
  NS_LOG_FUNCTION (this);
  NS_LOG_INFO ("Initializing RED params.");

  m_cautious = 0;
  m_ptc = m_linkBandwidth.GetBitRate () / (8.0 * m_meanPktSize);

  //   DataRate m_drain_rate;
  // double m_marking_threshold;
  // double m_vq;

  // Time m_lastSet;  

  m_drain_rate=m_linkBandwidth.GetBitRate()*(0.95);
  m_marking_threshold=6000;
  m_vq=0;
  m_lastSet=Simulator::Now ();

  if (m_isARED)
    {
      // Set m_minTh, m_maxTh and m_qW to zero for automatic setting
      m_minTh = 0;
      m_maxTh = 0;
      m_qW = 0;

      // Turn on m_isAdaptMaxP to adapt m_curMaxP
      m_isAdaptMaxP = true;
    }



  if (m_minTh == 0 && m_maxTh == 0)
    {
      m_minTh = 5.0;

      // set m_minTh to max(m_minTh, targetqueue/2.0) [Ref: http://www.icir.org/floyd/papers/adaptiveRed.pdf]
      double targetqueue = m_targetDelay.GetSeconds() * m_ptc;

      if (m_minTh < targetqueue / 2.0 )
        {
          m_minTh = targetqueue / 2.0;
        }
      if (GetMaxSize ().GetUnit () == QueueSizeUnit::BYTES)
        {
          m_minTh = m_minTh * m_meanPktSize;
        }

      // set m_maxTh to three times m_minTh [Ref: http://www.icir.org/floyd/papers/adaptiveRed.pdf]
      m_maxTh = 3 * m_minTh;
    }

  NS_ASSERT (m_minTh <= m_maxTh);

  m_qAvg = 0.0;
  m_count = 0;
  m_countBytes = 0;
  m_old = 0;
  m_idle = 1;

  double th_diff = (m_maxTh - m_minTh);
  if (th_diff == 0)
    {
      th_diff = 1.0; 
    }
  m_vA = 1.0 / th_diff;
  m_curMaxP = 1.0 / m_lInterm;
  m_vB = -m_minTh / th_diff;

  if (m_isGentle)
    {
      m_vC = (1.0 - m_curMaxP) / m_maxTh;
      m_vD = 2.0 * m_curMaxP - 1.0;
    }
  m_idleTime = NanoSeconds (0);


  if (m_qW == 0.0)
    {
      m_qW = 1.0 - std::exp (-1.0 / m_ptc);
    }
  else if (m_qW == -1.0)
    {
      double rtt = 3.0 * (m_linkDelay.GetSeconds () + 1.0 / m_ptc);

      if (rtt < 0.1)
        {
          rtt = 0.1;
        }
      m_qW = 1.0 - std::exp (-1.0 / (10 * rtt * m_ptc));
    }
  else if (m_qW == -2.0)
    {
      m_qW = 1.0 - std::exp (-10.0 / m_ptc);
    }

  if (m_bottom == 0)
    {
      m_bottom = 0.01;
      // Set bottom to at most 1/W, where W is the delay-bandwidth
      // product in packets for a connection.
      // So W = m_linkBandwidth.GetBitRate () / (8.0 * m_meanPktSize * m_rtt.GetSeconds())
      double bottom1 = (8.0 * m_meanPktSize * m_rtt.GetSeconds()) / m_linkBandwidth.GetBitRate();
      if (bottom1 < m_bottom)
        {
          m_bottom = bottom1;
        }
    }

  NS_LOG_DEBUG ("\tm_delay " << m_linkDelay.GetSeconds () << "; m_isWait " 
                             << m_isWait << "; m_qW " << m_qW << "; m_ptc " << m_ptc
                             << "; m_minTh " << m_minTh << "; m_maxTh " << m_maxTh
                             << "; m_isGentle " << m_isGentle << "; th_diff " << th_diff
                             << "; lInterm " << m_lInterm << "; va " << m_vA <<  "; cur_max_p "
                             << m_curMaxP << "; v_b " << m_vB <<  "; m_vC "
                             << m_vC << "; m_vD " <<  m_vD);
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

  if ((m_isARED || m_isAdaptMaxP) && m_isFengAdaptive)
    {
      NS_LOG_ERROR ("m_isAdaptMaxP and m_isFengAdaptive cannot be simultaneously true");
    }

  return true;
}

} // namespace ns3
