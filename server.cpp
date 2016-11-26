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
#include <vector>
#include <algorithm>
#include <unistd.h>


#include <fstream>
#include <iostream>
#include <sstream>

#include "packet.cpp"

using std::string;
using std::cout;
using std::endl;
using std::ios;
using std::ifstream;
using std::vector;
using std::max;

vector<char> getFileBuffer(string filePath);
string getIP(string host);


class ServerState {
public:
	const size_t MSS = Packet::MAX_DATA_SIZE;
	const size_t ONE = 1;
	const size_t MAX_SEQ_NUM = 30720;
	uint16_t seqNum, ackNum, windowSize = MSS * 15, cwnd = MSS, ssthresh = MSS * 15, clientWindowSize = 1024, lastAckedPacket;
	//size_t MAX_MSG_SIZE = MSS + Packet::HEADERSIZE;
	size_t MAX_ACK_SIZE = Packet::HEADERSIZE;

	int retrans = 500; //ms
	ServerState(){
		seqNum = 0;//TODO random, deal with acks looping around
		lastAckedPacket = seqNum;
	}
	//returns latest ack of the packet recvd
	uint16_t recvPacket(int clientSockfd){
		void* buf[MAX_ACK_SIZE];
		int bytesRecved = 0;

		memset(buf, 0, MAX_ACK_SIZE);
		bytesRecved = recv(clientSockfd, buf, MAX_ACK_SIZE, 0);
		if (bytesRecved == -1) {
			perror("recv");
		}
		Packet recv(buf, bytesRecved);
		cout << "recv " << recv.getSeqNum() << " size " << recv.getDataSize();
		ackNum = (recv.getSeqNum() + max(recv.getDataSize(), ONE)) % MAX_SEQ_NUM;

		// Deal with updating last Ack when ack numbers loop around
		if(recv.getAck() < (MAX_SEQ_NUM/2) && lastAckedPacket > (MAX_SEQ_NUM/2)){
			lastAckedPacket = recv.getAckNum();
		} else {
			lastAckedPacket = max(lastAckedPacket, recv.getAckNum());
		}


		clientWindowSize = recv.getWindowSize();
		cout << "Receiving packet " << ackNum << " laskAcked " << lastAckedPacket << endl;

		return lastAckedPacket;
	}
	//returns seqNum next packet to send
	uint16_t sendPacket(int clientSockfd, void* buf, size_t size, bool syn, bool ack, bool fin){
		Packet pSend(seqNum, ackNum, windowSize, syn, ack, fin, buf, size);

		// Deal with finding out how many bytes are unacked when ack loops around
		size_t unackedBytes = getUnackedBytes();
		//If we have too many unacked packets, wait.
		//cout << "Unacked bytes... " << unackedBytes  << " seqNum "<< seqNum << " last acked " << lastAckedPacket << endl;
		while(unackedBytes > clientWindowSize){
			usleep(1000);
			unackedBytes = getUnackedBytes();
			//cout << "Waiting... " << bytesOutstanding << endl;
		}
		pSend.sendPacket(clientSockfd);
		cout << "Sending packet " << seqNum << " " << cwnd << " " << ssthresh << " length " << pSend.getDataSize();
		if(syn){
			cout << " " << "SYN";
		}
		if(fin){
			cout << " " << "FIN";
		}
		cout << endl;

		seqNum = (seqNum + max(pSend.getDataSize(), ONE)) % MAX_SEQ_NUM;
		return seqNum;
	}

	size_t getUnackedBytes(){
		if(seqNum < (MAX_SEQ_NUM/2) && lastAckedPacket > (MAX_SEQ_NUM/2)){
			return seqNum + (MAX_SEQ_NUM - lastAckedPacket); // numbers looped around
		}
		return seqNum - lastAckedPacket; //numbers didnt loop around
	}
};

void sendDataPacketsThread(int clientSockfd, ServerState* serverState, string fileDir);
void recvPacketsThread(int clientSockfd, ServerState* serverState);



int main(int argc, char *argv[])
{
	string fileDir = "./files/bear.jpg"; // TODO change
	//string fileDir = "./files/bear.jpg"; // TODO change
	string hostname;
	int port;
	string ip;

	if(argc <= 1){
		hostname = "localhost";
		port = 4000;
	}
	else if(argc == 4){
		hostname = string(argv[1]);
		port = atoi(argv[2]);
	}

	ip = getIP(hostname);

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
	// TCP state variables
	ServerState* serverState = new ServerState;


	while(runServer){
		// accept a new connection
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

		void* dummy = 0;

		//Recving SYN
		serverState->recvPacket(clientSockfd);
		//Sending SYN ACK
		serverState->sendPacket(clientSockfd, dummy, 0, 1, 1, 0);
		//Recv ACK
		serverState->recvPacket(clientSockfd);
		//Read data
		std::vector<char> fileBytes = getFileBuffer(fileDir);
		//Send data packet
		//serverState.sendPacket(clientSockfd,fileBytes.data(),fileBytes.size(), 0, 1, 0);

		std::thread sendThread(sendDataPacketsThread, clientSockfd, serverState, fileDir);
		sendThread.detach();

		recvPacketsThread(clientSockfd, serverState);
	}
	delete(serverState);
}

vector<char> getFileBuffer(string filePath) {
	std::ifstream file(filePath, std::ios::binary | std::ios::ate);
	if (!file.is_open()){
		std::cerr << "Error opening file";
	}
	std::streamsize size = file.tellg();
	file.seekg(0, std::ios::beg);


	std::vector<char> buffer(size);
	file.read(buffer.data(), size);
	//buffer.back() = '\0';
	return buffer;
}

void recvPacketsThread(int clientSockfd, ServerState* serverState){
	while(true){
		serverState->recvPacket(clientSockfd);
	}
}

void sendDataPacketsThread(int clientSockfd, ServerState* serverState, string fileDir){
	std::vector<char> fileBytes = getFileBuffer(fileDir);
	int dataSent = 0;
	char* data = fileBytes.data();
	int totalData = fileBytes.size();
	while(dataSent < totalData){
		if(totalData - dataSent < serverState->MSS){
			serverState->sendPacket(clientSockfd,data + dataSent,totalData - dataSent,0,1,0);
			dataSent += totalData - dataSent;
		} else {
			serverState->sendPacket(clientSockfd,data + dataSent,serverState->MSS,0,1,0);
			dataSent += serverState->MSS;
		}
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
