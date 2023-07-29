#include "cmctl.h"

LoaderInfo* GLoaderinfo = NULL;

extern idtr_t IDT_PTR;
LoaderInfo* GetLoaderinfo()
{
    return GLoaderinfo;
}

void ldrkrl_entry(LoaderInfo* info)
{
    GLoaderinfo = info;

    init_curs(info);
    clear_screen(VGADP_DFVL);
    init_bstartparm();
    return;
}



void kerror(char_t* kestr)
{
    kprint("INITKLDR DIE ERROR:%s\n",kestr);
    for(;;);
    return;
}


#pragma GCC push_options
#pragma GCC optimize("O0")
void die(u32_t dt)
{

    u32_t dttt=dt,dtt=dt;
    if(dt==0)
    {
        for(;;);
    }

    for(u32_t i=0;i<dt;i++)
    {
        for(u32_t j=0;j<dtt;j++)
        {
            for(u32_t k=0;k<dttt;k++)
            {
                ;
            }
        }
    }
    return;
}
#pragma GCC pop_options