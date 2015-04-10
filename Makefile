#### RVM Library Makefile

CFLAGS  = -Wall -g -I.
LFLAGS  =
CC      = g++
RM      = /bin/rm -rf
AR      = ar rc
RANLIB  = ranlib

LIBRARY = librvm.a
LIB_SRC = rvm.cpp
LIB_OBJ = $(patsubst %.cpp,%.o,$(LIB_SRC))

TEST = test
_OBJ = abort basic multi-abort multi truncate
OBJ = $(patsubst %,$(TEST)/%.o,$(_OBJ))

all: $(LIBRARY) $(OBJ) $(_OBJ)

%.o: %.cpp
	$(CC) -c $(CFLAGS) $< -o $@

%: $(TEST)/%.o
	$(CC) -L./ $(CFLAGS) $< -o $@ -lrvm

$(LIBRARY): $(LIB_OBJ)
	$(AR) $(LIBRARY) $(LIB_OBJ)
	$(RANLIB) $(LIBRARY)

clean:
	$(RM) $(LIBRARY) $(LIB_OBJ)
	$(RM) $(TEST)/*.o
	$(RM) abort basic multi-abort multi truncate
