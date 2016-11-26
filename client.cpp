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


using std::string;
using std::cout;
using std::endl;
using std::ios;
using std::ifstream;
using std::vector;
using std::istringstream;

bool writeResponseToFile(string fileName, string msg){
	istringstream ss(msg);
	string temp;
	bool inBody = false;
	std::ofstream out(fileName);
	while(std::getline(ss,temp,'\n')){
		if(inBody){
			out << temp << endl;
		}
		if(temp.size() == 0) {inBody = true;}
	}
	return true;
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


class urlparser{
public:
	string host;
	int port;
	string filePath;
    string fileName;
	urlparser(){
		host = "localhost";
		port = 4000;
		filePath = "/index.html";
        fileName = "/index.html";
	}
	void parse(string url){
		int offset = 0;
		if(url.find("http://") != string::npos){
			offset = 7;
		}
		string relevant = url.substr(offset);
		string hostport = relevant.substr(0, relevant.find("/"));
		size_t colonpos = hostport.find(":");
		string portstr;
		if(colonpos != string::npos){
			port = stoi(hostport.substr(colonpos+1));
			host = hostport.substr(0, colonpos);
		} else {
			port = 80; //default http port, TODO change?
			host = hostport;
		}
        if (relevant.find("/") != string::npos) {
            filePath = relevant.substr(relevant.find("/"));
            fileName = relevant.substr(relevant.find_last_of("/"));
            if (filePath == "/") {
                fileName = "/index.html";
            }
        }
        else {
            filePath="";
            fileName="";
        }
	}
};


int main(int argc, char *argv[]){

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

	//SEND
	string msg = "hello";
	if (send(sockfd, msg.c_str(), msg.size(), 0) == -1) {
		perror("send");
		return 4;
	}
	cout << "Request sent..." << endl;
	//RECV

	cout << "Receiving response... " << endl;

	size_t MAX_MSG_SIZE = 10000;
	void* buf[MAX_MSG_SIZE];
	memset(buf, '\0', sizeof(buf));
	int bytesRecved = recv(sockfd, buf, MAX_MSG_SIZE, 0);
	if (bytesRecved == -1) {
		perror("recv");
		return 5;
	}
	cout << (char*) buf << endl;

	close(sockfd);
	cout << "Connection closed..." << endl;


}

