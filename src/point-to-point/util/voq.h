/*
 * voq.h
 *
 *  Created on: Dec 7, 2020
 *      Author: wqy
 */

#ifndef SRC_POINT_TO_POINT_UTIL_VOQ_H_
#define SRC_POINT_TO_POINT_UTIL_VOQ_H_

#include "ns3/object.h"
#include "ns3/callback.h"
#include "ns3/ptr.h"
#include "ns3/packet.h"
#include "ns3/address.h"
#include "ns3/net-device.h"
#include "ns3/settings.h"
#include <vector>
#include <queue>
#include <map>

namespace ns3 {

/*
 * The unit of packet
 * used for routing and send
 */
class PacketUnit : public Object{
	friend class VOQ;
public:
	typedef Callback<void, Ptr<Packet>, CustomHeader&, uint32_t, uint32_t> ReceiveCallback;

	PacketUnit(ReceiveCallback receiveCallback, Ptr<Packet> pkt, CustomHeader& ch, uint32_t dst, uint32_t outDev, uint32_t qIndex){
		m_pkt = pkt;
		m_receiveCallback = receiveCallback;
		m_ch = ch;
		m_dst = dst;
		m_outDev = outDev;
		m_qIndex = qIndex;
	}

	uint32_t GetPacketSize(){
		return m_pkt->GetSize();
	}

	void GoOnReceive(){
		return m_receiveCallback(m_pkt, m_ch, m_outDev, m_qIndex);
	}

private:
	Ptr<Packet> m_pkt;
	ReceiveCallback m_receiveCallback;
	CustomHeader m_ch;
	uint32_t m_dst;
	uint32_t m_outDev;
	uint32_t m_qIndex;
};

/*
 * The VOQ shared in the switch
 */
class VOQ : public Object{
	friend class SwitchMmu;
	friend class SwitchNode;
	friend class VOQGroup;

public:
	/*
	 * Map of BDP: < <switch_id, node_id>, BDP >
	 * initial when build topology
	 * node_id includes id of host and dst-ToR
	 */
	static std::map<std::pair<uint32_t, uint32_t>, uint32_t> BDPMAP;

	/*
	 * For statistic
	 */
	static uint32_t maxVOQNum;
	static uint32_t maxActiveDstNum;
	static std::map<uint32_t, uint32_t> node_maxVOQNum[Settings::SWITCHSCALE];

	/*
	 * For statistic
	 * < < <switch_id, group_id> , voq_id>, buffer>
	 */
	static std::map<std::pair<std::pair<uint32_t, uint32_t>, uint32_t>, uint32_t> VOQBuffer;

	/*
	 * For statistic
	 * Get total VOQ buffer of a switch
	 */
	static uint32_t GetTotalBytes(uint32_t switch_id);

	/*
	 * For statistic
	 * Get total VOQ buffer of a voq group
	 */
	static uint32_t GetTotalBytes(uint32_t switch_id, uint32_t group_id);

	VOQ(uint32_t switch_id, uint32_t group_id, uint32_t voq_id);

	void Enqueue(Ptr<PacketUnit> pkt);
	Ptr<PacketUnit> Dequeue();
	uint32_t GetTotalBytes();
	/*
	 * Dequeue and transfer packet until all packets has sent or there is not enough window
	 */
	void CheckAndSendAll();
	/*
	 * Dequeue and transfer one packet if has left window
	 * @return: success or not
	 */
	bool CheckAndSendOne();

	Callback<bool, uint32_t, uint32_t> m_checkWinCallback;
	Callback<void, uint32_t, uint32_t, uint32_t, uint32_t, Ptr<Packet> > m_dequeueCallback;	// bound with VOQGroup::VOQDequeueCallback

private:
	std::queue<Ptr<PacketUnit> > m_pkts;
	uint32_t m_voq_id;		// voq id
	uint32_t m_group_id;	// voq group id
	uint32_t m_switch_id;		// the switch id, used to calculate win
};

}



#endif /* SRC_POINT_TO_POINT_UTIL_VOQ_H_ */
