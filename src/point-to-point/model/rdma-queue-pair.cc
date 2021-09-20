#include <ns3/hash.h>
#include <ns3/uinteger.h>
#include <ns3/seq-ts-header.h>
#include <ns3/udp-header.h>
#include <ns3/ipv4-header.h>
#include <ns3/simulator.h>
#include "ns3/ppp-header.h"
#include "ns3/rdma-queue-pair.h"
#include "ns3/settings.h"
#include <assert.h>

namespace ns3 {

/**************************
 * RdmaOperation
 *************************/
TypeId RdmaOperation::GetTypeId (void)
{
	static TypeId tid = TypeId ("ns3::RdmaOperation")
		.SetParent<Object> ()
		;
	return tid;
}

RdmaOperation::RdmaOperation(uint16_t pg, Ipv4Address _sip, Ipv4Address _dip, uint16_t _sport, uint16_t _dport){
	m_pg = pg;
	sip = _sip;
	dip = _dip;
	sport = _sport;
	dport = _dport;
	snd_nxt = 0;
	snd_una = 0;
	startTime = Simulator::Now();
	m_lastActionTime = Simulator::Now();
	m_qpid = 0;
	m_msgSeq = 0;
	m_size = 0;
	m_src = 0;
	m_dst = 0;
	timeouted = false;
	m_received_last_ack = false;
}

void RdmaOperation::SetQPId(uint32_t qpId){
	m_qpid = qpId;
}

void RdmaOperation::SetMSGSeq(uint32_t msgSeq){
	m_msgSeq = msgSeq;
}

void RdmaOperation::SetSize(uint32_t size){
	m_size = size;
}

void RdmaOperation::SetSrc(uint32_t src){
	m_src = src;
}

void RdmaOperation::SetDst(uint32_t dst){
	m_dst = dst;
}

void RdmaOperation::SetTestFlow(bool isTestFlow){
	m_isTestFlow = isTestFlow;
}

void RdmaOperation::SetFlowId(uint32_t flow_id){
	m_flow_id = flow_id;
}

void RdmaOperation::ResetLastActionTime(){
	m_lastActionTime = Simulator::Now();
}

void RdmaOperation::Recover(){
	timeouted = true;
	m_timeout_psn = m_sent_psn;
	if (!Settings::IsPacketLevelRouting())
		snd_nxt = snd_una;
}

uint64_t RdmaOperation::GetBytesLeft(){
	uint64_t result = m_size >= snd_nxt ? m_size - snd_nxt : 0;
	if (Settings::IsPacketLevelRouting() && timeouted && m_timeout_psn.size() > 0){
		/**
		 * When the routing is packet-level and has timeouted
		 * --> the packets in `m_timeout_psn` will be sent
		 */
		if (m_size%Settings::packet_payload != 0){
			result = (m_timeout_psn.size() - 1) * Settings::packet_payload;
			if (m_timeout_psn.count(m_size - m_size%Settings::packet_payload))
				// if last packet has not sent --> add the last packet
				result += m_size%Settings::packet_payload;
			else
				// if last packet has sent --> add a whole packet
				result += Settings::packet_payload;
		}else{
			result = m_timeout_psn.size() * Settings::packet_payload;
		}
	}
	return result;
}

uint64_t RdmaOperation::GetOnTheFly(){
	if (!Settings::IsPacketLevelRouting())
		return snd_nxt - snd_una;
	else{
		uint32_t result = 0;
		if (m_size%Settings::packet_payload != 0){
			result = (m_sent_psn.size() - 1) * Settings::packet_payload;
			if (m_sent_psn.count(m_size - m_size%Settings::packet_payload))
				// if last packet has not sent --> add the last packet
				result += m_size%Settings::packet_payload;
			else
				// if last packet has sent --> add a whole packet
				result += Settings::packet_payload;
		}else{
			result = m_sent_psn.size() * Settings::packet_payload;
		}
		if (timeouted) {
			/**
			 * When flow has triggered timeout,
			 * m_timeout_psn stands for the remaining packets which should retransmit
			 * m_sent_psn stands for the all packets which should retransmit
			 * so, m_sent_psn - m_timeout_psn stands for the inflight packets
			 */
			uint32_t timeout_sent = 0;
			if (m_size%Settings::packet_payload != 0){
				timeout_sent = (m_timeout_psn.size() - 1) * Settings::packet_payload;
				if (m_timeout_psn.count(m_size - m_size%Settings::packet_payload))
					// if last packet has not sent --> add the last packet
					timeout_sent += m_size%Settings::packet_payload;
				else
					// if last packet has sent --> add a whole packet
					timeout_sent += Settings::packet_payload;
			}else{
				timeout_sent = m_timeout_psn.size() * Settings::packet_payload;
			}
			assert(timeout_sent <= result);
			result -= timeout_sent;
		}
		return result;
	}
}

bool RdmaOperation::IsFinished(){
	if (!Settings::IsPacketLevelRouting())
		return snd_una >= m_size;
	else
		return m_received_last_ack;
}

void RdmaOperation::Acknowledge(uint64_t ack, bool isLastACK){
	if (Settings::IsPacketLevelRouting()){
		if (m_sent_psn.count(ack)) {
			m_sent_psn.erase(ack);
			if (m_timeout_psn.count(ack))
				m_timeout_psn.erase(ack);
			assert(0 == m_sent_psn.count(ack));
			UpdateReceivedACK();
		}
		if (isLastACK) m_received_last_ack = true;
	}else{
		if (ack > snd_una){
			snd_una = ack;
		}
	}
}

void RdmaOperation::UpdateReceivedACK(){
	assert(Settings::IsPacketLevelRouting());
	if (m_sent_psn.empty()){
		// all sent packets have received.
		snd_una = snd_nxt;
	}else{
		snd_una = *(m_sent_psn.begin());
	}
}


/**************************
 * RdmaQueuePair
 *************************/
TypeId RdmaQueuePair::GetTypeId (void)
{
	static TypeId tid = TypeId ("ns3::RdmaQueuePair")
		.SetParent<Object> ()
		;
	return tid;
}

RdmaQueuePair::RdmaQueuePair(uint16_t pg, Ipv4Address _sip, Ipv4Address _dip, uint16_t _sport, uint16_t _dport){
	m_isTestFlow = false;
	m_last_print_rate = 0;
	startTime = Simulator::Now();
	sip = _sip;
	dip = _dip;
	sport = _sport;
	dport = _dport;
	m_size = 0;
	snd_nxt = snd_una = 0;
	m_pg = pg;
	m_ipid = 0;
	m_win = 0;
	m_baseRtt = 0;
	m_max_rate = 0;
	m_var_win = false;
	m_rate = 0;
	m_nextAvail = Time(0);

	m_received_last_ack = false;
	timeouted = false;

	mlx.m_alpha = 1;
	mlx.m_alpha_cnp_arrived = false;
	mlx.m_first_cnp = true;
	mlx.m_decrease_cnp_arrived = false;
	mlx.m_rpTimeStage = 0;
	hp.m_lastUpdateSeq = 0;

	for (uint32_t i = 0; i < sizeof(hp.keep) / sizeof(hp.keep[0]); i++)
		hp.keep[i] = 0;
	hp.m_incStage = 0;
	hp.m_lastGap = 0;
	hp.u = 1;
	for (uint32_t i = 0; i < IntHeader::maxHop; i++){
		hp.hopState[i].u = 1;
		hp.hopState[i].incStage = 0;
	}

	tmly.m_lastUpdateSeq = 0;
	tmly.m_incStage = 0;
	tmly.lastRtt = 0;
	tmly.rttDiff = 0;

	dctcp.m_lastUpdateSeq = 0;
	dctcp.m_caState = 0;
	dctcp.m_highSeq = 0;
	dctcp.m_alpha = 1;
	dctcp.m_ecnCnt = 0;
	dctcp.m_batchSizeOfAlpha = 0;
}

/*
 * The callback of device's DequeueAndTransmit method
 * for timeout retransmission
 */
void RdmaQueuePair::SetDevDequeueCallback(DevDequeueCallback devDequeueCallback){
	m_devDequeueCallback = devDequeueCallback;
}

/*
 * Most of the time, used for !Settings::qp_mode
 * for timeout retransmission to reset retransmission event
 */
void RdmaQueuePair::ResetRTOEvent(){
	m_eventTimeoutRetransmission.Cancel();
	m_eventTimeoutRetransmission = Simulator::Schedule(MicroSeconds(Settings::rto_us), &RdmaQueuePair::RecoverQueue, this);
}

/*
 * Most of the time, used for !Settings::qp_mode
 * for timeout retransmission
 */
void RdmaQueuePair::RecoverQueue(){
	timeouted = true;
	Settings::timeout_times++;
	m_timeout_psn = m_sent_psn;
	if (!Settings::IsPacketLevelRouting()){
		snd_nxt = snd_una;		// do not restore snd_next when exist unordered packet;
	}
	m_devDequeueCallback();
}

/*
 * Most of the time, used for Settings::qp_mode
 * for timeout retransmission
 * when a new action happen, reset this message's last action time(used for calculate timeout time)
 */
void RdmaQueuePair::ResetMSGRTOTime(uint32_t msgSeq){
	std::priority_queue<Ptr<RdmaOperation>, std::vector<Ptr<RdmaOperation> >, RdmaOperationLastActionTimeCMP> all_inflight_msgs;

	bool find = false;
	uint32_t unfinshed_num = rdma_msgs_unfinished.size();
	for (uint32_t i = 0; i < unfinshed_num; i++){
		Ptr<RdmaOperation> curr = rdma_msgs_unfinished.front();
		rdma_msgs_unfinished.pop();
		if (msgSeq == curr->m_msgSeq){
			find = true;
			curr->ResetLastActionTime();
		}
		if (curr->GetBytesLeft() == 0 && curr->GetOnTheFly())
			all_inflight_msgs.push(curr);
		rdma_msgs_unfinished.push(curr);
	}

	uint32_t sending_num = rdma_msgs.size();
	for (uint32_t i = 0; i < sending_num; i++){
		Ptr<RdmaOperation> curr = rdma_msgs.front();
		rdma_msgs.pop();
		if (!find && msgSeq == curr->m_msgSeq){
			find = true;
			curr->ResetLastActionTime();
		}
		if (curr->GetBytesLeft() == 0 && curr->GetOnTheFly())
			all_inflight_msgs.push(curr);
		rdma_msgs.push(curr);
	}

	/*
	 * has found timeout msg in unfinished queue
	 * --> reset timeout time
	 */
	if (all_inflight_msgs.size() > 0){
		Time earliest_timeout = all_inflight_msgs.top()->m_lastActionTime + MicroSeconds(Settings::rto_us);
		ResetRTOEvent(earliest_timeout);
	}
}

/*
 * Most of the time, used for Settings::qp_mode
 * for timeout retransmission to reset QP's retransmission event
 */
void RdmaQueuePair::ResetRTOEvent(Time earliest){
	m_eventTimeoutRetransmission.Cancel();
	Time t;
	if (earliest <= Simulator::Now())
		t = Time(0);
	else
		t = earliest - Simulator::Now();
	m_eventTimeoutRetransmission = Simulator::Schedule(t, &RdmaQueuePair::RecoverMSG, this);
}

/*
 * Most of the time, used for Settings::qp_mode
 * for timeout retransmission
 */
void RdmaQueuePair::RecoverMSG(){
	/*
	 * unfinished queue:
	 * find timeout msgs
	 */
	std::priority_queue<Ptr<RdmaOperation>, std::vector<Ptr<RdmaOperation> >, RdmaOperationMsgSeqCMP> targets;
	uint32_t unfinshed_num = rdma_msgs_unfinished.size();
	for (uint32_t i = 0; i < unfinshed_num; i++){
		Ptr<RdmaOperation> curr = rdma_msgs_unfinished.front();
		rdma_msgs_unfinished.pop();
		if (curr->m_lastActionTime + MicroSeconds(Settings::rto_us) <= Simulator::Now()){
			curr->Recover();
			targets.push(curr);
		}
		rdma_msgs_unfinished.push(curr);
	}

	/*
	 * sending queue:
	 * 1. find timeout msgs
	 * 2. move timeout msgs in unfinished queue back to sending queue
	 */
	uint32_t targets_num = targets.size();
	if (targets_num > 0){
		LoadMsgStatus();
	}

	uint32_t sending_num = rdma_msgs.size();
	for (uint32_t i = 0; i < sending_num; i++){
		Ptr<RdmaOperation> curr = rdma_msgs.front();
		rdma_msgs.pop();
		// todo: think that whether set sending message timeout ?
		if (curr->m_lastActionTime + MicroSeconds(Settings::rto_us) <= Simulator::Now()){
			curr->Recover();
		}
		while (targets.size() > 0 && targets.top()->m_msgSeq < curr->m_msgSeq){
			rdma_msgs.push(targets.top());
			targets.pop();
		}
		rdma_msgs.push(curr);
	}

	if (targets_num > 0){
		LoadQPStatus();
	}

	if (targets_num > 0){
		// call nic to send packets
		Settings::timeout_times++;
		m_devDequeueCallback();
	}

}

void RdmaQueuePair::PrintRate(){
	if (m_last_print_rate != m_rate.GetBitRate()){
		Settings::rate_out << m_qpid << " " << Simulator::Now().GetTimeStep() << " " << m_rate.GetBitRate() << std::endl;
		m_last_print_rate = m_rate.GetBitRate();
	}
}

uint32_t RdmaQueuePair::GetMsgNumber(){
	return rdma_msgs.size() + rdma_msgs_unfinished.size();
}

/*
 * Load next msg's status as qp status
 */
void RdmaQueuePair::LoadQPStatus(){
	if (rdma_msgs.size() > 0){
		Ptr<RdmaOperation> msg = rdma_msgs.front();
		msg->startTransferTime = Simulator::Now();
		m_size = msg->m_size;
		m_msgSeq = msg->m_msgSeq;
		sport = msg->sport;
		dport = msg->dport;
		snd_nxt = msg->snd_nxt;
		snd_una = msg->snd_una;
		m_src = msg->m_src;
		m_dst = msg->m_dst;
		m_flow_id = msg->m_flow_id;
		m_isTestFlow = msg->m_isTestFlow;
		timeouted = msg->timeouted;
		m_sent_psn = msg->m_sent_psn;
		m_timeout_psn = msg->m_timeout_psn;
		m_received_last_ack = msg->m_received_last_ack;

		if (Settings::reset_qp_rate){
			m_win = 0;
			m_baseRtt = 0;
			m_max_rate = 0;
			m_var_win = false;
			m_rate = 0;
			m_nextAvail = Time(0);
			mlx.m_alpha = 1;
			mlx.m_alpha_cnp_arrived = false;
			mlx.m_first_cnp = true;
			mlx.m_decrease_cnp_arrived = false;
			mlx.m_rpTimeStage = 0;

			hp.m_lastUpdateSeq = 0;
			for (uint32_t i = 0; i < sizeof(hp.keep) / sizeof(hp.keep[0]); i++)
				hp.keep[i] = 0;
			hp.m_incStage = 0;
			hp.m_lastGap = 0;
			hp.u = 1;
			for (uint32_t i = 0; i < IntHeader::maxHop; i++){
				hp.hopState[i].u = 1;
				hp.hopState[i].incStage = 0;
			}

			tmly.m_lastUpdateSeq = 0;
			tmly.m_incStage = 0;
			tmly.lastRtt = 0;
			tmly.rttDiff = 0;

			dctcp.m_lastUpdateSeq = 0;
			dctcp.m_caState = 0;
			dctcp.m_highSeq = 0;
			dctcp.m_alpha = 1;
			dctcp.m_ecnCnt = 0;
			dctcp.m_acknumRTT = 0;
			dctcp.m_batchSizeOfAlpha = 0;
		}
	}else{
		m_msgSeq++;		// indeed, no sense...
	}
}

/*
 * Load qp status as current msg status
 */
void RdmaQueuePair::LoadMsgStatus(){
	if (rdma_msgs.size() > 0){
		Ptr<RdmaOperation> msg = rdma_msgs.front();
		msg->m_size = m_size;
		msg->m_msgSeq = m_msgSeq;
		msg->sport = sport;
		msg->dport = dport;
		msg->snd_nxt = snd_nxt;
		msg->snd_una = snd_una;
		msg->m_src = m_src;
		msg->m_dst = m_dst;
		msg->m_flow_id = m_flow_id;
		msg->m_isTestFlow = m_isTestFlow;
		msg->timeouted = timeouted;
		msg->m_sent_psn = m_sent_psn;
		msg->m_timeout_psn = m_timeout_psn;
		msg->m_received_last_ack = m_received_last_ack;
	}
}

Ptr<RdmaOperation> RdmaQueuePair::PeekRdmaOperation(){
	if (rdma_msgs.size() > 0)
		return rdma_msgs.front();
	else
		return NULL;
}

void RdmaQueuePair::AddRdmaOperation(Ptr<RdmaOperation> msg){
	rdma_msgs.push(msg);
	if (rdma_msgs.size() == 1){
		LoadQPStatus();
	}
}

/*
 * Called when current msg has sent all of packets
 * -->  move current msg into unfinished queue, and load status of next msg
 */
void RdmaQueuePair::MoveRdmaOperationToUnfinished(){
	LoadMsgStatus();
	Ptr<RdmaOperation> curr = rdma_msgs.front();
	rdma_msgs_unfinished.push(curr);
	rdma_msgs.pop();
	LoadQPStatus();
}

/*
 * When receive a NACK of a msg in unfinished queue, move it back
 */
bool RdmaQueuePair::RecoverMsg(uint32_t msg_seq){
	Ptr<RdmaOperation> target_msg = NULL;
	uint32_t unfinshed_num = rdma_msgs_unfinished.size();
	for (uint32_t i = 0; i < unfinshed_num; i++){
		Ptr<RdmaOperation> curr = rdma_msgs_unfinished.front();
		rdma_msgs_unfinished.pop();
		if (msg_seq != curr->m_msgSeq){
			rdma_msgs_unfinished.push(curr);
		}else{
			target_msg = curr;
			target_msg->Recover();
		}
	}
	if (!!target_msg){
		LoadMsgStatus();
		uint32_t ongoing_num = rdma_msgs.size();
		bool added = false;
		// move target message to sending queue
		for (uint32_t i = 0; i < ongoing_num; i++){
			Ptr<RdmaOperation> curr = rdma_msgs.front();
			if (!added && curr->m_msgSeq > target_msg->m_msgSeq){
				rdma_msgs.push(target_msg);
				added = true;
			}
			rdma_msgs.pop();
			rdma_msgs.push(curr);
		}
		if (!added){
			rdma_msgs.push(target_msg);
		}
		LoadQPStatus();
	}
	return !!target_msg;
}

/*
 * Called when msg receive last ACK, i.e., when msg finish
 * --> Remove msg from unfinished queue
 */
bool RdmaQueuePair::RemoveRdmaOperation(uint32_t msgSeq){
	bool result = false;
	// msg has been put into unfinished queue
	// remove msg from msg_unfinished queue
	uint32_t unfinshed_num = rdma_msgs_unfinished.size();
	for (uint32_t i = 0; i < unfinshed_num; i++){
		Ptr<RdmaOperation> curr = rdma_msgs_unfinished.front();
		rdma_msgs_unfinished.pop();
		if (msgSeq != curr->m_msgSeq){
			rdma_msgs_unfinished.push(curr);
		}else{
			result = true;
		}
	}
	if (!result){
		uint32_t sending_num = rdma_msgs.size();
		for (uint32_t i = 0; i < sending_num; i++){
			Ptr<RdmaOperation> curr = rdma_msgs.front();
			rdma_msgs.pop();
			if (msgSeq != curr->m_msgSeq){
				rdma_msgs.push(curr);
			}else{
				result = true;
			}
		}
	}

	if (GetMsgNumber() == 0){
		// no unfinished msg -- empty QP
		Simulator::Cancel(m_eventTimeoutRetransmission);
	}

	return result;
}

/*
 * For test flow, generate next msg
 */
void RdmaQueuePair::ContinueTestFlow(){
	if (rdma_msgs.size() > 0){
		// create message
		Ptr<RdmaOperation> msg = Create<RdmaOperation>(rdma_msgs.front()->m_pg, rdma_msgs.front()->sip, rdma_msgs.front()->dip, rdma_msgs.front()->sport+1, rdma_msgs.front()->dport);
		msg->SetSrc(rdma_msgs.front()->m_src);
		msg->SetDst(rdma_msgs.front()->m_dst);
		msg->SetMSGSeq(rdma_msgs.front()->m_msgSeq+1);
		msg->SetQPId(rdma_msgs.front()->m_qpid);
		msg->SetSize(rdma_msgs.front()->m_size);
		msg->SetTestFlow(rdma_msgs.front()->m_isTestFlow);
		rdma_msgs.push(msg);
	}
}

/*
 * Check whether the last ack of a msg
 */
Ptr<RdmaOperation> RdmaQueuePair::CheckFinish(uint32_t msg_seq, uint32_t ack_num){
	Ptr<RdmaOperation> result = NULL;
	bool find = false;
	uint32_t unfinshed_num = rdma_msgs_unfinished.size();
	for (uint32_t i = 0; i < unfinshed_num; i++){
		Ptr<RdmaOperation> curr = rdma_msgs_unfinished.front();
		if (curr->m_msgSeq == msg_seq){
			find = true;
			if (curr->IsFinished())
				result = curr;
		}
		rdma_msgs_unfinished.pop();
		rdma_msgs_unfinished.push(curr);
	}
	if (!find){
		uint32_t sending_num = rdma_msgs.size();
		for (uint32_t i = 0; i < sending_num; i++){
			Ptr<RdmaOperation> curr = rdma_msgs.front();
			if (curr->m_msgSeq == msg_seq){
				if (curr->IsFinished())
					result = curr;
			}
			rdma_msgs.pop();
			rdma_msgs.push(curr);
		}
	}
	return result;
}

/*
 * update the sending message's snd_una
 */
void RdmaQueuePair::UpdateReceivedACK(){
	assert(Settings::IsPacketLevelRouting());
	if (m_sent_psn.empty()){
		// all sent packets have received.
		snd_una = snd_nxt;
	}else{
		snd_una = *(m_sent_psn.begin());
	}
}

void RdmaQueuePair::SetSize(uint64_t size){
	m_size = size;
}

void RdmaQueuePair::SetWin(uint32_t win){
	m_win = win;
}

void RdmaQueuePair::SetBaseRtt(uint64_t baseRtt){
	m_baseRtt = baseRtt;
}

void RdmaQueuePair::SetVarWin(bool v){
	m_var_win = v;
}

void RdmaQueuePair::SetQPId(uint32_t qp){
	m_qpid = qp;
}

void RdmaQueuePair::SetMSGSeq(uint32_t msgSeq){
	m_msgSeq = msgSeq;
}

void RdmaQueuePair::SetSrc(uint32_t src){
	m_src = src;
}

void RdmaQueuePair::SetDst(uint32_t dst){
	m_dst = dst;
}

void RdmaQueuePair::SetTestFlow(bool isTestFlow){
	m_isTestFlow = isTestFlow;
}

void RdmaQueuePair::SetFlowId(uint32_t flow_id){
	m_flow_id = flow_id;
}

uint64_t RdmaQueuePair::GetBytesLeft(){
	uint64_t result = m_size >= snd_nxt ? m_size - snd_nxt : 0;
	if (Settings::IsPacketLevelRouting() && timeouted && m_timeout_psn.size() > 0){
		/**
		 * When the routing is packet-level and has timeouted
		 * --> the packets in `m_timeout_psn` will be sent
		 */
		if (m_size%Settings::packet_payload != 0){
			result = (m_timeout_psn.size() - 1) * Settings::packet_payload;
			if (m_timeout_psn.count(m_size - m_size%Settings::packet_payload))
				// if last packet has not sent --> add the last packet
				result += m_size%Settings::packet_payload;
			else
				// if last packet has sent --> add a whole packet
				result += Settings::packet_payload;
		}else{
			result = m_timeout_psn.size() * Settings::packet_payload;
		}
	}
	return result;
}


uint64_t RdmaQueuePair::GetNxtPSN(){
	uint64_t seq = snd_nxt;
	if (Settings::IsPacketLevelRouting()){
		if (seq >= m_size && timeouted && m_timeout_psn.size() > 0){
			/**
			 * Under packet-level-routing, don't use go-back-N.
			 * Flow has sent out all packets at once (qp->snd_nxt >= qp->m_size),
			 * however, till timeout triggered(qp->timeouted), there are some in-flight data packet(m_timeout_psn).
			 * These (ACKs of)packets may dropped, so retransmit these packets.
			 */
			seq = *(m_timeout_psn.begin());
		}
	}
	return seq;
}

uint32_t RdmaQueuePair::GetHash(void){
	union{
		struct {
			uint32_t sip, dip;
			uint16_t sport, dport;
		};
		char c[12];
	} buf;
	buf.sip = sip.Get();
	buf.dip = dip.Get();
	buf.sport = sport;
	buf.dport = dport;
	return Hash32(buf.c, 12);
}

void RdmaQueuePair::Acknowledge(uint64_t ack, bool isLastACK){
	if (Settings::IsPacketLevelRouting()){
		if (m_sent_psn.count(ack)) {
			m_sent_psn.erase(ack);
			if (m_timeout_psn.count(ack))
				m_timeout_psn.erase(ack);
			assert(0 == m_sent_psn.count(ack));
			UpdateReceivedACK();
		}
		if (isLastACK) m_received_last_ack = true;
	}else{
		if (ack > snd_una){
			snd_una = ack;
		}
	}
}

void RdmaQueuePair::Acknowledge(uint64_t ack, uint32_t msg_seq, bool isLastACK){
	bool find = false;
	uint32_t unfinshed_num = rdma_msgs_unfinished.size();
	for (uint32_t i = 0; i < unfinshed_num; i++){
		Ptr<RdmaOperation> curr = rdma_msgs_unfinished.front();
		if (curr->m_msgSeq == msg_seq){
			find = true;
			curr->Acknowledge(ack, isLastACK);
		}
		rdma_msgs_unfinished.pop();
		rdma_msgs_unfinished.push(curr);
	}
	if (!find){
		uint32_t sending_num = rdma_msgs.size();
		for (uint32_t i = 0; i < sending_num; i++){
			Ptr<RdmaOperation> curr = rdma_msgs.front();
			if (curr->m_msgSeq == msg_seq){
				curr->Acknowledge(ack, isLastACK);
			}
			rdma_msgs.pop();
			rdma_msgs.push(curr);
		}
	}
}

uint64_t RdmaQueuePair::GetOnTheFly(){
	if (!Settings::IsPacketLevelRouting())
		return snd_nxt - snd_una;
	else{
		uint32_t result = 0;
		if (m_size%Settings::packet_payload != 0){
			result = (m_sent_psn.size() - 1) * Settings::packet_payload;
			if (m_sent_psn.count(m_size - m_size%Settings::packet_payload))
				// if last packet has not sent --> add the last packet
				result += m_size%Settings::packet_payload;
			else
				// if last packet has sent --> add a whole packet
				result += Settings::packet_payload;
		}else{
			result = m_sent_psn.size() * Settings::packet_payload;
		}
		if (timeouted) {
			/**
			 * When flow has triggered timeout,
			 * m_timeout_psn stands for the remaining packets which should retransmit
			 * m_sent_psn stands for the all packets which should retransmit
			 * so, m_sent_psn - m_timeout_psn stands for the inflight packets
			 */
			uint32_t timeout_sent = 0;
			if (m_size%Settings::packet_payload != 0){
				timeout_sent = (m_timeout_psn.size() - 1) * Settings::packet_payload;
				if (m_timeout_psn.count(m_size - m_size%Settings::packet_payload))
					// if last packet has not sent --> add the last packet
					timeout_sent += m_size%Settings::packet_payload;
				else
					// if last packet has sent --> add a whole packet
					timeout_sent += Settings::packet_payload;
			}else{
				timeout_sent = m_timeout_psn.size() * Settings::packet_payload;
			}
			assert(timeout_sent <= result);
			result -= timeout_sent;
		}
		return result;
	}
}

bool RdmaQueuePair::IsWinBound(){
	uint64_t w = GetWin();
	return w != 0 && GetOnTheFly() >= w;
}

uint64_t RdmaQueuePair::GetWin(){
	if (m_win == 0)
		return 0;
	uint64_t w;
	if (m_var_win){
		w = m_win * m_rate.GetBitRate() / m_max_rate.GetBitRate();
		if (w == 0)
			w = 1; // must > 0
	}else{
		w = m_win;
	}
	return w;
}

uint64_t RdmaQueuePair::HpGetCurWin(){
	if (m_win == 0)
		return 0;
	uint64_t w;
	if (m_var_win){
		w = m_win * hp.m_curRate.GetBitRate() / m_max_rate.GetBitRate();
		if (w == 0)
			w = 1; // must > 0
	}else{
		w = m_win;
	}
	return w;
}

bool RdmaQueuePair::IsFinished(){
	if (!Settings::IsPacketLevelRouting())
		return snd_una >= m_size;
	else
		return m_received_last_ack;
}

/*********************
 * RdmaRxQueuePair
 ********************/
TypeId RdmaRxQueuePair::GetTypeId (void)
{
	static TypeId tid = TypeId ("ns3::RdmaRxQueuePair")
		.SetParent<Object> ()
		;
	return tid;
}

RdmaRxQueuePair::RdmaRxQueuePair(){
	sip = dip = sport = dport = 0;
	m_ipid = 0;
	ReceiverNextExpectedSeq = 0;
	m_nackTimer = Time(0);
	m_milestone_rx = 0;
	m_lastNACK = 0;
	m_received_last_psn_packet = false;
}

uint32_t RdmaRxQueuePair::GetHash(void){
	union{
		struct {
			uint32_t sip, dip;
			uint16_t sport, dport;
		};
		char c[12];
	} buf;
	buf.sip = sip;
	buf.dip = dip;
	buf.sport = sport;
	buf.dport = dport;
	return Hash32(buf.c, 12);
}

bool RdmaRxQueuePair::ReceivedAll(uint32_t payload){
	if (!m_received_last_psn_packet) return false;
	uint32_t expected = 0;
	for (auto it:m_received_psn){
//		std::cout << it << std::endl;
		if (expected != it) return false;
		expected += payload;
	}
	return true;
}

/*********************
 * RdmaQueuePairGroup
 ********************/
TypeId RdmaQueuePairGroup::GetTypeId (void)
{
	static TypeId tid = TypeId ("ns3::RdmaQueuePairGroup")
		.SetParent<Object> ()
		;
	return tid;
}

RdmaQueuePairGroup::RdmaQueuePairGroup(void){
}

uint32_t RdmaQueuePairGroup::GetN(void){
	return m_qps.size();
}

Ptr<RdmaQueuePair> RdmaQueuePairGroup::Get(uint32_t idx){
	return m_qps[idx];
}

Ptr<RdmaQueuePair> RdmaQueuePairGroup::operator[](uint32_t idx){
	return m_qps[idx];
}

void RdmaQueuePairGroup::AddQp(Ptr<RdmaQueuePair> qp){
	m_qps.push_back(qp);
}

#if 0
void RdmaQueuePairGroup::AddRxQp(Ptr<RdmaRxQueuePair> rxQp){
	m_rxQps.push_back(rxQp);
}
#endif

void RdmaQueuePairGroup::Clear(void){
	m_qps.clear();
}

}
