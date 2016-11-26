#include <string>
#include <thread>
#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <sstream>
#include <fstream>
#include <netdb.h>
#include "packet.cpp"


using std::string;
using std::cout;
using std::endl;
using std::ios;
using std::ifstream;

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

int main()
{
	string hostname = "localhost";
	int port = 4000;
	string ip = getIP(hostname);

	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in serverAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(port);     // short, network byte order
	serverAddr.sin_addr.s_addr = inet_addr(ip.c_str());
	memset(serverAddr.sin_zero, '\0', sizeof(serverAddr.sin_zero));
	if (connect(sockfd, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) == -1) {
		perror("connect");
		return 2;
	}
	cout << "Connection established..." << endl;

	struct sockaddr_in clientAddr;
	socklen_t clientAddrLen = sizeof(clientAddr);
	if (getsockname(sockfd, (struct sockaddr *)&clientAddr, &clientAddrLen) == -1) {
		perror("getsockname");
		return 3;
	}

	char ipstr[INET_ADDRSTRLEN] = {'\0'};
	inet_ntop(clientAddr.sin_family, &clientAddr.sin_addr, ipstr, sizeof(ipstr));
	std::cout << "Set up a connection from: " << ipstr << ":" <<
			ntohs(clientAddr.sin_port) << std::endl;



	size_t MAX_MSG_SIZE = 1032;
	void* buf[MAX_MSG_SIZE];
	memset(buf, '\0', sizeof(buf));
	int bytesRecved = recv(sockfd, buf, MAX_MSG_SIZE, 0);
	cout << "Recved packet... " << endl;
	Packet pRecv(buf, bytesRecved);
	pRecv.printInfo();


	void* delme[1];
	memset(delme, '\0', sizeof(delme));
		Packet p(22,2,22,true,true,false,delme,1);
		cout << p.getRawPacketSize() << "raw packet size send" << endl;
		if (send(sockfd, delme, 1, 0) == -1) {
			perror("send");
			return 4;
		}

}
