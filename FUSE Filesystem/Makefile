
TARGETS := pfs

PFS_OBJS   := pfs.o

OBJS := $(HELLO_OBJS) $(PFS_OBJS)

CC := gcc

CFLAGS += -D_REENTRANT -D_LIBC_REENTRANT -D_THREAD_SAFE
CFLAGS += -Wall
CFLAGS += -Wunused
CFLAGS += -Wshadow
CFLAGS += -Wdeclaration-after-statement
CFLAGS += -Wdisabled-optimization
CFLAGS += -Wpointer-arith
CFLAGS += -Wredundant-decls
CFLAGS += -g -O2

# for FUSE
CFLAGS += -D_FILE_OFFSET_BITS=64 -DFUSE_USE_VERSION=22
LDFLAGS += -lfuse

%.o: %.c
	$(CC) -o $*.o $< -c $(CFLAGS)

.PHONY: all clean mount-hello mount umount

all: $(TARGETS)

clean:
	-rm -f $(TARGETS) $(OBJS) *~ *.bak core*

mount: pfs
	[ -d /tmp/fuse ] || mkdir -p /tmp/fuse
	./pfs /tmp/fuse

umount:
	-[ -d /tmp/fuse ] && fusermount -u /tmp/fuse || exit 0
	rm -rf /tmp/fuse

pfs: $(PFS_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

