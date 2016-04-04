#include <signal.h>
#include <unistd.h>
#include <stdio.h>

void handler(int signal, siginfo_t *siginfo, void *f) {
	if (signal == SIGUSR1) {
		printf("SIGUSR1 from %d\n", siginfo->si_pid);
	} else if (signal == SIGUSR2) {
		printf("SIGUSR2 from %d\n", siginfo->si_pid);
	}
}

int main() {
	struct sigaction act;
	act.sa_handler = NULL;
	act.sa_sigaction = handler;
	act.sa_flags = SA_SIGINFO;
	if (sigaction(SIGUSR1, &act, NULL) == -1 || sigaction(SIGUSR2, &act, NULL) == -1) {
		return 1;
	}
	if (!sleep(10)) {
		printf("No signals were caught\n");
	}
	return 0;
}

