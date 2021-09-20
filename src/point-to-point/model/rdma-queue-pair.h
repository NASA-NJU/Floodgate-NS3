#ifndef RDMA_QUEUE_PAIR_H
#define RDMA_QUEUE_PAIR_H

#include <ns3/object.h>
#include <ns3/packet.h>
#include <ns3/ipv4-address.h>
#include <ns3/data-rate.h>
#include <ns3/event-id.h>
#include <ns3/custom-header.h>
#include <ns3/int-header.h>
#include <vector>
#include <queue>
#include "ns3/settings.h"

namespace ns3 {

/*
 * wqy, add on Oct 20, 2020.
 * RdmaOperation class is for supporting several messages in a same QueuePair.
 */
class RdmaOperation : public Object {
public:
	bool m_isTestFlow;
	uint32_t m_qpid;
	uint32_t m_msgSeq;
	uint64_t m_size;
	Time startTime;
	Time startTransferTime;
	Time m_lastActionTime;
	Ipv4Address sip, dip;
	uint16_t sport, dport;
	uint16_t m_pg;
	uint32_t m_src;
	uint32_t m_dst;
	uint32_t m_flow_id;

	/**
	 * runtime
	 */
	uint64_t snd_nxt, snd_una; // next seq to send, the highest unacked seq
	bool timeouted;
	bool m_received_last_ack;
	std::set<uint32_t> m_sent_psn;
	std::set<uint32_t> m_timeout_psn;

	static TypeId GetTypeId (void);
	RdmaOperation(uint16_t pg, Ipv4Address _sip, Ipv4Address _dip, uint16_t _sport, uint16_t _dport);
	void SetQPId(uint32_t);
	void SetMSGSeq(uint32_t);
	void SetSize(uint32_t);
	void SetSrc(uint32_t);
	void SetDst(uint32_t);
	void SetFlowId(uint32_t);
	void SetTestFlow(bool);
	void ResetLastActionTime();
	void Recover();
	uint64_t GetOnTheFly();
	uint64_t GetBytesLeft();
	bool IsFinished();
	void Acknowledge(uint64_t ack, bool isLastACK = false);
	void UpdateReceivedACK();

};

struct RdmaOperationMsgSeqCMP
{
    bool operator()(Ptr<RdmaOperation> a, Ptr<RdmaOperation> b)
    {
        return a->m_msgSeq  > b->m_msgSeq;
    }
};

struct RdmaOperationLastActionTimeCMP
{
    bool operator()(Ptr<RdmaOperation> a, Ptr<RdmaOperation> b)
    {
        return a->m_lastActionTime  > b->m_lastActionTime;
    }
};

class RdmaQueuePair : public Object {
public:
	// wqy
	uint32_t m_qpid;
	uint32_t m_msgSeq;
	uint32_t m_src;
	uint32_t m_dst;
	uint32_t m_flow_id;
	std::queue<Ptr<RdmaOperation> >  rdma_msgs;	// msg which still has packets to send
	std::queue<Ptr<RdmaOperation> >  rdma_msgs_unfinished;	// msgs which has sent all packets, but hasn't received last ack
	bool m_isTestFlow;
	uint64_t m_last_print_rate;

	Time startTime;
	Ipv4Address sip, dip;
	uint16_t sport, dport;
	uint64_t m_size;
	uint64_t snd_nxt, snd_una; // next seq to send, the highest unacked seq
	uint16_t m_pg;
	uint16_t m_ipid;

	uint32_t m_win; // bound of on-the-fly packets
	uint64_t m_baseRtt; // base RTT of this qp
	DataRate m_max_rate; // max rate
	bool m_var_win; // variable window size
	Time m_nextAvail;	//< Soonest time of next send
	uint32_t wp; // current window of packets
	uint32_t lastPktSize;

	bool m_received_last_ack;
	std::set<uint32_t> m_sent_psn;
	std::set<uint32_t> m_timeout_psn;
	bool timeouted;

	/******************************
	 * runtime states
	 *****************************/
	DataRate m_rate;	//< Current rate
	struct {
		DataRate m_targetRate;	//< Target rate
		EventId m_eventUpdateAlpha;
		double m_alpha;
		bool m_alpha_cnp_arrived; // indicate if CNP arrived in the last slot
		bool m_first_cnp; // indicate if the current CNP is the first CNP
		EventId m_eventDecreaseRate;
		bool m_decrease_cnp_arrived; // indicate if CNP arrived in the last slot
		uint32_t m_rpTimeStage;
		EventId m_rpTimer;
	} mlx;
	struct {
		uint32_t m_lastUpdateSeq;
		DataRate m_curRate;
		IntHop hop[IntHeader::maxHop];
		uint32_t keep[IntHeader::maxHop];
		uint32_t m_incStage;
		double m_lastGap;
		double u;
		struct {
			double u;
			DataRate Rc;
			uint32_t incStage;
		}hopState[IntHeader::maxHop];
	} hp;
	struct{
		uint32_t m_lastUpdateSeq;
		DataRate m_curRate;
		uint32_t m_incStage;
		uint64_t lastRtt;
		double rttDiff;
	} tmly;
	struct{
		uint32_t m_lastUpdateSeq;
		uint32_t m_caState;
		uint32_t m_highSeq; // when to exit cwr
		double m_alpha;
		uint32_t m_ecnCnt;
		uint32_t m_batchSizeOfAlpha;

		// to handle disordered packets
		uint32_t m_acknumRTT;
	} dctcp;

	/***********
	 * methods
	 **********/
	static TypeId GetTypeId (void);
	RdmaQueuePair(uint16_t pg, Ipv4Address _sip, Ipv4Address _dip, uint16_t _sport, uint16_t _dport);
	void SetSize(uint64_t size);
	void SetWin(uint32_t win);
	void SetBaseRtt(uint64_t baseRtt);
	void SetVarWin(bool v);
	void SetQPId(uint32_t);
	void SetMSGSeq(uint32_t);
	void SetSrc(uint32_t);
	void SetDst(uint32_t);
	void SetFlowId(uint32_t);
	void SetTestFlow(bool);

	/*
	 * wqy, on Nov 2, 2020
	 * For timeout retransmission
	 */
	typedef Callback<void> DevDequeueCallback;
	DevDequeueCallback m_devDequeueCallback;
	EventId m_eventTimeoutRetransmission;
	// callback of nic's sending phase
	void SetDevDequeueCallback(DevDequeueCallback devDequeuCallback);
	// under non qp_mode, reset RTO depending on Now()
	void ResetRTOEvent();
	// under non qp_mode, recover QP's status
	void RecoverQueue();
	// under qp_mode, reset RTO depending on parameter
	void ResetRTOEvent(Time earliest);
	// under qp_mode, once a new action(receive ACK/send packet) happens, update this msg's RTO startTime - lastActionTime
	void ResetMSGRTOTime(uint32_t msgSeq);
	// under qp_mode, when trigger TRO, recover msg's status and move timeout msg back to sending queue
	void RecoverMSG();

	/*
	 * wqy, on Oct 20, 2020
	 * For QP mode
	 */
	Ptr<RdmaOperation> CheckFinish(uint32_t msg_seq, uint32_t ack_num);
	void UpdateReceivedACK();
	void AddRdmaOperation(Ptr<RdmaOperation>);
	bool RemoveRdmaOperation(uint32_t);
	void ContinueTestFlow();
	void LoadQPStatus();
	void LoadMsgStatus();
	uint32_t GetMsgNumber();
	Ptr<RdmaOperation> PeekRdmaOperation();
	void MoveRdmaOperationToUnfinished();	// when a msg sent all packets, move into unfinished queue
	bool RecoverMsg(uint32_t msg_seq);	// when receive a NACK of a msg in unfinished queue, move it back
	void PrintRate();

	uint64_t GetNxtPSN();
	uint64_t GetBytesLeft();
	uint32_t GetHash(void);
	void Acknowledge(uint64_t ack, bool isLastACK = false);
	void Acknowledge(uint64_t ack, uint32_t msgSeq, bool isLastACK = false);
	uint64_t GetOnTheFly();
	bool IsWinBound();
	uint64_t GetWin(); // window size calculated from m_rate
	bool IsFinished();
	uint64_t HpGetCurWin(); // window size calculated from hp.m_curRate, used by HPCC
};

class RdmaRxQueuePair : public Object { // Rx side queue pair
public:
	struct ECNAccount{
		uint16_t qIndex;
		uint8_t ecnbits;
		uint16_t qfb;
		uint16_t total;

		ECNAccount() { memset(this, 0, sizeof(ECNAccount));}
	};
	ECNAccount m_ecn_source;
	uint32_t m_flowId;
	uint32_t sip, dip;
	uint16_t sport, dport;
	uint16_t m_ipid;
	uint32_t ReceiverNextExpectedSeq;
	Time m_nackTimer;
	int32_t m_milestone_rx;
	uint32_t m_lastNACK;
	EventId QcnTimerEvent; // if destroy this rxQp, remember to cancel this timer

	/**
	 * add by wqy on 2020/11/9
	 * Used when use packet-level-routing
	 */
	bool m_received_last_psn_packet;
	std::set<uint32_t> m_received_psn;

	/**
	 * add by wqy on 2021/3/8
	 * For queuing time analysis
	 */
	std::vector<uint64_t> m_queuingTime[Settings::MAXHOP];	// ns

	static TypeId GetTypeId (void);
	RdmaRxQueuePair();
	uint32_t GetHash(void);
	bool ReceivedAll(uint32_t payload);
};

class RdmaQueuePairGroup : public Object {
public:
	std::vector<Ptr<RdmaQueuePair> > m_qps;
	//std::vector<Ptr<RdmaRxQueuePair> > m_rxQps;

	static TypeId GetTypeId (void);
	RdmaQueuePairGroup(void);
	uint32_t GetN(void);
	Ptr<RdmaQueuePair> Get(uint32_t idx);
	Ptr<RdmaQueuePair> operator[](uint32_t idx);
	void AddQp(Ptr<RdmaQueuePair> qp);
	//void AddRxQp(Ptr<RdmaRxQueuePair> rxQp);
	void Clear(void);
};

}

#endif /* RDMA_QUEUE_PAIR_H */
