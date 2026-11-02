CC := mpicc
CFLAGS := -std=c17 -O3 -march=native -Wall -Wextra -Wpedantic
LDLIBS := -lX11 -lm
TARGET := julia
NP ?= 4

# No cluster normalmente nao ha X11: compile com "make X11=0".
ifeq ($(X11),0)
CFLAGS += -DNO_X11
LDLIBS := -lm
endif

.PHONY: all clean run

all: $(TARGET)

$(TARGET): julia.c
	$(CC) $(CFLAGS) $< -o $@ $(LDLIBS)

run: $(TARGET)
	mpirun -n $(NP) ./$(TARGET) --frames 2000 $(ARGS)

clean:
	rm -f $(TARGET)
