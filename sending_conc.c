#include <stdio.h>
#include <pthread.h>
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
 
int send_pin = 18;
int clock_pin = 23;

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




const int b_size = 5;
volatile int buffer[5];
volatile int p = 0;
volatile int r = 0;

void *putData() {
	FILE* to_send = fopen("test.txt", "r");
	char c;
	int i;
	while((c = fgetc(to_send)) != 255) {	
		i = 7;
		while(i>=0) {
			if(buffer[p%b_size]==-1) {
				int bit = (c >> i & 1);
				buffer[p%b_size] = bit;
				i--;
				p++;
			}
		}
	}
	int d = 0;
	while(d < 8) {
		if(buffer[p%b_size]==-1) {
			buffer[p%b_size]=0;
			p++;
			d++;
		}
	}
	fclose(to_send);
}

void *sendData() {
	int clk = 1;
	int finished = 0;
	while(1) {
		if(buffer[r%b_size]==-1 || r >= p) { }
		else {
			int bit = buffer[r%b_size];
			GPIO_CLR = 1 << send_pin;
			GPIO_SET = bit << send_pin;
			if(bit==0) finished++;
			else finished=0;
			usleep(1);
			GPIO_CLR = 1 << clock_pin;
			GPIO_SET = clk << clock_pin;
			usleep(1);
			clk ^= 1;
			buffer[r%b_size] = -1;
			r++;
			if(finished==8) break;
		}
	}
}

int main() {
	int i;
	clock_t begin, end;
	double time_spent;

	begin = clock();

	for(i=0; i<b_size;i++) {
		buffer[i]=-1;
	}

	setup_io();

	INP_GPIO(send_pin);
	INP_GPIO(clock_pin);
	OUT_GPIO(send_pin);
	OUT_GPIO(clock_pin);
	GPIO_CLR = 1 << clock_pin;

	pthread_t pth1;
	pthread_t pth2;
	
	pthread_create(&pth1,NULL,putData,"Putting data...");
	pthread_create(&pth2,NULL,sendData,"Reading data...");

	pthread_join(pth1,NULL);
	pthread_join(pth2,NULL);

	end = clock();
	time_spent = (double)(end - begin) / CLOCKS_PER_SEC;
	printf("Done in %f\r\n", time_spent);
	return 0;
}
