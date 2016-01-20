#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <time.h>

// Access from ARM Running Linux 
#define BCM2708_PERI_BASE        0x3F000000
#define GPIO_BASE                (BCM2708_PERI_BASE + 0x200000) /* GPIO controller */
 
#define PAGE_SIZE (4*1024)
#define BLOCK_SIZE (4*1024)
 
int clock_pin = 23;
int send_pin1 = 18;
int send_pin2 = 4;
int send_pin3 = 27;
int send_pin4 = 5;

int  mem_fd;
void *gpio_map;
 
// I/O access
volatile unsigned *gpio;
 
 
// GPIO setup macros. Always use INP_GPIO(x) before using OUT_GPIO(x) or SET_GPIO_ALT(x,y)
#define INP_GPIO(g) *(gpio+((g)/10)) &= ~(7<<(((g)%10)*3))
#define OUT_GPIO(g) *(gpio+((g)/10)) |=  (1<<(((g)%10)*3))
#define SET_GPIO_ALT(g,a) *(gpio+(((g)/10))) |= (((a)<=3?(a)+4:(a)==4?3:2)<<(((g)%10)*3))
 
#define GPIO_SET *(gpio+7)  // sets   bits which are 1 ignores bits which are 0
#define GPIO_CLR *(gpio+10) // clears bits which are 1 ignores bits which are 0
 
#define GET_GPIO(g) (*(gpio+13)&(1<<g)) // 0 if LOW, (1<<g) if HIGH
 
#define GPIO_PULL *(gpio+37) // Pull up/pull down
#define GPIO_PULLCLK0 *(gpio+38) // Pull up/pull down clock

void setup_io()
{
   /* open /dev/mem */
  if ((mem_fd = open("/dev/mem", O_RDWR|O_SYNC) ) < 0) {
    printf("can't open /dev/mem \n");
    exit(-1);
  }
 
   /* mmap GPIO */
  gpio_map = mmap(
    NULL,             //Any adddress in our space will do
    BLOCK_SIZE,       //Map length
    PROT_READ|PROT_WRITE,// Enable reading & writting to mapped memory
    MAP_SHARED,       //Shared with other processes
    mem_fd,           //File to map
    GPIO_BASE         //Offset to GPIO peripheral
  );
 
  close(mem_fd); //No need to keep mem_fd open after mmap

  if (gpio_map == MAP_FAILED) {
    printf("mmap error %d\n", (int)gpio_map);//errno also set!
    exit(-1);
  }

  // Always use volatile pointer!
  gpio = (volatile unsigned *)gpio_map;
 
} // setup_io

void send_function(char file[]) {
	// printf("Begin send_function\r\n");
	FILE* to_send = fopen(file, "r");
	char c;
	int i;
	OUT_GPIO(send_pin1);
	OUT_GPIO(send_pin2);
	OUT_GPIO(send_pin3);
	OUT_GPIO(send_pin4);
	OUT_GPIO(clock_pin);

	/*
	GPIO_SET = 1 << send_pin1;
	sleep(1);
	GPIO_SET = 1 << send_pin2;
	sleep(1);
	GPIO_SET = 1 << send_pin3;
	sleep(1);
	GPIO_SET = 1 << send_pin4;
	sleep(1);


	GPIO_CLR = 1 << send_pin1;
	GPIO_CLR = 1 << send_pin2;
	GPIO_CLR = 1 << send_pin3;
	GPIO_CLR = 1 << send_pin4;
	*/

	int clk = 1;
	GPIO_CLR = 1 << clock_pin;
	int bitbuf[8];
	// printf("Before while\r\n");
	while((c = fgetc(to_send)) != 255) {
		//printf("["); fflush(0);
		for(i = 7; i >= 0; i--) {
			int bit = (c >> i & 1);
			bitbuf[i] = bit;
			//printf("%i", bit); fflush(0);
			//if(i==4) printf("|");  fflush(0);
		}

			GPIO_CLR = 1 << send_pin1;
			GPIO_SET = bitbuf[0] << send_pin1;
			GPIO_CLR = 1 << send_pin2;
			GPIO_SET = bitbuf[1] << send_pin2;
			GPIO_CLR = 1 << send_pin3;
			GPIO_SET = bitbuf[2] << send_pin3;
			GPIO_CLR = 1 << send_pin4;
			GPIO_SET = bitbuf[3] << send_pin4;
		 	usleep(2);
			GPIO_CLR = 1 << clock_pin;
			GPIO_SET = clk << clock_pin;
			clk ^= 1;
		 	usleep(2);

			GPIO_CLR = 1 << send_pin1;
			GPIO_SET = bitbuf[4] << send_pin1;
			GPIO_CLR = 1 << send_pin2;
			GPIO_SET = bitbuf[5] << send_pin2;
			GPIO_CLR = 1 << send_pin3;
			GPIO_SET = bitbuf[6] << send_pin3;
			GPIO_CLR = 1 << send_pin4;
			GPIO_SET = bitbuf[7] << send_pin4;
		 	usleep(2);
			GPIO_CLR = 1 << clock_pin;
			GPIO_SET = clk << clock_pin;
			clk ^= 1;
		 	usleep(2);
	//printf("]\r\n");
	}
	int d;

	GPIO_CLR = 1 << send_pin1;
	GPIO_CLR = 1 << send_pin2;
	GPIO_CLR = 1 << send_pin3;
	GPIO_CLR = 1 << send_pin4;
	for(d = 0; d < 16; d++) {
		GPIO_CLR = 1 << clock_pin;
		GPIO_SET = clk << clock_pin;
		clk ^= 1;
	 	usleep(1);
	}
	fclose(to_send);
}

int main(int argc, char *argv[]) {
	if(argc != 2) {
		printf("You forgot a file asshole.\r\n");
		return 0;
	}

	setup_io();
	send_function(argv[1]);
	printf("Done!\r\n");

	return 0;
}