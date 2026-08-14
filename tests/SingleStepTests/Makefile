ifeq ($(HAVE_ARMv6),1)
ARCH = armv6
CPU = arm11mpcore
else
ARCH = armv4t
CPU = arm946 # arm920 not supported in qemu...
endif

AS = arm-linux-gnueabi-as
ASFLAGS += -march=$(ARCH) -mfloat-abi=soft
CXX = arm-linux-gnueabi-g++
CFLAGS += -march=$(ARCH) -mfloat-abi=soft -Wall -ggdb -O2
CXXFLAGS += $(CFLAGS)
LDFLAGS += -static
EMU = qemu-arm -cpu $(CPU)

OBJS = tester.o Cyclone.o

.PHONY: all test clean cyclone

all: cyclone tester

test: cyclone tester
	$(EMU) tester

clean:
	$(RM) $(OBJS) tester
	$(MAKE) -C ../.. clean

cyclone:
	$(MAKE) -C ../..

tester: $(OBJS)
	$(CXX) -o $@ $^ $(LDFLAGS)

Cyclone.o: ../../Cyclone.s
	$(AS) $(ASFLAGS) -o $@ ../../Cyclone.s
