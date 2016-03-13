#include <unistd.h>

int main() {
	char buffer[1024];
	while (1) {
		ssize_t r = read(STDIN_FILENO, buffer, 1024);
		if (r == -1) {
			return 1; 		
		}
		if (r == 0) {
			break;
		}
		size_t w = 0;
		while (w != r) {
			ssize_t nw = write(STDOUT_FILENO, buffer + w, r - w);
			if (nw == -1) {
				return 1;
			}
			w += nw;
		}
	}
	return 0;
}
