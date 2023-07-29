/**********************************************************
		转换显示字符串文件vgastr.c
***********************************************************
				彭东 ＠ 2011.12.15.07.30
**********************************************************/

#include "cmctl.h"
__attribute__((section(".data"))) cursor_t curs;

void DrawBlack(u32_t pix)
{
    u32_t* p = (u32_t*)curs.vmem_s;
    for(uint_t y = 0; y < curs.h; y++)
    {
       for (uint_t x = 0; x < curs.w; x++)
       {
           p[y * curs.w + x] = pix;
       }        
    }
    

    // for (int i = 0; i < 0x13c680; i++)
    // {
    //     p[i] = pix;
    // }
    return;
}


void Debug()
{
    // u32_t* p = (u32_t*)0xC0000000;
    // for (int i = 0; i < 0x13c680; i++)
    // {
    //     p[i] = 0xff1867e0;
    // }
    DrawBlack(0xff1867ff);
    return;
}

PUBLIC void init_curs(LoaderInfo* info)
{
    //knl_spinlock_init(&curs.c_lock);
    //knl_spinlock_init(&curs.p_lock);
	curs.vmem_s=(uint_t)info->FrameBase;
	curs.vmem_e=VGA_RAM_END;
	curs.cvmemadr=0;
	curs.x=0;
	curs.y=0;
    curs.w=info->With;
    curs.h=info->High;
    // Debug();
	return;
}

PUBLIC void GxH_strwrite(char_t* str,cursor_t* cursptr)
{

	//u32_t flags;
	uint_t straddr =cursptr->x+cursptr->y*80*2;
	char_t* p_strdst=(char_t*)(cursptr->vmem_s+straddr);
	u32_t tfindx=FALSE;
	//u32_t sidx=0;
	while(*str)
	{
		
		if(*str==10)
		{
			tfindx=TRUE;
			str++;
			if(*str==0)
			{
				break;
			}
		}
		
		current_curs(cursptr,VGACHAR_DF_CFLG);
		
		*p_strdst=*str++;
		p_strdst+=2;
	}
		
	if(tfindx==TRUE)
	{
		current_curs(cursptr,VGACHAR_LR_CFLG);
	}

	return;
}


PUBLIC char_t* numberk(char_t* str,uint_t n, sint_t base)
{
	register char_t *p;
	char_t strbuf[36];
        p = &strbuf[36];
        *--p = 0;
      
	if (n == 0)
        {
                *--p = '0';
        }
        
        else
        {
                do
                {
                        *--p = "0123456789abcdef"[n % base]; 
                }while( n /= base);
        }
        while(*p!=0)
        {
		*str++=*p++;
	}
        return str;
}

PUBLIC void set_curs(u32_t x, u32_t y)
{
	curs.x=x;
	curs.y=y;
	return;
}

PUBLIC void clear_screen(u16_t srrv)
{
	curs.x=0;
	curs.y=0;
	
    DrawBlack(0);
	return;
}

PUBLIC void put_one_char(char_t cr,uint_t x,uint_t y)
{
	
	char_t* p_strdst=(char_t*)(VGASTR_RAM_BASE+(x+(y*80*2)));
	*p_strdst=cr;
	return;
}

PUBLIC void close_curs()
{
	//out_u8(VGACTRL_REG_ADR,VGACURS_REG_INX);
	//out_u8(VGACTRL_REG_DAT,VGACURS_CLOSE);
	return;
}



PUBLIC char_t* strcopy(char_t* buf,char_t* str_s)
{
    while(*str_s)
    {
        *buf=*str_s;
        buf++;
        str_s++;
    }
    return buf;
}

PUBLIC void vsprintfk(char_t* buf,const char_t* fmt,va_list_t args)
{
    char_t* p =buf;
    va_list_t next_arg=args;
    while(*fmt)
    {
        if(*fmt != '%')
        {
            *p++ = *fmt++;
            continue;
        }
        fmt++;
        switch(*fmt)
        {
            case 'x':
                p=numberk(p,*((long*)next_arg),16);
                next_arg+=sizeof(long);
                fmt++;

                break;
            case 'd':
                p=numberk(p,*((long*)next_arg),10);
                next_arg+=sizeof(long);
                fmt++;
                break;
                case 's':
                   p=strcopy(p,(char_t*)(*((long*)next_arg)));
                   next_arg+=sizeof(long);
                   fmt++;
                   break;
            default:
                break;
        }
    }
    *p=0;
    return;
}

PUBLIC void kprint(const char_t* fmt,...)
{
    char_t buf[512];
    //u32_t flags;
    //spin_lock_disable_irq(&curs.p_lock,&flags);
    //memset((void*)buf,0,512);
    va_list_t arg=(va_list_t)((char_t*)(&fmt)+sizeof(long));

    vsprintfk(buf,fmt,arg);
    GxH_strwrite(buf,&curs);
    //spin_unlock_enabled_irq(&curs.p_lock,&flags);
    return;
}
