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

int clock_pin = 24;
int data_pin1 = 25;
int data_pin2 = 17;
int data_pin3 = 22;
int data_pin4 = 6;
volatile int buffer[256];
volatile int in_buffer;
volatile int r = 0;
volatile int p = 0;
volatile int is_done = 0;

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

void *receiveData() {
  setup_io();
  INP_GPIO(clock_pin);
  INP_GPIO(data_pin1);
  INP_GPIO(data_pin2);
  INP_GPIO(data_pin3);
  INP_GPIO(data_pin4);
  int prev_clk = 1;
  int clk = 1;
  int buf[8];
  int first = 1;
  int finished = 0;
  while(1) {
    clk = GET_GPIO(clock_pin) >> clock_pin;
    if(prev_clk != clk) {
      if(first) {
        first=0;
      } else {
        //printf("prev: %i, clk: %i\r\n", prev_clk, clk);
        int bit1 = GET_GPIO(data_pin1) >> data_pin1;
        int bit2 = GET_GPIO(data_pin2) >> data_pin2;
        int bit3 = GET_GPIO(data_pin3) >> data_pin3;
        int bit4 = GET_GPIO(data_pin4) >> data_pin4;
        if(bit1==0 && bit2==0 &&bit3==0 &&bit4==0) finished++;
        else finished = 0;
        
        buffer[p] = bit1;
        buffer[p+1] = bit2;
        buffer[p+2] = bit3;
        buffer[p+3] = bit4;
        p += 4;
        if(p>=256) p = 0;

        if(finished==2) break;
      }
    }
  prev_clk = clk;
  sleep(1);
  is_done = 1;
  }

  int i;
  for(i = 0; i <= p; i+=8) {
    //printf("%i: %i, [%i]\r\n", i, arrbit[i], p); fflush(0);
    //int buf2[8] = {buffer[i], buffer[i+1], buffer[i+2], buffer[i+3], buffer[i+4], buffer[i+5], buffer[i+6], buffer[i+7]};
    //printf("%c", get_char(buf2)); fflush(0);
  }
}

void *convertData() {
  int in_buffer;
  while(1) {
    if (r < p && r <= 255) {
      printf("%i, %i\r\n", p,r);
      r++;
    }
  }
}


int main() {
  pthread_t pth0;
  pthread_t pth1;
  
  pthread_create(&pth0,NULL,receiveData,"Putting data...");
  pthread_create(&pth1,NULL,convertData,"Putting data...");

  pthread_join(pth0,NULL);
  pthread_join(pth1,NULL);
}