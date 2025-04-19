#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h> // for SYS_mkdir
#include <errno.h>       // for errno
#include <string.h>      // for strerror

int main(int argc, char ** argv){
	if (argc == 1){
		fprintf(stderr, "minsh: Argument required\n");
		return 1;
	}
	for (int i = 1 ; i < argc ; i++){
		// Direct system call to mkdir
		long result = syscall(SYS_mkdir, argv[i], 0755);
		if (result < 0){
			fprintf(stderr, "minsh: %s - %s\n", argv[i], strerror(errno));
			continue;
		}
	}
	return 0;
}
