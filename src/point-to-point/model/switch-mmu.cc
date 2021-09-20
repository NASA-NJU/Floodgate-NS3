#include <iostream>
#include <fstream>
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/object-vector.h"
#include "ns3/uinteger.h"
#include "ns3/log.h"
#include "ns3/assert.h"
#include "ns3/global-value.h"
#include "ns3/boolean.h"
#include "ns3/broadcom-node.h"
#include "ns3/random-variable.h"
#include "ns3/switch-mmu.h"
#include "ns3/hash-functions.h"
#include "ns3/broadcom-egress-queue.h"
#include <assert.h>
#include <list>
#include "limits.h"

NS_LOG_COMPONENT_DEFINE("SwitchMmu");
namespace ns3 {
#include "ns3/broadcom-egress-queue.h"
	uint32_t SwitchMmu::max_egress_queue_bytes = 0;
	uint64_t SwitchMmu::max_nxt_data_psn = 0;
	uint32_t SwitchMmu::sentCN = 0;
	uint32_t SwitchMmu::switch_byte_credit_counter = 0;
	uint32_t SwitchMmu::switch_timeout_credit_counter = 0;

	TypeId SwitchMmu::GetTypeId(void){
		static TypeId tid = TypeId("ns3::SwitchMmu")
			.SetParent<Object>()
			.AddConstructor<SwitchMmu>();
		return tid;
	}

	SwitchMmu::SwitchMmu(void){
		buffer_size = 12 * 1024 * 1024;
		reserve = 4 * 1024;
		resume_offset = 3 * 1024;

		// headroom
		shared_used_bytes = 0;
		memset(hdrm_bytes, 0, sizeof(hdrm_bytes));
		memset(ingress_bytes, 0, sizeof(ingress_bytes));
		memset(paused, 0, sizeof(paused));
		memset(pfc_mode, PFC_ON_QUEUE, sizeof(pfc_mode));	// use traditional PFC by default
		memset(pfc_th_static, 0, sizeof(pfc_th_static));	// use dynamic threshold by default
		memset(m_tx_data, 0, sizeof(m_tx_data));
		memset(m_tx_ctrl, 0, sizeof(m_tx_ctrl));
		memset(m_tx_switchACK, 0, sizeof(m_tx_switchACK));
		memset(egress_bytes, 0, sizeof(egress_bytes));
		memset(m_nxt_data_psn, 0, sizeof(m_nxt_data_psn));
		memset(m_rcv_ack_psn, 0, sizeof(m_rcv_ack_psn));
		memset(m_rcv_data_psn, 0, sizeof(m_rcv_data_psn));
		memset(m_rcv_data, 0, sizeof(m_rcv_data));
		memset(m_rcv_ctrl, 0, sizeof(m_rcv_ctrl));
		memset(m_rcv_switchACK, 0, sizeof(m_rcv_switchACK));
		memset(m_nxt_ack_psn, 0, sizeof(m_nxt_ack_psn));
		memset(m_lst_ack_psn, 0, sizeof(m_lst_ack_psn));

		// floodgate
		for (uint32_t i = 0; i < Settings::SWITCHSCALE; ++i){
			m_ingressDstCredits[i].clear();
			for (uint32_t j = 0; j < Settings::host_num; ++j){
				m_lstrcv_ack_time[i][j] = Time(0);
				m_ingressLastTime[i][j] = Time(0);
			}
		}
	}
	bool SwitchMmu::CheckIngressAdmission(uint32_t port, uint32_t qIndex, uint32_t psize){
		if (psize + hdrm_bytes[port][qIndex] > headroom[port] && psize + GetSharedUsed(port, qIndex) > GetPfcThreshold(port)){
			printf("%lu %u Drop: queue:%u,%u: Headroom full\n", Simulator::Now().GetTimeStep(), node_id, port, qIndex);
			for (uint32_t i = 1; i < 64; i++)
				printf("(%u,%u)", hdrm_bytes[i][qIndex], ingress_bytes[i][qIndex]);
			printf("\n");
			return false;
		}
		return true;
	}
	bool SwitchMmu::CheckEgressAdmission(uint32_t port, uint32_t qIndex, uint32_t psize){
		return true;
	}
	void SwitchMmu::UpdateIngressAdmission(uint32_t port, uint32_t qIndex, uint32_t psize){
		uint32_t new_bytes = ingress_bytes[port][qIndex] + psize;
		if (new_bytes <= reserve){
			ingress_bytes[port][qIndex] += psize;
		}else {
			uint32_t thresh = GetPfcThreshold(port);
			if (new_bytes - reserve > thresh){
				hdrm_bytes[port][qIndex] += psize;
			}else {
				ingress_bytes[port][qIndex] += psize;
				shared_used_bytes += std::min(psize, new_bytes - reserve);
			}
		}
	}
	void SwitchMmu::UpdateEgressAdmission(uint32_t port, uint32_t qIndex, uint32_t psize){
		egress_bytes[port][qIndex] += psize;
	}
	void SwitchMmu::RemoveFromIngressAdmission(uint32_t port, uint32_t qIndex, uint32_t psize){
		uint32_t from_hdrm = std::min(hdrm_bytes[port][qIndex], psize);
		uint32_t from_shared = std::min(psize - from_hdrm, ingress_bytes[port][qIndex] > reserve ? ingress_bytes[port][qIndex] - reserve : 0);
		hdrm_bytes[port][qIndex] -= from_hdrm;
		ingress_bytes[port][qIndex] -= psize - from_hdrm;
		shared_used_bytes -= from_shared;
	}
	void SwitchMmu::RemoveFromEgressAdmission(uint32_t port, uint32_t qIndex, uint32_t psize){
		egress_bytes[port][qIndex] -= psize;
	}
	bool SwitchMmu::CheckShouldPause(uint32_t port, uint32_t qIndex){
		return !paused[port][qIndex] && (hdrm_bytes[port][qIndex] > 0 || GetSharedUsed(port, qIndex) >= GetPfcThreshold(port));
	}
	bool SwitchMmu::CheckShouldResume(uint32_t port, uint32_t qIndex){
		if (!paused[port][qIndex])
			return false;
		uint32_t shared_used = GetSharedUsed(port, qIndex);
		return hdrm_bytes[port][qIndex] == 0 && (shared_used == 0 || shared_used + resume_offset <= GetPfcThreshold(port));
	}
	void SwitchMmu::SetPause(uint32_t port, uint32_t qIndex){
		paused[port][qIndex] = true;
	}
	void SwitchMmu::SetResume(uint32_t port, uint32_t qIndex){
		paused[port][qIndex] = false;
	}

	uint32_t SwitchMmu::GetPfcThreshold(uint32_t port){
		if (pfc_mode[port] == PFC_OFF)
			return INT_MAX;
		if (pfc_th_static[port] > 0)
			return pfc_th_static[port];
		return (buffer_size - total_hdrm - total_rsrv - shared_used_bytes) >> pfc_a_shift[port];

	}
	uint32_t SwitchMmu::GetSharedUsed(uint32_t port, uint32_t qIndex){
		uint32_t used = ingress_bytes[port][qIndex];
		return used > reserve ? used - reserve : 0;
	}
	bool SwitchMmu::ShouldSendCN(uint32_t ifindex, uint32_t qIndex){
		uint32_t bytes = egress_bytes[ifindex][qIndex];
		max_egress_queue_bytes = std::max(max_egress_queue_bytes, bytes);

		if (qIndex == 0)
			return false;
		if (bytes > kmax[ifindex]){
			++sentCN;
			return true;
		}
		if (bytes > kmin[ifindex]){
			double p = pmax[ifindex] * double(bytes - kmin[ifindex]) / (kmax[ifindex] - kmin[ifindex]);
			if (UniformVariable(0, 1).GetValue() < p){
				++sentCN;	
				return true;
			}
		}
		return false;
	}
	void SwitchMmu::ConfigEcn(uint32_t port, uint32_t _kmin, uint32_t _kmax, double _pmax){
		kmin[port] = _kmin * 1000;
		kmax[port] = _kmax * 1000;
		pmax[port] = _pmax;
	}
	void SwitchMmu::ConfigHdrm(uint32_t port, uint32_t size){
		headroom[port] = size;
	}
	void SwitchMmu::ConfigNPort(uint32_t n_port){
		total_hdrm = 0;
		total_rsrv = 0;
		for (uint32_t i = 1; i <= n_port; i++){
			total_hdrm += headroom[i];
			total_rsrv += reserve;
		}
	}
	void SwitchMmu::ConfigBufferSize(uint32_t size){
		buffer_size = size;
	}

	/*----------------------------Floodgate--------------------------------*/
	std::ofstream SwitchMmu::win_out;

	void SwitchMmu::ConfigVOQGroup(uint32_t group_id, uint32_t voq_limit, bool dynamic_hash){
		if (m_voqGroups.find(group_id) == m_voqGroups.end()){
			Ptr<VOQGroup> voqGroup = Create<VOQGroup>(node_id, group_id, voq_limit, dynamic_hash);
			voqGroup->m_checkWinCallback = MakeCallback(&SwitchMmu::CheckDstWin, this);
			voqGroup->m_dequeueCallback = MakeCallback(&SwitchMmu::VOQDequeueCallback, this);
			m_voqGroups[group_id] = voqGroup;
		}else{
			m_voqGroups[group_id]->m_voq_limit = voq_limit;
			m_voqGroups[group_id]->m_dynamic_hash = dynamic_hash;
		}
	}

	void SwitchMmu::ConfigDst(uint32_t dst, uint32_t group_id){
		assert(group_id == (uint32_t)-1 || m_voqGroups.find(group_id) != m_voqGroups.end());	// make sure group_id makes sense
		m_dst2group[dst] = group_id;
	}

	bool SwitchMmu::ShouldIgnore(uint32_t dst){
		assert(m_dst2group.find(dst) != m_dst2group.end());
		return m_dst2group[dst] == (uint32_t)-1;
	}

	/**
	 * Register destination if not yet
	 * (i.e. assign runtime switch window)
	 */
	void SwitchMmu::EnsureRegisteredWin(uint32_t dst){
		if (m_wins.find(dst) == m_wins.end()){
			if (Settings::switch_ack_mode != Settings::HOST_PER_PACKET_ACK && Settings::reset_only_ToR_switch_win)
				dst = Settings::host_num + dst/Settings::host_per_rack;	// dst-ToR id

			std::map<std::pair<uint32_t, uint32_t>, uint32_t>::iterator it = VOQ::BDPMAP.find(std::make_pair(node_id, dst));
			if (it != VOQ::BDPMAP.end()){
				m_wins[dst] = it->second;
				if (Settings::switch_absolute_psn)
					win_out << node_id << " " << dst << " " << Simulator::Now().GetNanoSeconds() << " " << m_wins[dst] << " " << GetInflightBytes(dst) << std::endl;
				else
					win_out << node_id << " " << dst << " " << Simulator::Now().GetNanoSeconds() << " " << m_wins[dst] << std::endl;
			}else{
				NS_ASSERT("undefined VOQ window");
				assert(false);
			}
		}
	}

	/**
	 * Get VOQ of destination
	 * Create if not exist
	 */
	Ptr<VOQ> SwitchMmu::GetVOQ(uint32_t dst){
		NS_LOG_FUNCTION (this << dst);

		assert(m_dst2group.find(dst) != m_dst2group.end());
		Ptr<VOQGroup> group = m_voqGroups[m_dst2group[dst]];

		return group->GetVOQ(dst);
	}

	uint32_t SwitchMmu::GetBufferDst(uint32_t dst){
		if (m_buffering.find(dst) == m_buffering.end()) return 0;
		return m_buffering[dst];
	}

	uint32_t SwitchMmu::GetInflightBytes(uint32_t dst){
		uint32_t inflight = 0;
		for (uint32_t i = 0; i < Settings::SWITCHSCALE; ++i){
			assert(m_nxt_data_psn[i][dst] >= m_rcv_ack_psn[i][dst]);
			inflight += (m_nxt_data_psn[i][dst] - m_rcv_ack_psn[i][dst]);
		}
		return inflight;
	}

	bool SwitchMmu::CheckDstWin(uint32_t dst, uint32_t pktSize){
		NS_LOG_FUNCTION (this << dst << pktSize);
		EnsureRegisteredWin(dst);
		if (Settings::switch_absolute_psn){
			if (GetInflightBytes(dst) + pktSize > m_wins[dst])
				return false;
			return true;
		}else{
			if (m_wins[dst] < pktSize)
				return false;
			return true;
		}
	}

	void SwitchMmu::RecoverWin(SwitchACKTag acktag, uint32_t dev){
		NS_LOG_FUNCTION (this);
		for (uint32_t i = 0; i < Settings::host_num; ++i){
			assert(Settings::hostId2IpMap.count(i) > 0);
			if (acktag.getACKEntry(i) > 0){
				uint64_t resume_credit = acktag.getACKEntry(i);
				if (Settings::switch_absolute_psn) {
					uint32_t tmp = dev;
					if (Settings::reset_only_ToR_switch_win){
						tmp = i/Settings::host_per_rack;
					}
					if (resume_credit > m_rcv_ack_psn[tmp][i]){
						resume_credit -= m_rcv_ack_psn[tmp][i];
						m_rcv_ack_psn[tmp][i] = acktag.getACKEntry(i);
					}else
						resume_credit = 0;
				}
				UpdateWin(i, resume_credit, dev, true);
			}
		}
	}

	void SwitchMmu::UpdateWin(uint32_t dst, uint32_t pktsize, uint32_t dev, bool is_add){
		NS_LOG_FUNCTION (this << dst << pktsize << is_add);
		UpdateDstWin(dst, pktsize, is_add);
	}

	void SwitchMmu::UpdateDstWin(uint32_t dst, uint32_t pktsize, bool is_add){
		NS_LOG_FUNCTION (this << dst << pktsize << is_add);
		if (ShouldIgnore(dst)) return;
		if (Settings::switch_absolute_psn){
			win_out << this->node_id << " " << dst << " " << Simulator::Now().GetNanoSeconds() << " " << m_wins[dst] << " " << GetInflightBytes(dst) << std::endl;
			return;
		}
		if (is_add){
			assert(m_wins.find(dst) != m_wins.end());
			m_wins[dst] += pktsize;
		}else{
			EnsureRegisteredWin(dst);
			m_wins[dst] -= pktsize;
		}
		win_out << this->node_id << " " << dst << " " << Simulator::Now().GetNanoSeconds() << " " << m_wins[dst] << std::endl;
	}

	/*
	 * when packet is passing to egress, update PSN counter and tag SwitchPSNTag
	 */
	void SwitchMmu::UpdateDataPSN(uint32_t dev, uint32_t dst, Ptr<Packet> packet){
		assert(Settings::switch_absolute_psn);
		uint32_t tmp = dev;
		if (Settings::reset_only_ToR_switch_win) tmp = dst/Settings::host_per_rack;
		// send data with absolute psn
		m_nxt_data_psn[tmp][dst] += packet->GetSize();
		SwitchPSNTag psnTag;
		psnTag.SetPSN(m_nxt_data_psn[tmp][dst]);
		packet->AddPacketTag(psnTag);
		SwitchMmu::max_nxt_data_psn = std::max(m_nxt_data_psn[tmp][dst], SwitchMmu::max_nxt_data_psn);
	}

	/*
	 * (RR schedule)
	 * Dequeue all window-allowed packets
	 */
	void SwitchMmu::CheckAndSendVOQ(){
		NS_LOG_FUNCTION (this);

		// RR schedule
		std::queue<Ptr<VOQ> > dstLeft;
		std::map<uint32_t, uint32_t>::iterator it = m_wins.begin();
		while (it != m_wins.end()){
			if (it->second > 0){
				uint32_t dst = it->first;
				if (ShouldIgnore(dst)) return;
				assert(m_dst2group.find(dst) != m_dst2group.end());
				Ptr<VOQGroup> group = m_voqGroups[m_dst2group[dst]];

				Ptr<VOQ> voq = group->FindVOQ(dst);
				if (!!voq && voq->CheckAndSendOne()){
					dstLeft.push(voq);
				}
			}
			it++;
		}

		while (!dstLeft.empty()){
			Ptr<VOQ> voq = dstLeft.front(); dstLeft.pop();
			if (!!voq && voq->CheckAndSendOne()){
				dstLeft.push(voq);
			}
		}
	}

	/*
	 * Dequeue all window-allowed packets of a specific dst
	 */
	void SwitchMmu::CheckAndSendVOQ(uint32_t dst){
		NS_LOG_FUNCTION (this);

		if (ShouldIgnore(dst)) return;
		assert(m_dst2group.find(dst) != m_dst2group.end());
		Ptr<VOQGroup> group = m_voqGroups[m_dst2group[dst]];

		Ptr<VOQ> voq = group->FindVOQ(dst);
		if (!!voq){
			voq->CheckAndSendAll();
		}else{
			assert(GetBufferDst(dst) == 0);
		}
	}

	uint32_t SwitchMmu::VOQDequeueCallback(uint32_t dst, uint32_t pktsize, uint32_t outDev, Ptr<Packet> pkt){
		NS_LOG_FUNCTION (this << dst << pktsize);

		// when packet is passing to egress, update PSN counter and tag SwitchPSNTag
		if (Settings::switch_absolute_psn) UpdateDataPSN(outDev, dst, pkt);

		UpdateDstWin(dst, pktsize, false);
		UpdateBufferingCount(dst, pktsize, false);
		return m_buffering[dst];
	}

	void SwitchMmu::UpdateBufferingCount(uint32_t dst, uint32_t pktSize, bool isadd){
		NS_LOG_FUNCTION (this << dst << pktSize << isadd);

		if (isadd){
			if (m_buffering.find(dst) == m_buffering.end()){
				m_buffering.insert(std::make_pair(dst, pktSize));
			}else{
				m_buffering[dst] += pktSize;
			}
		}else{
			assert(m_buffering.find(dst) != m_buffering.end());
			m_buffering[dst] -= pktSize;
		}

	}

	void SwitchMmu::UpdateMaxVOQNum(){
		assert(node_id >= Settings::host_num && node_id - Settings::host_num < Settings::SWITCHSCALE);
		uint32_t voq_n = 0;
		std::map<uint32_t, Ptr<VOQGroup> >::iterator it = m_voqGroups.begin();
		while (m_voqGroups.end() != it){
			voq_n += it->second->VOQs.size();
			if (VOQ::node_maxVOQNum[node_id - Settings::host_num].find(it->first) ==
					VOQ::node_maxVOQNum[node_id - Settings::host_num].end())
				VOQ::node_maxVOQNum[node_id - Settings::host_num].insert(std::make_pair(it->first, 0));
			VOQ::node_maxVOQNum[node_id - Settings::host_num][it->first] =
					std::max(VOQ::node_maxVOQNum[node_id - Settings::host_num][it->first], voq_n);
			it++;
		}
		VOQ::maxVOQNum = std::max(VOQ::maxVOQNum, voq_n);

	}

	void SwitchMmu::UpdateMaxActiveDstNum(){
		uint32_t dst_n = 0;
		std::map<uint32_t, uint32_t>::iterator it = m_buffering.begin();
		while (m_buffering.end() != it){
			if (it->second > 0) ++dst_n;
			it++;
		}
		VOQ::maxActiveDstNum = std::max(VOQ::maxActiveDstNum, dst_n);
	}

	void SwitchMmu::AddCreditCounter(uint32_t dev, uint32_t dst, uint32_t bytes){
		assert(dev < Settings::SWITCHSCALE);
		if (m_ingressDstCredits[dev].count(dst) == 0) m_ingressDstCredits[dev][dst] == 0;
		m_ingressDstCredits[dev][dst] += bytes;
	}

	void SwitchMmu::UpdateIngressLastSendTime(uint32_t dev, uint32_t dst){
		assert(dev < Settings::SWITCHSCALE);
		m_ingressLastTime[dev][dst] = Simulator::Now();
		ResetCreditTimer();
	}

	void SwitchMmu::ResetCreditTimer(){
		if (Settings::switch_credit_interval == 0) return;
		m_creditTimer.Cancel();

		Time minTime = Simulator::Now() + Seconds(1);	// the initial value should be large enough
		uint32_t dev = -1;
		uint32_t dst = -1;
		for (uint32_t i = 0; i < Settings::SWITCHSCALE; ++i){
			for (uint32_t j = 0; j < Settings::host_num; ++j){
				if (m_ingressLastTime[i][j] != Time(0) && m_ingressLastTime[i][j] < minTime){
					minTime = m_ingressLastTime[i][j];
					dev = i;
					dst = j;
				}
			}
		}

		if (dev != (uint32_t)-1){
			assert(minTime + MicroSeconds(Settings::switch_credit_interval) >= Simulator::Now());
			Time t = minTime + MicroSeconds(Settings::switch_credit_interval) - Simulator::Now();
			m_creditTimer = Simulator::Schedule(t, &SwitchMmu::CreditTimeout, this, dev, dst);
		}
	}

	void SwitchMmu::CreditTimeout(uint32_t dev, uint32_t dst){
		assert(dev < Settings::SWITCHSCALE && dst < Settings::host_num && Settings::switch_ack_mode != Settings::HOST_PER_PACKET_ACK);
		bool sentCredit = false;
		if (Settings::switch_ack_mode == Settings::SWITCH_INGRESS_CREDIT){
			if (CheckIngressCreditCounter(dev, 1)) {	// send switchACK when there are accumulate credits
				SwitchACKTag acktag = GetSwitchACKTag(dev);
				m_creditIngressTimerCallback(dev, acktag);
				sentCredit = true;
			}
		}else{
			uint64_t credit = GetIngressDstCreditCounter(dev, dst);
			if (credit > 0) {	// send switchACK when there are accumulate credits
				if (Settings::switch_absolute_psn) {
					credit = m_nxt_ack_psn[dev][dst];
				}
				m_creditDstTimerCallback(dev, Settings::hostId2IpMap[dst], 0, credit);
				sentCredit = true;
			}
		}
		if (!sentCredit) {
			UpdateIngressLastSendTime(dev, dst);
		}else{
			++switch_timeout_credit_counter;
		}
	}

	uint32_t SwitchMmu::GetIngressDstCreditCounter(uint32_t dev, uint32_t dst){
		assert(dev < Settings::SWITCHSCALE);
		if (Settings::switch_absolute_psn){
			if (Settings::switch_ack_th_m == 0 || m_buffering[dst] <= m_th_ack){
				assert(m_nxt_ack_psn[dev][dst] >= m_lst_ack_psn[dev][dst]);
				return m_nxt_ack_psn[dev][dst] - m_lst_ack_psn[dev][dst];
			}
		}else{
			if (m_ingressDstCredits[dev].find(dst) != m_ingressDstCredits[dev].end()){
				// check active delay switchACK
				if (Settings::switch_ack_th_m == 0 || m_buffering[dst] <= m_th_ack)
					return m_ingressDstCredits[dev][dst];
			}
		}
		return 0;
	}

	bool SwitchMmu::CheckIngressCreditCounter(uint32_t dev, uint32_t th){
		assert(dev < Settings::SWITCHSCALE);
		uint32_t sum = 0;
		for (uint32_t i = 0; i < Settings::host_num; ++i){
			sum += GetIngressDstCreditCounter(dev, i);
		}
		if (sum >= th) return true;
		return false;
	}

	void SwitchMmu::CleanIngressDstCreditCounter(uint32_t dev, uint32_t dst_id, uint64_t ackPSN){
		if (Settings::switch_absolute_psn){
			m_lst_ack_psn[dev][dst_id] = ackPSN;
		}else{
			assert(m_ingressDstCredits[dev].find(dst_id) != m_ingressDstCredits[dev].end());
			assert(ackPSN == m_ingressDstCredits[dev][dst_id]);
			m_ingressDstCredits[dev][dst_id] = 0;
		}
	}

	void SwitchMmu::CleanIngressCreditCounter(uint32_t dev, SwitchACKTag& acktag){
		assert(dev < Settings::SWITCHSCALE);
		if (Settings::switch_absolute_psn){
			for (uint32_t i = 0; i < Settings::host_num; ++i){
				if (acktag.getACKEntry(i) > 0) m_lst_ack_psn[dev][i] = acktag.getACKEntry(i);
			}
		}else{
			if (Settings::switch_ack_th_m == 0) m_ingressDstCredits[dev].clear();
			else{
				std::map<uint32_t, uint32_t>::iterator it = m_ingressDstCredits[dev].begin();
				while(it != m_ingressDstCredits[dev].end()){
					uint32_t dst = it->first;
					assert(it->second >= acktag.getACKEntry(dst));
					it->second -= acktag.getACKEntry(dst);
					it++;
				}
			}
		}
	}

	SwitchACKTag SwitchMmu::GetSwitchACKTag(uint32_t dev){
		assert(dev < Settings::SWITCHSCALE);
		SwitchACKTag acktag;
		if (Settings::switch_absolute_psn){
			for (uint32_t i = 0; i < Settings::host_num; ++i){
				if (m_nxt_ack_psn[dev][i] > m_lst_ack_psn[dev][i]){
					// check active delay switchACK
					if (Settings::switch_ack_th_m == 0 || m_buffering[i] <= m_th_ack)
						acktag.SetACKEntry(i, m_nxt_ack_psn[dev][i]);
				}
			}
		}else{
			std::map<uint32_t, uint32_t>::iterator it = m_ingressDstCredits[dev].begin();
			while(it != m_ingressDstCredits[dev].end()){
				if (it->second > 0){
					// check active delay switchACK
					if (Settings::switch_ack_th_m == 0 || m_buffering[it->first] <= m_th_ack)
						acktag.SetACKEntry(it->first, it->second);
				}
				it++;
			}
		}
		return acktag;
	}

	SwitchACKTag SwitchMmu::GetDstsSwitchACKTag(uint32_t dev, std::set<uint32_t> dsts){
		assert(dev < Settings::SWITCHSCALE && Settings::switch_ack_mode == Settings::SWITCH_INGRESS_CREDIT && Settings::switch_absolute_psn);
		SwitchACKTag acktag;
		for (uint32_t i = 0; i < Settings::host_num; ++i){
			if (m_nxt_ack_psn[dev][i] > m_lst_ack_psn[dev][i] || dsts.count(i) > 0){	// when SwitchACK dropped, should check dsts.count(i) > 0
				// check active delay switchACK
				if (Settings::switch_ack_th_m == 0 || m_buffering[i] <= m_th_ack)
					acktag.SetACKEntry(i, m_nxt_ack_psn[dev][i]);
			}
		}
		return acktag;
	}

	void SwitchMmu::UpdateSynTime(uint32_t dev, uint32_t dst, SwitchACKTag& acktag){
		if (Settings::switch_absolute_psn == 0 || Settings::switch_syn_timeout_us == 0) return;
		if (Settings::switch_ack_mode == Settings::SWITCH_DST_CREDIT)
			m_lstrcv_ack_time[dev][dst] = Simulator::Now();
		else if (Settings::switch_ack_mode == Settings::SWITCH_INGRESS_CREDIT){
			for (uint32_t i = 0; i < Settings::host_num; ++i){
				if (acktag.getACKEntry(i) > 0) m_lstrcv_ack_time[dev][i] = Simulator::Now();
			}
		}

		if (!m_syn_timeout_event[dev].IsRunning())
			m_syn_timeout_event[dev] = Simulator::Schedule(MicroSeconds(Settings::switch_syn_timeout_us), &SwitchMmu::SynTimeout, this, dev);
	}

	void SwitchMmu::SynTimeout(uint32_t dev){
		m_syn_timeout_event[dev].Cancel();
		SwitchSYNTag syntag = GetSwitchSYNTag(dev);
		if (syntag.GetPacketSize() > 0) m_synTimerCallback(dev, syntag);	// has in-flight data/SwitchACK -> send SYN
		m_syn_timeout_event[dev] = Simulator::Schedule(MicroSeconds(Settings::switch_syn_timeout_us), &SwitchMmu::SynTimeout, this, dev);
	}

	SwitchSYNTag SwitchMmu::GetSwitchSYNTag(uint32_t dev){
		SwitchSYNTag tag;
		for (uint32_t i = 0; i < Settings::host_num; ++i){
			if (m_rcv_ack_psn[dev][i] < m_nxt_data_psn[dev][i] && m_lstrcv_ack_time[dev][i] + MicroSeconds(Settings::switch_syn_timeout_us) >= Simulator::Now()){
				// has in-flight data/SwitchACK && hasn't receive SwitchACK for a period of time
				tag.SetPSNEntry(i, m_rcv_ack_psn[dev][i], m_nxt_data_psn[dev][i]);
			}
		}
		return tag;
	}

	std::set<uint32_t> SwitchMmu::CheckSYN(SwitchSYNTag& syntag, uint32_t dev){
		NS_LOG_FUNCTION (this);
		std::set<uint32_t> dsts;
		for (uint32_t i = 0; i < Settings::host_num; ++i){
			assert(Settings::hostId2IpMap.count(i) > 0);
			if (syntag.GetDataPSNEntry(i) > m_rcv_data_psn[dev][i]){	// has dropped data packets
				m_nxt_ack_psn[dev][i] = syntag.GetDataPSNEntry(i);
				m_rcv_data_psn[dev][i] = syntag.GetDataPSNEntry(i);
			}
			if (syntag.GetACKPSNEntry(i) < m_nxt_ack_psn[dev][i])	// has dropped packets
				dsts.insert(i);
		}
		return dsts;
	}
}
