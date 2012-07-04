#include <stdio.h>
#include <sys/types.h>
#include <fcntl.h>

int main(int argc, char *argv[])
{
	int fd;
	char buf[2];
	fd = open ("/dev/bobodog_io_control", O_RDWR);
	if (fd == -1)
	{
		printf ("open /dev/bobodog_io_control fail, fd = %d !\n", fd);
		exit(-1);
	}

#if 0
	buf[0] = 0;
	while(1)
	{
		buf[1] = 0;
		write(fd, buf, sizeof(buf));
		sleep(3);
		buf[1] = 1;
		write(fd, buf, sizeof(buf));
		sleep(3);
	}
#else

	buf[0] = 1;
	buf[1] = 0;
	write(fd, buf, sizeof(buf));
	sleep(3);
	buf[1] = 1;
	write(fd, buf, sizeof(buf));
	sleep(3);

	buf[0] = 0;
	read (fd, buf, sizeof(buf));
	printf ("gpio's val = %d !\n", buf[1]);
#endif
	close(fd);
}
