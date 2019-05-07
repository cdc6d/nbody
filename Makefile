# You may need to set the following environment variables:
#  - CFLAGS: e.g. -I/sw/include/SDL2
#  - LFLAGS: e.g. -L/sw/lib

main: main.c
	cc -o main{,.c} $(CFLAGS) $(LFLAGS) -lSDL2 -lm

main.html: main.c
	emcc  -s USE_SDL=2 -o main.{html,c}

.PHONY: clean
clean:
	rm -f main{,.html,.js,.o}
