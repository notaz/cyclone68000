CFLAGS += -Wall -ggdb
ifdef CONFIG_FILE
CFLAGS += -DCONFIG_FILE="\"$(CONFIG_FILE)\""
endif
ifdef HAVE_ARMv6
CFLAGS += -DHAVE_ARMv6=$(HAVE_ARMv6)
endif
CXXFLAGS += $(CFLAGS)

CXX_BUILD := $(if $(CXX_FOR_BUILD),$(CXX_FOR_BUILD),$(CXX))

OBJS = Main.o Ea.o OpAny.o OpArith.o OpBranch.o OpLogic.o OpMove.o Disa/Disa.o

all: Cyclone.s

Cyclone.s: cyclone_gen
	./$<

cyclone_gen: $(OBJS)
	$(CXX_BUILD) -o $@ $^ $(LDFLAGS)

%.o: %.cpp app.h config.h Cyclone.h
	$(CXX_BUILD) $(CPPFLAGS) $(CXXFLAGS) -c -o $@ $<

Disa/%.o: Disa/%.c app.h config.h Cyclone.h
	$(CC_FOR_BUILD) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

clean:
	$(RM) $(OBJS) cyclone_gen Cyclone.s

$(OBJS): app.h config.h Cyclone.h
ifdef CONFIG_FILE
$(OBJS): $(CONFIG_FILE)
endif
