/*
 * function.cc
 *
 *  Created on: Jan 14, 2018
 *      Author: tbc
 */

#include "function.h"
#include "ns3/settings.h"

namespace ns3 {

uint8_t (*Function::func)(uint32_t,uint32_t,uint32_t) = NULL;
double Function::para = 0;


void Function::SetMode(std::string mode,uint32_t nPrio, uint32_t maxPkts,uint32_t maxTimeNs){
	if(mode == "sjf"){
		para = nPrio/log(Settings::threshold_slope*maxPkts*1.01+Settings::threshold_bias);
		func = sjf;
	}else if(mode == "srtf"){
		para = nPrio/log(Settings::threshold_slope*maxPkts*1.01+Settings::threshold_bias);
		func = srtf;
	}else if(mode == "las"){
		para = nPrio/log(Settings::threshold_slope*maxPkts*1.01+Settings::threshold_bias);
		func = las;
	}else if(mode == "edf"){
		para = nPrio/log(maxTimeNs/1298.0*1.01);
		func = edf;
	}else if(mode == "lst"){
		para = nPrio/log(maxTimeNs/1298.0*1.01);
		func = lst;
	}else{
		std::cout << "invalid mode" << std::endl;
	}
}

uint8_t Function::sjf(uint32_t totalPkts, uint32_t remPkts, uint32_t remTimeNs){
	return (uint8_t)(log(Settings::threshold_slope*totalPkts+Settings::threshold_bias)*para);
}

uint8_t Function::srtf(uint32_t totalPkts, uint32_t remPkts, uint32_t remTimeNs){
	return (uint8_t)(log(Settings::threshold_slope*remPkts+Settings::threshold_bias)*para);
}

uint8_t Function::las(uint32_t totalPkts, uint32_t remPkts, uint32_t remTimeNs){
	return (uint8_t)(log(Settings::threshold_slope*(totalPkts-remPkts)+Settings::threshold_bias)*para);
}

uint8_t Function::edf(uint32_t totalPkts, uint32_t remPkts, uint32_t remTimeNs){
	double x = log(remTimeNs/1298.0);
	return x>0?((uint8_t)(x*para)):0xff;
}

uint8_t Function::lst(uint32_t totalPkts, uint32_t remPkts, uint32_t remTimeNs){
	if(remPkts*1298>=remTimeNs) return 0xff;
	double x = log(remTimeNs/1298.0-remPkts);
	return x>0?((uint8_t)(x*para)):0xff;
}

} /* namespace ns3 */
