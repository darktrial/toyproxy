all:
	$(CC) -O1 -o proxy proxy.c 
clean:
	rm -rf proxy 
