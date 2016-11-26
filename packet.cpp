#ifndef PACKET_
#define PACKET_

#include <iostream>
#include <cstring>

class Packet{
private:
void copyInt16(uint16_t value, void* buffer) {
	std::memcpy(buffer, reinterpret_cast<void*>(&value), 2);
}

public:
	size_t DATA_SIZE = 1024;
	size_t HEADERSIZE = 8;
	void* buf;
	uint16_t seqNum, ackNum, windowSize;
	bool ack, syn, fin;
	size_t dataSize;
	
	//Assumes buf is 1000 bytes
	Packet(uint16_t seqNum_, uint16_t ackNum_, uint16_t windowSize_, bool ack_, bool syn_, bool fin_, void* data_, size_t dataSize_) :
	seqNum(seqNum_), ackNum(ackNum_), windowSize(windowSize_), ack(ack_), syn(syn_), fin(fin_), dataSize(dataSize_)
	{	
		
		if(dataSize > DATA_SIZE || dataSize < 0){
			std::cerr << "Invalid data size" << std::endl;
		}
		void* buffer[dataSize_ + HEADERSIZE];
		buf = buffer;
		
		
		memset(buf, '\0', dataSize_ + HEADERSIZE);
		copyInt16(seqNum, buf);
		copyInt16(ackNum, ((char*)buf)+2);
		copyInt16(windowSize, ((char*)buf)+4);
		int16_t header4 = 0;
		
		header4 = header4 | (fin << 0);
		header4 = header4 | (syn << 1);
		header4 = header4 | (ack << 2);
		
		copyInt16(header4, ((char*)buf)+6);
		
		std::memcpy(((char*)buf)+HEADERSIZE, data_, dataSize);
	}
	
	Packet(void* rawPacket, int packetSize){
		
	}
	
	uint16_t getSeqNum(){
		void* temp[2];
		std::memcpy(temp, buf, 2);
		int16_t* reint = reinterpret_cast<int16_t*>(temp);
		return *reint;
	}
	
	uint16_t getAckNum(){
		void* temp[2];
		std::memcpy(temp, ((char*)buf)+2, 2);
		int16_t* reint = reinterpret_cast<int16_t*>(temp);
		return *reint;
	}
	uint16_t getWindowSize(){
		void* temp[2];
		std::memcpy(temp, ((char*)buf)+4, 2);
		int16_t* reint = reinterpret_cast<int16_t*>(temp);
		return *reint;
	}
	bool getFin(){
		void* temp[2];
		std::memcpy(temp, ((char*)buf)+6, 2);
		int16_t* reint = reinterpret_cast<int16_t*>(temp);
		return (*reint & 1);
	}
	
	bool getSyn(){
		void* temp[2];
		std::memcpy(temp, ((char*)buf)+6, 2);
		int16_t* reint = reinterpret_cast<int16_t*>(temp);
		return (*reint & (1 << 1));
	}
	bool getAck(){
		void* temp[2];
		std::memcpy(temp, ((char*)buf)+6, 2);
		int16_t* reint = reinterpret_cast<int16_t*>(temp);
		return (*reint & (1 << 2));
	}
};

#endif