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

char get_char(int buf[]) {
	int multiplier = 1;
	int i;
	int bin = 0;
	for(i=0; i < 8; i++) {
		bin += (multiplier * buf[i]);
		multiplier *= 2;
	}
return bin;
}

int clock_pin = 24;
int data_pin1 = 25;
int data_pin2 = 17;
int data_pin3 = 22;
int data_pin4 = 11;
int arrbit[100000000];

void receive_function() {
	INP_GPIO(clock_pin);
	INP_GPIO(data_pin1);
	INP_GPIO(data_pin2);
	INP_GPIO(data_pin3);
	INP_GPIO(data_pin4);
	int prev_clk = 1;
	int clk = 1;
	int buf[8];
	int p = 0;
	int first = 1;
	int startcount =  1;
	int finished = 0;
	clock_t start = 0;
	while(1) {
		clk = GET_GPIO(clock_pin) >> clock_pin;
		if(prev_clk != clk) {
			if(first) {
				first=0;
			} else {
				int bit1 = GET_GPIO(data_pin1) >> data_pin1;
				int bit2 = GET_GPIO(data_pin2) >> data_pin2;
				int bit3 = GET_GPIO(data_pin3) >> data_pin3;
				int bit4 = GET_GPIO(data_pin4) >> data_pin4;
				if(bit1==0 && bit2==0 &&bit3==0 &&bit4==0) finished++;
				else finished = 0;

				arrbit[p] = bit1;
				arrbit[p+1] = bit2;
				arrbit[p+2] = bit3;
				arrbit[p+3] = bit4;
				p += 4;
				if(finished==4) break;
				if(startcount == 1) {
					start = clock();
					printf("Started\n");
					startcount = 0;
				}
			}
		}
		prev_clk = clk;

	}

	double time = (double) (clock() - start)/CLOCKS_PER_SEC;
	printf("Sending complete in: %.6fs\n", time);
	printf("Counter-32: %.6fs\n", p-32);
	printf("bits/second:  %f\n", (p-32)/time);

	FILE* to_receive = fopen(argv[1], "w");
	int i;

	for(i = 0; i <= p-24; i+=8) {
		int buf2[8] = {arrbit[i], arrbit[i+1], arrbit[i+2], arrbit[i+3], arrbit[i+4], arrbit[i+5], arrbit[i+6], arrbit[i+7]};
		fprintf(to_receive, "%c", get_char(buf2)); fflush(0);
	}
	fclose(to_receive);
}


int main(int argc, char *argv[]) {
	if(argc != 2) {
		printf("You forgot a file asshole.\r\n");
		return 0;
	} else {
		setup_io();
		receive_function(argv[1]);
		printf("Done! [p=%i, percentage buffer filled:%f]\r\n", p, (double)(((double)p/(double)100000000)*100));
	}
	return 0;
}