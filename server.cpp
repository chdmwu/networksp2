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
#include <list>

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
	int dataSent, totalData;
	std::vector<char> fileBytes;
    bool handshake1 = false;
    bool establishedTCP = false;
    std::vector<std::pair<int,std::clock_t>> retran_timer;
    bool finishSend = false;
    int unackedBytes = 0;
    int seqCycles=0;
    int ackCycles=0;
    int initSeq;
	ServerState(string fileDir){
        int tmp =rand();
		seqNum = rand()%MAX_SEQ_NUM;
        initSeq=seqNum; //TODO random
		lastAckedPacket = seqNum;
		dupAcks = 0;
		state = "SS";
		fileBytes = getFileBuffer(fileDir);
		dataSent = 0;
		totalData = fileBytes.size();
	}
	~ServerState(){
	}
	//returns latest ack of the packet recvd
	uint16_t recvPacket(int sockfd){
		cout << "Running receiving packet" << endl;
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
		//cout << "Syn Value " << recv.getSyn();
        if (recv.getSyn()==1 && !handshake1) {
            ackNum = (recv.getSeqNum() + ONE) % MAX_SEQ_NUM;
            handshake1 = true;
            cout << "handshake established " << endl;

        }
		else {
            ackNum = (recv.getSeqNum() +recv.getDataSize()) % MAX_SEQ_NUM;
        }
		if (handshake1) {
            // Deal with updating last Ack when ack numbers loop around
            if (recv.getAck() < (MAX_SEQ_NUM / 2) && lastAckedPacket > (MAX_SEQ_NUM / 2)) {
                lastAckedPacket = recv.getAckNum();
            } else {
                lastAckedPacket = max(lastAckedPacket, recv.getAckNum());
            }
            cout << "Recv Seq" << recv.getSeqNum() << "Recv Ack" << recv.getAckNum()<<endl;
            cout << "Serv Seq" << ackNum << "Serv Ack" << seqNum <<endl;
            if (recv.getSeqNum()==ackNum && lastAckedPacket == (seqNum+ONE)%MAX_SEQ_NUM) {
                establishedTCP = true;
                seqNum= (seqNum + ONE) % MAX_SEQ_NUM;
                cout << "established tcp" <<endl;
            }
            clientWindowSize = recv.getWindowSize();

            //update cwnd
            //new ack
            if (establishedTCP) {
                if (lastAckedPacket != prevAck) {
                    if (lastAckedPacket < prevAck){
                        ackCycles+=1;
                    }
                    if (retran_timer.size()) {
                        for (int ii = retran_timer.size() - 1; ii >= 0; ii--) {
                            if (retran_timer[ii].first < lastAckedPacket+ackCycles*MAX_SEQ_NUM) {
                                retran_timer.erase(retran_timer.begin() + ii);
                            }
                        }
                    }
                    if (state == "SS") {
                        cwnd += MSS;
                        dupAcks = 0;
                        if (cwnd >= ssthresh) {
                            state = "CA";
                        }
                    } else if (state == "CA") {
                        cwnd += 1; //add one, said in project specs?
                        dupAcks = 0;
                    } else if (state == "FR") {
                        dupAcks = 0;
                        cwnd = ssthresh;
                        state = "CA";
                        //TODO Fast recovery and timeouts
                    }
                } else if (lastAckedPacket == prevAck) { // dup ack
                    if (state == "SS") {
                        dupAcks++;
                    } else if (state == "CA") {
                        dupAcks++;
                    } else if (state == "FR") {
                        cwnd += MSS;
                    }
                    cout << "Dup ACK " << dupAcks << endl;
                    if (dupAcks >= 3 && (state == "SS" || state == "CA")) {
                        state = "FR";
                        ssthresh = max(cwnd / 2, (int) MSS);
                        cwnd = ssthresh + 3 * MSS;
                        resendPacket(sockfd, lastAckedPacket+ackCycles*MAX_SEQ_NUM); //retransmit packet
                    }
                }
                cout << "State " << state << endl;
                cout << "Receiving packet " << recv.getAckNum() << endl;
                prevAck = lastAckedPacket;
                unackedBytes = getUnackedBytes();
            }
        }
		return lastAckedPacket;
	}
	//returns seqNum next packet to send
	uint16_t sendPacket(int sockfd, void* buf, size_t size, bool syn, bool ack, bool fin, bool retransmit, int retran_seq){
        //cout << "running send packet " << endl;
        unackedBytes = getUnackedBytes();
        int sendSeqNum = seqNum;
        if (retransmit) {
            sendSeqNum = retran_seq % MAX_SEQ_NUM;
        }
        Packet pSend(sendSeqNum, ackNum, windowSize, ack, syn, fin, buf, size);
		// Deal with finding out how many bytes are unacked when ack loops around

		//If we have too many unacked packets, wait.
		//cout << "Unacked bytes... " << unackedBytes  << " seqNum "<< seqNum << " last acked " << lastAckedPacket << endl;
        //cout << "Unacked " << unackedBytes << " cwnd " << cwnd << endl;

		while(unackedBytes > min(clientWindowSize, cwnd) && !retransmit){
            //cout << "Sleeping for a while" << endl;
			usleep(1000);
			unackedBytes = getUnackedBytes();
            //cout << "Waiting... " << unackedBytes << endl;
		}

        if (retran_timer.size()) {
            double duration;
            duration = ( std::clock() - retran_timer.front().second ) / (double) CLOCKS_PER_SEC;
            cout << "retran head " << retran_timer.front().first << "duration " << duration << endl;
        }
        int bytes = sendto(sockfd, pSend.getRawPacketPointer(), pSend.getRawPacketSize(), 0, &clientAddr, clientAddrSize);
        if (establishedTCP) {

            if (retransmit){
                cout << "adding " << retran_seq << " to retran" << endl;
                retran_timer.push_back(std::make_pair(retran_seq,clock()));
                cout << "removing " << retran_timer.front().first << " from retran" << endl;
                retran_timer.erase(retran_timer.begin());

            }
            else{
                cout << "adding " << sendSeqNum+seqCycles*MAX_SEQ_NUM << " to retran" << endl;
                retran_timer.push_back(std::make_pair(sendSeqNum+seqCycles*MAX_SEQ_NUM,clock()));
            }

            cout << "retran length after send " << retran_timer.size() << endl;
        }
        if(bytes == -1){
            std::cerr << "ERROR send" << endl;
        }
        cout << "Sending packet " << sendSeqNum << " " << cwnd << " " << ssthresh << " length " << pSend.getDataSize() << "ack num" << ackNum;
        if(retransmit){
            cout << " " << "Retransmission";
        }
        if(syn){
            cout << " " << "SYN";
        }
        if(fin){
            cout << " " << "FIN";
        }

        cout << endl;
        if (!retransmit) {
            if (seqNum + max(pSend.getDataSize(), ONE) > MAX_SEQ_NUM) {
                seqCycles+=1;
            }
            seqNum = (seqNum + pSend.getDataSize()) % MAX_SEQ_NUM;

        }
		return seqNum;
	}

	size_t getUnackedBytes(){
		//if(seqNum < (MAX_SEQ_NUM/2) && lastAckedPacket > (MAX_SEQ_NUM/2)){
		//	return seqNum + (MAX_SEQ_NUM - lastAckedPacket); // numbers looped around
		//}
		return (seqNum - lastAckedPacket + MAX_SEQ_NUM) % MAX_SEQ_NUM; //numbers didnt loop around
	}

	void sendDataPacketsThread(int sockfd){
		char* data = fileBytes.data();
		while(dataSent < totalData){
			//cout << "Data sent" << dataSent << "TotalData" <<totalData<<endl;
			if(totalData - dataSent < MSS){
				sendPacket(sockfd,data + dataSent,totalData - dataSent,0,1,0,0,0);
                dataSent += totalData-dataSent;
			} else {
				sendPacket(sockfd,data + dataSent,MSS,0,1,0,0,0);
                dataSent += MSS;
			}
		}
        finishSend = true;
	}
    void checkTimeoutThread(int sockfd){
        clock_t lastCheck = std::clock();
        while(true){
            double duration;
            duration = (std::clock() - lastCheck) / (double) CLOCKS_PER_SEC;
            if (duration > 0.1) {
                clock_t lastCheck = std::clock();
                //cout << "Retran timer size is " << retran_timer.size() << endl;
                if (retran_timer.size()) {
                    std::clock_t start = retran_timer.front().second;
                    double duration;
                    duration = (std::clock() - start) / (double) CLOCKS_PER_SEC;
                    //cout << "retran dur is " << duration << endl;
                    if (duration > 0.5) {
                        cout << "Experience timeout " << endl;
                        resendPacket(sockfd, retran_timer.front().first);
                        ssthresh = max(cwnd / 2, (int) MSS);
                        cwnd = MSS;
                        dupAcks = 0;
                        state = "SS";
                    }
                }
            }
        }
    }
	void resendPacket(int sockfd, int index){
		char* data = fileBytes.data();
		if(totalData - index < MSS){
			sendPacket(sockfd,data + index-1-initSeq,totalData - index,0,1,0,1,index);
		} else {
			sendPacket(sockfd,data + index-1-initSeq,MSS,0,1,0,1,index);
		}
	}
};

void sendDataPacketsThread(int clientSockfd, ServerState* serverState, string fileDir);
void recvPacketsThread(int clientSockfd, ServerState* serverState);
void doSendPackets(int sockfd, ServerState* serverState){
	serverState->sendDataPacketsThread(sockfd);
}
void doTimeout(int sockfd, ServerState* serverState){
    serverState->checkTimeoutThread(sockfd);
}


int main(int argc, char *argv[])
{
	string fileDir = "./test.png"; // TODO change
	//string fileDir = "./files/bear.jpg"; // TODO change
	string hostname;
	int port;
	string ip;

	if(argc <= 3){
		hostname = "localhost";
		port = 4000;
	}
	else if(argc == 3){
		hostname = string(argv[1]);
		port = atoi(argv[2]);
	}

	ip = "10.0.0.1";
    if (argc>1 && string(argv[1]) == "localhost"){
        ip = getIP(hostname);
    }

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

	// TCP state variables
	ServerState* serverState = new ServerState(fileDir);
	bool runServer = true;


	while(runServer){
		cout << "Running Server " << endl;
		void* dummy = 0;

		//Recving SYN
        std::thread recvThread(recvPacketsThread,sockfd, serverState);
        recvThread.detach();

        clock_t lastCheck = std::clock();
        while(!serverState->handshake1) {
            double duration;
            duration = (std::clock() - lastCheck) / (double) CLOCKS_PER_SEC;
            if (duration > 0.1) {
                clock_t lastCheck = std::clock();
            }
        }


        cout << "Established handshake" << endl;
        serverState->sendPacket(sockfd, dummy, 0, 1, 1,0,0,0);
        std::clock_t start;
        double duration;

        start = std::clock();
        while (!serverState->establishedTCP) {
            duration = ( std::clock() - start ) / (double) CLOCKS_PER_SEC;
            if (duration > 0.5) {
                serverState->sendPacket(sockfd, dummy, 0, 1, 1,0,0,0);
                start = std::clock();
            }
        }

        std::vector<char> fileBytes = getFileBuffer(fileDir);
        //Send data packet
        //serverState.sendPacket(clientSockfd,fileBytes.data(),fileBytes.size(), 0, 1, 0);

        std::thread sendThread(doSendPackets, sockfd, serverState);
        sendThread.detach();
        doTimeout(sockfd, serverState);




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
