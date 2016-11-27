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

	int retrans = 500; //ms
	ClientState(){
		seqNum = 0;//TODO random
		lastAckedPacket = seqNum;
	}

	void recvPacket(int sockfd, string filePath = ""){
		void* buf[MAX_MSG_SIZE];
		int bytesRecved = 0;

		memset(buf, 0, MAX_MSG_SIZE);
		bytesRecved = recv(sockfd, buf, MAX_MSG_SIZE, 0);
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
		this->sendPacket(sockfd, dummy, 0, 0, 1, 0);

	}
	void sendPacket(int sockfd, void* buf, size_t size, bool syn, bool ack, bool fin){
		Packet pSend(seqNum, ackNum, windowSize, syn, ack, fin, buf, size);
		pSend.sendPacket(sockfd);
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

void recvDataPacketThread(int sockfd, string fileName, ClientState* clientState);


int main(int argc, char *argv[]){
	string fileName = "./testout.jpg";

	//Delete old file, if it exists
	std::remove(fileName.c_str());
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
	ClientState* clientState = new ClientState;
	void* dummy = 0;

	//3 way handshake
	//SEND SYN
	clientState->sendPacket(sockfd,dummy,0,1,0,0);
	//RECV SYN ACK
	//clientState->recvPacket(sockfd);
	//SEND ACK
	//clientState->sendPacket(sockfd,dummy,0,0,1,0);

	//Ready to recv data

	//RECV DATA
	//clientState.recvPacket(sockfd,fileName);
	//std::thread recvDataThread(recvDataPacketThread, sockfd, fileName, clientState);
	//recvDataThread.detach();

	recvDataPacketThread(sockfd,fileName,clientState);


	delete(clientState);
	close(sockfd);
	cout << "Connection closed..." << endl;
}

void recvDataPacketThread(int sockfd, string fileName, ClientState* clientState){
	while(true){ //TODO fin
		clientState->recvPacket(sockfd,fileName);
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

