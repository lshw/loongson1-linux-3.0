#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <linux/soundcard.h>

static void print_help()
{
  printf("usage：plz input the control num:\n");
  printf("0 adjust volume\n");
  printf("1 record source selcet\n");
  printf("2 mute or not\n");
  printf("3 adjust pcm out\n");
  printf("input mixer options:");
}

int main(int argc, char *argv[])
{
  int fd;
  int left, right, level, mute;
  int device;
  char *dev;
  int status;
  int i;
  fd = open("/dev/mixer", O_RDWR);
  if (fd == -1)
  {
    perror("unable to open /dev/mixer");
    exit(1);
  }
  
  print_help();  
  scanf("%d\n", &i);

  switch(i) {
    case 0:
      printf("front master left volume:");//音量调节值范围为0~31
      scanf("%d", &left);
      printf("front master right volume:");
      scanf("%d", &right);
      level = (left << 8) + right;
    
      status = ioctl(fd, MIXER_WRITE(SOUND_MIXER_VOLUME), &level);
      if (status == -1)
      {
        perror("MIXER_WRITE ioctl failed\n");
      }
      break;
    case 1:
//		level = SOUND_MASK_MIC;
		level = SOUND_MASK_LINE;
//		level = SOUND_MASK_LINE1;

      status = ioctl(fd, SOUND_MIXER_WRITE_RECSRC, (unsigned long) &level);
      if (status == -1)
      {
	perror("MIXER_WRITE ioctl failed\n");
      }
      break;
    case 2:
      printf("mute or not:");//1 - 静音 0 - 取消静音
      scanf("%d", &mute); 
      mute = mute << 15;

      status = ioctl(fd, MIXER_WRITE(SOUND_MIXER_MUTE), &mute);
      if (status == -1)
      {
	perror("MIXER_WRITE ioctl failed\n");
      }
      break;
    case 3:
      printf("pcm out left volume:");//pcm out 的音量调节值范围为0~31
      scanf("%d", &left);
      printf("pcm out right volume:");
      scanf("%d", &right);
      level = (left << 8) + right;
    
      status = ioctl(fd, MIXER_WRITE(SOUND_MIXER_PCM), &level);
      if (status == -1)
      {
        perror("MIXER_WRITE ioctl failed\n");
      }
      break;
    default:
      break;
  }

  close(fd);
  return 0;
}
