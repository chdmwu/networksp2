#include <string>
#include <thread>
#include <iostream>
#include "packet.cpp"

int main()
{	
	void* buf[1024];
  	Packet p(22,2,22,true,true,false,buf,1024);
	Packet p2(p.buf, 1032);
	std::cout<< p.getSeqNum() <<std::endl;
	std::cout<< p.getAckNum() <<std::endl;
	std::cout<< p.getWindowSize() <<std::endl;
	std::cout<< p.getAck() <<std::endl;
	std::cout<< p.getSyn() <<std::endl;
	std::cout<< p.getFin() <<std::endl;
	std::cout<< p2.getSeqNum() <<std::endl;
	std::cout<< p2.getAckNum() <<std::endl;
	std::cout<< p2.getWindowSize() <<std::endl;
	std::cout<< p2.getAck() <<std::endl;
	std::cout<< p2.getSyn() <<std::endl;
	std::cout<< p2.getFin() <<std::endl;
}
