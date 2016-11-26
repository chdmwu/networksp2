
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <thread>
#include <netdb.h>


#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>
#include "packet.cpp"


using std::string;
using std::cout;
using std::endl;
using std::ios;
using std::ifstream;

std::vector<char> getFileBuffer(std::string filePath);
string getIP(string host);


int main(){

	string fileDir = "./files/test.txt";
	string hostname = "localhost";
	int port = 4000;
	string ip = getIP(hostname);
	std::cout << "Creating server" << std::endl;
	// create a socket using TCP IP
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);


	// allow others to reuse the address
	int yes = 1;
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
		perror("setsockopt");
		return 1;
	}

	// bind address to socket
	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);     // short, network byte order
	addr.sin_addr.s_addr = inet_addr(ip.c_str());
	memset(addr.sin_zero, '\0', sizeof(addr.sin_zero));

	if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
		perror("bind");
		return 2;
	}

	// set socket to listen status
	if (listen(sockfd, 1) == -1) {
		perror("listen");
		return 3;
	}

	bool runServer = true;
	while(runServer){
		struct sockaddr_in clientAddr;
		socklen_t clientAddrSize = sizeof(clientAddr);
		int clientSockfd = accept(sockfd, (struct sockaddr*)&clientAddr, &clientAddrSize);

		if (clientSockfd == -1) {
			perror("accept");
			return 4;
		}
		char ipstr[INET_ADDRSTRLEN] = {'\0'};
		inet_ntop(clientAddr.sin_family, &clientAddr.sin_addr, ipstr, sizeof(ipstr));
		std::cout << "Accept a connection from: " << ipstr << ":" <<
				ntohs(clientAddr.sin_port) << std::endl;


		std::vector<char> fileBytes = getFileBuffer(fileDir);
		Packet p(22,2,22,true,true,false, (void*) fileBytes.data(), fileBytes.size());
		std::cout << "Sending packet size " << p.getRawPacketSize() << ", File size " << fileBytes.size() << std::endl;

		if (send(clientSockfd, p.getRawPacketPointer(), p.getRawPacketSize(), 0) == -1){
						perror("send");
		}



		size_t MAX_MSG_SIZE = 1032;
		void* buf[MAX_MSG_SIZE];
		memset(buf, '\0', sizeof(buf));
		int bytesRecved = recv(sockfd, buf, MAX_MSG_SIZE, 0);
		if(bytesRecved == -1){
			std::cerr << "error recv";
		}
		cout << "Recved packet... " << bytesRecved <<  endl;
		Packet pRecv(buf, bytesRecved);
		pRecv.printInfo();

	}

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


string getIP(string host){
	struct addrinfo hints;
	  struct addrinfo* res;

	  // prepare hints
	  memset(&hints, 0, sizeof(hints));
	  hints.ai_family = AF_INET; // IPv4
	  hints.ai_socktype = SOCK_STREAM; // TCP

	  // get address
	  int status = 0;
	  if ((status = getaddrinfo(host.c_str(), "80", &hints, &res)) != 0) {
	    std::cerr << "getaddrinfo: " << gai_strerror(status) << std::endl;
	  }

	  std::cout << "IP addresses for " << host << ": " << std::endl;

	  for(struct addrinfo* p = res; p != 0; p = p->ai_next) {
	    // convert address to IPv4 address
	    struct sockaddr_in* ipv4 = (struct sockaddr_in*)p->ai_addr;

	    // convert the IP to a string and print it:
	    char ipstr[INET_ADDRSTRLEN] = {'\0'};
	    inet_ntop(p->ai_family, &(ipv4->sin_addr), ipstr, sizeof(ipstr));
	    std::cout << "  " << ipstr << std::endl;
	    // std::cout << "  " << ipstr << ":" << ntohs(ipv4->sin_port) << std::endl;
	    return string(ipstr);
	  }

	  freeaddrinfo(res); // free the linked list
	  return "";
}

