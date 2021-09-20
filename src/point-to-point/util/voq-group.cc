/*
 * voq-group.cc
 *
 *  Created on: Jan 21, 2021
 *      Author: wqy
 */

#include <assert.h>
#include "ns3/simulator.h"
#include "ns3/log.h"
#include "ns3/assert.h"
#include "ns3/ipv4-header.h"
#include "ns3/hash-functions.h"
#include "ns3/settings.h"
#include "voq-group.h"

#define DEBUG_MODE 0

NS_LOG_COMPONENT_DEFINE("VOQGROUP");

namespace ns3 {

	VOQGroup::VOQGroup(uint32_t switch_id, uint32_t group_id, uint32_t voq_limit, bool dynamic_hash){
		m_switch_id = switch_id;
		m_group_id = group_id;
		m_voq_limit = voq_limit;
		m_dynamic_hash = dynamic_hash;
	}

	/**
	 * Given a dst, find VOQid
	 * Note: only search, don't allocate
	 * For no limit voq version and static hash version, it needn't to allocate naturally.
	 * @return -1 means no voq for this dst for now (only return under dynamic hash version)
	 */
	uint32_t VOQGroup::FindVOQId(uint32_t dst){
		if (m_voq_limit == 0) return dst;

		uint32_t voq_id = -1;
		if (m_dynamic_hash){
			if (m_dst2voq.find(dst) != m_dst2voq.end()){
				voq_id = m_dst2voq[dst];
			}else{
				// when no voq for this dst, there must no packets buffering in VOQ
//				assert(m_buffering.find(dst) == m_buffering.end() || m_buffering[dst] == 0);
			}
		}else{
			char s[20];
			sprintf(s, "%d", dst);
			voq_id = HashFunctions::APHash(s, strlen(s))%m_voq_limit;
		}
	//	std::cout << "Hash map: dst-" << dst_id << ", voq-" << voq_id << std::endl;
		return voq_id;
	}

	/**
	 * Given a dst, get VOQid
	 * if dst has no voq yet, return next available voq_id for this dst
	 * Note that:
	 * When !Settings::dynamic_hash_voq, this function is the same as FindVOQId
	 * and hasFind will always be true.
	 * Therefore, hasFind only useful when use dynamic_hash_voq
	 */
	uint32_t VOQGroup::GetVOQId(uint32_t dst, bool& hasFind_dyn){
		uint32_t voq_id = FindVOQId(dst);
		hasFind_dyn = (voq_id != (uint32_t)-1);

		if (!hasFind_dyn){
			assert(m_dynamic_hash);
			/*
			 * no voq for this dst yet
			 * --> get a avaliable voq_id for this dst
			 */
			if (VOQs.size() < m_voq_limit){
				/*
				 * has remaining empty voqs
				 * --> allocate an empty one
				 */
				for (uint32_t i = 0; i < m_voq_limit; ++i){
					if (VOQs.find(i) == VOQs.end()){
						voq_id = i;
					}
				}
			}else{
				/*
				 * todo: can use power-of-2 to optimize
				 * voqs have been used up
				 * --> do hash
				 */
				char s[20];
				sprintf(s, "%d", dst);
				voq_id = HashFunctions::APHash(s, strlen(s))%m_voq_limit;
			}
		}
		return voq_id;
	}

	Ptr<VOQ> VOQGroup::FindVOQ(uint32_t dst){
		NS_LOG_FUNCTION (this << dst);
		uint32_t voq_id = FindVOQId(dst);
		if (voq_id == (uint32_t)-1 || VOQs.find(voq_id) == VOQs.end()) return NULL;

		return VOQs[voq_id];
	}

	/**
	 * Get VOQ of destination
	 * Create if not exist
	 */
	Ptr<VOQ> VOQGroup::GetVOQ(uint32_t dst){
		NS_LOG_FUNCTION (this << dst);

		bool hasFind_dyn = true;	// only use for dynamic_hash_voq
		uint32_t voq_id = GetVOQId(dst, hasFind_dyn);

		/**
		 * if no such voq yet
		 * (Note that when not use dynamic hash, hasFind will always be true, so need to check VOQs)
		 * --> register destination(assign window)
		 * --> create VOQ
		 */
		if ((!m_dynamic_hash && VOQs.find(voq_id) == VOQs.end())
				|| (m_dynamic_hash && !hasFind_dyn)){

			if (VOQs.find(voq_id) == VOQs.end()){	// when hash collision, VOQ may exist
				Ptr<VOQ> voq = Create<VOQ>(m_switch_id, m_group_id, voq_id);
				voq->m_checkWinCallback = MakeCallback(&VOQGroup::CheckWin, this);
				voq->m_dequeueCallback = MakeCallback(&VOQGroup::VOQDequeueCallback, this);
				VOQs[voq_id] = voq;
			}

			if (m_dynamic_hash){
				assert(m_dst2voq.find(dst) == m_dst2voq.end());
				m_dst2voq.insert(std::make_pair(dst, voq_id));
			}
		}

		assert(voq_id != (uint32_t)-1 && VOQs.find(voq_id) != VOQs.end());
		return VOQs[voq_id];
	}

	bool VOQGroup::CheckWin(uint32_t dst, uint32_t pktSize){
		return m_checkWinCallback(dst, pktSize);
	}

	void VOQGroup::VOQDequeueCallback(uint32_t dst, uint32_t pktSize, uint32_t outDev, uint32_t voqId, Ptr<Packet> pkt){
		assert(FindVOQId(dst) == voqId);
		uint32_t buffering = m_dequeueCallback(dst, pktSize, outDev, pkt);

		/*
		 * if no buffering packet of this dst
		 * -> reset the map from dst to voq
		 */
		if (m_dynamic_hash && buffering == 0){
			m_dst2voq.erase(dst);
			/*
			 * if no packet in this voq
			 * --> release it
			 */
			if (VOQs[voqId]->m_pkts.empty()){
				VOQs.erase(voqId);
			}

		}
	}
}
