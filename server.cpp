#include <string>
#include <thread>
#include <vector>
#include <iostream>
#include <fstream>
#include "packet.cpp"

using namespace std;

std::vector<char> getFileBuffer(std::string filePath);

int main()
{
	std::vector<char> fileBytes = getFileBuffer("./test.txt");
	std::cout << fileBytes.size() << std::endl;
	std::cout << "file bytes size " << fileBytes.size() << std::endl;
	
	Packet p(22,2,22,true,true,false, (void*) fileBytes.data(), fileBytes.size());
	std::cout<< p.getSeqNum() <<std::endl;
	std::cout<< p.getAckNum() <<std::endl;
	std::cout<< p.getWindowSize() <<std::endl;
	std::cout<< p.getAck() <<std::endl;
	std::cout<< p.getSyn() <<std::endl;
	std::cout<< p.getFin() <<std::endl;
	std::cout<< p.getDataSize() <<std::endl;
	
	
	
}

std::vector<char> getFileBuffer(std::string filePath) {
	std::ifstream file(filePath, std::ios::binary | std::ios::ate);
	std::streamsize size = file.tellg();
	file.seekg(0, std::ios::beg);

	std::vector<char> buffer(size);
	file.read(buffer.data(), size);
	//buffer.back() = '\0';
	return buffer;
}

