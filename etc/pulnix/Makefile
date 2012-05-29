CFLAGS += -Wall -O2 -m32
all: gvsp.so

%.so: %.c
	$(CC) $(CFLAGS) -shared $< -o $@
