CC = g++
OBJS = main.o \
			 ServiceBuilder.o \
			 Service.o \
			 ServerBuilder.o \
			 Connection.o \
			 P2pTable.o \
			 ImageNetwork.o \
			 SocketException.o
HEADERS = ServiceBuilder.h \
			 Service.h \
			 ServerBuilder.h \
			 Connection.h \
			 P2pTable.h \
			 ImageNetwork.h \
			 SocketException.h
CXXFLAGS = -Wall -Wno-deprecated -std=c++11
LFLAGS = $(CXXFLAGS) 
EXE = peer

peer: $(OBJS) 
	$(CC) $(LFLAGS) -o $(EXE) $(OBJS)

main.o: main.cpp $(HEADERS)
	$(CC) $(CXXFLAGS) -c main.cpp

ServiceBuilder.o: ServiceBuilder.h Service.h SocketException.h
	$(CC) $(CXXFLAGS) -c ServiceBuilder.cpp

Service.o: Service.h Connection.h SocketException.h
	$(CC) $(CXXFLAGS) -c Service.cpp

ServerBuilder.o: ServerBuilder.h Connection.h SocketException.h
	$(CC) $(CXXFLAGS) -c ServerBuilder.cpp

Connection.o: Connection.h SocketException.h
	$(CC) $(CXXFLAGS) -c Connection.cpp

P2pTable.o: P2pTable.h Connection.h
	$(CC) $(CXXFLAGS) -c P2pTable.cpp

ImageNetwork.o: ImageNetwork.h Service.h Connection.h P2pTable.h
	$(CC) $(CXXFLAGS) -c ImageNetwork.cpp

SocketException.o: SocketException.h
	$(CC) $(CXXFLAGS) -c SocketException.cpp

clean:
	\rm *.o $(EXE)
