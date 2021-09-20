/*
 * flow-generator.cc
 *
 *  Created on: Nov 16, 2017
 *      Author: tbc
 */

#include "flow-generator.h"
#include "ns3/settings.h"
namespace ns3 {


Ptr<RandomVariableStream> FlowGenerator::GetDataMiningStream (uint32_t seed, uint32_t& mean, uint32_t& maxx) { // KB
	Ptr<EmpiricalRandomVariable> stream = CreateObject<EmpiricalRandomVariable> ();
	stream->SetStream (seed);
	stream->CDF (1, 0.0);
	stream->CDF (1, 0.5);
	stream->CDF (2, 0.6);
	stream->CDF (3, 0.7);
	stream->CDF (7, 0.8);
	stream->CDF (267, 0.9);
	stream->CDF (2107, 0.95);
	stream->CDF (66667, 0.99);
	stream->CDF (666667, 1.0);
	mean = 5116;
	maxx = 666667;

	if(!Settings::pktsmode){
		//mean = mean*1538;

		mean = mean * 1460;
		maxx = maxx*1460;
	}

	Settings::maxx = maxx;
	Settings::homa_longflow_threshold = 10.0*1024*1024;// like phost

	Settings::cdfFromFile.clear();
	Settings::cdfFromFile.push_back(std::make_pair(1*1460,0.5));
	Settings::cdfFromFile.push_back(std::make_pair(2*1460,0.6));
	Settings::cdfFromFile.push_back(std::make_pair(3*1460,0.7));
	Settings::cdfFromFile.push_back(std::make_pair(7*1460,0.8));
	Settings::cdfFromFile.push_back(std::make_pair(267*1460,0.9));
	Settings::cdfFromFile.push_back(std::make_pair(2107*1460,0.95));
	Settings::cdfFromFile.push_back(std::make_pair(66667*1460,0.99));
	Settings::cdfFromFile.push_back(std::make_pair(666667*1460,1));

	return stream;
}

Ptr<RandomVariableStream> FlowGenerator::GetWebSearchStream (uint32_t seed, uint32_t& mean, uint32_t& maxx) { // KB

	//stream is actually in pkts mode
	Ptr<EmpiricalRandomVariable> stream = CreateObject<EmpiricalRandomVariable> ();
	stream->SetStream (seed);
	stream->CDF (6, 0.0);
	stream->CDF (6, 0.15);
	stream->CDF (13, 0.2);
	stream->CDF (19, 0.3);
	stream->CDF (33, 0.4);
	stream->CDF (53, 0.53);
	stream->CDF (133, 0.6);
	stream->CDF (667, 0.7);
	stream->CDF (1333, 0.8);
	stream->CDF (3333, 0.9);
	stream->CDF (6667, 0.97);
	stream->CDF (20000, 1.0);
	mean = 1138;
	maxx = 20000;

	//it is used to generate the right flow_arrival_time
	if(!Settings::pktsmode){

		//mean = mean*1538;
		mean = mean * 1460;
		maxx = maxx*1460;
	}

	Settings::maxx = maxx;
	Settings::homa_longflow_threshold = 10.0*1024*1024;

	Settings::cdfFromFile.clear();
	Settings::cdfFromFile.push_back(std::make_pair(6*1460,0.15));
	Settings::cdfFromFile.push_back(std::make_pair(13*1460,0.2));
	Settings::cdfFromFile.push_back(std::make_pair(19*1460,0.3));
	Settings::cdfFromFile.push_back(std::make_pair(33*1460,0.4));
	Settings::cdfFromFile.push_back(std::make_pair(53*1460,0.53));
	Settings::cdfFromFile.push_back(std::make_pair(133*1460,0.6));
	Settings::cdfFromFile.push_back(std::make_pair(667*1460,0.7));
	Settings::cdfFromFile.push_back(std::make_pair(1000*1460,0.75));//addede for priority solving
	Settings::cdfFromFile.push_back(std::make_pair(1333*1460,0.8));
	Settings::cdfFromFile.push_back(std::make_pair(2333*1460,0.85));//addede for priority solving
	Settings::cdfFromFile.push_back(std::make_pair(3333*1460,0.9));
	Settings::cdfFromFile.push_back(std::make_pair(5000*1460,0.935));//addede for priority solving
	Settings::cdfFromFile.push_back(std::make_pair(6667*1460,0.97));
	Settings::cdfFromFile.push_back(std::make_pair(20000*1460,1.0));


	return stream;
}

Ptr<RandomVariableStream> FlowGenerator::GetIcmStream (uint32_t seed, uint32_t& mean, uint32_t& maxx) { // KB
	Ptr<EmpiricalRandomVariable> stream = CreateObject<EmpiricalRandomVariable> ();
	stream->SetStream (seed);
	stream->CDF (1, 0.0);
	stream->CDF (1, 0.5);
	stream->CDF (2, 0.6);
	stream->CDF (3, 0.7);
	stream->CDF (5, 0.75);
	stream->CDF (7, 0.8);
	stream->CDF (40, 0.8125);
	stream->CDF (72, 0.825);
	stream->CDF (137, 0.85);
	stream->CDF (267, 0.9);
	stream->CDF (1187, 0.95);
	stream->CDF (2107, 1.0);
	mean = 134;
	maxx = 2107;

	if(!Settings::pktsmode){
		mean = mean*1538;
		maxx = maxx*1460;
	}

	Settings::maxx = maxx;
	Settings::homa_longflow_threshold = 100.0*1024;

	Settings::cdfFromFile.clear();
	Settings::cdfFromFile.push_back(std::make_pair(1*1460,0.5));
	Settings::cdfFromFile.push_back(std::make_pair(2*1460,0.6));
	Settings::cdfFromFile.push_back(std::make_pair(3*1460,0.7));
	Settings::cdfFromFile.push_back(std::make_pair(5*1460,0.75));
	Settings::cdfFromFile.push_back(std::make_pair(7*1460,0.8));
	Settings::cdfFromFile.push_back(std::make_pair(40*1460,0.8125));
	Settings::cdfFromFile.push_back(std::make_pair(72*1460,0.825));
	Settings::cdfFromFile.push_back(std::make_pair(137*1460,0.85));
	Settings::cdfFromFile.push_back(std::make_pair(267*1460,0.9));
	Settings::cdfFromFile.push_back(std::make_pair(1187*1460,0.95));
	Settings::cdfFromFile.push_back(std::make_pair(2107*1460,1.0));

	return stream;
}

Ptr<RandomVariableStream> FlowGenerator::GetBurstStream (uint32_t seed, uint32_t& mean, uint32_t& maxx) { // KB
	Ptr<EmpiricalRandomVariable> stream = CreateObject<EmpiricalRandomVariable> ();
	stream->SetStream (seed);
	stream->CDF (1, 0.0);
	stream->CDF (200, 0.5);
	stream->CDF (1000, 1.0);
	mean = 350;
	maxx = 1000;

	if(!Settings::pktsmode){
		mean = mean*1538;
		maxx = maxx*1460;
	}

	Settings::maxx = maxx;
	Settings::homa_longflow_threshold = 100.0*1024;
	return stream;
}

Ptr<RandomVariableStream> FlowGenerator::GetFabricatedHeavyMiddle (uint32_t seed, uint32_t & mean, uint32_t & maxx, uint32_t flow_cdf){
	Ptr<EmpiricalRandomVariable> stream = CreateObject<EmpiricalRandomVariable> ();
	stream->SetStream(seed);
	for(std::vector<std::pair<uint32_t,double> >::iterator sizeProb=Settings::cdfFromFile.begin(); sizeProb!=Settings::cdfFromFile.end();sizeProb++){
		stream->CDF(sizeProb->first,sizeProb->second);
		if(sizeProb->second == 1)
			maxx = sizeProb->first;
	}

	Settings::maxx = maxx;

	mean = Settings::avgSizeFromFile;
	if(flow_cdf==8)
		mean *= 1460;

	return stream;
}

} /* namespace ns3 */
