DEPS:=scrypt/readpass.c\
      scrypt/crypto_scrypt-nosse.c\
	  scrypt/sha256.c\

# fno-builtin-memset asks compiler do not optimize memset() calls
GCC_FLAGS:=-Wall\
	       -fno-builtin-memset\

PHONY: build
build: BIN?=./bin/cpwd
build:
	$(info #Building...)
	mkdir -p ./bin
	gcc $(GCC_FLAGS) main.c $(DEPS) -o $(BIN)
