//#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <linux/types.h>
#include <string.h>

#include "tca9548.h"

#define CMD_I2C_SLAVE_FORCE	0x0706
#define SLAVE_TCA9548_ADDR  0x70

const static unsigned char cmd_unsealbq27621_wr[][2] =
{
	{0x00,0x00},
    {0x01,0x80},
	{0x00,0x00},
    {0x01,0x80},
	{0x00,0x13},
	{0x01,0x00}
};

static unsigned char cmd_chgChemID_wr[][2] =
{
	{0x00,0x31},
	{0x01,0x00}
};

static unsigned char cmd_updateblock[][2] =
{
	{0x60,0x00},
	{0x3e,0x52},
	{0x3f,0x00}
};

const static unsigned char cmd_exit_updateblock[][2] = 
{
	{ 0x00, 0x42 },
	{ 0x01, 0x00 },
	{ 0x00, 0x20 },
	{ 0x01, 0x00 }
};

const static unsigned char cmd_initdata[][2] =
{
	{ 0x43, 0x18 },
	{ 0x44, 0x9c },
	{ 0x45, 0x5B },
	{ 0x46, 0x0E },
	{ 0x49, 0x0c },
	{ 0x4a, 0x1C },
	{ 0x54, 0x01 },
	{ 0x55, 0xFD }
};

int open_master(const char *pname)
{
   int fd = -1 ;
   if(NULL == pname)
   {
	 return -1;
   }

   return open(I2C_CONTROLLER_NAME,O_RDWR);
}

int select_slave(const int fd,const unsigned int addr)
{
   return ioctl(fd,CMD_I2C_SLAVE_FORCE,addr);
}

int enable_switcher_chan(const int fd,const int en_chx)
{
   int ret = -1;
   unsigned char val = (unsigned char)en_chx;

   if((en_chx < 0x01) || ((en_chx > 0x80)))
   {
     return -1;
   }

   ret = select_slave(fd,SLAVE_TCA9548_ADDR);
   if(ret < 0)
   {
     return -1;
   }
   return write_device(fd,&val,1);
}

int disable_switcher_chan(const int fd)
{
   int ret = -1;
   unsigned char val = DISABLE_TCA9548_ALLCH;

   ret = select_slave(fd,SLAVE_TCA9548_ADDR);
   if(ret < 0)
   {
     return -1;
   }
   return write_device(fd,&val,1);
}

int write_device(const int fd,const unsigned char *pdata,const unsigned int len)
{
	if((fd < 0) || (NULL == pdata) || (len < 0))
	{
       return -1;
	}
	return write(fd,pdata,len);
}

void close_master(const int *pfd)
{
    close(*pfd);
}
/*
  1.enable switcher-chan2
  2.select_device, use the addr->SLAVE_BQ27621_ADDR
  3.return 0,failed to enter configmode,1,successful
*/
int bq27621_unseal(const int fd)
{
	int i,ret;	
	static unsigned char cmd = 0x06;
	int flage = 0;
	unsigned short status = 0;

	for(i=0;i<ARRAY_SIZE(cmd_unsealbq27621_wr);i++) //sizeof(cmd_unsealbq27621_wr[i]
	{
	   ret = write_device(fd,cmd_unsealbq27621_wr[i],sizeof(cmd_unsealbq27621_wr[i]));
#if ZML_DEBUG
	   printf("\n bq27621_enter_configmode,ret=%d, buf[0]=%x,buf[1]=%x\n",ret,
			  cmd_unsealbq27621_wr[i][0],
              cmd_unsealbq27621_wr[i][1]
	   );
#endif
	}//for

	ret = write(fd,&cmd,sizeof(cmd));
	for(i=0;i<5;i++)
	{
		ret = read(fd, &status, sizeof(status));
#if ZML_DEBUG
		printf("\nret=%d,status=0x%x\n", ret, status);
#endif
		if ((status & 0x0010) == 0x0010)
	    {
		  flage = 1;
		  break;
	    }
	    else
	    {
          sleep(1);
	    }
	}
    return flage;
}

/*
   this function will control the bq27621 exit from config mode,and sealed it again   
*/
int bq27621_exit_configmode(const int fd) 
{
	int ret = -1;
	int i;

	for (i = 0; i<ARRAY_SIZE(cmd_exit_updateblock); i++)
	{
		ret = write_device(fd, cmd_exit_updateblock[i], sizeof(cmd_exit_updateblock[i]));		
	}
	return ret;
}


/*you should call bq27621_enter_configmode first
  return value, none zero means success!
*/
int bq27621_chg_chemID(const int fd,const unsigned short chemID)
{
	int i;
	int ret = 1;

	if(chemID != INVALID_CHEM_ID)
	{
	   if(CHEM_ID_1210 == chemID)
	   {
		  cmd_chgChemID_wr[0][1] = 0x31;
	   }
	   else if(CHEM_ID_354 == chemID)
	   {
		  cmd_chgChemID_wr[0][1] = 0x32;
	   }
	   for(i=0;i<ARRAY_SIZE(cmd_chgChemID_wr);i++)
	   {
		  ret = write_device(fd,cmd_chgChemID_wr[i],sizeof(cmd_chgChemID_wr[i]));
#if ZML_DEBUG
		  printf("\n ret=%d, function=%s \n",ret,__func__);
#endif
	   }
	}//if(chemID != INVALID_CHEM_ID)

    return ret ;
}
/*you should call bq27621_enter_configmode first
  return value, none zero means success!
  type: 0 setup to the update, chksum == 0x00
        1 verify the update,you should set the chksum param
*/
int bq27621_update_blcokram(const int fd,const int type,unsigned char chksum)
{
    int i,ret;

	if (0 == type)
	{
		cmd_updateblock[0][0] = 0x61;
		cmd_updateblock[0][1] = 0x00;
	}
	else
	{
		cmd_updateblock[0][0] = 0x60;
		cmd_updateblock[0][1] = chksum;
	}
	for(i=0;i<ARRAY_SIZE(cmd_updateblock);i++)
	{
	   ret = write_device(fd, cmd_updateblock[i], sizeof(cmd_updateblock[i]));	   
	}
	return ret;
}

int bq27621_readword(const int fd, const unsigned char reg_offset, unsigned char* buf)
{
	unsigned char temp = reg_offset;

	if (NULL == buf)
	{
       return -1;
	}
	write_device(fd, &(temp), sizeof(temp));
	return read(fd, buf, 2);
}

/*
对ba27621进行初始化，设置默认数值,-1失败，1 成功
*/
int init_bq27621(const int fd)
{
	int ret = -1;
	int temp;
	int i;
	unsigned short status = 0;
	unsigned char old_chksum = 0x0;
	unsigned char newchksum = 0x0;
	unsigned char old_dc[2] = { 0x0 };
	unsigned char old_de[2] = { 0x0 };
	unsigned char old_tv[2] = { 0x0 };
	unsigned char old_tr[2] = { 0x0 };
	unsigned char bufcmd[2] = { 0x0 };
	
	ret = bq27621_unseal(fd);
	if (ret != 1)
	{
		return -1;		
	}
	ret = bq27621_update_blcokram(fd,0,0x00);
	if (ret < 0)
	{
		return -1;
	}
	bq27621_readword(fd, REGOFFSET_CHECKSUM, &old_chksum);

	bq27621_readword(fd, REGOFFSET_DESIGNCAPCITY, old_dc);
	bq27621_readword(fd, REGOFFSET_DESIGNENERGY, old_de);
	bq27621_readword(fd, REGOFFSET_TERMINATEVOL, old_tv);
	bq27621_readword(fd, REGOFFSET_TAPERATE, old_tr);
	
	temp = 0xFF - old_chksum;	
	temp -= (old_de[0] + old_de[1] + old_tv[0] + old_tv[1] + old_tr[0] + old_tr[1] + old_dc[0] + old_dc[1]);
	for (i = 0; i < ARRAY_SIZE(cmd_initdata); i++)
	{
		temp += (cmd_initdata[i][1]);
		ret = write_device(fd, (cmd_initdata[i]), sizeof(cmd_initdata[i]));
	}
	newchksum = (unsigned char)temp;
	newchksum = 0xFF - newchksum;
	ret = bq27621_update_blcokram(fd, 1, newchksum);
	if (ret < 0)
	{
		return -1;
	}
	bq27621_readword(fd, REGOFFSET_CHECKSUM, (unsigned char*)(&status));
	if ((unsigned char)status == newchksum)
	{
		ret = 1;
	}
	else
	{
		ret = -1;
	}
	bq27621_exit_configmode(fd);
	return ret;
}
/*
   juge the bq2621 is charging or not,1: discharging, 0 charging
*/
int bq27621_ischarging(const int fd)
{
	unsigned short status = 0;
	int ret = -1;
	unsigned char cmd[1] = { REGOFFSET_GETFLAGS };

	write(fd, cmd, 1);
	ret = read(fd, (unsigned char*)(&status), 2);
	if (ret > 0)
	{
		if (status & 0x0001)
		{
			return 1;
		}
		else
		{
			return 0;
		}		
	}
	else
	{
		return -1;
	}
}
/*
show value of the predicted RemainingCapacity( )expressed as a percentage of FullChargeCapacity( ), 
with a range of 0 to 100%,-1 means failed 
*/
int bq27621_stateofcharg(const int fd)
{
	unsigned short status = 0;
	int ret = -1;
	unsigned char cmd[1] = { REGOFFSET_STATEOFCHARGE };

	write(fd, cmd, 1);
	ret = read(fd, (unsigned char*)(&status), 2);
	if (ret > 0)
	{
		return status;
	}
	else
	{
		return -1;
	}	
}

/*
get the voltage of bq27621,-1 means failed 
*/
int bq27621_getvoltage(const int fd)
{
	unsigned short status = 0;
	signed int ret = -1;
	unsigned char cmd[1] = { REGOFFSET_VOLTAGE };
	
	write(fd,cmd,1);
	ret = read(fd, (unsigned char*)(&status), 2);

	if (ret > 0)
	{		
		return status;
	}
	else
	{		
		return -1;
	}
}

/*
get the current of bq27621,-1 means failed
*/
int bq27621_getcurrent(const int fd)
{
	short status = 0;
	int ret = -1;
	unsigned char cmd[1] = { REGOFFSET_CURRENT };

	write(fd, cmd, 1);
	ret = read(fd, (unsigned char*)(&status), 2);
	if (ret > 0)
	{
		if (status & 0x8000)
		{
			status = (~status);
		}
		return status;
	}
	else
	{
		return -1;
	}
}
/*
get the average power of bq27621,-1 means failed
*/
int bq27621_getavgpower(const int fd)
{
	short status = 0;
	int ret = -1;
	unsigned char cmd[1] = { REGOFFSET_AVGPOWER };

	write(fd, cmd, 1);
	ret = read(fd, (unsigned char*)(&status), 2);

	if (ret > 0)
	{
		if (status & 0x8000)
		{
			status = (~status);
		}
		return status;
	}
	else
	{
		return -1;
	}
}

int bq27621_getremainingcapcity(const int fd)
{
	short status = 0;
	int ret = -1;
	unsigned char cmd[1] = { REGOFFSET_GETREMAININGCAPCITY };

	write(fd, cmd, 1);
	ret = read(fd, (unsigned char*)(&status), 2);
	
	if (ret > 0)
	{
		return status;
	}
	else
	{
		return -1;
	}
}

int bq27621_getfullchargecapcity(const int fd)
{
	short status = 0;
	int ret = -1;
	unsigned char cmd[1] = { REGOFFSET_GETRFULLCHARGECAPCITY };

	write(fd, cmd, 1);
	ret = read(fd, (unsigned char*)(&status), 2);

	if (ret > 0)
	{
		return status;
	}
	else
	{
		return -1;
	}
}