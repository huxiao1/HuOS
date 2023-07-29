#include "cmctl.h"

LoaderInfo* GLInfo = NULL;

bool_t HeadMemoryIsFree(LoaderInfo* info, uint_t start, uint_t size)
{
    uint_t end = start + size;
    e820map_t* emap;

    if(NULL == info)
    {
        return FALSE;
    }

    emap = info->Mem.E820Map;

    for(uint_t i = 0; i < info->Mem.E820MapNR; i++)
    {
        if(emap[i].type == RAM_USABLE)
        {
            if((start >= emap[i].saddr) && 
                (emap[i].saddr + (emap[i].lsize - 1) > end))
            {
                return TRUE;
            }
        }
    }
    return FALSE;
}

void inithead_entry(LoaderInfo* info)
{
    GLInfo = info;
    init_curs(info);
    clear_screen(VGADP_DFVL);
    write_ldrkrlfile();
    return;
}

void write_realintsvefile()
{
    fhdsc_t *fhdscstart = find_file("initldrsve.bin");
    if (fhdscstart == NULL)
    {
        error("not file initldrsve.bin");
    }
    if(HeadMemoryIsFree(GLInfo, REALDRV_PHYADR, (uint_t)fhdscstart->fhd_frealsz) == FALSE)
    {
        error("not Memory space");
    }
    m2mcopy((void *)((uint_t)(fhdscstart->fhd_intsfsoff) + LDRFILEADR),
            (void *)REALDRV_PHYADR, (sint_t)fhdscstart->fhd_frealsz);
    return;
}

fhdsc_t *find_file(char_t *fname)
{
    mlosrddsc_t *mrddadrs = MRDDSC_ADR;
    if (mrddadrs->mdc_endgic != MDC_ENDGIC ||
        mrddadrs->mdc_rv != MDC_RVGIC ||
        mrddadrs->mdc_fhdnr < 2 ||
        mrddadrs->mdc_filnr < 2)
    {
        error("no mrddsc");
    }

    s64_t rethn = -1;
    fhdsc_t *fhdscstart = (fhdsc_t *)((uint_t)(mrddadrs->mdc_fhdbk_s) + LDRFILEADR);

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
        error("not find file");
    }
    return &fhdscstart[rethn];
}

void write_ldrkrlfile()
{
    fhdsc_t *fhdscstart = find_file("initldrkrl.bin");
    if (fhdscstart == NULL)
    {
        error("not file initldrkrl.bin");
    }
    if(HeadMemoryIsFree(GLInfo, ILDRKRL_PHYADR, (uint_t)fhdscstart->fhd_frealsz) == FALSE)
    {
        error("not Memory space");
    }
    m2mcopy((void *)((uint_t)(fhdscstart->fhd_intsfsoff) + LDRFILEADR),
            (void *)ILDRKRL_PHYADR, (sint_t)fhdscstart->fhd_frealsz);
    
    return;
}


void error(char_t *estr)
{
    Debug();
    // kprint("INITLDR DIE ERROR:%s\n", estr);
    for (;;)
        ;
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
