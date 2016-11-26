#include <string>
#include <thread>
#include <iostream>
#include "packet.cpp"
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
#include <vector>
#include <algorithm>


using std::string;
using std::cout;
using std::endl;
using std::ios;
using std::ifstream;
using std::vector;
using std::istringstream;
using std::ofstream;
using std::max;

string getIP(string host);

class ClientState {
public:
	const size_t MSS = Packet::MAX_DATA_SIZE;
	const size_t ONE = 1;
	uint16_t seqNum, ackNum, windowSize = 15*MSS;
	size_t MAX_MSG_SIZE = 10000; //TODO fix

	int retrans = 500; //ms
	ClientState(){
		seqNum = 0;//TODO random
	}

	void recvPacket(int clientSockfd, string filePath = ""){
		void* buf[MAX_MSG_SIZE];
		int bytesRecved = 0;

		memset(buf, 0, MAX_MSG_SIZE);
		bytesRecved = recv(clientSockfd, buf, MAX_MSG_SIZE, 0);
		if (bytesRecved == -1) {
			perror("recv");
		}
		Packet recv(buf, bytesRecved);
		ackNum = recv.getSeqNum() + max(recv.getDataSize(), ONE);
		cout << "Receiving packet " << ackNum << endl;
		if(recv.getDataSize() > 0){
			recv.writeToFile(filePath);
		}
	}
	void sendPacket(int sockfd, void* buf, size_t size, bool syn, bool ack, bool fin){
		Packet pSend(seqNum, ackNum, windowSize, syn, ack, fin, buf, size);
		pSend.sendPacket(sockfd);
		cout << "Sending packet " << seqNum;
		if(syn){
			cout << " " << "SYN";
		}
		if(fin){
			cout << " " << "FIN";
		}
		cout << endl;

		seqNum += max(pSend.getDataSize(), ONE);
	}
};

int main(int argc, char *argv[]){
	string fileName = "./testout.txt";
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

	//TCP State variables
	ClientState clientState;
	void* dummy = 0;

	//SEND SYN
	clientState.sendPacket(sockfd,dummy,0,1,0,0);
	//RECV SYN ACK
	clientState.recvPacket(sockfd);
	//SEND ACK
	clientState.sendPacket(sockfd,dummy,0,0,1,0);
	//RECV DATA
	clientState.recvPacket(sockfd,fileName);

	/**
	//SEND SYN
	Packet sendSyn(seqNum,ackNum,windowSize,0,1,0,dummy,0);
	if (sendSyn.sendPacket(sockfd) == -1) {
		perror("send");
		return 4;
	}
	seqNum += max(sendSyn.getDataSize(), ONE);
	cout << "Sending packet " << ackNum << " " << "SYN" << endl;

	//RECV SYN ACK
	memset(buf, 0, MAX_MSG_SIZE);
	bytesRecved = recv(sockfd, buf, MAX_MSG_SIZE, 0);
	if (bytesRecved == -1) {
		perror("recv");
	}
	Packet recvSyn(buf, bytesRecved);
	ackNum = recvSyn.getSeqNum() + max(recvSyn.getDataSize(), ONE);
	cout << "Receiving packet " << ackNum << endl;

	//SEND ACK
	Packet sendAck(seqNum,ackNum,windowSize,1,1,0,dummy,0);
	if (sendAck.sendPacket(sockfd) == -1) {
		perror("send");
		return 4;
	}
	cout << "Sending packet " << ackNum << endl;

	//RECV data
	memset(buf, 0, MAX_MSG_SIZE);
	bytesRecved = recv(sockfd, buf, MAX_MSG_SIZE, 0);
	if (bytesRecved == -1) {
		perror("recv");
	}
	Packet dataPacket(buf, bytesRecved);
	ackNum = dataPacket.getSeqNum() + max(dataPacket.getDataSize(), ONE);
	cout << "Receiving packet " << ackNum << endl;
	dataPacket.writeToFile(fileName);
	*/

	/**
	size_t MAX_MSG_SIZE = 10000;
	void* buf[MAX_MSG_SIZE];
	memset(buf, '\0', sizeof(buf));
	int bytesRecved = recv(sockfd, buf, MAX_MSG_SIZE, 0);
	if (bytesRecved == -1) {
		perror("recv");
		return 5;
	}
	Packet pRecv(buf, bytesRecved);
	pRecv.printInfo();
	char* writeBuf[pRecv.getDataSize()];
	pRecv.getData(writeBuf);
	//writeToFile(writeBuf, pRecv.getDataSize(),"./test2.txt");
	pRecv.writeToFile("./test2.txt");
	*/

	close(sockfd);
	cout << "Connection closed..." << endl;
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

