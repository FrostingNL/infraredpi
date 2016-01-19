#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

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
  for(i=7; i >= 0; i--) {
    bin += (multiplier * buf[i]);
    multiplier *= 2;
  }
  //printf("Bin: %i, char: %c\r\n", bin, bin);
  return bin;
}

int clock_pin = 24;
int data_pin = 25;
int arrbit[100000000];

int main() {
	setup_io();
	INP_GPIO(clock_pin);
	int prev_clk = 1;
    int clk = 1;
    int p = 0;
    int first = 1;
    int finished = 0;
	while(1) {
		clk = GET_GPIO(clock_pin) >> clock_pin;
		if(prev_clk != clk) {
      		if(first) {
        		first=0;
    		} else {
  		  		int bit = GET_GPIO(data_pin) >> data_pin;
		        if(bit==0) finished++;
		        else finished = 0;
		        arrbit[p] = bit;
		        
		        p++;
		        if(finished==24) break;
			}
    	}
		prev_clk = clk;
	}

  FILE* to_receive = fopen("toreceive.txt", "w");
  int i;
  for(i = 0; i <= p-24; i+=8) {
    int buf2[8] = {arrbit[i], arrbit[i+1], arrbit[i+2], arrbit[i+3], arrbit[i+4], arrbit[i+5], arrbit[i+6], arrbit[i+7]};
    fprintf(to_receive, "%c", get_char(buf2)); fflush(0);
  }
  fclose(to_receive);
  printf("Done! [p=%i, percentage buffer filled:%f]\r\n", p, (double)(((double)p/(double)100000000)*100));

	return 0;

	return 0;
}