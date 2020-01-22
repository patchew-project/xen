/**
 * Copyright (c) 2018 Anup Patel.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * @file linker.ld
 * @author Anup Patel (anup@brainfault.org)
 * @brief CPU specific linker script
 */

#include <xen/cache.h>
#include <asm/percpu.h>
#undef ENTRY
#undef ALIGN

OUTPUT_ARCH(riscv)
ENTRY(start)

PHDRS
{
  text PT_LOAD ;
#if defined(BUILD_ID)
  note PT_NOTE ;
#endif
}

SECTIONS
{
  . = XEN_VIRT_START;
  . = ALIGN(PAGE_SIZE); /* Need this to create proper sections */
  PROVIDE(_code_start = .);

  /* Beginning of the code section */

  .text :
  {
    PROVIDE(_text_start = .);
    _stext = .;            /* Text section */
    *(.entry)
    *(.text)
    . = ALIGN(8);
    _etext = .;             /* End of text section */
    PROVIDE(_text_end = .);
  } :text = 0x9090

  . = ALIGN(PAGE_SIZE); /* Ensure next section is page aligned */

  .init :
  {
    PROVIDE(_init_start = .);

    *(.init.text)
    . = ALIGN(8);

    PROVIDE(__setup_start = .);
    *(.setup.init);
    PROVIDE(__setup_end = .);

    . = ALIGN(PAGE_SIZE);
    *(.devtree)
    . = ALIGN(PAGE_SIZE);

    PROVIDE(_init_end = .);

    . = ALIGN(PAGE_SIZE);

    PROVIDE(_initdata_start = .);
    *(.init.data)
    . = ALIGN(8);
    PROVIDE(_initdata_end = .);
  }

  . = ALIGN(PAGE_SIZE); /* Ensure next section is page aligned */

  .cpuinit :
  {
    PROVIDE(_cpuinit_start = .);
    *(.cpuinit.*)
    . = ALIGN(8);
    PROVIDE(_cpuinit_end = .);
  }

  . = ALIGN(PAGE_SIZE); /* Ensure next section is page aligned */

  .spinlock :
  {
    PROVIDE(_spinlock_start = .);
    *(.spinlock.*)
    . = ALIGN(8);
    PROVIDE(_spinlock_end = .);
  }

  /* End of the code sections */

  /* Beginning of the read-only data sections */

  . = ALIGN(PAGE_SIZE); /* Ensure next section is page aligned */

  .rodata :
  {
    PROVIDE(_rodata_start = .);
    *(.rodata .rodata.*)
    . = ALIGN(8);
     _srodata = .;
     /* Bug frames table */
    __start_bug_frames = .;
    *(.bug_frames.0)
    __stop_bug_frames_0 = .;
    *(.bug_frames.1)
    __stop_bug_frames_1 = .;
    *(.bug_frames.2)
    __stop_bug_frames_2 = .;
    *(.rodata)
    *(.rodata.*)
    *(.data.rel.ro)
    *(.data.rel.ro.*)

   . = ALIGN(POINTER_ALIGN);
   __param_start = .;
   *(.data.param)
   __param_end = .;

   __proc_info_start = .;
   *(.proc.info)
   __proc_info_end = .;
    PROVIDE(_rodata_end = .);
  }

  . = ALIGN(4);
  .note.gnu.build-id : {
       __note_gnu_build_id_start = .;
       *(.note.gnu.build-id)
       __note_gnu_build_id_end = .;
  } :note :text

  /* End of the read-only data sections */

  /* Beginning of the read-write data sections */

  . = ALIGN(PAGE_SIZE); /* Ensure next section is page aligned */

  .percpu :
  {
    PROVIDE(_percpu_start = .);
    *(.percpu)
    . = ALIGN(8);
    PROVIDE(_percpu_end = .);
  }

  . = ALIGN(PAGE_SIZE); /* Ensure next section is page aligned */

  .data :
  {
    PROVIDE(_data_start = .);

   *(.data.page_aligned)
   *(.data)
   . = ALIGN(8);
   __start_schedulers_array = .;
   *(.data.schedulers)
   __end_schedulers_array = .;
   *(.data.rel)
   *(.data.rel.*)
   CONSTRUCTORS

    PROVIDE(_data_end = .);
  }

  . = ALIGN(PAGE_SIZE);             /* Init code and data */
  __init_begin = .;
  .init.text : {
       _sinittext = .;
       *(.init.text)
       _einittext = .;
  } :text
  . = ALIGN(PAGE_SIZE);
  .init.data : {
       *(.init.rodata)
       *(.init.rodata.rel)
       *(.init.rodata.str*)

       . = ALIGN(POINTER_ALIGN);
       __setup_start = .;
       *(.init.setup)
       __setup_end = .;

       __initcall_start = .;
       *(.initcallpresmp.init)
       __presmp_initcall_end = .;
       *(.initcall1.init)
       __initcall_end = .;

       . = ALIGN(4);
       __alt_instructions = .;
       *(.altinstructions)
       __alt_instructions_end = .;
       . = ALIGN(4);
       *(.altinstr_replacement)

       *(.init.data)
       *(.init.data.rel)
       *(.init.data.rel.*)

       . = ALIGN(8);
       __ctors_start = .;
       *(.ctors)
       *(.init_array)
       *(SORT(.init_array.*))
       __ctors_end = .;

  } :text
  . = ALIGN(STACK_SIZE);
  __init_end = .;

  . = ALIGN(PAGE_SIZE); /* Ensure next section is page aligned */

  .bss :
  {
    PROVIDE(_bss_start = .);
   *(.bss.stack_aligned)
   . = ALIGN(PAGE_SIZE);
   *(.bss.page_aligned)
   *(.bss)
   . = ALIGN(SMP_CACHE_BYTES);
   __per_cpu_start = .;
   *(.bss.percpu)
   . = ALIGN(SMP_CACHE_BYTES);
   *(.bss.percpu.read_mostly)
   . = ALIGN(SMP_CACHE_BYTES);
   __per_cpu_data_end = .;
    PROVIDE(_bss_end = .);
  }

  . = ALIGN(PAGE_SIZE); /* Ensure next section is page aligned */

  .hvc_stack :
  {
    PROVIDE(_hvc_stack_start = .);
    . = . + (STACK_SIZE * CONFIG_NR_CPUS);
    . = ALIGN(8);
    PROVIDE(_hvc_stack_end = .);
  }

  /* End of the read-write data sections */

  . = ALIGN(PAGE_SIZE); /* Need this to create proper pages */

  PROVIDE(_code_end = .);
}
