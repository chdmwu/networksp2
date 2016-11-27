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
using std::min;

vector<char> getFileBuffer(string filePath);
string getIP(string host);


class ServerState {
public:
	const size_t MSS = Packet::MAX_DATA_SIZE;
	const size_t ONE = 1;
	const size_t MAX_SEQ_NUM = 30720;
	uint16_t seqNum, ackNum, windowSize = MSS * 15, cwnd = MSS, ssthresh = MSS * 15, clientWindowSize = 1024, lastAckedPacket;
	uint16_t prevAck;
	//size_t MAX_MSG_SIZE = MSS + Packet::HEADERSIZE;
	size_t MAX_ACK_SIZE = Packet::HEADERSIZE;
	string state; // SS, CA, FR <- allowable states
	int dupAcks;
	struct sockaddr clientAddr;
	socklen_t clientAddrSize;
	int retrans = 500; //ms
	ServerState(){
		seqNum = 0;//TODO random
		lastAckedPacket = seqNum;
		dupAcks = 0;
		state = "SS";
	}
	~ServerState(){
	}
	//returns latest ack of the packet recvd
	uint16_t recvPacket(int sockfd){
		void* buf[MAX_ACK_SIZE];
		int bytesRecved = 0;

		memset(buf, 0, MAX_ACK_SIZE);
		struct sockaddr addr2;
		//socklen_t fromlen;
		//Receive a packet
		clientAddrSize = sizeof(clientAddr);
		bytesRecved = recvfrom(sockfd, buf, MAX_ACK_SIZE, 0, &addr2, &clientAddrSize);
		memcpy(&clientAddr,&addr2,clientAddrSize);
		if (bytesRecved == -1) {
			perror("recv");
		}
		if(bytesRecved < MAX_ACK_SIZE){
			return 0;
		}
		Packet recv(buf, bytesRecved);
		//cout << "recv " << recv.getSeqNum() << " size " << recv.getDataSize();
		ackNum = (recv.getSeqNum() + max(recv.getDataSize(), ONE)) % MAX_SEQ_NUM;

		// Deal with updating last Ack when ack numbers loop around
		if(recv.getAck() < (MAX_SEQ_NUM/2) && lastAckedPacket > (MAX_SEQ_NUM/2)){
			lastAckedPacket = recv.getAckNum();
		} else {
			lastAckedPacket = max(lastAckedPacket, recv.getAckNum());
		}

		clientWindowSize = recv.getWindowSize();

		//update cwnd
		//new ack
		if(lastAckedPacket != prevAck){
			if(state == "SS"){
				cwnd += MSS;
				dupAcks = 0;
				if(cwnd >= ssthresh){
					state = "CA";
				}
			} else if(state == "CA") {
				cwnd += 1; //add one, said in project specs?
				dupAcks = 0;
			} else if(state == "FR") {
				dupAcks = 0;
				cwnd = ssthresh;
				//TODO Fast recovery and timeouts
			}
		} else if (lastAckedPacket == prevAck){ // dup ack
			if(state == "SS"){
				dupAcks++;
			} else if(state == "CA") {
				dupAcks++;
			} else if(state == "FR") {
				cwnd += MSS;
				//TODO Fast recovery and timeouts
			}
			if(dupAcks >= 3 && (state == "SS" || state == "CA")){
				state = "FR";
				ssthresh = max(cwnd/2, (int) MSS);
				cwnd = ssthresh + 3 * MSS;
			}

		}

		cout << "Receiving packet " << ackNum << " laskAcked " << lastAckedPacket << endl;
		prevAck = lastAckedPacket;
		return lastAckedPacket;
	}
	//returns seqNum next packet to send
	uint16_t sendPacket(int sockfd, void* buf, size_t size, bool syn, bool ack, bool fin){
		Packet pSend(seqNum, ackNum, windowSize, syn, ack, fin, buf, size);

		// Deal with finding out how many bytes are unacked when ack loops around
		size_t unackedBytes = getUnackedBytes();
		//If we have too many unacked packets, wait.
		//cout << "Unacked bytes... " << unackedBytes  << " seqNum "<< seqNum << " last acked " << lastAckedPacket << endl;
		while(unackedBytes > min(clientWindowSize, cwnd)){
			usleep(10000);
			unackedBytes = getUnackedBytes();
			//cout << "Waiting... " << bytesOutstanding << endl;
		}
		//pSend.sendPacket(clientSockfd);
		int bytes = sendto(sockfd, pSend.getRawPacketPointer(), pSend.getRawPacketSize(), 0, &clientAddr, clientAddrSize);
		if(bytes == -1){
			std::cerr << "ERROR send" << endl;
		}
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
	int sockfd = socket(AF_INET, SOCK_DGRAM, 0);


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

/*
	char buf[10];
	memset(buf, 0, 10);
	struct sockaddr* addr2 = new sockaddr;
	socklen_t fromlen;
	cout<<"waiting"<<endl;
	recvfrom(sockfd, buf, 10, 0, addr2, &fromlen);
	cout<<"response"<<endl;
	cout<<buf<<endl;

	cout << "family "<< addr2->sa_family<<endl;
	cout << "data "<< addr2->sa_data[2]<<endl;
	string msg = "bye";
	sendto(sockfd, msg.c_str(),msg.size(),0,addr2,fromlen);
	*/


	// TCP state variables
	ServerState* serverState = new ServerState;
	bool runServer = true;


	while(runServer){

		void* dummy = 0;

		//serverState->recvPacket(sockfd); //WTF????
		//Recving SYN
		serverState->recvPacket(sockfd);
		//Sending SYN ACK
		serverState->sendPacket(sockfd, dummy, 0, 1, 1, 0);
		//Recv ACK
		serverState->recvPacket(sockfd);
		//Read data
		std::vector<char> fileBytes = getFileBuffer(fileDir);
		//Send data packet
		//serverState.sendPacket(clientSockfd,fileBytes.data(),fileBytes.size(), 0, 1, 0);

		std::thread sendThread(sendDataPacketsThread, sockfd, serverState, fileDir);
		sendThread.detach();

		recvPacketsThread(sockfd, serverState);

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

void recvPacketsThread(int sockfd, ServerState* serverState){
	while(true){
		serverState->recvPacket(sockfd);
	}
}

void sendDataPacketsThread(int sockfd, ServerState* serverState, string fileDir){
	std::vector<char> fileBytes = getFileBuffer(fileDir);
	int dataSent = 0;
	char* data = fileBytes.data();
	int totalData = fileBytes.size();
	while(dataSent < totalData){
		if(totalData - dataSent < serverState->MSS){
			serverState->sendPacket(sockfd,data + dataSent,totalData - dataSent,0,1,0);
			dataSent += totalData - dataSent;
		} else {
			serverState->sendPacket(sockfd,data + dataSent,serverState->MSS,0,1,0);
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
