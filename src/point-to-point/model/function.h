/*
 * function.h
 *
 *  Created on: Jan 14, 2018
 *      Author: tbc
 */

#ifndef SRC_INTERNET_NEW_FUNCTION_H_
#define SRC_INTERNET_NEW_FUNCTION_H_

#include <cmath>
#include <string>
#include <string>
#include "ns3/nstime.h"

namespace ns3 {

class Function {
public:

	static void SetMode(std::string mode,uint32_t nPrio, uint32_t maxPkts,uint32_t maxTimeNs);

	// remTime: deadlineTime - currentTime
	// return 0xff means deadline missing
	static inline uint8_t GetPriority(uint32_t totalPkts, uint32_t remPkts, uint32_t remTimeNs){
		return func(totalPkts,remPkts,remTimeNs);
	}

private:
	static uint8_t (*func)(uint32_t,uint32_t,uint32_t);
	static double para;

	static uint8_t sjf(uint32_t totalPkts, uint32_t remPkts, uint32_t remTimeNs);
	static uint8_t srtf(uint32_t totalPkts, uint32_t remPkts, uint32_t remTimeNs);
	static uint8_t las(uint32_t totalPkts, uint32_t remPkts, uint32_t remTimeNs);
	static uint8_t edf(uint32_t totalPkts, uint32_t remPkts, uint32_t remTimeNs);
	static uint8_t lst(uint32_t totalPkts, uint32_t remPkts, uint32_t remTimeNs);

};

} /* namespace ns3 */

#endif /* SRC_INTERNET_NEW_FUNCTION_H_ */
