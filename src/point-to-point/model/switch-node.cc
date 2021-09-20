#include "ns3/ipv4.h"
#include "ns3/packet.h"
#include "ns3/ipv4-header.h"
#include "ns3/pause-header.h"
#include "ns3/flow-id-tag.h"
#include "ns3/boolean.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"
#include "ns3/switch-node.h"
#include "ns3/qbb-net-device.h"
#include "ns3/ppp-header.h"
#include "ns3/int-header.h"
#include "ns3/settings.h"
#include "ns3/broadcom-egress-queue.h"
#include "ns3/log.h"
#include "ns3/random-variable.h"
#include <assert.h>

namespace ns3 {

TypeId SwitchNode::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::SwitchNode")
    .SetParent<Node> ()
    .AddConstructor<SwitchNode> ()
	.AddAttribute("EcnEnabled",
			"Enable ECN marking.",
			BooleanValue(false),
			MakeBooleanAccessor(&SwitchNode::m_ecnEnabled),
			MakeBooleanChecker())
	.AddAttribute("CcMode",
			"CC mode.",
			UintegerValue(0),
			MakeUintegerAccessor(&SwitchNode::m_ccMode),
			MakeUintegerChecker<uint32_t>())
	.AddAttribute("AckHighPrio",
			"Set high priority for ACK/NACK or not",
			UintegerValue(0),
			MakeUintegerAccessor(&SwitchNode::m_ackHighPrio),
			MakeUintegerChecker<uint32_t>())
	.AddAttribute("OutputRealTimeBuffer",
			"Output realTime buffer.",
			BooleanValue(true),
			MakeBooleanAccessor(&SwitchNode::output_realtime_buffer),
			MakeBooleanChecker())
	.AddTraceSource ("RealtimeQueueLength", "print realtime queue length",
			MakeTraceSourceAccessor (&SwitchNode::m_traceRealtimeQueue))
	.AddTraceSource ("RealtimeSwitchBw", "print realtime switch throughput",
			MakeTraceSourceAccessor (&SwitchNode::m_traceRealtimeSwitchBw))
  ;
  return tid;
}

SwitchNode::SwitchNode(){
	m_ecmpSeed = m_id;
	m_node_type = 1;
	m_isToR = false;
	m_isCore = false;
	m_mmu = CreateObject<SwitchMmu>();
	m_mmu->m_creditIngressTimerCallback = MakeCallback(&SwitchNode::SendAccSwitchACK, this);
	m_mmu->m_creditDstTimerCallback = MakeCallback(&SwitchNode::SendSwitchACK, this);
	m_mmu->m_synTimerCallback = MakeCallback(&SwitchNode::SendSYN, this);
	m_drill_candidate = 2;	// for drill: power of two

	for (uint32_t i = 0; i < pCnt; i++)
		for (uint32_t j = 0; j < pCnt; j++)
			for (uint32_t k = 0; k < qCnt; k++)
				m_bytes[i][j][k] = 0;
	for (uint32_t i = 0; i < pCnt; i++)
		m_txBytes[i] = 0;

	Simulator::Schedule(Seconds(2), &SwitchNode::PrintQlength, this);
}

void SwitchNode::ResetQueueStatisticsInterval(){

	// only used when DRILL_LOAD_INTERVAL_SENT for now
	if (Settings::drill_load_mode != Settings::DRILL_LOAD_INTERVAL_SENT) return;

	for (uint32_t i = 0; i < m_devices.size(); i++){
		Ptr<QbbNetDevice> device = DynamicCast<QbbNetDevice>(m_devices[i]);
		if (!!device && !!device->GetQueue())
		{
			device->GetQueue()->ResetStatistics();
		}
	}
	m_eventResetQueueStatitics.Cancel();
	m_eventResetQueueStatitics = Simulator::Schedule(MicroSeconds(Settings::queue_statistic_interval_us), &SwitchNode::ResetQueueStatisticsInterval, this);
}

/*---------------------------------DRILL----------------------------------------------*/
uint32_t SwitchNode::CalculateInterfaceLoad (uint32_t interface){
	Ptr<QbbNetDevice> device = DynamicCast<QbbNetDevice>(m_devices[interface]);
	assert(!!device && !!device->GetQueue());
	if (Settings::drill_load_mode == Settings::DRILL_LOAD_INTERVAL_SENT){
		return device->GetQueue()->GetTotalReceivedBytes();
	}else{
		return device->GetQueue()->GetNBytesTotal();
	}
}

uint32_t SwitchNode::SelectEgressByDRILL(std::vector<int>& nexthops, CustomHeader &ch){

	uint32_t leastLoadInterface = 0;
	uint32_t leastLoad = std::numeric_limits<uint32_t>::max ();

	std::random_shuffle (nexthops.begin (), nexthops.end ());

	std::map<uint32_t, uint32_t>::iterator itr = m_previousBestInterfaceMap.find (ch.dip);

	if (itr != m_previousBestInterfaceMap.end ())
	{
	  leastLoadInterface = itr->second;
	  leastLoad = CalculateInterfaceLoad (itr->second);
	}

	uint32_t sampleNum = m_drill_candidate < nexthops.size () ? m_drill_candidate : nexthops.size ();

	for (uint32_t samplePort = 0; samplePort < sampleNum; samplePort ++)
	{
	  uint32_t sampleLoad = CalculateInterfaceLoad (nexthops[samplePort]);
	  if (sampleLoad < leastLoad)
	  {
		leastLoad = sampleLoad;
		leastLoadInterface = nexthops[samplePort];
	  }
	}

	m_previousBestInterfaceMap[ch.dip] = leastLoadInterface;
	return leastLoadInterface;
}

int SwitchNode::GetOutDev(Ptr<const Packet> p, CustomHeader &ch){
	// look up entries
	auto entry = m_rtTable.find(ch.dip);

	// no matching entry
	if (entry == m_rtTable.end())
		return -1;

	// entry found
	auto &nexthops = entry->second;

	// pick one next hop based on hash
	uint32_t egress = 0;
	if (Settings::routing_mode == Settings::ROUTING_DRILL){
		egress = SelectEgressByDRILL(nexthops, ch);
	}
	else{
		uint32_t idx = 0;
		if (Settings::qp_mode){
			union {
				uint8_t u8[4];
				uint32_t u32[1];
			} buf;
			QPTag qptag;
			p->PeekPacketTag(qptag);
			buf.u32[0] = qptag.GetQPID();
			idx = EcmpHash(buf.u8, 4, m_ecmpSeed);
		}else{
			union {
				uint8_t u8[4+4+2+2];
				uint32_t u32[3];
			} buf;
			buf.u32[0] = ch.sip;
			buf.u32[1] = ch.dip;
			if (ch.l3Prot == 0x6)
				buf.u32[2] = ch.tcp.sport | ((uint32_t)ch.tcp.dport << 16);
			else if (ch.l3Prot == 0x11)
				buf.u32[2] = ch.udp.sport | ((uint32_t)ch.udp.dport << 16);
			else if (ch.l3Prot == 0xFC || ch.l3Prot == 0xFD)
				buf.u32[2] = ch.ack.sport | ((uint32_t)ch.ack.dport << 16);
			idx = EcmpHash(buf.u8, 12, m_ecmpSeed);
		}
		egress = nexthops[idx % nexthops.size()];
	}

	/**
	 * Go symmetric route if has set it
	 */
	if (Settings::symmetic_routing_mode == Settings::SYMMETRIC_RECEIVER){
		SymmetricRoutingTag symmetrictag;
		if (p->PeekPacketTag(symmetrictag)){
			Ptr<Packet> packet = ConstCast<Packet>(p);
			packet->RemovePacketTag(symmetrictag);
			egress = symmetrictag.getReceiverLeafIngress();
			packet->AddPacketTag(symmetrictag);
			p = packet;
			return egress;
		}
	}

	return egress;
}

void SwitchNode::CheckAndSendPfc(uint32_t inDev, uint32_t qIndex){
	Ptr<QbbNetDevice> device = DynamicCast<QbbNetDevice>(m_devices[inDev]);
	if (m_mmu->CheckShouldPause(inDev, qIndex)){
		device->SendPfc(qIndex, 0);
		m_mmu->SetPause(inDev, qIndex);
	}
}
void SwitchNode::CheckAndSendResume(uint32_t inDev, uint32_t qIndex){
	Ptr<QbbNetDevice> device = DynamicCast<QbbNetDevice>(m_devices[inDev]);
	if (m_mmu->CheckShouldResume(inDev, qIndex)){
		device->SendPfc(qIndex, 1);
		m_mmu->SetResume(inDev, qIndex);
	}
}

void SwitchNode::SendToDev(Ptr<Packet>p, CustomHeader &ch){

	// call ResetQueueStatitics if necessary
	if (!m_eventResetQueueStatitics.IsRunning()) ResetQueueStatisticsInterval();

	FlowIdTag t;
	p->PeekPacketTag(t);
	uint32_t inDev = t.GetFlowId();

	// update statistics
	if (ch.l3Prot == 0x11){
		m_mmu->m_rcv_data[inDev] += p->GetSize();
	}else{
		m_mmu->m_rcv_ctrl[inDev] += p->GetSize();
		if (ch.l3Prot == 0xFB){
			m_mmu->m_rcv_switchACK[inDev] += p->GetSize();
		}
	}

	SwitchSYNTag syntag;
	if (m_mmu->m_use_floodgate && p->PeekPacketTag(syntag)){
		ReceiveSYN(syntag, ch, inDev);
		return;
	}

	SwitchPSNTag psntag;
	if (Settings::switch_absolute_psn && p->PeekPacketTag(psntag)){

		// remove and copy PSNTag as IngressPSNTag
		p->RemovePacketTag(psntag);
		SwitchIngressPSNTag ingress_psntag;
		ingress_psntag.SetPSN(psntag.GetPSN());
		p->AddPacketTag(ingress_psntag);

		// update rcv_data_psn
		uint64_t data_psn = psntag.GetPSN();
		uint32_t dst_id = Settings::hostIp2IdMap[ch.dip];
		uint32_t tmp = inDev;
		if (Settings::reset_only_ToR_switch_win) tmp = Settings::hostIp2IdMap[ch.sip]/Settings::host_per_rack;
		if (data_psn > m_mmu->m_rcv_data_psn[tmp][dst_id]){
			m_mmu->m_rcv_data_psn[tmp][dst_id] = data_psn;
		}
	}

	SwitchACKTag acktag;
	if (m_mmu->m_use_floodgate && p->PeekPacketTag(acktag)){
		if (ResumeVOQWin(acktag, ch, inDev)) return;
	}

	int idx = GetOutDev(p, ch);
	if (idx >= 0){
		NS_ASSERT_MSG(m_devices[idx]->IsLinkUp(), "The routing table look up should return link that is up");

		if (Settings::symmetic_routing_mode != Settings::SYMMETRIC_OFF)
			TagForSymmeticRouting(p, ch, inDev);

		// determine the qIndex
		uint32_t qIndex;
		if (ch.l3Prot == 0xFA || ch.l3Prot == 0xFB || ch.l3Prot == 0xFF || ch.l3Prot == 0xFE || (m_ackHighPrio && (ch.l3Prot == 0xFD || ch.l3Prot == 0xFC))){  // Switch-PSN or Switch-ACK or QCN or PFC or NACK, go highest priority
			qIndex = 0;
		}else{
			qIndex = (ch.l3Prot == 0x06 ? 1 : ch.udp.pg); // if TCP, put to queue 1
		}

		if (qIndex != 0){ //not highest priority
			uint32_t pfc_fine = qIndex;
			if (m_mmu->pfc_mode[inDev] == SwitchMmu::PFC_ON_DST) pfc_fine = Settings::hostIp2IdMap[ch.dip];
			if (m_mmu->CheckIngressAdmission(inDev, pfc_fine, p->GetSize()) && m_mmu->CheckEgressAdmission(idx, qIndex, p->GetSize())){			// Admission control
				m_mmu->UpdateIngressAdmission(inDev, pfc_fine, p->GetSize());
				m_mmu->UpdateEgressAdmission(idx, qIndex, p->GetSize());
			}else{
				Settings::drop_packets++;
				QPTag qptag;
				if (p->PeekPacketTag(qptag))
					std::cout << "Drop:" << qptag.GetQPID() << " " << qptag.GetMsgSeq() << std::endl;
				else{
					std::cout << "Drop!! qIndex:" << qIndex << " pSize:" << p->GetSize() << std::endl;
				}
				return; // Drop
			}
			CheckAndSendPfc(inDev, pfc_fine);
		}
		m_bytes[inDev][idx][qIndex] += p->GetSize();

		/**
		 * When use Floodgate
		 * --> if it's a data packet,
		 * --> only when has enough remaining window,
		 * --> send this packet to egress port.
		 */
		if (!m_mmu->m_use_floodgate || CheckVOQWin(p, ch, inDev, idx, qIndex)){
			m_devices[idx]->SwitchSend(qIndex, p, ch);
		}

		DoStatistics();
	}
}

void SwitchNode::DoSwitchSend(Ptr<Packet>p, CustomHeader &ch, uint32_t outDev, uint32_t qIndex){
	m_devices[outDev]->SwitchSend(qIndex, p, ch);
}

uint32_t SwitchNode::EcmpHash(const uint8_t* key, size_t len, uint32_t seed) {
  uint32_t h = seed;
  if (len > 3) {
    const uint32_t* key_x4 = (const uint32_t*) key;
    size_t i = len >> 2;
    do {
      uint32_t k = *key_x4++;
      k *= 0xcc9e2d51;
      k = (k << 15) | (k >> 17);
      k *= 0x1b873593;
      h ^= k;
      h = (h << 13) | (h >> 19);
      h += (h << 2) + 0xe6546b64;
    } while (--i);
    key = (const uint8_t*) key_x4;
  }
  if (len & 3) {
    size_t i = len & 3;
    uint32_t k = 0;
    key = &key[i - 1];
    do {
      k <<= 8;
      k |= *key--;
    } while (--i);
    k *= 0xcc9e2d51;
    k = (k << 15) | (k >> 17);
    k *= 0x1b873593;
    h ^= k;
  }
  h ^= len;
  h ^= h >> 16;
  h *= 0x85ebca6b;
  h ^= h >> 13;
  h *= 0xc2b2ae35;
  h ^= h >> 16;
  return h;
}

void SwitchNode::SetEcmpSeed(uint32_t seed){
	m_ecmpSeed = seed;
}

void SwitchNode::AddTableEntry(Ipv4Address &dstAddr, uint32_t intf_idx){
	uint32_t dip = dstAddr.Get();
	m_rtTable[dip].push_back(intf_idx);
}

void SwitchNode::ClearTable(){
	m_rtTable.clear();
}

// This function can only be called in switch mode
bool SwitchNode::SwitchReceiveFromDevice(Ptr<NetDevice> device, Ptr<Packet> packet, CustomHeader &ch){
	SendToDev(packet, ch);
	return true;
}

void SwitchNode::SwitchNotifyDequeue(uint32_t ifIndex, uint32_t inDev, uint32_t qIndex, Ptr<Packet> p){

	CustomHeader ch(CustomHeader::L2_Header | CustomHeader::L3_Header | CustomHeader::L4_Header);
	ch.getInt = 1; // parse INT header
	p->PeekHeader(ch);

	if (qIndex != 0){

		uint32_t pfc_fine = qIndex;
		if (m_mmu->pfc_mode[inDev] == SwitchMmu::PFC_ON_DST) pfc_fine = Settings::hostIp2IdMap[ch.dip];

		m_mmu->RemoveFromIngressAdmission(inDev, pfc_fine, p->GetSize());
		m_mmu->RemoveFromEgressAdmission(ifIndex, qIndex, p->GetSize());
		m_bytes[inDev][ifIndex][qIndex] -= p->GetSize();

		if (m_ecnEnabled){
			bool egressCongested = m_mmu->ShouldSendCN(ifIndex, qIndex);
			if (egressCongested){
				PppHeader ppp;
				Ipv4Header h;
				p->RemoveHeader(ppp);
				p->RemoveHeader(h);
				h.SetEcn((Ipv4Header::EcnType)0x03);
				p->AddHeader(h);
				p->AddHeader(ppp);
			}
		}

		CheckAndSendResume(inDev, pfc_fine);
	}

	uint8_t* buf = p->GetBuffer();
	if (buf[PppHeader::GetStaticSize() + 9] == 0x11){ // udp packet
		IntHeader *ih = (IntHeader*)&buf[PppHeader::GetStaticSize() + 20 + 8 + 6]; // ppp, ip, udp, SeqTs, INT
		Ptr<QbbNetDevice> dev = DynamicCast<QbbNetDevice>(m_devices[ifIndex]);
		if (m_ccMode == 3){ // HPCC
			if (m_mmu->m_use_floodgate && m_mmu->GetBufferDst(ch.dip) > 0)	// only congested flows consider voq buffer
				ih->PushHop(Simulator::Now().GetTimeStep(), m_txBytes[ifIndex], m_mmu->egress_bytes[ifIndex][qIndex], dev->GetDataRate().GetBitRate());
			else
				ih->PushHop(Simulator::Now().GetTimeStep(), m_txBytes[ifIndex], dev->GetQueue()->GetNBytesTotal(), dev->GetDataRate().GetBitRate());
		}
	}
	m_txBytes[ifIndex] += p->GetSize();

	if (m_mmu->m_use_floodgate){
		if (ch.l3Prot == 0x11){	// send an udp

			// update statistics
			m_mmu->m_tx_data[ifIndex] += p->GetSize();

			bool ignore = m_mmu->ShouldIgnore(Settings::hostIp2IdMap[ch.sip]);	// it is srcToR or not
			if (!m_isToR || !ignore){	// downstream: not the srcToR which would check whether send SwitchACK

				uint32_t dst_id = Settings::hostIp2IdMap[ch.dip];

				uint32_t creditId = inDev;
				if (Settings::reset_only_ToR_switch_win){
					creditId = Settings::hostIp2IdMap[ch.sip]/Settings::host_per_rack;
				}
				if (Settings::switch_absolute_psn){
					// as downstream -> update nxt_ack_psn
					SwitchIngressPSNTag ingress_psntag;
					assert(p->RemovePacketTag(ingress_psntag));
					m_mmu->m_nxt_ack_psn[creditId][dst_id] = std::max(ingress_psntag.GetPSN(), m_mmu->m_nxt_ack_psn[creditId][dst_id]);
#if DEBUG_MODE
			if (Settings::hostIp2IdMap[ch.dip] == DEBUG_DST_ID){
				std::cout << Simulator::Now() << " " << m_id << " dequeue UDP "
						<< dst_id << " downstream " << psntag.GetPSN() <<
						" " << m_mmu->m_nxt_ack_psn[creditId][dst_id] << std::endl;
			}
#endif
				}else
					m_mmu->AddCreditCounter(creditId, dst_id, p->GetSize());

				/*
				 * check and send SwitchACK
				 */
				uint32_t upstream_num = 1;
				uint32_t bc = SwitchMmu::switch_byte_credit_counter;
				if (Settings::switch_ack_th_m > 0)	{	// When use delayACK, check all up-streams
					if (Settings::reset_only_ToR_switch_win){
						upstream_num = Settings::tor_num;
					}else{
						upstream_num = m_devices.size();
					}
				}
				for (uint32_t i = 0; i < upstream_num; ++i, creditId = (creditId+1) % upstream_num){
					if (Settings::switch_ack_mode == Settings::SWITCH_INGRESS_CREDIT){
						if (m_mmu->m_ingressLastTime[creditId][0] == Time(0)) m_mmu->UpdateIngressLastSendTime(creditId, 0);	// initialize ingress_lasttime
						if (m_mmu->CheckIngressCreditCounter(creditId, Settings::switch_byte_counter)) {
							SwitchACKTag acktag = m_mmu->GetSwitchACKTag(creditId);
							SendAccSwitchACK(creditId, acktag);
							++SwitchMmu::switch_byte_credit_counter;
						}
					}else if(Settings::switch_ack_mode == Settings::SWITCH_DST_CREDIT){
						if (m_mmu->m_ingressLastTime[creditId][dst_id] == Time(0)) m_mmu->UpdateIngressLastSendTime(creditId, dst_id);	// initialize ingress_lasttime
						uint64_t creditPippyback = m_mmu->GetIngressDstCreditCounter(creditId, dst_id);
						if (creditPippyback >= Settings::switch_byte_counter) {
							if (Settings::switch_absolute_psn) {
								creditPippyback = m_mmu->m_nxt_ack_psn[creditId][dst_id];
							}
							uint32_t ack_dst_ip = ch.sip;
							if (Settings::reset_only_ToR_switch_win)
								ack_dst_ip = Settings::hostId2IpMap[creditId*Settings::host_per_rack];
							SendSwitchACK(creditId, ch.dip, ack_dst_ip, creditPippyback);
							++SwitchMmu::switch_byte_credit_counter;
						}
					}
				}
				/*
				 * when no credits were sent, check K and T
				 * -> if K > 1 && T == 0, the last several(<K) credits may always cannot send back.
				 * -> when no packets in VOQ buffer, send credits back
				 */
				if (bc == SwitchMmu::switch_byte_credit_counter
						&& Settings::switch_byte_counter > 1 && Settings::switch_credit_interval == 0
						&& m_mmu->GetBufferDst(dst_id) == 0){
					// check all up-streams
					if (Settings::reset_only_ToR_switch_win){
						upstream_num = Settings::tor_num;
					}else{
						upstream_num = m_devices.size();
					}
					for (uint32_t i = 0; i < upstream_num; ++i, creditId = (creditId+1) % upstream_num){
						if (Settings::switch_ack_mode == Settings::SWITCH_INGRESS_CREDIT){
							if (m_mmu->CheckIngressCreditCounter(creditId, 1)) {
								SwitchACKTag acktag = m_mmu->GetSwitchACKTag(creditId);
								SendAccSwitchACK(creditId, acktag);
								++SwitchMmu::switch_byte_credit_counter;
							}
						}else if(Settings::switch_ack_mode == Settings::SWITCH_DST_CREDIT){
							uint64_t creditPippyback = m_mmu->GetIngressDstCreditCounter(creditId, dst_id);
							if (creditPippyback >= 1) {
								if (Settings::switch_absolute_psn) {
									creditPippyback = m_mmu->m_nxt_ack_psn[creditId][dst_id];
								}
								uint32_t ack_dst_ip = ch.sip;
								if (Settings::reset_only_ToR_switch_win)
									ack_dst_ip = Settings::hostId2IpMap[creditId*Settings::host_per_rack];
								SendSwitchACK(creditId, ch.dip, ack_dst_ip, creditPippyback);
								++SwitchMmu::switch_byte_credit_counter;
							}
						}
					}
				}
			}

		}else{
			m_mmu->m_tx_ctrl[ifIndex] += p->GetSize();
			if (ch.l3Prot == 0xFB)
				m_mmu->m_tx_switchACK[ifIndex] += p->GetSize();
		}

	}
}

/*-------------------------------Floodgate---------------------------------*/
bool SwitchNode::CheckVOQWin(Ptr<Packet> packet, CustomHeader &ch, uint32_t inDev, uint32_t outDev, uint32_t qIndex){

	/*
	 * When receive data
	 * --> Check Window of this VOQ
	 * --> buffer packet in VOQ if there isn't enough window.
	 */
	uint32_t dst_id = Settings::hostIp2IdMap[ch.dip];
	if (ch.l3Prot == 0x11){	// udp

		/**
		 * It is unnecessary to block the host under this ToR.
		 * What's more, blocking the host under this ToR may cause dead-lock.
		 * The ignore list was set when build topology.
		 */
		if (m_mmu->ShouldIgnore(dst_id)) return true;

		QPTag qptag;
		packet->PeekPacketTag(qptag);

		/*
		 * the last packet may go through floodgate when the window is less than MTU,
		 * in which way, the last packet may be received earlier than the packets before,
		 * thus, dis-ordered packet appears.
		 * --> to avoid this situation, should check the buffering packets in voq.
		 */
		if (m_mmu->CheckDstWin(dst_id, packet->GetSize()) && m_mmu->GetBufferDst(dst_id) == 0){

			m_mmu->UpdateDstWin(dst_id, packet->GetSize(), false); // go through VOQ

			// when packet is passing to egress, update PSN counter and tag SwitchPSNTag
			if (Settings::switch_absolute_psn)
				m_mmu->UpdateDataPSN(outDev, dst_id, packet);


		}else{ // buffer in VOQ, do not route
			Ptr<PacketUnit> curr = Create<PacketUnit>(MakeCallback(&SwitchNode::DoDequeueVOQ, this), packet, ch, dst_id, outDev, qIndex);

			// note that we didn't create VOQ until packet should buffer in VOQ
			Ptr<VOQ> voq = m_mmu->GetVOQ(dst_id);
			voq->Enqueue(curr);

			m_mmu->UpdateBufferingCount(dst_id, packet->GetSize(), true);

			m_mmu->UpdateMaxVOQNum();
			m_mmu->UpdateMaxActiveDstNum();

			return false;
		}

	}

	return true;
}

/*
* When receive ack
* --> restore window and route some data packets into egress
*/
bool SwitchNode::ResumeVOQWin(SwitchACKTag acktag, CustomHeader &ch, uint32_t inDev){

	uint32_t dst_id = Settings::hostIp2IdMap[ch.sip];
	// for handling loss: update lstrcv_ack_time and reset timeout event
	if (Settings::reset_only_ToR_switch_win)
		m_mmu->UpdateSynTime(dst_id/Settings::host_per_rack, dst_id, acktag);
	else
		m_mmu->UpdateSynTime(inDev, dst_id, acktag);

	if (Settings::switch_ack_mode == Settings::SWITCH_INGRESS_CREDIT){

		m_mmu->RecoverWin(acktag, inDev);
		m_mmu->CheckAndSendVOQ();
	}else if (Settings::switch_ack_mode == Settings::SWITCH_DST_CREDIT || Settings::switch_ack_mode == Settings::HOST_PER_PACKET_ACK){

		uint32_t resume = acktag.getAckedSize();
		if (Settings::switch_absolute_psn){
			uint32_t id = inDev;
			if (Settings::reset_only_ToR_switch_win) id = dst_id/Settings::host_per_rack;
			if (resume > m_mmu->m_rcv_ack_psn[id][dst_id]){
				m_mmu->m_rcv_ack_psn[id][dst_id] = acktag.getAckedSize();
			}
			resume = 0;
		}
		m_mmu->UpdateWin(dst_id, resume, inDev, true);		// restore window
		m_mmu->CheckAndSendVOQ(dst_id);			// Routing VOQ's packets to egress

	}

	if (Settings::switch_ack_mode == Settings::SWITCH_DST_CREDIT || Settings::switch_ack_mode == Settings::SWITCH_INGRESS_CREDIT)
		return true;	// no longer transfer switch-ACK

	return false;	// go on transferring switch-ACK
}

void SwitchNode::SendAccSwitchACK(uint32_t dev, SwitchACKTag acktag){
	assert(Settings::switch_ack_mode == Settings::SWITCH_INGRESS_CREDIT);

	if (Settings::reset_only_ToR_switch_win){
		uint32_t src = m_id * Settings::host_per_rack;
		uint32_t dst = dev * Settings::host_per_rack;

		Ptr<QbbNetDevice> tmp_device = DynamicCast<QbbNetDevice>(m_devices[1]);
		Ipv4Address dst_add(Settings::hostId2IpMap[dst]);
		Ipv4Address src_add(Settings::hostId2IpMap[src]);
		Ptr<Packet> p = tmp_device->GetSwitchACKPacket(acktag, src_add.Get(), dst_add.Get());

		CustomHeader ch(CustomHeader::L2_Header | CustomHeader::L3_Header | CustomHeader::L4_Header);
		p->PeekHeader(ch);
		int outId = GetOutDev(p, ch);
		Ptr<QbbNetDevice> outDev = DynamicCast<QbbNetDevice>(m_devices[outId]);
		outDev->SwitchSend(0, p, ch);
	}else{
		Ptr<QbbNetDevice> device = DynamicCast<QbbNetDevice>(m_devices[dev]);
		device->SendSwitchACK(acktag, 0, 0);
	}
	m_mmu->CleanIngressCreditCounter(dev, acktag);
	m_mmu->UpdateIngressLastSendTime(dev, 0);	// one ingress one timer
}

void SwitchNode::SendSwitchACK(uint32_t inDev, uint32_t dst_ip, uint32_t src_ip, uint64_t size){
	assert(Settings::switch_ack_mode == Settings::SWITCH_DST_CREDIT);
	SwitchACKTag tag;
	tag.setAckedSize(size);
	Ptr<QbbNetDevice> device = DynamicCast<QbbNetDevice>(m_devices[inDev]);
	device->SendSwitchACK(tag, dst_ip, src_ip);
	m_mmu->CleanIngressDstCreditCounter(inDev, Settings::hostIp2IdMap[dst_ip], size);
	m_mmu->UpdateIngressLastSendTime(inDev, Settings::hostIp2IdMap[dst_ip]);
}

void SwitchNode::SendSYN(uint32_t dev, SwitchSYNTag syntag){
	if (Settings::reset_only_ToR_switch_win){
		uint32_t src = m_id * Settings::host_per_rack;
		uint32_t dst = dev * Settings::host_per_rack;

		Ptr<QbbNetDevice> tmp_device = DynamicCast<QbbNetDevice>(m_devices[1]);
		Ipv4Address dst_add(Settings::hostId2IpMap[dst]);
		Ipv4Address src_add(Settings::hostId2IpMap[src]);
		Ptr<Packet> p = tmp_device->GetSwitchSYNPacket(syntag, src_add.Get(), dst_add.Get());

		CustomHeader ch(CustomHeader::L2_Header | CustomHeader::L3_Header | CustomHeader::L4_Header);
		p->PeekHeader(ch);
		int outId = GetOutDev(p, ch);
		Ptr<QbbNetDevice> outDev = DynamicCast<QbbNetDevice>(m_devices[outId]);
		outDev->SwitchSend(0, p, ch);
	}else{
		Ptr<QbbNetDevice> device = DynamicCast<QbbNetDevice>(m_devices[dev]);
		device->SendSwitchSYN(syntag, 0, 0);
	}
	SwitchMmu::win_out << m_id << " " << dev << " " << Simulator::Now().GetNanoSeconds() << " send SYN " << syntag.GetDataPSNEntry(0) << " "
			<< syntag.GetACKPSNEntry(0) << " " << m_mmu->m_nxt_data_psn[dev][0] << " " << m_mmu->m_rcv_ack_psn[dev][0] << std::endl;

}

/*
 * When receive SYN from upstream
 * -> Check syn and send SwitchACK if necessary
 */
void SwitchNode::ReceiveSYN(SwitchSYNTag& syntag, CustomHeader &ch, uint32_t inDev){
	uint32_t fine = inDev;
	if (Settings::reset_only_ToR_switch_win){
		fine = Settings::hostIp2IdMap[ch.sip]/Settings::host_per_rack;
	}
	std::set<uint32_t> dsts = m_mmu->CheckSYN(syntag, fine);
	SwitchMmu::win_out << m_id << " " << Settings::hostIp2IdMap[ch.dip] << " " << Simulator::Now().GetNanoSeconds() << " receive SYN " <<
			dsts.size() << " " << syntag.GetDataPSNEntry(0) << " " <<
			syntag.GetACKPSNEntry(0) << " " << m_mmu->m_lst_ack_psn[fine][0] << " " << m_mmu->m_nxt_ack_psn[fine][0] << std::endl;
	if (!dsts.empty()){
		if (Settings::switch_ack_mode == Settings::SWITCH_INGRESS_CREDIT){
			SwitchACKTag acktag = m_mmu->GetDstsSwitchACKTag(fine, dsts);
			SendAccSwitchACK(fine, acktag);
		}else if (Settings::switch_ack_mode == Settings::SWITCH_DST_CREDIT){
			while (!dsts.empty()){
				uint32_t dst_id = *dsts.begin();
				uint32_t id = inDev;
				if (Settings::reset_only_ToR_switch_win) id = Settings::hostIp2IdMap[ch.dip]/Settings::host_per_rack;
				SendSwitchACK(inDev, Settings::hostId2IpMap[*dsts.begin()], ch.dip, m_mmu->m_nxt_ack_psn[id][dst_id]);
				dsts.erase(dsts.begin());
			}
		}
	}
}

/*
 * The callback when a packet dequeued from VOQ
 */
void SwitchNode::DoDequeueVOQ(Ptr<Packet>p, CustomHeader &ch, uint32_t outDev, uint32_t qIndex){
	DoSwitchSend(p, ch, outDev, qIndex); // go through
}
/*-----------------------For symmetric routing---------------------------------------*/

void SwitchNode::TagForSymmeticRouting(Ptr<Packet> p, CustomHeader &ch, uint32_t inDev){
	/**
	 * Record on the ingress port on receiver ToR, which indicate the passing spine
	 * --> so, just cover the tag before
	 */
	if ( (Settings::symmetic_routing_mode == Settings::SYMMETRIC_RECEIVER
			&& ch.l3Prot == 0x11))		// include SYMMETRIC_RECEIVER --> tag data packet
	{
		RecordRoutingTag recordtag;
		if (p->PeekPacketTag(recordtag)) p->RemovePacketTag(recordtag);
		recordtag.setReceiverLeafIngress(inDev);
		p->AddPacketTag(recordtag);
	}
}

/*------------------------------------For statistics---------------------------------*/
uint32_t SwitchNode::DoStatistics(){
	uint32_t switch_buffer = 0;

	uint32_t egress_buffer = 0;
	for (uint32_t i = 0; i < m_devices.size(); i++){
		Ptr<QbbNetDevice> device = DynamicCast<QbbNetDevice>(m_devices[i]);
		if (!!device && !!device->GetQueue()){
			uint32_t qlength = device->GetQueue()->GetNBytesTotal();
			switch_buffer += qlength;
			if (Settings::max_port_length < qlength){
				Settings::max_port_length = qlength;
				Settings::max_port_index = device->GetQueue()->m_queueId;
			}
		}

		for (uint32_t j = 0; j < qCnt; ++j){
			assert(j == 3 || m_mmu->egress_bytes[i][j] == 0);
			egress_buffer += m_mmu->egress_bytes[i][j];
		}
	}

	uint32_t VOQ_buffer = 0;
	std::map<uint32_t, uint32_t>::iterator it = m_mmu->m_buffering.begin();
	while (it != m_mmu->m_buffering.end()){
		VOQ_buffer += it->second;
		it++;
	}

	assert(VOQ_buffer == VOQ::GetTotalBytes(m_id));
	assert(egress_buffer <= switch_buffer + VOQ::GetTotalBytes(m_id));
	Settings::max_switch_length = std::max(Settings::max_switch_length, switch_buffer + VOQ::GetTotalBytes(m_id));
	return switch_buffer;
}


void SwitchNode::PrintQlength(){
	if (output_realtime_buffer){
		uint32_t switch_buffer = DoStatistics();
		for (uint32_t i = 0; i < m_devices.size(); i++){
			Ptr<QbbNetDevice> device = DynamicCast<QbbNetDevice>(m_devices[i]);
			if (!!device && !!device->GetQueue()){
				uint32_t qlength = device->GetQueue()->GetNBytesTotal();
				m_traceRealtimeQueue(m_id, device->GetQueue()->m_queueId,
						qlength, device->GetQueue()->m_bytesInQueue[3], m_mmu->egress_bytes[device->GetIfIndex()][3],
						VOQ::GetTotalBytes(m_id, 0), VOQ::GetTotalBytes(m_id), switch_buffer + VOQ::GetTotalBytes(m_id));
				m_traceRealtimeSwitchBw(m_id, device->GetQueue()->m_queueId,
						m_mmu->m_tx_data[i], m_mmu->m_tx_ctrl[i], m_mmu->m_tx_switchACK[i],
						m_mmu->m_rcv_data[i], m_mmu->m_rcv_ctrl[i], m_mmu->m_rcv_switchACK[i]);
				m_mmu->m_tx_data[i] = 0;
				m_mmu->m_tx_ctrl[i] = 0;
				m_mmu->m_tx_switchACK[i] = 0;
				m_mmu->m_rcv_data[i] = 0;
				m_mmu->m_rcv_ctrl[i] = 0;
				m_mmu->m_rcv_switchACK[i] = 0;
			}
		}
		Simulator::Schedule(MicroSeconds(Settings::buffer_interval), &SwitchNode::PrintQlength, this);
	}
}

} /* namespace ns3 */
