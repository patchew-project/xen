#ifndef __XEN_MIGRATION_STREAM_H__
#define __XEN_MIGRATION_STREAM_H__

#if !defined(__XEN__) && !defined(__XEN_TOOLS__)
#error "Migration stream definitions are intended for use by node control tools only"
#endif

/*
 * C structures for the Migration and Live Update.
 * See docs/specs/libxc-migration-stream.pandoc
 */

#include "xen.h"

/*
 * The domain types are used in the libxc stream domain header and will also
 * be used in corresponding records for live update.
 */
#define DHDR_TYPE_X86_PV  0x00000001U
#define DHDR_TYPE_X86_HVM 0x00000002U

/*
 * Record Header
 */
struct mr_rhdr
{
    uint32_t type;
    uint32_t length;
};

/* All records must be aligned up to an 8 octet boundary */
#define REC_ALIGN_ORDER               (3U)
/* Somewhat arbitrary - 128MB */
#define REC_LENGTH_MAX                (128U << 20)

#define REC_TYPE_END                        0x00000000U
#define REC_TYPE_PAGE_DATA                  0x00000001U
#define REC_TYPE_X86_PV_INFO                0x00000002U
#define REC_TYPE_X86_PV_P2M_FRAMES          0x00000003U
#define REC_TYPE_X86_PV_VCPU_BASIC          0x00000004U
#define REC_TYPE_X86_PV_VCPU_EXTENDED       0x00000005U
#define REC_TYPE_X86_PV_VCPU_XSAVE          0x00000006U
#define REC_TYPE_SHARED_INFO                0x00000007U
#define REC_TYPE_X86_TSC_INFO               0x00000008U
#define REC_TYPE_HVM_CONTEXT                0x00000009U
#define REC_TYPE_HVM_PARAMS                 0x0000000aU
#define REC_TYPE_TOOLSTACK                  0x0000000bU
#define REC_TYPE_X86_PV_VCPU_MSRS           0x0000000cU
#define REC_TYPE_VERIFY                     0x0000000dU
#define REC_TYPE_CHECKPOINT                 0x0000000eU
#define REC_TYPE_CHECKPOINT_DIRTY_PFN_LIST  0x0000000fU

#define REC_TYPE_OPTIONAL             0x80000000U
#define REC_TYPE_LIVE_UPDATE          0x40000000U

/* PAGE_DATA */
struct mr_page_data_header
{
    uint32_t count;
    uint32_t _res1;
    uint64_t pfn[0];
};

#define PAGE_DATA_PFN_MASK  0x000fffffffffffffULL
#define PAGE_DATA_TYPE_MASK 0xf000000000000000ULL

/* X86_PV_INFO */
struct mr_x86_pv_info
{
    uint8_t guest_width;
    uint8_t pt_levels;
    uint8_t _res[6];
};

/* X86_PV_P2M_FRAMES */
struct mr_x86_pv_p2m_frames
{
    uint32_t start_pfn;
    uint32_t end_pfn;
    uint64_t p2m_pfns[0];
};

/* X86_PV_VCPU_{BASIC,EXTENDED,XSAVE,MSRS} */
struct mr_x86_pv_vcpu_hdr
{
    uint32_t vcpu_id;
    uint32_t _res1;
    uint8_t context[0];
};

/* X86_TSC_INFO */
struct mr_x86_tsc_info
{
    uint32_t mode;
    uint32_t khz;
    uint64_t nsec;
    uint32_t incarnation;
    uint32_t _res1;
};

/* HVM_PARAMS */
struct mr_hvm_params_entry
{
    uint64_t index;
    uint64_t value;
};

struct mr_hvm_params
{
    uint32_t count;
    uint32_t _res1;
    struct mr_hvm_params_entry param[0];
};

#endif /* __XEN_MIGRATION_STREAM_H__ */

/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
