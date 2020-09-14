/*
 * xen/common/efi/pe.c
 *
 * PE executable header parser.
 *
 * Derived from https://github.com/systemd/systemd/blob/master/src/boot/efi/pe.c
 * commit 07d5ed536ec0a76b08229c7a80b910cb9acaf6b1
 *
 * Copyright (C) 2015 Kay Sievers <kay@vrfy.org>
 * Copyright (C) 2020 Trammell Hudson <hudson@trmm.net>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 */


#include "efi.h"

struct DosFileHeader {
    UINT8   Magic[2];
    UINT16  LastSize;
    UINT16  nBlocks;
    UINT16  nReloc;
    UINT16  HdrSize;
    UINT16  MinAlloc;
    UINT16  MaxAlloc;
    UINT16  ss;
    UINT16  sp;
    UINT16  Checksum;
    UINT16  ip;
    UINT16  cs;
    UINT16  RelocPos;
    UINT16  nOverlay;
    UINT16  reserved[4];
    UINT16  OEMId;
    UINT16  OEMInfo;
    UINT16  reserved2[10];
    UINT32  ExeHeader;
} __attribute__((packed));

#define PE_HEADER_MACHINE_ARM64         0xaa64
#define PE_HEADER_MACHINE_X64           0x8664
#define PE_HEADER_MACHINE_I386          0x014c

struct PeFileHeader {
    UINT16  Machine;
    UINT16  NumberOfSections;
    UINT32  TimeDateStamp;
    UINT32  PointerToSymbolTable;
    UINT32  NumberOfSymbols;
    UINT16  SizeOfOptionalHeader;
    UINT16  Characteristics;
} __attribute__((packed));

struct PeHeader {
    UINT8   Magic[4];
    struct PeFileHeader FileHeader;
} __attribute__((packed));

struct PeSectionHeader {
    UINT8   Name[8];
    UINT32  VirtualSize;
    UINT32  VirtualAddress;
    UINT32  SizeOfRawData;
    UINT32  PointerToRawData;
    UINT32  PointerToRelocations;
    UINT32  PointerToLinenumbers;
    UINT16  NumberOfRelocations;
    UINT16  NumberOfLinenumbers;
    UINT32  Characteristics;
} __attribute__((packed));

const void *__init pe_find_section(const CHAR8 *image, const UINTN image_size,
                                   const char *section_name, UINTN *size_out)
{
    const struct DosFileHeader *dos = (const void*)image;
    const struct PeHeader *pe;
    const struct PeSectionHeader *sect;
    const UINTN name_len = strlen(section_name);
    UINTN offset = 0;
    UINTN i;

    if ( name_len > sizeof(sect->Name) ||
         image_size < sizeof(*dos) ||
         memcmp(dos->Magic, "MZ", 2) != 0 )
        return NULL;

    offset = dos->ExeHeader;
    pe = (const void *) &image[offset];

    offset += sizeof(*pe);
    if ( image_size < offset ||
         memcmp(pe->Magic, "PE\0\0", 4) != 0 )
        return NULL;

    /* PE32+ Subsystem type */
#if defined(__arm__) || defined (__aarch64__)
    if ( pe->FileHeader.Machine != PE_HEADER_MACHINE_ARM64 )
        return NULL;
#elif defined(__x86_64__)
    if ( pe->FileHeader.Machine != PE_HEADER_MACHINE_X64 )
        return NULL;
#else
    /* unknown architecture */
    return NULL;
#endif

    offset += pe->FileHeader.SizeOfOptionalHeader;

    for ( i = 0; i < pe->FileHeader.NumberOfSections; i++ )
    {
        sect = (const void *)&image[offset];
        if ( image_size < offset + sizeof(*sect) )
            return NULL;

        if ( memcmp(sect->Name, section_name, name_len) != 0 ||
             image_size < sect->VirtualSize + sect->VirtualAddress )
        {
            offset += sizeof(*sect);
            continue;
        }

        if ( size_out )
            *size_out = sect->VirtualSize;

        return &image[sect->VirtualAddress];
    }

    return NULL;
}
