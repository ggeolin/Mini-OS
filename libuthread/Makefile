targets 	:= libuthread.a
objs		:= queue.o context.o uthread.o sem.o preempt.o

CC		:= gcc
CFLAGS		:= -Wall -Wextra -Werror -MMD
#CFLAGS	+= -g

ifneq ($(V), 1)
Q = @
endif

all	: $(targets)

deps := $(patsubst %.o, %.d, $(objs))
-include $(deps)

libuthread.a : $(objs)
	@echo "COMPRESSING $@"
	$(Q)ar rcs $@ $^

%.o : %.c
	@echo "CC $@"
	$(Q)$(CC) $(CFLAGS) -c -o $@ $<

clean:
	@echo "clean"
	$(Q)rm -f $(targets) $(objs) $(deps)
