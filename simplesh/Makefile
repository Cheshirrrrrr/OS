all: simplesh

midtemsh: simplesh.cpp
	clang++ simplesh.cpp -fsanitize=address -std=c++11 -Wall -Wextra -o simplesh

clean:
	rm -rf simplesh
