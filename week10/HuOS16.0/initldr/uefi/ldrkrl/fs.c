#include "cmctl.h"

void fs_entry()
{
    return;
}

int strcmpl(const char *a, const char *b)
{

    while (*b && *a && (*b == *a))
    {

        b++;

        a++;
    }

    return *b - *a;
}

fhdsc_t *get_fileinfo(char_t *fname, machbstart_t *mbsp)
{
    //mlosrddsc_t* mrddadrs=is_imgheadmrddsc();

    mlosrddsc_t *mrddadrs = (mlosrddsc_t *)((uint_t)(mbsp->mb_imgpadr + MLOSDSC_OFF));
    if (mrddadrs->mdc_endgic != MDC_ENDGIC ||
        mrddadrs->mdc_rv != MDC_RVGIC ||
        mrddadrs->mdc_fhdnr < 2 ||
        mrddadrs->mdc_filnr < 2)
    {
        Debug();
        kerror("no mrddsc");
    }

    /*if(mrddadrs==NULL)
    {
        error("no mrddsc");
    }*/
    s64_t rethn = -1;
    fhdsc_t *fhdscstart = (fhdsc_t *)((uint_t)(mrddadrs->mdc_fhdbk_s) + ((uint_t)(mbsp->mb_imgpadr)));

    for (u64_t i = 0; i < mrddadrs->mdc_fhdnr; i++)
    {
        if (strcmpl(fname, fhdscstart[i].fhd_name) == 0)
        {
            rethn = (s64_t)i;
            goto ok_l;
        }
    }
    rethn = -1;
ok_l:
    if (rethn < 0)
    {
        kerror("not find file");
    }
    return &fhdscstart[rethn];
}

int move_krlimg(machbstart_t *mbsp, u64_t cpyadr, u64_t cpysz)
{

    if (0xffffffff <= (cpyadr + cpysz) || 1 > cpysz)
    {
        return 0;
    }
    void *toadr = (void *)((uint_t)(P4K_ALIGN(cpyadr + cpysz)));
    sint_t tosz = (sint_t)mbsp->mb_imgsz;
    if (0 != adrzone_is_ok(mbsp->mb_imgpadr, mbsp->mb_imgsz, cpyadr, cpysz))
    {
        if (NULL == chk_memsize((e820map_t *)((uint_t)(mbsp->mb_e820padr)), (uint_t)mbsp->mb_e820nr, (u64_t)(toadr), (u64_t)tosz))
        {
            return -1;
        }
        m2mcopy((void *)((uint_t)mbsp->mb_imgpadr), toadr, tosz);
        mbsp->mb_imgpadr = (u64_t)(toadr);
        return 1;
    }
    return 2;
}

void init_krlfile(machbstart_t *mbsp)
{
    u64_t sz = r_file_to_padr(mbsp, IMGKRNL_PHYADR, "Cosmos.bin");
    if (0 == sz)
    {
        Debug();
        kerror("r_file_to_padr err");
    }
    mbsp->mb_krlimgpadr = IMGKRNL_PHYADR;
    mbsp->mb_krlsz = sz;
    // mbsp->mb_nextwtpadr = P4K_ALIGN(mbsp->mb_krlimgpadr + mbsp->mb_krlsz);
    mbsp->mb_kalldendpadr = mbsp->mb_krlimgpadr + mbsp->mb_krlsz;
    return;
}

void init_defutfont(machbstart_t *mbsp)
{
    u64_t sz = 0;
    uint_t dfadr = 0;
    sz = ReadFileToMemPhyAddr(mbsp, &dfadr, "font.fnt");
    if (0 == sz)
    {
        Debug();
        kerror("ReadFileToMemPhyAddr err");
    }
    mbsp->mb_bfontpadr = (u64_t)(dfadr);
    mbsp->mb_bfontsz = sz;
    mbsp->mb_kalldendpadr = mbsp->mb_bfontpadr + mbsp->mb_bfontsz;
    return;
}

void get_file_rpadrandsz(char_t *fname, machbstart_t *mbsp, uint_t *retadr, uint_t *retsz)
{
    u64_t padr = 0, fsz = 0;
    if (NULL == fname || NULL == mbsp)
    {
        *retadr = 0;
        return;
    }
    fhdsc_t *fhdsc = get_fileinfo(fname, mbsp);
    if (fhdsc == NULL)
    {
        *retadr = 0;
        return;
    }

    padr = fhdsc->fhd_intsfsoff + mbsp->mb_imgpadr;
    if (padr > 0xffffffff)
    {

        *retadr = 0;
        return;
    }
    fsz = (uint_t)fhdsc->fhd_frealsz;
    if (fsz > 0xffffffff)
    {
        *retadr = 0;
        return;
    }
    *retadr = (uint_t)padr;
    *retsz = (uint_t)fsz;
    return;
}

u64_t get_filesz(char_t *filenm, machbstart_t *mbsp)
{
    if (filenm == NULL || mbsp == NULL)
    {
        return 0;
    }
    fhdsc_t *fhdscstart = get_fileinfo(filenm, mbsp);
    if (fhdscstart == NULL)
    {
        return 0;
    }
    return fhdscstart->fhd_frealsz;
}

u64_t get_wt_imgfilesz(machbstart_t *mbsp)
{
    u64_t retsz = LDRFILEADR;
    mlosrddsc_t *mrddadrs = MRDDSC_ADR;
    if (mrddadrs->mdc_endgic != MDC_ENDGIC ||
        mrddadrs->mdc_rv != MDC_RVGIC ||
        mrddadrs->mdc_fhdnr < 2 ||
        mrddadrs->mdc_filnr < 2)
    {
        return 0;
    }
    if (mrddadrs->mdc_filbk_e < 0x4000)
    {
        return 0;
    }
    retsz += mrddadrs->mdc_filbk_e;
    retsz -= LDRFILEADR;
    mbsp->mb_imgpadr = LDRFILEADR;
    mbsp->mb_imgsz = retsz;
    return retsz;
}

u64_t r_file_to_padr(machbstart_t *mbsp, uint_t f2adr, char_t *fnm)
{
    uint_t fpadr = 0, sz = 0;
    uint_t start = 0;
    if (NULL == f2adr || NULL == fnm || NULL == mbsp)
    {
        return 0;
    }
    get_file_rpadrandsz(fnm, mbsp, &fpadr, &sz);
    // Debug();
    if (0 == fpadr || 0 == sz)
    {
        return 0;
    }
    start = LoaderAllocMemroy(sz);
    if((0 == start) || (start != f2adr))
    {
        return 0;
    }
    if (0 != chkadr_is_ok(mbsp, start, sz))
    {
        return 0;
    }
    m2mcopy((void *)fpadr, (void *)start, (sint_t)sz);
    return sz;
}

uint_t ReadFileToMemPhyAddr(machbstart_t *mbsp, uint_t* f2adr, char_t *fnm)
{
    uint_t fpadr = 0, sz = 0;
    uint_t start = 0;
    if(NULL == fnm || NULL == mbsp || NULL == f2adr)
    {
        return 0;
    }
    start = *f2adr;
    get_file_rpadrandsz(fnm, mbsp, &fpadr, &sz);
    if(0 == fpadr || 0 == sz)
    {
        return 0;
    }
    if(0 == start)
    {
        start = LoaderAllocMemroy(sz);
        if(0 == start)
        {
            return 0;
        }
    }
    else
    {
        if(MemoryIsFree(start, sz) == FALSE)
        {
            return 0;
        }
    }
    
    if(0 != chkadr_is_ok(mbsp, start, sz))
    {
        return 0;
    }
    m2mcopy((void *)fpadr, (void *)start, (sint_t)sz);
    *f2adr = start;
    return sz;
}

u64_t ret_imgfilesz()
{
    u64_t retsz = LDRFILEADR;
    mlosrddsc_t *mrddadrs = MRDDSC_ADR;
    if (mrddadrs->mdc_endgic != MDC_ENDGIC ||
        mrddadrs->mdc_rv != MDC_RVGIC ||
        mrddadrs->mdc_fhdnr < 2 ||
        mrddadrs->mdc_filnr < 2)
    {
        kerror("no mrddsc");
    }
    if (mrddadrs->mdc_filbk_e < 0x4000)
    {
        kerror("imgfile error");
    }
    retsz += mrddadrs->mdc_filbk_e;
    retsz -= LDRFILEADR;
    return retsz;
}
