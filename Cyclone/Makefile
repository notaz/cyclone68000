CFLAGS += -Wall -ggdb

OBJS = Main.o Ea.o OpAny.o OpArith.o OpBranch.o OpLogic.o OpMove.o Disa/Disa.o

all: cyclone.s

cyclone.s: cyclone_gen
	./$<

cyclone_gen: $(OBJS)
	$(CXX) -o $@ $^ $(LDFLAGS)

clean:
	$(RM) $(OBJS) cyclone_gen cyclone.s
