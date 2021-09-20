#ifndef SRC_POINT_TO_POINT_MODEL_SETTINGS_H_
#define SRC_POINT_TO_POINT_MODEL_SETTINGS_H_

#include <fstream>
#include <cstdio>
#include <stdint.h>
#include <string>
#include <algorithm>
#include <vector>
#include <numeric>
#include <sstream>
#include <cstring>
#include <iostream>
#include <iomanip>
#include <list>
#include "ns3/nstime.h"
#include "ns3/string.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"
#include <map>

namespace ns3{

class Settings {
public:
	Settings() {}
	virtual ~Settings() {}

	static const uint32_t NODESCALE = 2000;
	static const uint32_t SWITCHSCALE = 200;
	static const uint32_t MAXHOP = 8;

	// for bw analysis
	static std::vector<double> bwList[NODESCALE];
	static std::vector<double> QDelayList[NODESCALE];
	static double host_bw_Bps;

	static std::vector<double> ctrlList[NODESCALE];
	static std::vector<double> tpList[NODESCALE];

	// for common setting
	static uint32_t packet_payload;
	static uint32_t MTU;

	// for timeout retransmission
	static uint32_t rto_us;

	// for qp mode
	static bool qp_mode;
	static bool reset_qp_rate;

	// for flow generation
	static bool pktsmode;
	static uint32_t maxx; // max flow size
	static double homa_longflow_threshold;
	static std::vector<std::pair<uint32_t,double> > cdfFromFile;
	static int avgSizeFromFile;
	static double threshold_slope;
	static double threshold_bias;
	static uint32_t free_token;
	static uint32_t max_bdp;

	// for statistic
	static std::ofstream switch_buffer_out[SWITCHSCALE];
	static std::ofstream switch_bw_out[SWITCHSCALE];
	static std::ofstream bw_out;
	static std::ofstream rate_out;
	static std::ofstream queuing_out;
	static uint32_t bw_interval;
	static uint32_t buffer_interval;
	static uint32_t host_num;
	static uint32_t host_per_rack;
	static uint32_t switch_num;
	static uint32_t tor_num;
	static uint32_t core_num;
	static uint32_t timeout_times;
	static uint32_t drop_packets;
	static uint32_t max_port_length;
	static uint32_t max_port_index;
	static uint32_t max_switch_voq_length;
	static uint32_t max_switch_length;
	static uint32_t max_voq_length;

	/*---------------------For Floodgate-------------*/
	/**
	 * The map between hosts' IP and ID
	 * initial when build topology
	 */
	static std::map<uint32_t, uint32_t> hostIp2IdMap;
	static std::map<uint32_t, uint32_t> hostId2IpMap;

	static bool use_floodgate;
	static double switch_win_m;
	/**
	 * (true)ONLY_TOR: only ToRs deploy Floodgate; only dst-ToR send switch-ack
	 * (false)ALL_SWITCHES: link-by-link control
	 */
	static bool reset_only_ToR_switch_win;
	/**
	 * isolate all up and down streams' dst on switches besides core
	 * (0) turn off
	 * (1) turn on: use ignore as isolation solution
	 * (2) turn on: use different VOQ as isolation solution
	 */
	static uint32_t reset_isolate_down_host;
	static const uint32_t RESET_ISOLATE_IGNORE = 1;
	static const uint32_t RESET_ISOLATE_DIFFGROUP = 2;		// default
	/*
	 * define the way to send ACK/credit
	 * HOST_PER_PACKET_ACK: reuse ACK as SwitchCredit/SwitchACK; host must support per-packet-ack
	 * SWITCH_DST_CREDITT: switches are responsible for sending SwitchACK; a SwitchACK will carry credits of a dst from a same ingress
	 * SWITCH_INGRESS_CREDIT: switches are responsible for sending SwitchACK; a SwitchACK will carry credits of different dsts from a same ingress
	 */
	enum { HOST_PER_PACKET_ACK = 0u, SWITCH_DST_CREDIT = 1u, SWITCH_INGRESS_CREDIT = 2u};
	static uint32_t switch_ack_mode;
	static uint32_t switch_byte_counter;		// 0/1: per packet ack; useless when HOST_PER_PACKET_ACK
	static double switch_credit_interval;		// 0: turn off; unit: us; useless when HOST_PER_PACKET_ACK or switch_byte_counter = 0/1
	/*
	 * handle packet-loss: absolute data/SwitchACK PSN + timeout
	 * only work with SWITCH_DST_CREDIT and SWITCH_INGRESS_CREDIT
	 */
	static uint64_t switch_absolute_psn;		// 0: turn off
	static uint32_t switch_syn_timeout_us;		// 0: turn off; useful when switch_absolute_psn turns on
	/*
	 * delayACK: the threshold to send SwitchACK
	 */
	static double switch_ack_th_m;

	/*
	 * for PFC
	 */
	static bool srcToR_dst_pfc;
	static uint32_t pfc_dst_alpha;
	static double pfc_th_static;

	/*---------------------For Load Balance/Routing/Solve disordered-------------*/
	/**
	 * Set routing method for install
	 * (0/Default) --> (per flow routing, non-symmetric for default)
	 * (1) DRILL --> (per packet routing, non-symmetric for default)
	 */
	static uint32_t routing_mode;
	static const uint32_t ROUTING_GLOBAL = 0;
	static const uint32_t ROUTING_DRILL = 1;
	/**
	 * (0/default) use port's queuing length as load
	 * (1) use port's received number of packets for a period of time as load
	 */
	static uint32_t drill_load_mode;
	static const uint32_t DRILL_LOAD_DEFAULT = 0;
	static const uint32_t DRILL_LOAD_INTERVAL_SENT = 1;
	/**
	 * Set symmetric mode
	 * (0) turn off
	 * (1) control packets from receiver to sender pass symmetric path with data packets from sender to receiver
	 */
	static uint32_t symmetic_routing_mode;
	static const uint32_t SYMMETRIC_OFF = 0;
	static const uint32_t SYMMETRIC_RECEIVER = 1;

	static bool IsPacketLevelRouting();

	/**
	 * (false)	Turn off the reaction of NACK.
	 * 		   	--> In this way, drop will cause unfinished message,
	 * 		   	--> however, this will avoid unnecessary retransmission when do packet-level routing.
	 * (true) 	Turn on the reaction of NACK
	 */
	static bool nack_reaction;

	/**
	 * The interval of reseting queue's statistics info
	 * (Only used when drill_load_mode == DRILL_LOAD_INTERVAL_SENT for now)
	 */
	static uint32_t queue_statistic_interval_us;

};


}

#endif /* SRC_POINT_TO_POINT_MODEL_SETTINGS_H_ */
