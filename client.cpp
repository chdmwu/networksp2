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
#include <ctime>

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
	vector<Packet*> outOfOrderPackets;
    bool establishedTCP;
    bool finished;
    bool finSent;
	bool finRcv;

	ClientState(int sockfd_, sockaddr_in* serverAddr_){
		seqNum = rand() % MAX_SEQ_NUM;//TODO random
		lastAckedPacket = seqNum;
		sockfd = sockfd_;
		serverAddr = serverAddr_;
		serverAddrSize = sizeof(*serverAddr);
		establishedTCP=false;
		finished=false;
        finSent=false;
		finRcv=false;
	}

	void recvPacket(string filePath = ""){
		void* buf[MAX_MSG_SIZE];
		int bytesRecved = 0;

		memset(buf, 0, MAX_MSG_SIZE);
		bytesRecved = recvfrom(sockfd, buf, MAX_MSG_SIZE, 0, (sockaddr*) serverAddr, &serverAddrSize);
		if (bytesRecved == -1) {
			perror("recv");
		}
		//Packet recv(buf, bytesRecved);
		Packet* recv = new Packet(buf, bytesRecved);
		//only increment ack if its the packet we expected
        cout << "Receiving packet " << recv->getSeqNum() << endl;
        if (recv->getSyn() && !establishedTCP) {
            seqNum = (seqNum + ONE) % MAX_SEQ_NUM;
            ackNum = (recv->getSeqNum()+ONE) % MAX_SEQ_NUM;
            establishedTCP = true;

        }
		if (recv -> getFin() && !finRcv){
			//cout << "received fin" <<endl;
			finRcv = true;
			ackNum = (recv->getSeqNum()+ONE) % MAX_SEQ_NUM;
		}
		if (finRcv && finSent && recv->getAckNum()==seqNum+1 && recv->getSeqNum()==ackNum){
            //cout << "received ack of fin"<<endl;
			finished=true;
		}
        if (establishedTCP && !recv->getSyn() && !finRcv && !finished) {
            if (ackNum == recv->getSeqNum()) {
                ackNum = (recv->getSeqNum() + recv->getDataSize()) % MAX_SEQ_NUM;
                if (establishedTCP) {
                    writeLock.lock();
                    recv->writeToFile(filePath);
                    writeLock.unlock();


                    if (outOfOrderPackets.size() > 0) {
                        // try to write out of order packets, now that we got a new packet
                        // this function should update the ack accordingly ? untested
                        tryWriteOutOfOrderPackets(filePath);
                    }

                    delete (recv); // Delete packets after they are written to file
                }
            } else {
                bool new_packet = true;
                for (int ii = 0; ii < outOfOrderPackets.size(); ii++) {
                    Packet *stored_recv = outOfOrderPackets.at(ii);
                    if (stored_recv->getSeqNum() == recv->getSeqNum()) {
                        new_packet = false;
                        break;
                    }
                }
                // save out of order packets, deal with writing them to file when we get others
                if (new_packet) {
                    if ((ackNum > (MAX_SEQ_NUM/2) && (recv->getSeqNum() > ackNum || recv->getSeqNum()<=ackNum-(MAX_SEQ_NUM/2)))
                        || (ackNum <= (MAX_SEQ_NUM/2) && (recv->getSeqNum()>ackNum && recv->getSeqNum()<ackNum+(MAX_SEQ_NUM/2) )))
					{
                        outOfOrderPackets.push_back(recv);
                        //cout << "Packet out of order packet " << recv->getSeqNum() << endl;
                        //cout << "Saved out of order packet " << outOfOrderPackets.size() << endl;
                    }
                }
            }

        }
        void* dummy = 0;


        // Send ack for this packet
		if (!finished){
			this->sendPacket(dummy, 0, 0, 1, 0,0);
		}



	}
	void sendPacket(void* buf, size_t size, bool syn, bool ack, bool fin,bool retran){
		Packet pSend(seqNum, ackNum, windowSize, ack, syn, fin, buf, size);
		//pSend.sendPacket(sockfd);
		cout << "Sending packet " << ackNum;
		int bytes = sendto(sockfd, pSend.getRawPacketPointer(), pSend.getRawPacketSize(), 0, (sockaddr*) serverAddr, serverAddrSize);
		if(bytes == -1){
			std::cerr << "ERROR send" << endl;
		}

		if(retran){
            cout << " " << "Retransmission";
        }
        if(syn){
			cout << " " << "SYN";
		}
		if(fin){
			cout << " " << "FIN";
		}
		cout << endl;
        seqNum = (seqNum + pSend.getDataSize()) % MAX_SEQ_NUM;
	}

	void tryWriteOutOfOrderPackets(string filePath){
		bool success = false;
		for(int ii=0;ii<outOfOrderPackets.size();ii++){
			Packet* recv = outOfOrderPackets.at(ii);
			if(recv->getSeqNum() == ackNum){
				ackNum = (recv->getSeqNum() + recv->getDataSize()) % MAX_SEQ_NUM;
				writeLock.lock();
				recv->writeToFile(filePath);
				writeLock.unlock();
				outOfOrderPackets.erase(outOfOrderPackets.begin() + ii);
				delete(recv); //delete packets after they are written to file
                success = true;
				break;
			}
		}
		if(success && outOfOrderPackets.size() > 0){
			tryWriteOutOfOrderPackets(filePath); //try to write another out of order packet if we successfully wrote one;
		}
	}
};

void recvDataPacketThread(string fileName, ClientState* clientState);


int main(int argc, char *argv[]){
    srand ( time(NULL) );
	string fileName = "./received.data";

	//Delete old file, if it exists
	std::remove(fileName.c_str());
	string hostname = "10.0.0.1";
	int port = 4000;

	if (argc>1){
		hostname=argv[1];
	}
	if(argc>2){
		port =atoi(argv[2]);
	}
    //cout <<"hostname"<<hostname<<"port"<<port<<endl;
	string server_ip = getIP(hostname);

	int sockfd;
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    /*struct sockaddr_in* clientAddr = new sockaddr_in;;
    clientAddr->sin_family = AF_INET;
    clientAddr->sin_port = htons(4001);     // short, network byte order, any port
    clientAddr->sin_addr.s_addr = inet_addr(client_ip.c_str());
    memset(clientAddr->sin_zero, '\0', sizeof(clientAddr->sin_zero));
    *//*
    if (getsockname(sockfd, (struct sockaddr *) clientAddr, &clientAddrLen) == -1) {
        perror("getsockname");
    }*//*
    if (bind(sockfd, (struct sockaddr*) clientAddr, sizeof(*clientAddr)) == -1) {
        perror("bind");
    }


    char ipstr[INET_ADDRSTRLEN] = {'\0'};
    inet_ntop(clientAddr->sin_family, &clientAddr->sin_addr, ipstr, sizeof(ipstr));
    std::cout << "Set up a connection from: " << ipstr << ":" <<
              ntohs(clientAddr->sin_port) << std::endl;*/

    /*
    struct sockaddr_in* serverAddr = new sockaddr_in;
    serverAddr->sin_family = AF_INET;
    serverAddr->sin_port = htons(port);     // short, network byte order
    serverAddr->sin_addr.s_addr = inet_addr(ip.c_str());
    memset(serverAddr->sin_zero, '\0', sizeof(serverAddr->sin_zero));
    */
    struct sockaddr_in* serverAddr = new sockaddr_in;
    serverAddr->sin_family = AF_INET;
    serverAddr->sin_port = htons(port);     // short, network byte order
    serverAddr->sin_addr.s_addr = inet_addr(server_ip.c_str());
    memset(serverAddr->sin_zero, '\0', sizeof(serverAddr->sin_zero));


    string msg = "hello";
    //sendto(sockfd, msg.c_str(), msg.size(), 0, (sockaddr*) serverAddr, sizeof(*serverAddr));

/*
	char buf[10];
	memset(buf, 0, 10);
	struct sockaddr* addr2 = new sockaddr;
	socklen_t fromlen;
	cout<<"waiting"<<endl;
	recvfrom(sockfd, buf, 10, 0, (sockaddr*) serverAddr, &fromlen);
	cout<<"response"<<endl;
	cout<<buf<<endl;
*/

    //TCP State variables
    ClientState* clientState = new ClientState(sockfd, serverAddr);
    void* dummy = 0;
	clientState->sendPacket(dummy,0,1,0,0,0);
    std::clock_t start;
    double duration;

    start = std::clock();

    std::thread rcvThread(recvDataPacketThread, fileName, clientState);
    rcvThread.detach();

    while (!clientState->establishedTCP){
        duration = ( std::clock() - start ) / (double) CLOCKS_PER_SEC;
        //cout << "Waited " << duration << " for SYNACK reply " << endl;
        if (duration > 0.5) {
            clientState->sendPacket(dummy,0,1,0,0,1);
            start = std::clock();
        }
    }

    while (!clientState->finRcv){
		usleep(10000);
        continue;
    }
	//cout<<"sending fin" << endl;
	clientState->sendPacket(dummy,0,0,0,1,0);
    clientState->finSent=true;
	start = std::clock();
	while (!clientState->finished) {
        duration = (std::clock() - start) / (double) CLOCKS_PER_SEC;
        //cout << "Waited " << duration << " for SYNACK reply " << endl;
        if (duration > 0.5) {
            clientState->sendPacket(dummy, 0, 0, 0, 1, 1);
            start = std::clock();
        }
    }
	usleep(1000000);
    clientState->finished=true;

	//End session
    delete (clientState);
    close(sockfd);
    //cout << "Connection closed..." << endl;
}

void recvDataPacketThread(string fileName, ClientState* clientState){
	while(!clientState->finished){
		clientState->recvPacket(fileName);
	}
    //cout << "recv thread ended"<<endl;
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

	//std::cout << "IP addresses for " << host << ": " << std::endl;

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

