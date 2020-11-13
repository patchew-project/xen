/* 
    Simple prototype Xen Store Daemon providing simple tree-like database.
    Copyright (C) 2005 Rusty Russell IBM Corporation

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; If not, see <http://www.gnu.org/licenses/>.
*/

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/mman.h>

#include "utils.h"
#include "xenstored_core.h"
#include "xenstored_osdep.h"

void write_pidfile(const char *pidfile)
{
	char buf[100];
	int len;
	int fd;

	fd = open(pidfile, O_RDWR | O_CREAT, 0600);
	if (fd == -1)
		barf_perror("Opening pid file %s", pidfile);

	/* We exit silently if daemon already running. */
	if (lockf(fd, F_TLOCK, 0) == -1)
		exit(0);

	len = snprintf(buf, sizeof(buf), "%ld\n", (long)getpid());
	if (write(fd, buf, len) != len)
		barf_perror("Writing pid file %s", pidfile);

	close(fd);
}

/*
 * We don't have a working elf.h available here, so let's define our very own
 * data structs and accessor macros for ELF notes.
 *
 * https://docs.oracle.com/cd/E23824_01/html/819-0690/chapter6-18048.html:
 * For 64–bit objects and 32–bit objects, each entry is an array of 4-byte
 * words in the format of the target processor.
 */
typedef struct
{
	uint32_t namesz;
	uint32_t descsz;
	uint32_t type;
} elf_note_hdr;

/* ELF Note accessors, copied from Xen's elf.h */
#define ELFNOTE_ALIGN(_n_) (((_n_)+3)&~3)
#define ELFNOTE_NAME(_n_) ((char*)(_n_) + sizeof(*(_n_)))
#define ELFNOTE_DESC(_n_) (ELFNOTE_NAME(_n_) + ELFNOTE_ALIGN((_n_)->namesz))
/* GNU LD: type == note (NT_GNU_BUILD_ID as in
 * https://sourceware.org/ml/binutils/2007-07/msg00012.html)*/
#define NT_GNU_BUILD_ID 3


void write_buildid_file(const char *buildid_file)
{
	unsigned int i = 0;
	FILE *fdesc;
	extern elf_note_hdr __buildid_note_section;
	unsigned int id_length = __buildid_note_section.descsz;
	char* desc = ELFNOTE_DESC(&__buildid_note_section);

	if (__buildid_note_section.type != NT_GNU_BUILD_ID)
		barf("Expected GNU_BUILDID note, but found type '%d'",
		     __buildid_note_section.type);

	fdesc = fopen(buildid_file, "w+");
	if (!fdesc)
		barf_perror("Error opening buildid file %s", buildid_file);

	/* We exit silently if daemon already running. */
	if (lockf(fileno(fdesc), F_TLOCK, 0) == -1)
		exit(0);

	for (i = 0; i < id_length; ++i)
		fprintf(fdesc, "%02x", (unsigned char)desc[i]);
	fprintf(fdesc, "\n");

	fclose(fdesc);
}

/* Stevens. */
void daemonize(void)
{
	pid_t pid;

	/* Separate from our parent via fork, so init inherits us. */
	if ((pid = fork()) < 0)
		barf_perror("Failed to fork daemon");
	if (pid != 0)
		exit(0);

	/* Session leader so ^C doesn't whack us. */
	setsid();

	/* Let session leader exit so child cannot regain CTTY */
	if ((pid = fork()) < 0)
		barf_perror("Failed to fork daemon");
	if (pid != 0)
		exit(0);

	/* Move off any mount points we might be in. */
	if (chdir("/") == -1)
		barf_perror("Failed to chdir");

	/* Discard our parent's old-fashioned umask prejudices. */
	umask(0);
}

void finish_daemonize(void)
{
	int devnull = open("/dev/null", O_RDWR);
	if (devnull == -1)
		barf_perror("Could not open /dev/null\n");
	dup2(devnull, STDIN_FILENO);
	dup2(devnull, STDOUT_FILENO);
	dup2(devnull, STDERR_FILENO);
	close(devnull);
	xprintf = trace;
}

void init_pipe(int reopen_log_pipe[2])
{
	if (pipe(reopen_log_pipe)) {
		barf_perror("pipe");
	}
}

void unmap_xenbus(void *interface)
{
	munmap(interface, getpagesize());
}

#ifndef __sun__
evtchn_port_t xenbus_evtchn(void)
{
	int fd;
	int rc;
	evtchn_port_t port;
	char str[20];

	fd = open(XENSTORED_PORT_DEV, O_RDONLY);
	if (fd == -1)
		return -1;

	rc = read(fd, str, sizeof(str) - 1);
	if (rc == -1)
	{
		int err = errno;
		close(fd);
		errno = err;
		return -1;
	}

	str[rc] = '\0';
	port = strtoul(str, NULL, 0);

	close(fd);
	return port;
}

void *xenbus_map(void)
{
	int fd;
	void *addr;

	fd = open(XENSTORED_KVA_DEV, O_RDWR);
	if (fd == -1)
		return NULL;

	addr = mmap(NULL, getpagesize(), PROT_READ|PROT_WRITE,
		MAP_SHARED, fd, 0);

	if (addr == MAP_FAILED)
		addr = NULL;

	close(fd);

	return addr;
}

void xenbus_notify_running(void)
{
}
#endif /* !__sun__ */
