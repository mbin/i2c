#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <fcntl.h>
#include <linux/types.h>
#include <string.h>


#include "../driver/myspi.h"
#include "gpio.h"


#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))


static unsigned short test[]={0x0400,0x1400,0x0401,0x1400,0x0402,0x1400,};
static unsigned char rx[1025]={0};

const char *fun_items[] =
{\
   "0:Quit",\
   "1:Test all register of FPGA",\
   "2:SPI Test",\
   "3:Test all register of CPLD",\
   "4:Disable 24M clock",\
   "5:Enbale 24M clock",\
//-------------------------------------------------------------
	"6:FPGA ccm-enable register",\
	"7:FPGA zero-set register",\
	"8:FPGA light-control register",\
	"9:FPGA laser-control register",\
	"10:FPGA color-switch register",\
	"11:FPGA video-source-switch register",\
	"12:FPGA zoom switch register",\
	"13:FPGA low 8bit calculus-time register",\
	"14:FPGA hight 8bit calculus-time register",\
	"15:FPGA low 8bit sensor-temperature register",\
	"16:FPGA hight 8bit sensor-temperature register",\	
};

struct spi_cmd
{
	unsigned char reg_addr;
	unsigned char optcode ;
	unsigned char* tips;
#define OPT_READ 1
#define OPT_RANDW 2
};

struct spi_cmd spi_array[] =
{
	{0x00,OPT_READ,"CPLD logic subversion register"},\
	{0x01,OPT_READ,"CPLD logic version register"},\
	{0x02,OPT_READ,"logic compiled day register(Month)"},\
	{0x03,OPT_READ,"logic compiled day register(Year)"},\
	{0x05,OPT_RANDW,"LCD control register"},\
	{0x06,OPT_RANDW,"LCOS control register"},\
	{0x07,OPT_RANDW,"Interrupte status register"},\
	{0x08,OPT_RANDW,"Interrupte mask register"},\
	{0x09,OPT_RANDW,"Reset control register"},\
	{0x0A,OPT_RANDW,"MISC control register"},\
	{0x0B,OPT_RANDW,"LCD brightness control register"},\
//-------------------------------------------------------------
	{0x00,OPT_RANDW,"FPGA ccm-enable register"},\
	{0x01,OPT_RANDW,"FPGA zero-set register"},\
	{0x02,OPT_RANDW,"FPGA light-control register"},\
	{0x03,OPT_RANDW,"FPGA laser-control register"},\
	{0x04,OPT_RANDW,"FPGA color-switch register"},\
	{0x05,OPT_RANDW,"FPGA video-source-switch register"},\

	{0x06,OPT_RANDW,"FPGA zoom switch register"},\
	{0x07,OPT_RANDW,"FPGA low 8bit calculus-time register"},\
	{0x08,OPT_RANDW,"FPGA hight 8bit calculus-time register"},\
	{0x09,OPT_RANDW,"FPGA low 8bit sensor-temperature register"},\
	{0x0A,OPT_RANDW,"FPGA hight 8bit sensor-temperature register"}

};



static void pabort(const char *s)
{
	perror(s);
	abort();
}

static void configspi(int fd, const enum spi_selchip chipidx,\
	   const enum spi_dataformat datformat,const enum spi_cshold spi_cshold)
{
	ioctl(fd,Cmd_reset,Cmd_reset);
	ioctl(fd,Cmd_enspi,chipidx);
	ioctl(fd,Cmd_dataformat,datformat);
	ioctl(fd,Cmd_Cshold,spi_cshold);
	ioctl(fd,Cmd_Selchip,chipidx);
}

static void enbale_epcs(const int action)
{
   if(1 == action)
   {
	 gpio_write(PIN_AS_MODE,HIGH_LEVEL); //输出高电平
	 gpio_write(PIN_EPCS_CONFIG,LOW_LEVEL); //输出低电平
   }
   else if(0 == action)
   {
	 gpio_write(PIN_AS_MODE,LOW_LEVEL); //输出高电平
	 gpio_write(PIN_EPCS_CONFIG,HIGH_LEVEL); //输出低电平
   }
}



static void testspi(int fd,const enum spi_selchip chipidx,\
            const unsigned short *pdata,const int len)
{
   int j,i=0;
   int ret = -1;
   unsigned short rd = 0 ;
   static unsigned short cmd_rd = 0x8400;

   if(Sel_chip1 == chipidx)
   {
	  write(fd,pdata,len);
	  write(fd,&cmd_rd,1);
	  ret = read(fd,&rd,1);
	  #if 1
	  if(ret>0)
	  {
		 printf("\n test data=0x%x,CPLD return value=0x%02x \n",(unsigned short)(*pdata),(unsigned char)rd);
	  }
	  #endif
   }
   else
   {
	  for(i=0;i<len;i++)
	  {
		 write(fd,pdata,len);
		 printf("\n the write value=0x%x \n",pdata[i]);
	  }
   }
}

static void testepcs(int fd)
{
   unsigned char cmd[10] = {0x006,0x00,0x00,0x00};
   int ret = 0 ;
   int i = 0;
   //write(fd,cmd,1);  //cmd to write enable ;

#if 1
   cmd[0] = 0xAB;
   cmd[1] = 0x00;
   cmd[2] = 0x00;
   cmd[3] = 0x00;
   cmd[4] = 0x00;
   write(fd,cmd,5);  //cmd to read status;
   ret = read(fd,rx,5);
   for(i=0;i<ret;i++)
   {
	 printf("\nrx=%x\n",rx[i]);
   }
#endif
}

static void testgpio(void)
{
#if 1
   gpio_export(PIN_AS_MODE);
   gpio_direction(PIN_AS_MODE,OUT);    // GPIO为输出状态
   gpio_write(PIN_AS_MODE,HIGH_LEVEL); //输出高电平

   gpio_export(PIN_EPCS_CONFIG);
   gpio_direction(PIN_EPCS_CONFIG,OUT);   // GPIO为输出状态
   gpio_write(PIN_EPCS_CONFIG,LOW_LEVEL); //输出低电平
#endif
   gpio_export(PIN_SPI_EN0);
   gpio_direction(PIN_SPI_EN0,OUT);   // GPIO为输出状态
   gpio_write(PIN_SPI_EN0,LOW_LEVEL); //输出低电平

}

static void  testgpio2(int fd)
{
 #if 0
   ioctl(fd,Cmd_Selpin,1);
   sleep(1);
   ioctl(fd,Cmd_Selpin,0);
	sleep(1);
   ioctl(fd,Cmd_Selpin,1);
	sleep(1);
	ioctl(fd,Cmd_Selpin,0);
 #endif
}

inline void process(const int fd,const int index,int needread)
{
	unsigned short int value = -1;
	unsigned short int temp = 0;
	int ret = -1;

	printf("\n%s %s's value\n",\
	(spi_array[index].optcode == OPT_READ)?"Will return":"Please input",\
		spi_array[index].tips);
	value = 0;
	if(spi_array[index].optcode == OPT_RANDW)
	{
		scanf("%d",&value);
		value &= 0x00FF;
	}
	temp = ((unsigned short)((spi_array[index].reg_addr)<<8));
	value |= temp;
	if(spi_array[index].optcode == OPT_READ)
	{
	   value |= 0x8000;
	}
	ret = write(fd,&value,1);
	printf("\nSend Command = 0x%04x %s\n",value,(ret>0)?"successfully":"failed");
	if((spi_array[index].optcode == OPT_RANDW) && (needread))
	{
		value &= 0xFF00;
		value |= 0x8000;
		ret = write(fd,&value,1);
		printf("\nSend Read Command = 0x%04x %s\n",value,(ret>0)?"successfully":"failed");
		value = 0;
		ret = read(fd,&value,sizeof(value));
	    printf("\nRead value = 0x%04x %s\n",(unsigned char)value,(ret>0)?"successfully":"failed");
	}
}

int lastcmd = 0;

int main(int argc,char *argv[])
{
   int fd = -1;
   int i = 0 ;
   int bexit=1;
   int value = -1;
   unsigned short tt = 0x0405;
   unsigned short reg_cmd;
   int ret = -1;
   int cnt = 0;
   //unsigned char configed_flage[ARRAY_SIZE(fun_items)] = {0};

#if 0
   gpio_export(PIN_AS_MODE);
   gpio_direction(PIN_AS_MODE,OUT);    // GPIO为输出状态
   gpio_export(PIN_EPCS_CONFIG);
   gpio_direction(PIN_EPCS_CONFIG,OUT);   // GPIO为输出状态
#endif

   #if 1
   fd = open("/dev/spi",O_RDWR);
   if (fd < 0)
   {
	pabort("can't open device");
   }
#if 0
   gpio_export(PIN_AS_MODE);
   gpio_direction(PIN_AS_MODE,OUT);    // GPIO为输出状态
   gpio_export(PIN_EPCS_CONFIG);
   gpio_direction(PIN_EPCS_CONFIG,OUT);   // GPIO为输出状态
#endif
   #endif

   while(bexit)
   {
	 for(i=0;i<ARRAY_SIZE(fun_items);i++)
	 {
	   printf("%s\n",fun_items[i]);
     }
	 printf("input your choice:0-%d\n",ARRAY_SIZE(fun_items)-1);
	 scanf("%d",&value);
	 if(value < 0 || value > ARRAY_SIZE(fun_items))
	 {
	   value = 0;
	   continue;
	 }
	 switch(value)
	 {
		case 0:
		default:
		{
           bexit = 0;
		}
		break;
		case 1:
		{
		  if(lastcmd != value)
		  {
			configspi(fd,Sel_chip0,format_16bit,Cs_active_unhold);
			lastcmd = value;
		  }
		  cnt = 6;
		  for(i=11;i<cnt+11;i++)
		  {
             process(fd,i,1);
		  }//for
		}
		break;
		case 2:
		{
		  if(lastcmd != value)
		  {
			configspi(fd,Sel_chip1,format_16bit,Cs_active_unhold);
			lastcmd = value;
          }
		  #if 1
		  for(i=0;i<=3;i++)
		  {
			testspi(fd,Sel_chip1,&tt,1);
			tt++;
		  }
		  tt = 0x0405;
		  #else
		  testspi(fd,Sel_chip1,test,ARRAY_SIZE(test));
		  #endif
		}
		break;
		case 3:
		{
		   if(lastcmd != value)
		   {
			 configspi(fd,Sel_chip1,format_16bit,Cs_active_unhold);
			 lastcmd = value;
           }
		   cnt = 10;
		   for(i=0;i<cnt;i++)
		   {
			 process(fd,i,1);
		   }//for
		}
		break;
		case 4:
		{
             ioctl(fd,Cmd_disable24M,Cmd_disable24M);
		}
		break;
		case 5:
		{
            ioctl(fd,Cmd_enable24M,Cmd_enable24M);
		}
		break;
		case 6:
		case 7:
		case 8:
		case 9:
		case 10:
		case 11:
		case 12:
		case 13:
		case 14:
		case 15:
		case 16:		
		{
		   if(lastcmd != value)
		   {
			 configspi(fd,Sel_chip0,format_16bit,Cs_active_unhold);
			 lastcmd = value;
			 printf("\nhave configed\n");
		   }
		   process(fd,(value + 5),0);
		}
		break;
	 }//switch
   }//while

   close(fd);
   return 0 ;
}
