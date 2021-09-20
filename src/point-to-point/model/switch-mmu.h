#ifndef SWITCH_MMU_H
#define SWITCH_MMU_H

#include <unordered_map>
#include <ns3/node.h>
#include "ns3/voq-group.h"
#include "ns3/settings.h"
#include "ns3/simulator.h"
#include "ns3/broadcom-egress-queue.h"
#include <list>

namespace ns3 {

class Packet;

class SwitchMmu: public Object{
public:
	static const uint32_t pCnt = 257;	// Number of ports used
	static const uint32_t qCnt = 8;	// Number of queues/priorities used

	static TypeId GetTypeId (void);

	SwitchMmu(void);

	bool CheckIngressAdmission(uint32_t port, uint32_t qIndex, uint32_t psize);
	bool CheckEgressAdmission(uint32_t port, uint32_t qIndex, uint32_t psize);
	void UpdateIngressAdmission(uint32_t port, uint32_t qIndex, uint32_t psize);
	void UpdateEgressAdmission(uint32_t port, uint32_t qIndex, uint32_t psize);
	void RemoveFromIngressAdmission(uint32_t port, uint32_t qIndex, uint32_t psize);
	void RemoveFromEgressAdmission(uint32_t port, uint32_t qIndex, uint32_t psize);

	bool CheckShouldPause(uint32_t port, uint32_t qIndex);
	bool CheckShouldResume(uint32_t port, uint32_t qIndex);
	void SetPause(uint32_t port, uint32_t qIndex);
	void SetResume(uint32_t port, uint32_t qIndex);

	uint32_t GetPfcThreshold(uint32_t port);
	uint32_t GetSharedUsed(uint32_t port, uint32_t qIndex);

	bool ShouldSendCN(uint32_t ifindex, uint32_t qIndex);

	void ConfigEcn(uint32_t port, uint32_t _kmin, uint32_t _kmax, double _pmax);
	void ConfigHdrm(uint32_t port, uint32_t size);
	void ConfigNPort(uint32_t n_port);
	void ConfigBufferSize(uint32_t size);

	// config
	uint32_t node_id;
	uint32_t buffer_size;
	uint32_t pfc_a_shift[pCnt];
	uint32_t reserve;
	uint32_t headroom[pCnt];
	uint32_t resume_offset;
	uint32_t kmin[pCnt], kmax[pCnt];
	double pmax[pCnt];
	uint32_t total_hdrm;
	uint32_t total_rsrv;
	enum { PFC_OFF = 0u, PFC_ON_QUEUE = 1u, PFC_ON_DST = 2u};
	uint8_t pfc_mode[pCnt];
	uint32_t pfc_th_static[pCnt];

	// runtime
	uint32_t shared_used_bytes;
	uint32_t egress_bytes[pCnt][qCnt];
	// per-queue/per-dst pause for each port
	uint32_t hdrm_bytes[pCnt][Settings::NODESCALE];
	uint32_t ingress_bytes[pCnt][Settings::NODESCALE];
	uint32_t paused[pCnt][Settings::NODESCALE];

	// statistic
	static uint32_t max_egress_queue_bytes;
	static uint32_t sentCN;

	/*--------------------------Floodgate-------------------------------*/
	static std::ofstream win_out;
	bool m_use_floodgate;

	std::map<uint32_t, uint32_t> m_wins;	// <dst_id, the remaining bytes which could send>
	uint32_t m_th_ack;		// threshold to send SwitchACK
	/*
	 * When use hash, a voq may buffer packets of several dsts,
	 * so we need a table to record the buffering packets of a specific dst.
	 */
	std::map<uint32_t, uint32_t> m_buffering; // <dst_id, the buffering bytes in voq> (can only count)

	std::map<uint32_t, uint32_t> m_dst2group;	// <dst_id, group_id>, configured
	std::map<uint32_t, Ptr<VOQGroup> > m_voqGroups;	// <group_id, group>

	void ConfigVOQGroup(uint32_t group_id, uint32_t voq_limit, bool dynamic_hash);
	void ConfigDst(uint32_t dst, uint32_t group_id);

	Ptr<VOQ> GetVOQ(uint32_t dst);
	void UpdateBufferingCount(uint32_t dst, uint32_t pktSize, bool isadd);
	uint32_t GetBufferDst(uint32_t dst);
	void EnsureRegisteredWin(uint32_t dst);
	bool ShouldIgnore(uint32_t dst);
	bool CheckDstWin(uint32_t dst, uint32_t pktSize);
	uint32_t GetInflightBytes(uint32_t dst);
	bool CheckEgressWin(uint32_t outDev, uint32_t pktSize);
	void UpdateDataPSN(uint32_t inDev, uint32_t dst, Ptr<Packet> pkt);
	void UpdateWin(uint32_t dst, uint32_t pktSize, uint32_t dev, bool isadd);
	void UpdateDstWin(uint32_t dst, uint32_t pktSize, bool isadd);
	void UpdateEgressWin(uint32_t dev, uint32_t pktSize, bool isadd);
	void RecoverWin(SwitchACKTag acktag, uint32_t dev);
	void CheckAndSendVOQ(uint32_t dst);
	void CheckAndSendVOQ();
	void CheckAndSendCache(uint32_t outDev);
	uint32_t VOQDequeueCallback(uint32_t dst, uint32_t pktSize, uint32_t outDev, Ptr<Packet> pkt);

	// for statistic
	void UpdateMaxVOQNum();
	void UpdateMaxActiveDstNum();
	uint64_t m_tx_data[pCnt];
	uint64_t m_tx_ctrl[pCnt];
	uint64_t m_tx_switchACK[pCnt];
	uint64_t m_rcv_data[pCnt];
	uint64_t m_rcv_ctrl[pCnt];
	uint64_t m_rcv_switchACK[pCnt];

	// for switch-ingress-ack mode
	std::map<uint32_t, uint32_t> m_ingressDstCredits[Settings::SWITCHSCALE];	// <dst_id, credit>
	EventId m_creditTimer;
	Time m_ingressLastTime[Settings::SWITCHSCALE][Settings::NODESCALE];		// <upstream/ingress, <dst_id, last_send_credit_time> >
	Callback<void, uint32_t, SwitchACKTag> m_creditIngressTimerCallback;
	Callback<void, uint32_t, uint32_t, uint32_t, uint64_t> m_creditDstTimerCallback;
	// statistics
	static uint32_t switch_byte_credit_counter;
	static uint32_t switch_timeout_credit_counter;

	void UpdateIngressLastSendTime(uint32_t inDev, uint32_t dst);
	void ResetCreditTimer();
	void CreditTimeout(uint32_t inDev, uint32_t dst);
	void AddCreditCounter(uint32_t inDev, uint32_t dst, uint32_t bytes);
	uint32_t GetIngressDstCreditCounter(uint32_t inDev, uint32_t dst);
	bool CheckIngressCreditCounter(uint32_t inDev, uint32_t th);
	void CleanIngressDstCreditCounter(uint32_t inDev, uint32_t dst, uint64_t ackPSN);
	void CleanIngressCreditCounter(uint32_t inDev, SwitchACKTag& acktag);
	SwitchACKTag GetSwitchACKTag(uint32_t inDev);
	SwitchACKTag GetDstsSwitchACKTag(uint32_t inDev, std::set<uint32_t> dsts);

	// for handling packetloss
	// for absolute data/SwitchACK PSN
	static uint64_t max_nxt_data_psn;
	uint64_t m_nxt_data_psn[Settings::SWITCHSCALE][Settings::NODESCALE];	// port as upstream
	uint64_t m_rcv_data_psn[Settings::SWITCHSCALE][Settings::NODESCALE];	// port as downstream
	uint64_t m_rcv_ack_psn[Settings::SWITCHSCALE][Settings::NODESCALE];	// port as upstream
	uint64_t m_nxt_ack_psn[Settings::SWITCHSCALE][Settings::NODESCALE];	// port as downstream
	uint64_t m_lst_ack_psn[Settings::SWITCHSCALE][Settings::NODESCALE];	// port as downstream
	// for syn timeout
	Time m_lstrcv_ack_time[Settings::SWITCHSCALE][Settings::NODESCALE];
	EventId m_syn_timeout_event[Settings::SWITCHSCALE];	// !reset_only_ToR_switch_win: #port, reset_only_ToR_switch_win: #ToR
	Callback<void, uint32_t, SwitchSYNTag> m_synTimerCallback;	// send syn packet
	void UpdateSynTime(uint32_t dev, uint32_t dst, SwitchACKTag& acktag);
	void SynTimeout(uint32_t dev);
	SwitchSYNTag GetSwitchSYNTag(uint32_t dev);
	std::set<uint32_t> CheckSYN(SwitchSYNTag& syntag, uint32_t dev);
};

} /* namespace ns3 */

#endif /* SWITCH_MMU_H */

