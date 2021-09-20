/*
 * flow-generator.h
 *
 *  Created on: Nov 16, 2017
 *      Author: tbc
 */

#ifndef SRC_INTERNET_NEW_FLOW_GENERATOR_H_
#define SRC_INTERNET_NEW_FLOW_GENERATOR_H_

#include "ns3/random-variable-stream.h"

namespace ns3 {

class FlowGenerator {
public:
	FlowGenerator() {}
	virtual ~FlowGenerator() {}

	static Ptr<RandomVariableStream> GetDataMiningStream (uint32_t seed, uint32_t& mean, uint32_t& maxx);
	static Ptr<RandomVariableStream> GetWebSearchStream (uint32_t seed, uint32_t& mean, uint32_t& maxx);
	static Ptr<RandomVariableStream> GetIcmStream (uint32_t seed, uint32_t& mean, uint32_t& maxx);
	static Ptr<RandomVariableStream> GetBurstStream (uint32_t seed, uint32_t& mean, uint32_t& maxx);
	static Ptr<RandomVariableStream> GetFabricatedHeavyMiddle (uint32_t seed, uint32_t& mean, uint32_t& maxx, uint32_t flow_cdf);
};

} /* namespace ns3 */

#endif /* SRC_INTERNET_NEW_FLOW_GENERATOR_H_ */
