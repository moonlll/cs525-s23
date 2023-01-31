objs := $(wildcard *.c)

all : ass1

ass1 : $(objs)
	cc -Wno-format-security -o ass1 $(objs)

ass1_debug : $(objs)
	cc -Wno-format-security -g -o ass1 $(objs)

clean : 
	rm -f ass1 *.o