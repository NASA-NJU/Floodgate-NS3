/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
* Copyright (c) 2006 Georgia Tech Research Corporation, INRIA
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
*/
#include <iostream>
#include <stdio.h>
#include "ns3/log.h"
#include "ns3/enum.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"
#include "ns3/simulator.h"
#include "ns3/drop-tail-queue.h"
#include "ns3/broadcom-egress-queue.h"


NS_LOG_COMPONENT_DEFINE("BEgressQueue");

namespace ns3 {

	NS_OBJECT_ENSURE_REGISTERED(BEgressQueue);

	uint32_t SwitchACKTag::switch_ack_payload;
	uint32_t SwitchACKTag::switch_ack_credit_bit;
	uint32_t SwitchACKTag::switch_ack_id_bit;
	uint32_t SwitchACKTag::max_switchack_size = 0;
	uint32_t SwitchSYNTag::switch_psn_bit = 8;
	uint32_t SwitchSYNTag::max_switchsyn_size = 0;

	TypeId BEgressQueue::GetTypeId(void)
	{
		static TypeId tid = TypeId("ns3::BEgressQueue")
			.SetParent<Queue>()
			.AddConstructor<BEgressQueue>()
			.AddAttribute("MaxBytes",
				"The maximum number of bytes accepted by this BEgressQueue.",
				DoubleValue(1000.0 * 1024 * 1024),
				MakeDoubleAccessor(&BEgressQueue::m_maxBytes),
				MakeDoubleChecker<double>())
			.AddTraceSource ("BeqEnqueue", "Enqueue a packet in the BEgressQueue. Multiple queue",
					MakeTraceSourceAccessor (&BEgressQueue::m_traceBeqEnqueue))
			.AddTraceSource ("BeqDequeue", "Dequeue a packet in the BEgressQueue. Multiple queue",
					MakeTraceSourceAccessor (&BEgressQueue::m_traceBeqDequeue))
			;

		return tid;
	}

	BEgressQueue::BEgressQueue() :
		Queue()
	{
		NS_LOG_FUNCTION_NOARGS();
		m_bytesInQueueTotal = 0;
		ResetStatistics();
		m_rrlast = 0;
		for (uint32_t i = 0; i < fCnt; i++)
		{
			m_bytesInQueue[i] = 0;
			m_queues.push_back(CreateObject<DropTailQueue>());
		}
	}

	BEgressQueue::~BEgressQueue()
	{
		NS_LOG_FUNCTION_NOARGS();
	}

	uint32_t BEgressQueue::GetTotalReceivedBytes () const{
		return m_nTotalReceivedBytes;
	}

	void
	BEgressQueue::ResetStatistics (void)
	{
	  NS_LOG_FUNCTION (this);
	  m_nTotalReceivedBytes = 0;
	  m_nTotalReceivedPackets = 0;
	  m_nTotalDroppedBytes = 0;
	  m_nTotalDroppedPackets = 0;
	}

	bool
		BEgressQueue::DoEnqueue(Ptr<Packet> p, uint32_t qIndex)
	{
		NS_LOG_FUNCTION(this << p);

		if (m_bytesInQueueTotal + p->GetSize() < m_maxBytes)  //infinite queue
		{
			m_queues[qIndex]->Enqueue(p);
			m_bytesInQueueTotal += p->GetSize();
			m_bytesInQueue[qIndex] += p->GetSize();
		}
		else
		{
			return false;
		}
		return true;
	}

	Ptr<Packet>
		BEgressQueue::DoDequeueRR(bool paused[], bool pfc_queue, std::map<uint32_t, uint32_t>& ip2id) //this is for switch only
	{
		NS_LOG_FUNCTION(this);

		if (m_bytesInQueueTotal == 0)
		{
			NS_LOG_LOGIC("Queue empty");
			return 0;
		}
		bool found = false;
		uint32_t qIndex;

		if (m_queues[0]->GetNPackets() > 0) //0 is the highest priority
		{
			found = true;
			qIndex = 0;
		}
		else
		{
			if (!found)
			{
				for (qIndex = 1; qIndex <= qCnt; qIndex++)
				{
					if (pfc_queue){	// pause queue
						if (!paused[(qIndex + m_rrlast) % qCnt] && m_queues[(qIndex + m_rrlast) % qCnt]->GetNPackets() > 0)  //round robin
						{
							found = true;
							break;
						}
					}else{	// pause dst
						if (m_queues[(qIndex + m_rrlast) % qCnt]->GetNPackets() > 0){
							Ptr<Packet> p = m_queues[(qIndex + m_rrlast) % qCnt]->Peek()->Copy();
							CustomHeader ch(CustomHeader::L2_Header | CustomHeader::L3_Header | CustomHeader::L4_Header);
							ch.getInt = 1; // parse INT header
							p->PeekHeader(ch);
							if (ch.l3Prot == 0x11){
								assert(ip2id.count(ch.dip) > 0);
								if (paused[ip2id[ch.dip]]) continue;
							}
							found = true;
							break;
						}
					}
				}
				qIndex = (qIndex + m_rrlast) % qCnt;
			}
		}
		if (found)
		{
			Ptr<Packet> p = m_queues[qIndex]->Dequeue();
			m_traceBeqDequeue(p, qIndex);
			m_bytesInQueueTotal -= p->GetSize();
			m_bytesInQueue[qIndex] -= p->GetSize();
			if (qIndex != 0)
			{
				m_rrlast = qIndex;
			}
			m_qlast = qIndex;
			NS_LOG_LOGIC("Popped " << p);
			NS_LOG_LOGIC("Number bytes " << m_bytesInQueueTotal);

			return p;
		}
		NS_LOG_LOGIC("Nothing can be sent");
		return 0;
	}

	bool
		BEgressQueue::Enqueue(Ptr<Packet> p, uint32_t qIndex)
	{
		NS_LOG_FUNCTION(this << p);
		//
		// If DoEnqueue fails, Queue::Drop is called by the subclass
		//
		bool retval = DoEnqueue(p, qIndex);
		if (retval)
		{
			NS_LOG_LOGIC("m_traceEnqueue (p)");
			m_traceEnqueue(p);
			m_traceBeqEnqueue(p, qIndex);

			uint32_t size = p->GetSize();
			m_nBytes += size;
			m_nTotalReceivedBytes += size;

			m_nPackets++;
			m_nTotalReceivedPackets++;

		}else{
			m_nTotalDroppedBytes += p->GetSize();
			m_nTotalDroppedPackets ++ ;
			std::cout << "BEgressQueue drop!!! qIndex:" << qIndex << std::endl;
		}
		return retval;
	}

	Ptr<Packet>
		BEgressQueue::DequeueRR(bool paused[], bool pfc_queue, std::map<uint32_t, uint32_t>& ip2id)
	{
		NS_LOG_FUNCTION(this);
		Ptr<Packet> packet = DoDequeueRR(paused, pfc_queue, ip2id);
		if (packet != 0)
		{
			NS_ASSERT(m_nBytes >= packet->GetSize());
			NS_ASSERT(m_nPackets > 0);
			m_nBytes -= packet->GetSize();
			m_nPackets--;
			NS_LOG_LOGIC("m_traceDequeue (packet)");
			m_traceDequeue(packet);
		}
		return packet;
	}

	bool
		BEgressQueue::DoEnqueue(Ptr<Packet> p)	//for compatiability
	{
		std::cout << "Warning: Call Broadcom queues without priority\n";
		uint32_t qIndex = 0;
		NS_LOG_FUNCTION(this << p);
		if (m_bytesInQueueTotal + p->GetSize() < m_maxBytes)
		{
			m_queues[qIndex]->Enqueue(p);
			m_bytesInQueueTotal += p->GetSize();
			m_bytesInQueue[qIndex] += p->GetSize();
		}
		else
		{
			return false;

		}
		return true;
	}


	Ptr<Packet>
		BEgressQueue::DoDequeue(void)
	{
		NS_ASSERT_MSG(false, "BEgressQueue::DoDequeue not implemented");
		return 0;
	}


	Ptr<const Packet>
		BEgressQueue::DoPeek(void) const	//DoPeek doesn't work for multiple queues!!
	{
		std::cout << "Warning: Call Broadcom queues without priority\n";
		NS_LOG_FUNCTION(this);
		if (m_bytesInQueueTotal == 0)
		{
			NS_LOG_LOGIC("Queue empty");
			return 0;
		}
		NS_LOG_LOGIC("Number bytes " << m_bytesInQueue);
		return m_queues[0]->Peek();
	}

	uint32_t
		BEgressQueue::GetNBytes(uint32_t qIndex) const
	{
		return m_bytesInQueue[qIndex];
	}


	uint32_t
		BEgressQueue::GetNBytesTotal() const
	{
		return m_bytesInQueueTotal;
	}

	uint32_t
		BEgressQueue::GetLastQueue()
	{
		return m_qlast;
	}

}
