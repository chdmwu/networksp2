CXX=g++
CXXOPTIMIZE= -O2
CXXFLAGS= -g -Wall -pthread -std=c++11 $(CXXOPTIMIZE)
USERID=EDIT_MAKE_FILE

# Add all .cpp files that need to be compiled for your server
SERVER_FILES=server.cpp packet.cpp
RENO_SERVER_FILES=reno-server.cpp packet.cpp

# Add all .cpp files that need to be compiled for your client
CLIENT_FILES=client.cpp packet.cpp

all: simple-tcp-server simple-tcp-client reno-server

*.o: *.cpp
	$(CXX) -o $@ $^ $(CXXFLAGS) $@.cpp

simple-tcp-server: $(SERVER_FILES:.cpp=.o)
	$(CXX) -o $@ $(CXXFLAGS) $(SERVER_FILES:.cpp=.o)

simple-tcp-client: $(CLIENT_FILES:.cpp=.o)
	$(CXX) -o $@ $(CXXFLAGS) $(CLIENT_FILES:.cpp=.o)

reno-server: $(RENO_SERVER_FILES:.cpp=.o)
	$(CXX) -o $@ $(CXXFLAGS) $(RENO_SERVER_FILES:.cpp=.o)
clean:
	rm -rf *.o *~ *.gch *.swp *.dSYM simple-tcp-server simple-tcp-client reno-server *.tar.gz

tarball: clean
	tar -cvf $(USERID).tar.gz *