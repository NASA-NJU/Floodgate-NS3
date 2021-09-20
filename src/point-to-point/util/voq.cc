/*
 * voq.cc
 *
 *  Created on: Dec 7, 2020
 *      Author: wqy
 */

#include <assert.h>
#include "ns3/simulator.h"
#include "ns3/log.h"
#include "ns3/assert.h"
#include "voq.h"
#include "ns3/ipv4-header.h"
#include "ns3/settings.h"

#define DEBUG_MODE 0

NS_LOG_COMPONENT_DEFINE("VOQ");

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (VOQ);

uint32_t VOQ::maxVOQNum = 0;
uint32_t VOQ::maxActiveDstNum = 0;
std::map<uint32_t, uint32_t> VOQ::node_maxVOQNum[Settings::SWITCHSCALE];
std::map<std::pair<uint32_t, uint32_t>, uint32_t> VOQ::BDPMAP;
std::map<std::pair<std::pair<uint32_t, uint32_t>, uint32_t>, uint32_t> VOQ::VOQBuffer;

uint32_t VOQ::GetTotalBytes(uint32_t switch_id){
	std::map<std::pair<std::pair<uint32_t, uint32_t>, uint32_t>, uint32_t>::iterator it = VOQ::VOQBuffer.begin();
	uint32_t total_bytes = 0;
	while (it != VOQ::VOQBuffer.end()){
		if (it->first.first.first == switch_id){
			total_bytes += it->second;
		}
		++it;
	}
	return total_bytes;
}

uint32_t VOQ::GetTotalBytes(uint32_t switch_id, uint32_t group_id){
	std::map<std::pair<std::pair<uint32_t, uint32_t>, uint32_t>, uint32_t>::iterator it = VOQ::VOQBuffer.begin();
	uint32_t total_bytes = 0;
	while (it != VOQ::VOQBuffer.end()){
		if (it->first.first.first == switch_id && it->first.first.second == group_id){
			total_bytes += it->second;
		}
		++it;
	}
	return total_bytes;
}

VOQ::VOQ(uint32_t switch_id, uint32_t group_id, uint32_t voq_id){
	NS_LOG_FUNCTION (this << switch_id << voq_id);
	m_switch_id = switch_id;
	m_group_id = group_id;
	m_voq_id = voq_id;
	if (VOQ::VOQBuffer.find(std::make_pair(std::make_pair(m_switch_id, m_group_id), m_voq_id)) == VOQ::VOQBuffer.end()){
		VOQ::VOQBuffer.insert(std::make_pair(std::make_pair(std::make_pair(m_switch_id, m_group_id), m_voq_id), 0));
	}
	NS_LOG_INFO(this << "initial VOQ: " << switch_id << " " << group_id << " " << voq_id);
}

void VOQ::Enqueue(Ptr<PacketUnit> pkt){
	NS_LOG_FUNCTION (this << pkt);
	m_pkts.push(pkt);
	VOQBuffer[std::make_pair(std::make_pair(m_switch_id, m_group_id), m_voq_id)] += pkt->m_pkt->GetSize();

#if DEBUG_MODE
		if (m_voq_id == 1){
			std::cout << Simulator::Now() << " " << m_switch_id << " enqueue UDP "
					<< m_pkts.size()
					<< " " << VOQBuffer[std::make_pair(std::make_pair(m_switch_id, m_group_id), m_voq_id)] << std::endl;
		}
#endif

	Settings::max_voq_length = std::max(Settings::max_voq_length, VOQBuffer[std::make_pair(std::make_pair(m_switch_id, m_group_id), m_voq_id)]);
	Settings::max_switch_voq_length = std::max(Settings::max_switch_voq_length, VOQ::GetTotalBytes(m_switch_id));
}

Ptr<PacketUnit> VOQ::Dequeue(){
	NS_LOG_FUNCTION (this);
	if (m_pkts.empty()) return NULL;
	Ptr<PacketUnit> front = m_pkts.front();
	m_pkts.pop();

	VOQBuffer[std::make_pair(std::make_pair(m_switch_id, m_group_id), m_voq_id)] -= front->m_pkt->GetSize();

	return front;
}

uint32_t VOQ::GetTotalBytes(){
	return VOQBuffer[std::make_pair(std::make_pair(m_switch_id, m_group_id), m_voq_id)];
}

/**
 * Dequeue and transfer packet until all packets has sent or there is not enough window
 */
void VOQ::CheckAndSendAll(){
	while (CheckAndSendOne());
}

/*
 * Dequeue and transfer one packet if has left window
 */
bool VOQ::CheckAndSendOne(){
	if (m_pkts.size() > 0){
		Ptr<PacketUnit> top = m_pkts.front();
		if (!m_checkWinCallback(top->m_dst, top->m_pkt->GetSize())) return false;
		m_pkts.pop();

		VOQBuffer[std::make_pair(std::make_pair(m_switch_id, m_group_id), m_voq_id)] -= top->m_pkt->GetSize();

		 // when voq dequeue a packet-> callback voq group controller to call upper control layer and release voq if necessary
		m_dequeueCallback(top->m_dst, top->m_pkt->GetSize(), top->m_outDev, m_voq_id, top->m_pkt);
#if DEBUG_MODE
		if (m_voq_id == 1){
			std::cout << Simulator::Now() << " " << m_switch_id << " send UDP "
					<< VOQ::hostIp2IdMap[top->m_dst] << " " << m_pkts.size()
					<< " " << VOQBuffer[std::make_pair(std::make_pair(m_switch_id, m_group_id), m_voq_id)] << std::endl;
		}
#endif
		// do routing to egress
		top->GoOnReceive();
		return true;
	}
	return false;
}

}


