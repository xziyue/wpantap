#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/time.h>


int main ()
{	
	fd_set set;
	struct timeval timeout;
	char buf[300];	
	int fd = open("/dev/net/wpantap", O_RDWR);
	int bytes;
	if (fd < 0){
		perror("open");
		printf("unable to open wpantap device\n");
		return 1;
	}

	
	while(1){

		FD_ZERO (&set);
		FD_SET (fd, &set);

		timeout.tv_sec = 1;
		timeout.tv_usec = 0;

		select(FD_SETSIZE, &set, NULL, NULL, &timeout);
		
		if (FD_ISSET(fd, &set)){
			bytes = read(fd, buf, 300);
			printf("read %d bytes from wpantap\n", bytes);
		}else{
			printf("no data is avaliable!\n");
		}
	}

	return 0;
}

