SERVER = bin/server
CLIENT = bin/client
INCLUDE = include
SRC = src/*.c

############################
# Compile
############################

all: server client

server:
	gcc -Wall -lpthread -I$(INCLUDE) src/server.c -o $(SERVER)

client:
	gcc -Wall -I$(INCLUDE) src/client.c -o $(CLIENT)


############################
# Run
############################

runserver: $(SERVER)
	./$(SERVER) 

runclient: $(CLIENT)      
	./$(CLIENT) localhost files/AliceInWonderland.txt

runlocal: $(CLIENT)
	./$(CLIENT) :: files/AliceInWonderland.txt


############################
# Debug / Clean
############################

memcheck: $(EXEC)
	valgrind -v --leak-check=full --show-leak-kinds=all ./$(EXEC)

cppcheck: $(EXEC)
	cppcheck .

clean:
	rm bin/*
