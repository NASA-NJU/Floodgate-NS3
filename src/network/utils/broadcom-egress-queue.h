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

#ifndef BROADCOM_EGRESS_H
#define BROADCOM_EGRESS_H

#include <queue>
#include "ns3/packet.h"
#include "ns3/queue.h"
#include "ns3/drop-tail-queue.h"
#include "ns3/point-to-point-net-device.h"
#include "ns3/event-id.h"
#include "ns3/simulator.h"
#include "ns3/settings.h"
#include <assert.h>

#define MAX_HOP 5

namespace ns3 {

	class TraceContainer;

	class BEgressQueue : public Queue {
	friend class SwitchNode;
	public:
		static TypeId GetTypeId(void);
		static const unsigned fCnt = 128; //max number of queues, 128 for NICs
		static const unsigned qCnt = 8; //max number of queues, 8 for switches
		uint32_t m_queueId;
		uint32_t m_switchId;
		BEgressQueue();
		virtual ~BEgressQueue();
		bool Enqueue(Ptr<Packet> p, uint32_t qIndex);
		Ptr<Packet> DequeueRR(bool paused[], bool pfc_queue, std::map<uint32_t, uint32_t>& ip2id);
		uint32_t GetNBytes(uint32_t qIndex) const;
		uint32_t GetNBytesTotal() const;
		uint32_t GetLastQueue();
		inline void SetSwitchId(uint32_t switchId) {m_switchId = switchId;}
		inline void SetQueueId(uint32_t queueId) {m_queueId = queueId;}

		uint32_t GetTotalReceivedBytes () const;
		void ResetStatistics();

		TracedCallback<Ptr<const Packet>, uint32_t> m_traceBeqEnqueue;
		TracedCallback<Ptr<const Packet>, uint32_t> m_traceBeqDequeue;

	private:
		bool DoEnqueue(Ptr<Packet> p, uint32_t qIndex);
		Ptr<Packet> DoDequeueRR(bool paused[], bool pfc_queue, std::map<uint32_t, uint32_t>& ip2id);
		//for compatibility
		virtual bool DoEnqueue(Ptr<Packet> p);
		virtual Ptr<Packet> DoDequeue(void);
		virtual Ptr<const Packet> DoPeek(void) const;
		double m_maxBytes; //total bytes limit
		uint32_t m_bytesInQueue[fCnt];
		uint32_t m_bytesInQueueTotal;
		uint32_t m_rrlast;
		uint32_t m_qlast;
		std::vector<Ptr<Queue> > m_queues; // uc queues
	};


	// add timestamp to analysis the queueing time
	class QueueingTag : public Tag
	{
	public:
	  QueueingTag(){ lastEnqueue = 0; cnt = 0; }
	  static TypeId GetTypeId (void){
		  static TypeId tid = TypeId ("ns3::QueueingTag") .SetParent<Tag> () .AddConstructor<QueueingTag> ();
		  return tid;
	  }
	  virtual TypeId GetInstanceTypeId (void) const{ return GetTypeId(); }
	  virtual uint32_t GetSerializedSize (void) const{ return sizeof(uint64_t)*10; }
	  virtual void Serialize (TagBuffer i) const{ i.WriteU64(cnt); i.WriteU64(lastEnqueue); for(int k=0;k<Settings::MAXHOP;++k) i.WriteU64(history[k]); }
	  virtual void Deserialize (TagBuffer i){ cnt = i.ReadU64(); lastEnqueue = i.ReadU64(); for(int k=0;k<Settings::MAXHOP;++k) history[k] = i.ReadU64(); }
	  double GetQueueingTimeUs() const{
		  uint64_t total = 0;
		  for(uint64_t k=0;k<cnt;++k) {
			  total += history[k];
		  }
		  return total/1000.0;
	  }
	  uint64_t GetActiveHop() {return cnt;}
	  uint64_t GetHopQueuingTime(uint64_t i) {
		  assert(i < cnt);
		  return history[i];
	  }
	  virtual void Print (std::ostream &os) const{ }
	  inline void Enqueue(){ lastEnqueue = Simulator::Now().GetNanoSeconds(); }
	  inline void Dequeue(){ history[cnt++] = Simulator::Now().GetNanoSeconds() - lastEnqueue; }
	private:
	  uint64_t cnt;
	  uint64_t lastEnqueue;
	  uint64_t history[Settings::MAXHOP]; //8 is the maximum hop
	};

	class QPTag : public Tag
	{
	public:
		QPTag(){ qpid = 0; msgSeq = 0;}
	  static TypeId GetTypeId (void){
		  static TypeId tid = TypeId ("ns3::QPTag") .SetParent<Tag> () .AddConstructor<QPTag> ();
		  return tid;
	  }
	  virtual TypeId GetInstanceTypeId (void) const{ return GetTypeId(); }
	  virtual uint32_t GetSerializedSize (void) const{ return sizeof(uint32_t)*2; }
	  virtual void Serialize (TagBuffer i) const{
		  i.WriteU32(qpid);
		  i.WriteU32(msgSeq);
	  }
	  virtual void Deserialize (TagBuffer i){
		  qpid = i.ReadU32();
		  msgSeq = i.ReadU32();
	  }
	  virtual void Print (std::ostream &os) const{ }
	  void SetQPID(uint32_t _qpid){qpid = _qpid;}
	  uint32_t GetQPID(){return qpid;}
	  void SetMsgSeq(uint32_t _msgSeq){msgSeq = _msgSeq;}
	  uint32_t GetMsgSeq(){return msgSeq;}
	private:
	  uint32_t qpid;
	  uint32_t msgSeq;
	};

	class SwitchIngressPSNTag : public Tag
	{
	public:
		SwitchIngressPSNTag(){ psn = 0; round = 0;}
	  static TypeId GetTypeId (void){
		  static TypeId tid = TypeId ("ns3::SwitchIngressPSNTag") .SetParent<Tag> () .AddConstructor<SwitchIngressPSNTag> ();
		  return tid;
	  }
	  virtual TypeId GetInstanceTypeId (void) const{ return GetTypeId(); }
	  virtual uint32_t GetSerializedSize (void) const{ return sizeof(uint32_t)+sizeof(uint64_t); }
	  virtual void Serialize (TagBuffer i) const{
		  i.WriteU64(psn);
		  i.WriteU32(round);
	  }
	  virtual void Deserialize (TagBuffer i){
		  psn = i.ReadU64();
		  round = i.ReadU32();
	  }
	  virtual void Print (std::ostream &os) const{ }
	  void SetPSN(uint64_t _psn){psn = _psn;}
	  uint64_t GetPSN(){return psn;}
	  void SetRound(uint32_t _round){round = _round;}
	  uint32_t GetRound(){return round;}
	private:
	  uint64_t psn;
	  uint32_t round;
	};

	class SwitchPSNTag : public Tag
	{
	public:
		SwitchPSNTag(){ psn = 0; round = 0;}
	  static TypeId GetTypeId (void){
		  static TypeId tid = TypeId ("ns3::SwitchPSNTag") .SetParent<Tag> () .AddConstructor<SwitchPSNTag> ();
		  return tid;
	  }
	  virtual TypeId GetInstanceTypeId (void) const{ return GetTypeId(); }
	  virtual uint32_t GetSerializedSize (void) const{ return sizeof(uint32_t)+sizeof(uint64_t); }
	  virtual void Serialize (TagBuffer i) const{
		  i.WriteU64(psn);
		  i.WriteU32(round);
	  }
	  virtual void Deserialize (TagBuffer i){
		  psn = i.ReadU64();
		  round = i.ReadU32();
	  }
	  virtual void Print (std::ostream &os) const{ }
	  void SetPSN(uint64_t _psn){psn = _psn;}
	  uint64_t GetPSN(){return psn;}
	  void SetRound(uint32_t _round){round = _round;}
	  uint32_t GetRound(){return round;}
	private:
	  uint64_t psn;
	  uint32_t round;
	};

	/*
	 * In ns3, packet only carry header and tag, there are no real payload,
	 * therefore, under switch-accumulate-credit mode, use tag to carry switch-credits' payload.
	 * Note: when use host/switch per-packet-ack mode, only a ackedSize will be carried in header.
	 */
	class SwitchACKTag: public Tag{
	public:
		static uint32_t switch_ack_payload; // related to byte_counter, credit_interval and topology scale;
		static uint32_t switch_ack_credit_bit;
		static uint32_t switch_ack_id_bit;
		static uint32_t max_switchack_size;
		SwitchACKTag(){
			for(uint32_t i = 0; i < Settings::host_num; ++i){
				acked_size[i] = 0;
			}
		}

		static TypeId GetTypeId (void){
		  static TypeId tid = TypeId ("ns3::SwitchACKTag") .SetParent<Tag> () .AddConstructor<SwitchACKTag> ();
		  return tid;
		}

		virtual TypeId GetInstanceTypeId (void) const{ return GetTypeId(); }
		virtual uint32_t GetSerializedSize (void) const{
			if (Settings::switch_ack_mode == Settings::SWITCH_INGRESS_CREDIT)
				return Settings::host_num*sizeof(uint64_t);
			else
				return sizeof(uint64_t);
		}

		uint32_t GetPacketSize(){
			uint32_t res = 0;
			if (Settings::switch_ack_mode == Settings::SWITCH_INGRESS_CREDIT){
				for (uint32_t i = 0; i < Settings::host_num;++i){
					if (acked_size[i] > 0) res += (switch_ack_credit_bit + switch_ack_id_bit);
				}
				max_switchack_size = std::max(res, max_switchack_size);
			}
			return res;
		}

		virtual void Serialize (TagBuffer b) const{
			if (Settings::switch_ack_mode == Settings::SWITCH_INGRESS_CREDIT){
				for (uint32_t i = 0; i < Settings::host_num;++i){
					b.WriteU64(acked_size[i]);
				}
			}else{
				b.WriteU64(acked_size[0]);
			}
		}
		virtual void Deserialize (TagBuffer b){
			if (Settings::switch_ack_mode == Settings::SWITCH_INGRESS_CREDIT){
				for (uint32_t i = 0; i < Settings::host_num;++i){
					acked_size[i] = b.ReadU64();
				}
			}else{
				acked_size[0] = b.ReadU64();
			}
		}
		virtual void Print (std::ostream &os) const{ }
		inline void setAckedSize(uint64_t id){
			acked_size[0] = id;
		}
		inline uint64_t getAckedSize(){
			return acked_size[0];
		}

		inline void SetACKEntry(uint32_t dst, uint64_t size){
			assert(dst < Settings::host_num);
			acked_size[dst] = size;
		}

		inline uint64_t getACKEntry(uint32_t dst){
			assert(dst < Settings::host_num);
			return acked_size[dst];
		}

	private:
		uint64_t acked_size[Settings::NODESCALE];
	};

	/**
	 * When use absolute-psn, upstream should send a syn packet to downstream to avoid drop several continuous packets periodically
	 */
	class SwitchSYNTag: public Tag{
		public:
		static uint32_t max_switchsyn_size;
		static uint32_t switch_psn_bit;
		SwitchSYNTag(){
			for(uint32_t i = 0; i < Settings::host_num; ++i){
				rcv_ack_size[i] = 0;
				nxt_data_size[i] = 0;
			}
		}

		static TypeId GetTypeId (void){
		  static TypeId tid = TypeId ("ns3::SwitchSYNTag") .SetParent<Tag> () .AddConstructor<SwitchSYNTag> ();
		  return tid;
		}

		virtual TypeId GetInstanceTypeId (void) const{ return GetTypeId(); }
		virtual uint32_t GetSerializedSize (void) const{
			return Settings::host_num*sizeof(uint64_t)*2;
		}

		uint32_t GetPacketSize(){
			uint32_t res = 0;
			for (uint32_t i = 0; i < Settings::host_num;++i){
				if (rcv_ack_size[i] > 0) res += (SwitchACKTag::switch_ack_id_bit + 2* switch_psn_bit);
			}
			max_switchsyn_size = std::max(res, max_switchsyn_size);
			return res;
		}

		virtual void Serialize (TagBuffer b) const{
			for (uint32_t i = 0; i < Settings::host_num;++i){
				b.WriteU64(rcv_ack_size[i]);
				b.WriteU64(nxt_data_size[i]);
			}
		}
		virtual void Deserialize (TagBuffer b){
			for (uint32_t i = 0; i < Settings::host_num;++i){
				rcv_ack_size[i] = b.ReadU64();
				nxt_data_size[i] = b.ReadU64();
			}
		}
		virtual void Print (std::ostream &os) const{ }

		inline void SetPSNEntry(uint32_t dst, uint64_t ack, uint64_t data){
			assert(dst < Settings::host_num);
			rcv_ack_size[dst] = ack;
			nxt_data_size[dst] = data;
		}

		inline uint64_t GetACKPSNEntry(uint32_t dst){
			assert(dst < Settings::host_num);
			return rcv_ack_size[dst];
		}

		inline uint64_t GetDataPSNEntry(uint32_t dst){
			assert(dst < Settings::host_num);
			return nxt_data_size[dst];
		}

	private:
		uint64_t rcv_ack_size[Settings::NODESCALE];
		uint64_t nxt_data_size[Settings::NODESCALE];
	};

	/**
	 * Choose the egress at sender-leaf depends on SymmetricRoutingTag
	 * Attention: must work with RecordRoutingTag
	 */
	class SymmetricRoutingTag: public Tag{
	public:
		SymmetricRoutingTag(){
			index = 0;
			for (uint32_t i = 0; i < MAX_HOP; ++i){
				ingress[i] = 0;
			}
		}
		static TypeId GetTypeId (void){
		  static TypeId tid = TypeId ("ns3::SymmetricRoutingTag") .SetParent<Tag> () .AddConstructor<SymmetricRoutingTag> ();
		  return tid;
		}
		virtual TypeId GetInstanceTypeId (void) const{ return GetTypeId(); }
		virtual uint32_t GetSerializedSize (void) const{ return (MAX_HOP+1)*sizeof(uint32_t);}
		virtual void Serialize (TagBuffer b) const{
			b.WriteU32(index);
			for (uint32_t i = 0; i < MAX_HOP; ++i){
				b.WriteU32(ingress[i]);
			}
		}
		virtual void Deserialize (TagBuffer b){
			index = b.ReadU32();
			for (uint32_t i = 0; i < MAX_HOP; ++i){
				ingress[i] = b.ReadU32();
			}
		}
		virtual void Print (std::ostream &os) const{ }

		inline void setReceiverLeafIngress(uint32_t id){
			assert(index < MAX_HOP);
			ingress[index++] = id;
		}

		inline void resetIndex(){
			index = 0;
		}

		inline void setIndex(uint32_t i){
			index = i;
		}

		inline uint32_t getIndex(){
			return index;
		}

		inline uint32_t getReceiverLeafIngress(){
			uint32_t i = ingress[index];
			++index;
			return i;
		}
	private:
		uint32_t index;
		uint32_t ingress[MAX_HOP];
	};

	/**
	 * Record the ingress of receiver leaf which indicates the passing spine
	 * Attention: must work with SymmetricRoutingTag
	 * (Only for leaf-spine topology for now)
	 */
	class RecordRoutingTag: public Tag{
	public:
		RecordRoutingTag(){
			index = 0;
			for (uint32_t i = 0; i < MAX_HOP; ++i){
				ingress[i] = 0;
			}
		}
		static TypeId GetTypeId (void){
		  static TypeId tid = TypeId ("ns3::RecordRoutingTag") .SetParent<Tag> () .AddConstructor<RecordRoutingTag> ();
		  return tid;
		}
		virtual TypeId GetInstanceTypeId (void) const{ return GetTypeId(); }
		virtual uint32_t GetSerializedSize (void) const{ return (MAX_HOP+1)*sizeof(uint32_t);}
		virtual void Serialize (TagBuffer b) const{
			b.WriteU32(index);
			for (uint32_t i = 0; i < MAX_HOP; ++i){
				b.WriteU32(ingress[i]);
			}
		}
		virtual void Deserialize (TagBuffer b){
			index = b.ReadU32();
			for (uint32_t i = 0; i < MAX_HOP; ++i){
				ingress[i] = b.ReadU32();
			}
		}
		virtual void Print (std::ostream &os) const{ }
		inline void setReceiverLeafIngress(uint32_t id){
			assert(index < MAX_HOP);
			ingress[index] = id;
			++index;
		}

		inline void resetIndex(){
			index = 0;
		}

		inline uint32_t getReceiverLeafIngress(uint32_t i){
			return ingress[i];
		}
	private:
		uint32_t index;
		uint32_t ingress[MAX_HOP];
	};

	/**
	 * Tag the last data/ack packet
	 */
	class LastPacketTag: public Tag{
	public:
		LastPacketTag(){
		}
		static TypeId GetTypeId (void){
		  static TypeId tid = TypeId ("ns3::LastPacketTag") .SetParent<Tag> () .AddConstructor<LastPacketTag> ();
		  return tid;
		}
		virtual TypeId GetInstanceTypeId (void) const{ return GetTypeId(); }
		virtual uint32_t GetSerializedSize (void) const{ return 0;}
		virtual void Serialize (TagBuffer i) const{
		}
		virtual void Deserialize (TagBuffer i){
		}
		virtual void Print (std::ostream &os) const{ }
	private:
	};

	class FlowTag: public Tag{
	public:
		FlowTag(){
			index = -1;
		}
		static TypeId GetTypeId (void){
		  static TypeId tid = TypeId ("ns3::FlowTag") .SetParent<Tag> () .AddConstructor<FlowTag> ();
		  return tid;
		}
		virtual TypeId GetInstanceTypeId (void) const{ return GetTypeId(); }
		virtual uint32_t GetSerializedSize (void) const{ return sizeof(uint32_t);}
		virtual void Serialize (TagBuffer i) const{
			i.WriteU32(index);
		}
		virtual void Deserialize (TagBuffer i){
			index = i.ReadU32();
		}
		inline void setIndex(uint32_t i){
			index = i;
		}

		inline uint32_t getIndex(){
			return index;
		}
		virtual void Print (std::ostream &os) const{ }
	private:
		uint32_t index;
	};

} // namespace ns3

#endif /* DROPTAIL_H */
