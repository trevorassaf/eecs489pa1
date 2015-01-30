CC = g++
OBJS = main.o \
			 ServiceBuilder.o \
			 Service.o \
			 ServerBuilder.o \
			 Connection.o \
			 SocketException.o
HEADERS = ServiceBuilder.h \
			 Service.h \
			 ServerBuilder.h \
			 Connection.h \
			 SocketException.h
CFLAGS = -Wall -Wno-deprecated
LFLAGS = $(CFLAGS) 
EXE = peer

peer: $(OBJS) 
	$(CC) $(LFLAGS) -o $(EXE) $(OBJS)

main.o: main.cpp $(HEADERS)
	$(CC) $(CFLAGS) -c main.cpp

ServiceBuilder.o: ServiceBuilder.h Service.h SocketException.h
	$(CC) $(CFLAGS) -c ServiceBuilder.cpp

Service.o: Service.h Connection.h SocketException.h
	$(CC) $(CFLAGS) -c Service.cpp

ServerBuilder.o: ServerBuilder.h Connection.h SocketException.h
	$(CC) $(CFLAGS) -c ServerBuilder.cpp

Connection.o: Connection.h SocketException.h
	$(CC) $(CFLAGS) -c Connection.cpp

SocketException.o: SocketException.h
	$(CC) $(CFLAGS) -c SocketException.cpp

clean:
	\rm *.o $(EXE)
