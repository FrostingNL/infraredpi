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
  for(i=0; i < 8; i++) {
    bin += (multiplier * buf[i]);
    multiplier *= 2;
  }
  //printf("Bin: %i, char: %c\r\n", bin, bin);
  return bin;
}

int clock_pin = 24;
int data_pin = 25;
int arrbit[10000];

int main() {
	setup_io();
	INP_GPIO(clock_pin);
	int prev_clk = 1;
  int clk = 1;
  int buf[8];
  int p = 0;
  int first = 1;
  int finished = 0;
	while(1) {
		clk = GET_GPIO(clock_pin) >> clock_pin;
		if(prev_clk != clk) {
      if(first) {
        first=0;
      } else {
        //printf("prev: %i, clk: %i\r\n", prev_clk, clk);
  		  int bit = GET_GPIO(data_pin) >> data_pin;
        if(bit==0) finished++;
        else finished = 0;
        arrbit[p] = bit;
        /*
        buf[p] = bit;
        if(p==7) {
          char c = get_char(buf);
          printf("%c\r\n", c); 
          p=-1;
        }
        */
        p++;
        if(finished==8) break;
		  }
    }
	prev_clk = clk;
	
  }

  int x;
  for(x=0; x<=p;x++) {
    printf("arrbit[%i]: %i\r\n", x, arrbit[x]);
  }

	return 0;
}