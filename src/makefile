CC = g++
OBJS = main.o \
			 ServiceBuilder.o \
			 Service.o \
			 Client.o \
			 ServerBuilder.o \
			 Server.o \
			 Connection.o \
			 SocketException.o
HEADERS = ServiceBuilder.h \
			 Service.h \
			 Client.h \
			 ServerBuilder.h \
			 Server.h \
			 Connection.h \
			 SocketException.h
CFLAGS = -Wall -Wno-deprecated
LFLAGS = $(CFLAGS) 
EXE = peer

INCLUDES = headers
SRC = cpps

peer: $(OBJS) 
	$(CC) $(LFLAGS) -o $(EXE) $(OBJS)

main.o: main.cpp 

ServiceBuilder.o: $(INCLUDES)/ServiceBuilder.h $(SRC)/ServiceBuilder.cpp $(INCLUDES)/Service.h $(INCLUDES)/SocketException.h
	$(CC) $(CFLAGS) $(SRC)/ServiceBuilder.cpp

Service.o: $(INCLUDES)/Service.h $(SRC)/Service.cpp $(INCLUDES)/Client.h $(INCLUDES)/SocketException.h
	$(CC) $(CFLAGS) $(SRC)/Service.cpp

Client.o: $(INCLUDES)/Client.h $(SRC)/Client.cpp $(INCLUDES)/Connection.h $(INCLUDES)/SocketException.h
	$(CC) $(CFLAGS) $(SRC)/Client.cpp

ServerBuilder.o: $(INCLUDES)/ServerBuilder.h $(SRC)/ServerBuilder.cpp $(INCLUDES)/Server.h $(INCLUDES)/SocketException.h
	$(CC) $(CFLAGS) $(SRC)/ServerBuilder.cpp

Server.o: $(INCLUDES)/Server.h $(SRC)/Server.cpp $(INCLUDES)/Connection.h $(INCLUDES)/SocketException.h
	$(CC) $(CFLAGS) $(SRC)/Server.cpp

Connection.o: $(INCLUDES)/Connection.h $(SRC)/Connection.cpp $(INCLUDES)/SocketException.h
	$(CC) $(CFLAGS) $(SRC)/Connection.cpp

SocketException.o: $(INCLUDES)/SocketException.h $(SRC)/SocketException.cpp
	$(CC) $(CFLAGS) $(SRC)/SocketException.cpp

clean:
	\rm *.o $(EXE)
