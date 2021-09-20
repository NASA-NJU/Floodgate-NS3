#include "ns3/settings.h"

namespace ns3{

	/**
	 * return true:
	 * L4 should solve dis-order of packet
	 * Receiver:
	 * 1. Reply to the corresponding ack directly after receiving the packet and set `m_received_psn.insert(curr_psn)`
	 * 2. The receiving end receives a packet marked with lasttag --> `received_last_psn_packet = true`
	 * 3. Every time a package is received, check if `m_received_last_psn_packet` and the previous packets are also received.
	 *    --> Send a lasttag ack to inform the sender that all packets have been received.
	 * Sender:
	 * 1. Send packets in order, `send_next += payload, m_sent_psn.insert(curr_psn)`
	 * 2. Received an ACK, ` m_sent psn.erase(curr_psn)`
	 * 3. The last PSN tag is sending, tag lasttag
	 * 4. Receiving the ack of lasttag tag indicates that the receiver has received it
	 * 5. Note that: for Go-Back-N that cannot be triggered by NACK, retransmission with timeout:
	 *    * `timeouted = true`
	 *    * If `send_nxt ≠ last_psn`, continue to send packets in sequence
	 *    * Otherwise, `m_timeout_psn = m_sent_psn', send packets which indicate by `m_timeout_psn`
	 */
	bool Settings::IsPacketLevelRouting(){
		switch (Settings::routing_mode){
		case Settings::ROUTING_DRILL:
			return true;
		default:
			return false;
		}
	}

	std::vector<double> Settings::bwList[NODESCALE];
	std::vector<double> Settings::QDelayList[NODESCALE];
	double Settings::host_bw_Bps;

	std::vector<double> Settings::ctrlList[NODESCALE];
	std::vector<double> Settings::tpList[NODESCALE];

	// l4, for timeout retransmission
	uint32_t Settings::rto_us = 5000;

	// for qp mode
	bool Settings::qp_mode = false;
	bool Settings::reset_qp_rate = false;

	uint32_t Settings::free_token = 9;
	uint32_t Settings::max_bdp;

	// for floodgate
	std::map<uint32_t, uint32_t> Settings::hostIp2IdMap;
	std::map<uint32_t, uint32_t> Settings::hostId2IpMap;
	// important settings
	bool Settings::use_floodgate = false;
	double Settings::switch_win_m = 1.5;
	uint32_t Settings::switch_ack_mode = Settings::SWITCH_DST_CREDIT;
	bool Settings::reset_only_ToR_switch_win = false;
	uint32_t Settings::switch_byte_counter = 1;
	double Settings::switch_credit_interval = 10;	// us
	double Settings::switch_ack_th_m = 0;		// active delay SwitchACK or not(supported only when SWTICH_ACCUMULATE_CREDIT); performance has no outstanding improvement
	uint64_t Settings::switch_absolute_psn = 0;
	uint32_t Settings::switch_syn_timeout_us = 1000;
	// fixed settings(validated performance)
	uint32_t Settings::reset_isolate_down_host = 1;		// always use 1: isolate upstream and downstream at ToR (performance is not bad, but more intuitive)

	// for PFC
	bool Settings::srcToR_dst_pfc = true;
	uint32_t Settings::pfc_dst_alpha = 8;
	double Settings::pfc_th_static = 1.5;		// should work with "USE_DYNAMIC_PFC_THRESHOLD 0" and "SRCTOR_DST_PFC 1"

	// for flow generation
	bool Settings::pktsmode;
	uint32_t Settings::maxx; // max flow size
	double Settings::homa_longflow_threshold;
	std::vector<std::pair<uint32_t,double> > Settings::cdfFromFile;
	int Settings::avgSizeFromFile;
	double Settings::threshold_slope;
	double Settings::threshold_bias;

	// for statistic
	uint32_t Settings::timeout_times = 0;
	uint32_t Settings::drop_packets = 0;
	std::ofstream Settings::switch_buffer_out[SWITCHSCALE];
	std::ofstream Settings::switch_bw_out[SWITCHSCALE];
	std::ofstream Settings::bw_out;
	std::ofstream Settings::rate_out;
	std::ofstream Settings::queuing_out;
	uint32_t Settings::bw_interval = 10;
	uint32_t Settings::buffer_interval = 10;
	uint32_t Settings::host_num = 144;
	uint32_t Settings::switch_num = 13;
	uint32_t Settings::host_per_rack = 16;
	uint32_t Settings::tor_num = 9;
	uint32_t Settings::core_num = 4;
	uint32_t Settings::max_port_length = 0;
	uint32_t Settings::max_port_index = 0;
	uint32_t Settings::max_voq_length = 0;
	uint32_t Settings::max_switch_voq_length = 0;
	uint32_t Settings::max_switch_length = 0;

	uint32_t Settings::packet_payload = 1460;
	uint32_t Settings::MTU = 1538;

	// for routing
	uint32_t Settings::routing_mode = 0;
	uint32_t Settings::drill_load_mode = 0;
	uint32_t Settings::symmetic_routing_mode = 0;
	uint32_t Settings::queue_statistic_interval_us = 10;

	bool Settings::nack_reaction = false;

}
