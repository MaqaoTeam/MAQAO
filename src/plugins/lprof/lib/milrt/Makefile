#####################################
#    LIBMIL_RT  Library    #
#####################################

INCLUA=-I/home/acharifrubial/dev/workspace/MAQAODEV/src/LuaJIT/src/
LIBLUADIR=-L/home/acharifrubial/dev/workspace/MAQAODEV/src/LuaJIT/src/ -L/home/acharifrubial/dev/workspace/MAQAODEV/lib/
LIBLUAP=/home/acharifrubial/dev/workspace/MAQAODEV/src/LuaJIT/src/libluajit.a
#LIBLUA=-lluajit

CC=gcc
CFLAGS=-Wall -g -O3 -fPIC $(INCLUA) #-fno-inline-functions #-DCALL_FORTRAN
LDFLAGS=-L. $(LIBLUADIR) -ldl -lrt $(LIBLUA) -liomp5 
OBJS=libmilrt.o 
LIB=libmilrt.so libmilrt.a

all: $(LIB) 
	
%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.so: $(OBJS)
	$(CC) -shared -o $@ $^ $(LIBLUAP) $(LDFLAGS)



%.a: $(OBJS)
	$(AR) $(ARFLAGS) $@ $^ $(LIBLUAP) 

clean:
	rm -f *~ $(OBJS) ${LIB} *.o *.so *.a


