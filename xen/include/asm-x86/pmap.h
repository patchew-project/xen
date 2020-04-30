#ifndef __X86_PMAP_H__
#define __X86_PMAP_H__

/* Large enough for mapping 5 levels of page tables with some headroom */
#define NUM_FIX_PMAP 8

void *pmap_map(mfn_t mfn);
void pmap_unmap(void *p);

#endif	/* __X86_PMAP_H__ */
