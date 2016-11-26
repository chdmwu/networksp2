#include <string>
#include <thread>
#include <iostream>
#include "packet.cpp"

int main()
{	
	void* buf[1024];
  	Packet p(17,55,999,true,true,false,buf,1024 );
	std::cout<< p.getSeqNum() <<std::endl;
	std::cout<< p.getAckNum() <<std::endl;
	std::cout<< p.getWindowSize() <<std::endl;
	std::cout<< p.getAck() <<std::endl;
	std::cout<< p.getSyn() <<std::endl;
	std::cout<< p.getFin() <<std::endl;
}
