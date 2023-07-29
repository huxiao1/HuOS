#include "cmctl.h"


void write_pixcolor(machbstart_t* mbsp,u32_t x,u32_t y,pixl_t pix)
{

    u8_t* p24bas =(u8_t*)(uint_t)(mbsp->mb_ghparm.gh_framphyadr);
    if(mbsp->mb_ghparm.gh_onepixbits == 24)
    {
            uint_t p24adr=(x+(y*mbsp->mb_ghparm.gh_x))*3;
            p24bas=(u8_t*)((uint_t)(p24adr+mbsp->mb_ghparm.gh_framphyadr));
            p24bas[0]=(u8_t)(pix);
            p24bas[1]=(u8_t)(pix>>8);//pvalptr->cl_g;
            p24bas[2]=(u8_t)(pix>>16);//pvalptr->cl_r;
            return;
    }
    u32_t* phybas=(u32_t*)(uint_t)(mbsp->mb_ghparm.gh_framphyadr);
    phybas[x+(y*mbsp->mb_ghparm.gh_x)]=pix;
    //u32_t* phybas=(u32_t*)mbsp->mb_ghparm.gh_framphyadr;
    //phybas[x+(y*1024)]=*((u32_t*)pvalptr);
    return;
}

void bmp_print(void* bmfile,machbstart_t* mbsp)
{
    if(NULL==bmfile)
    {
        return;
    }
    pixl_t pix=0;
    bmdbgr_t* bpixp=NULL;
    bmfhead_t* bmphdp=(bmfhead_t*)bmfile;
    bitminfo_t* binfp= (bitminfo_t*)(bmphdp+1);
    uint_t img=(uint_t)bmfile+bmphdp->bf_off;
    bpixp=(bmdbgr_t*)img;
    int l=0;//binfp->bi_h;
    int k=0;
    int ilinebc = (((binfp->bi_w*24) + 31) >> 5) << 2;
    for(int y=639;y>=129;y--,l++)
    {
        k=0;
        for(int x=322;x<662;x++)
        {
            pix=BGRA(bpixp[k].bmd_r,bpixp[k].bmd_g,bpixp[k].bmd_b);
            write_pixcolor(mbsp,x,y,pix);
            k++;
        }
        bpixp=(bmdbgr_t*)((uint_t)bpixp+ilinebc);
    }
    // Debug();

    return;
}

void logo(machbstart_t* mbsp)
{
    uint_t retadr=0,sz=0;
    get_file_rpadrandsz("logo.bmp",mbsp,&retadr,&sz);
    if(0==retadr)
    {
        kerror("logo getfilerpadrsz err");
    }
    
    bmp_print((void*)retadr,mbsp);

    return;
}


void init_graph(machbstart_t* mbsp)
{
    LoaderInfo* info = GetLoaderinfo();
    if(NULL == info)
    {
        Debug();
        return; 
    }
    graph_t_init(&mbsp->mb_ghparm);
    if(mbsp->mb_ghparm.gh_mode!=BGAMODE)
    {
        mbsp->mb_ghparm.gh_mode=VBEMODE;
        mbsp->mb_ghparm.gh_vbemodenr=0x118;

        mbsp->mb_ghparm.gh_x = (u32_t)info->With;
        mbsp->mb_ghparm.gh_y = (u32_t)info->High;
        mbsp->mb_ghparm.gh_framphyadr = (u32_t)info->FrameBase;
        mbsp->mb_ghparm.gh_onepixbits = 0x20;
    }

    init_kinitfvram(mbsp);
    logo(mbsp);
    return;
}

void graph_t_init(graph_t* initp)
{
    memset(initp,0,sizeof(graph_t));
    return;
}

void init_kinitfvram(machbstart_t* mbsp)
{
    uint_t start = 0, sz = 0;
    if(mbsp->mb_ghparm.gh_mode == VBEMODE)
    { 
        sz = mbsp->mb_ghparm.gh_x * mbsp->mb_ghparm.gh_y * (mbsp->mb_ghparm.gh_onepixbits/8);
        start = LoaderAllocMemroy(sz);
        if(0 == start)
        {
            Debug();
            return;
        }
        mbsp->mb_fvrmphyadr = start;
        mbsp->mb_fvrmsz = sz;
        memset((void*)start, 0, sz);
    }
    else
    {
        start = LoaderAllocMemroy(KINITFRVM_SZ);
        if(0 == start)
        {
            Debug();
            return;
        }
        mbsp->mb_fvrmphyadr = start;
        mbsp->mb_fvrmsz = KINITFRVM_SZ;
        memset((void*)start,0,KINITFRVM_SZ);
    }
    return;
}



u32_t utf8_to_unicode(utf8_t* utfp,int* retuib)
{
    u8_t uhd=utfp->utf_b1,ubyt=0;
    u32_t ucode=0,tmpuc=0;
    if(0x80>uhd)//0xbf&&uhd<=0xbf
    {
        ucode=utfp->utf_b1&0x7f;
        *retuib=1;
        return ucode;
    }
    if(0xc0<=uhd&&uhd<=0xdf)//0xdf
    {
        ubyt=utfp->utf_b1&0x1f;
        tmpuc|=ubyt;
        ubyt=utfp->utf_b2&0x3f;
        ucode=(tmpuc<<6)|ubyt;
        *retuib=2;
        return ucode;
    }
    if(0xe0<=uhd&&uhd<=0xef)//0xef
    {
        ubyt=utfp->utf_b1&0x0f;
        tmpuc|=ubyt;
        ubyt=utfp->utf_b2&0x3f;
        tmpuc<<=6;
        tmpuc|=ubyt;
        ubyt=utfp->utf_b3&0x3f;
        ucode=(tmpuc<<6)|ubyt;
        *retuib=3;
        return ucode;
    }
    if(0xf0<=uhd&&uhd<=0xf7)//0xf7
    {
        ubyt=utfp->utf_b1&0x7;
        tmpuc|=ubyt;
        ubyt=utfp->utf_b2&0x3f;
        tmpuc<<=6;
        tmpuc|=ubyt;
        ubyt=utfp->utf_b3&0x3f;
        tmpuc<<=6;
        tmpuc|=ubyt;
        ubyt=utfp->utf_b4&0x3f;
        ucode=(tmpuc<<6)|ubyt;
        *retuib=4;
        return ucode;
    }
    if(0xf8<=uhd&&uhd<=0xfb)//0xfb
    {
        ubyt=utfp->utf_b1&0x3;
        tmpuc|=ubyt;
        ubyt=utfp->utf_b2&0x3f;
        tmpuc<<=6;
        tmpuc|=ubyt;
        ubyt=utfp->utf_b3&0x3f;
        tmpuc<<=6;
        tmpuc|=ubyt;
        ubyt=utfp->utf_b4&0x3f;
        tmpuc<<=6;
        tmpuc|=ubyt;
        ubyt=utfp->utf_b5&0x3f;
        ucode=(tmpuc<<6)|ubyt;
        *retuib=5;
        return ucode;
    }
    if(0xfc<=uhd&&uhd<=0xfd)//0xfd
    {
        ubyt=utfp->utf_b1&0x1;
        tmpuc|=ubyt;
        ubyt=utfp->utf_b2&0x3f;
        tmpuc<<=6;
        tmpuc|=ubyt;
        ubyt=utfp->utf_b3&0x3f;
        tmpuc<<=6;
        tmpuc|=ubyt;
        ubyt=utfp->utf_b4&0x3f;
        tmpuc<<=6;
        tmpuc|=ubyt;
        ubyt=utfp->utf_b5&0x3f;
        tmpuc<<=6;
        tmpuc|=ubyt;
        ubyt=utfp->utf_b6&0x3f;
        ucode=(tmpuc<<6)|ubyt;
        *retuib=6;
        return ucode;
    }
    *retuib=0;
    return 0;
}
