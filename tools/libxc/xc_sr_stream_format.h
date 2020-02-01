#ifndef __STREAM_FORMAT__H
#define __STREAM_FORMAT__H

#include <xen/migration_stream.h>

/*
 * C structures for the Migration v2 stream format.
 * See docs/specs/libxc-migration-stream.pandoc
 */

#include <inttypes.h>

/*
 * Image Header
 */
struct xc_sr_ihdr
{
    uint64_t marker;
    uint32_t id;
    uint32_t version;
    uint16_t options;
    uint16_t _res1;
    uint32_t _res2;
};

#define IHDR_MARKER  0xffffffffffffffffULL
#define IHDR_ID      0x58454E46U
#define IHDR_VERSION 2

#define _IHDR_OPT_ENDIAN 0
#define IHDR_OPT_LITTLE_ENDIAN (0 << _IHDR_OPT_ENDIAN)
#define IHDR_OPT_BIG_ENDIAN    (1 << _IHDR_OPT_ENDIAN)

/*
 * Domain Header
 */
struct xc_sr_dhdr
{
    uint32_t type;
    uint16_t page_shift;
    uint16_t _res1;
    uint32_t xen_major;
    uint32_t xen_minor;
};

#endif
/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
