/*
 * hash.h
 *
 *  Created on: Dec 20, 2020
 *      Author: wqy
 */

#ifndef SRC_POINT_TO_POINT_UTIL_HASH_FUNCTIONS_H_
#define SRC_POINT_TO_POINT_UTIL_HASH_FUNCTIONS_H_

namespace ns3 {
	class HashFunctions {
	public:
		static unsigned int RSHash(char* str, unsigned int len);
		static unsigned int JSHash(char* str, unsigned int len);
		static unsigned int PJWHash(char* str, unsigned int len);
		static unsigned int ELFHash(char* str, unsigned int len);
		static unsigned int BKDRHash(char* str, unsigned int len);
		static unsigned int SDBMHash(char* str, unsigned int len);
		static unsigned int DJBHash(char* str, unsigned int len);
		static unsigned int DEKHash(char* str, unsigned int len);
		static unsigned int BPHash(char* str, unsigned int len);
		static unsigned int FNVHash(char* str, unsigned int len);
		static unsigned int APHash(char* str, unsigned int len);
	};
}
#endif /* SRC_POINT_TO_POINT_UTIL_HASH_FUNCTIONS_H_ */
