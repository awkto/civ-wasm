/* bios.h: MSC 5.1 declarations (disk services, emulated by src/platform). */
#ifndef RECIV_BIOS_H
#define RECIV_BIOS_H

#define _DISK_RESET 0
#define _DISK_VERIFY 4
struct diskinfo_t { unsigned int drive, head, track, sector, nsectors; void *buffer; };

unsigned int _bios_disk(unsigned int, struct diskinfo_t *);
#endif
