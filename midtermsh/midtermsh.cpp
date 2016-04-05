#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <algorithm>
#include <vector>
#include <string>
#include <iostream>

ssize_t writeAll(int fd, const char *buf, size_t count) {
	size_t w = 0;
	while (w != count) {
		ssize_t nw = write(fd, buf + w, count - w);
		if (nw == -1) {
			return -1;
		}
		w += nw;
	}
	return w;
}

int countArgs(char *buffer, char delimeter) {
	if (buffer[0] == '\0') {
		return 0;
	}
	int words = 0;
	for (size_t i = 1;;i++) {
		if (!(buffer[i - 1] == ' ') && !(buffer[i - 1] == delimeter) && !(buffer[i - 1] == '\0') && (buffer[i] == ' ' || buffer[i] == delimeter || buffer[i] == '\0')) {
			words++;
		}
		if (buffer[i] == delimeter || buffer[i] == '\0') {
			return words;
		}
	}
} 

char* nextCommand(char *buffer, size_t *newId, char *delimeter) {
	size_t i = 0;
	while (buffer[i] == ' ') {
		i++;
	}
	size_t startId = i;
	while (buffer[i] != ' ' && buffer[i] != '\0' && buffer[i] != *delimeter) {
		i++;
	}
	size_t endId = i;
	*delimeter = buffer[i];
	*newId = endId;
	if (startId == endId) {
		return 0;
	}
	char *result = new char[endId - startId + 1];
	i = 0;
	while (i < endId - startId) {
		result[i] = buffer[startId + i];
		i++;
	}
	return result;
}

size_t nextDelimeterId(char *buffer, char delimeter) {
	size_t i = 0;
	while (buffer[i] == ' ') {
		i++;
	}
	if (buffer[i] == delimeter) {
		i++;
	}
	return i;
}

void signalHandler(int signal) {
}

ssize_t findDelimeter(char *buffer, size_t len, char delimeter) {
	for (size_t i = 0; i < len; i++) {
		if (buffer[i] == '\0') {
			break;
		}
		if (buffer[i] == delimeter) {
			return i;
		}
	}
	return -1;
}

std::vector<int> children;

char** createArgs(int argc, char **argv) {
	char** result = new char*[argc + 1];
	for (size_t i = 0; i < argc; i++) {
		size_t length = strlen(argv[i]);
		result[i] = new char[length];
		for (size_t j = 0; j < length; j++) {
			result[i][j] = argv[i][j];
		}
	}
	result[argc] = NULL;
	return result;
}

void killChildren(int ignored) {
	for (size_t i = 0; i < children.size(); i++) {
		kill(children[i], SIGKILL);
	}
	children.clear();
}

int runArgs(std::vector<char**> &args, size_t n) {
	if (n == 0) {
		return 0;
	}
	int pipefd[n][2];
	std::vector<int> pids(n);
	for (size_t i = 0; i + 1 < n; i++) {
		if (pipe(pipefd[i]) < 0) {
			return -1;
		}
	}
	for (size_t i = 0; i < n; i++) {
		pids[i] = fork();
		if (pids[i] == -1) {
			return -1;
		}
		if (pids[i] == 0) {
			if (i != 0) {
				dup2(pipefd[i - 1][0], STDIN_FILENO);
				close(pipefd[i - 1][1]);
			}
			if (i != n - 1) {
				dup2(pipefd[i][1], STDOUT_FILENO);
				close(pipefd[i][0]);
			}
			_exit(execvp(args[i][0], args[i]));
		}
	}
	for (size_t i = 0; i + 1 < n; i++) {
		close(pipefd[i][0]);
		close(pipefd[i][1]);
	}

	children = pids;
	struct sigaction act;
	memset(&act, '\0', sizeof act);
	act.sa_handler = &killChildren;

	if (sigaction(SIGINT, &act, 0) < 0) {
		return -1;
	}

	int status;
	for (size_t i = 0; i < n; i++) {
		waitpid(children[i], &status, 0);
	}
	children.clear();
	return 0;
}

int main() {
	struct sigaction action;
	memset(&action, '\0', sizeof action);
	action.sa_handler = &signalHandler;

	if (sigaction(SIGINT, &action, NULL) < 0) {
		return -1;
	}
	
	char *buffer = new char[4096];
	while (1) {
		if (writeAll(STDOUT_FILENO, "$ ", 2) == -1) {
			return 1;
		}

		ssize_t result;
		ssize_t pos;
		size_t size = 0;
		while (1) {
			pos = findDelimeter(buffer, size, '\n');
			if (pos != -1) {
				break;
			}
			result = read(STDIN_FILENO, buffer + size, 4096 - size);
			if (result == 0) {
				break;
			}
			if (result > 0) {
				size += result;
			} else {
				pos = -2;
				result = -1;
				break;
			}
		}
		if (result == 0) {
			pos = -3;
		}

		if (pos == -2) {
			if (writeAll(STDOUT_FILENO, "\n$ ", 3) == -1) {
				return 1;
			}
			continue;
		}

		if (pos == -3) {
			return 0;
		}
		buffer[pos] = '\0';
		std::vector<char**> args(2048);
		size_t argId = 0;
		while (1) {
			char delimeter = '|';
			int commands = countArgs(buffer, delimeter);
			if (commands == 0) {
				break;
			}
			char *argv[commands];
			size_t newId = 0;
			for (int i = 0; i < commands; i++) {
				argv[i] = nextCommand(buffer, &newId, &delimeter);
				delimeter = '|';
				buffer += newId;
			}
			args[argId] = new char*();
			args[argId] = createArgs(commands, argv);
			newId = nextDelimeterId(buffer, '|');
			buffer += newId;
			argId++;
		}
		runArgs(args, argId);
	}
	return 0;
}
