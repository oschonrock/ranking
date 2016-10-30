
# program name variable
P = ranking

# these are the object file dependencies
OBJS   = regatta.o sailor.o curl.o

# compiler and linker options
CFLAGS = -std=c11 -Wall -pedantic -g -O0 `curl-config --cflags` `xml2-config --cflags`
LDLIBS = -lthr -lcrypto -lpcre `curl-config --libs` `xml2-config --libs`

# compile the main program from its prg.c source file and the obj.o files
$(P)     : $(P).c $(OBJS)
	$(CC) $(CFLAGS) -o $(.TARGET) $(.ALLSRC) $(LDLIBS)

# for each object file create a dependency on its obj.c source and obj.h header files, and compile them to obj.o with cc -c ...
$(OBJS) : $(.PREFIX).c $(.PREFIX).h
	$(CC) $(CFLAGS) -c $(.PREFIX).c 

clean :
	rm -f *.core *.o ranking
