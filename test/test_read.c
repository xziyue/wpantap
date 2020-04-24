#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <arpa/inet.h> 
#include <sys/select.h>
#include <sys/time.h>
#include <errno.h>
#include <stdarg.h>

int main(){

	int fd = open("/dev/net/wpantap", O_RDWR);
	if (fd < 0){
		perror("open");
		printf("unable to open wpantap device\n");
		return 1;
	}
	
	char buf[300];
	int bytes = read(fd, buf, 300);
	//printf("read %d bytes\n", bytes);

	for(int i = 0; i < bytes; ++i){
		putchar(buf[i]);
	}
	

	close(fd);
}
