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
#include <mutex>


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
	const size_t MAX_SEQ_NUM = 30720;
	uint16_t seqNum, ackNum, windowSize = 15*MSS, lastAckedPacket;
	size_t MAX_MSG_SIZE = MSS + Packet::HEADERSIZE;
	std::mutex writeLock;
	int sockfd;
	struct sockaddr_in* serverAddr;
	socklen_t serverAddrSize;

	int retrans = 500; //ms
	ClientState(int sockfd_, sockaddr_in* serverAddr_){
		seqNum = 0;//TODO random
		lastAckedPacket = seqNum;
		sockfd = sockfd_;
		serverAddr = serverAddr_;
	}

	void recvPacket(string filePath = ""){
		void* buf[MAX_MSG_SIZE];
		int bytesRecved = 0;

		memset(buf, 0, MAX_MSG_SIZE);
		bytesRecved = recvfrom(sockfd, buf, MAX_MSG_SIZE, 0, (sockaddr*) serverAddr, &serverAddrSize);
		if (bytesRecved == -1) {
			perror("recv");
		}
		Packet recv(buf, bytesRecved);
		//only increment ack if its the packet we expected
		if(ackNum == recv.getSeqNum()){
			ackNum = (recv.getSeqNum() + max(recv.getDataSize(), ONE)) % MAX_SEQ_NUM;
			if(recv.getDataSize() > 0){
				writeLock.lock();
				recv.writeToFile(filePath);
				writeLock.unlock();
			}
		} else {
			// TODO: save out of order packets, deal with writing them to file once we have all of them
		}
		void* dummy = 0;
		cout << "Receiving packet " << ackNum << " length " << recv.getDataSize() << endl;

		// Send ack for this packet
		this->sendPacket(dummy, 0, 0, 1, 0);

	}
	void sendPacket(void* buf, size_t size, bool syn, bool ack, bool fin){
		Packet pSend(seqNum, ackNum, windowSize, syn, ack, fin, buf, size);
		//pSend.sendPacket(sockfd);
		int bytes = sendto(sockfd, pSend.getRawPacketPointer(), pSend.getRawPacketSize(), 0, (sockaddr*) serverAddr, serverAddrSize);
		if(bytes == -1){
			std::cerr << "ERROR send" << endl;
		}
		cout << "Sending packet " << seqNum << " acking " << ackNum;
		if(syn){
			cout << " " << "SYN";
		}
		if(fin){
			cout << " " << "FIN";
		}
		cout << endl;

		seqNum = (seqNum + max(pSend.getDataSize(), ONE)) % MAX_SEQ_NUM;
	}
};

void recvDataPacketThread(string fileName, ClientState* clientState);


int main(int argc, char *argv[]){
	string fileName = "./testout.jpg";

	//Delete old file, if it exists
	std::remove(fileName.c_str());
	string hostname = "localhost";
	int port = 4000;




	string ip = getIP(hostname);
	int sockfd;
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);

	struct sockaddr_in* clientAddr = new sockaddr_in;;
	clientAddr->sin_family = AF_INET;
	clientAddr->sin_port = htons(4001);     // short, network byte order, any port
	clientAddr->sin_addr.s_addr = inet_addr(ip.c_str());
	memset(clientAddr->sin_zero, '\0', sizeof(clientAddr->sin_zero));
	/*
	if (getsockname(sockfd, (struct sockaddr *) clientAddr, &clientAddrLen) == -1) {
		perror("getsockname");
	}*/
	if (bind(sockfd, (struct sockaddr*) clientAddr, sizeof(*clientAddr)) == -1) {
		perror("bind");
	}


	char ipstr[INET_ADDRSTRLEN] = {'\0'};
	inet_ntop(clientAddr->sin_family, &clientAddr->sin_addr, ipstr, sizeof(ipstr));
	std::cout << "Set up a connection from: " << ipstr << ":" <<
			ntohs(clientAddr->sin_port) << std::endl;

	/*
	struct sockaddr_in* serverAddr = new sockaddr_in;
	serverAddr->sin_family = AF_INET;
	serverAddr->sin_port = htons(port);     // short, network byte order
	serverAddr->sin_addr.s_addr = inet_addr(ip.c_str());
	memset(serverAddr->sin_zero, '\0', sizeof(serverAddr->sin_zero));
	*/
	struct sockaddr_in serverAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(4000);     // short, network byte order
	serverAddr.sin_addr.s_addr = inet_addr(ip.c_str());
	memset(serverAddr.sin_zero, '\0', sizeof(serverAddr.sin_zero));

	string msg = "hello";
	sendto(sockfd, msg.c_str(), msg.size(), 0, (sockaddr*) &serverAddr, sizeof(serverAddr));


	//TCP State variables
	//ClientState* clientState = new ClientState(sockfd, serverAddr);
	void* dummy = 0;

	//clientState->sendPacket(dummy,0,1,0,0);


	//Ready to recv data
	//recvDataPacketThread(fileName,clientState);


	//delete(clientState);
	//close(sockfd);
	cout << "Connection closed..." << endl;
}

void recvDataPacketThread(string fileName, ClientState* clientState){
	while(true){ //TODO fin
		clientState->recvPacket(fileName);
	}
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

