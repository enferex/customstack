APP     = customstack
CFLAGS += -g3 -O0 -m64

all: $(APP)

$(APP): $(APP).c
	$(CC) -o $@ $^ $(CFLAGS)
