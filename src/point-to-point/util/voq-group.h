/*
 * voq-group.h
 *
 *  Created on: Jan 21, 2021
 *      Author: wqy
 */

#ifndef SRC_POINT_TO_POINT_UTIL_VOQ_GROUP_H_
#define SRC_POINT_TO_POINT_UTIL_VOQ_GROUP_H_

#include "ns3/object.h"
#include "ns3/callback.h"
#include "ns3/ptr.h"
#include "ns3/packet.h"
#include "ns3/address.h"
#include "ns3/net-device.h"
#include "ns3/voq.h"
#include <vector>
#include <queue>
#include <map>

namespace ns3 {

class VOQGroup : public Object{
	friend class SwitchMmu;
	friend class SwitchNode;
public:
	std::map<uint32_t, Ptr<VOQ> > VOQs;	// <voq_id, voq>
	// only used when use dynamic hash
	std::map<uint32_t, uint32_t> m_dst2voq; // <dst, voq_id>

	uint32_t m_switch_id;
	uint32_t m_group_id;
	Callback<bool, uint32_t, uint32_t> m_checkWinCallback;
	Callback<uint32_t, uint32_t, uint32_t, uint32_t, Ptr<Packet> > m_dequeueCallback;	// bound with SwitchMmu::VOQDequeueCallback

	// configure
	uint32_t m_voq_limit;
	bool m_dynamic_hash;

	VOQGroup(uint32_t switch_id, uint32_t group_id, uint32_t limit, bool m_dynamic_hash);
	bool CheckWin(uint32_t dst, uint32_t pktSize);
	/*
	 * when voq group dequeue a packet
	 * --> callback mmu to update window and buffering counter
	 * --> if the voq has no pkts left, release it
	 */
	void VOQDequeueCallback(uint32_t dst, uint32_t pktSize, uint32_t outDev, uint32_t voqId, Ptr<Packet> pkt);

	uint32_t FindVOQId(uint32_t dst);
	uint32_t GetVOQId(uint32_t dst, bool& hasFind_dyn);
	Ptr<VOQ> FindVOQ(uint32_t dst);
	Ptr<VOQ> GetVOQ(uint32_t dst);
	uint32_t GetBufferDst(uint32_t dst);
};
}




#endif /* SRC_POINT_TO_POINT_UTIL_VOQ_GROUP_H_ */
