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
	
	char buf[20];

	/* Construct raw packet payload, length and FCS gets added in the kernel */
	buf[0] = 0x21; /* Frame Control Field */
	buf[1] = 0xc8; /* Frame Control Field */
	buf[2] = 0x8b; /* Sequence number */
	buf[3] = 0xff; /* Destination PAN ID 0xffff */
	buf[4] = 0xff; /* Destination PAN ID */
	buf[5] = 0x02; /* Destination short address 0x0002 */
	buf[6] = 0x00; /* Destination short address */
	buf[7] = 0x23; /* Source PAN ID 0x0023 */
	buf[8] = 0x00; /* */
	buf[9] = 0x60; /* Source extended address ae:c2:4a:1c:21:16:e2:60 */
	buf[10] = 0xe2; /* */
	buf[11] = 0x16; /* */
	buf[12] = 0x21; /* */
	buf[13] = 0x1c; /* */
	buf[14] = 0x4a; /* */
	buf[15] = 0xc2; /* */
	buf[16] = 0xae; /* */
	buf[17] = 0xAA; /* Payload */
	buf[18] = 0xBB; /* */
	buf[19] = 0xCC; /* */

	write(fd, buf, sizeof(buf));

	close(fd);
}
