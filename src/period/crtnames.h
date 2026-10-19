/* crtnames.h: every C library name the game uses maps to src/crt's
 * crt_* implementation, whether or not the module includes the header
 * (several declare library functions themselves). Included by prelude.h. */
#ifndef RECIV_CRTNAMES_H
#define RECIV_CRTNAMES_H
#define _bios_disk crt_bios_disk
#define _dos_close crt_dos_close
#define _dos_creat crt_dos_creat
#define _dos_freemem crt_dos_freemem
#define _dos_getdrive crt_dos_getdrive
#define _dos_getvect crt_dos_getvect
#define _dos_open crt_dos_open
#define _dos_read crt_dos_read
#define _dos_setvect crt_dos_setvect
#define _dos_write crt_dos_write
#define _iobuf crt_iobuf
#define abs crt_abs
#define atoi crt_atoi
#define atol crt_atol
#define close crt_close
#define errno crt_errno
#define exit crt_exit
#define fclose crt_fclose
#define fgetc crt_fgetc
#define fgets crt_fgets
#define fopen crt_fopen
#define fprintf crt_fprintf
#define fread crt_fread
#define free crt_free
#define fscanf crt_fscanf
#define fseek crt_fseek
#define ftell crt_ftell
#define fwrite crt_fwrite
#define getch crt_getch
#define int86 crt_int86
#define intdos crt_intdos
#define itoa crt_itoa
#define kbhit crt_kbhit
#define labs crt_labs
#define lseek crt_lseek
#define ltoa crt_ltoa
#define malloc crt_malloc
#define memcmp crt_memcmp
#define memcpy crt_memcpy
#define memset crt_memset
#define movedata crt_movedata
#define open crt_open
#define perror crt_perror
#define printf crt_printf
#define rand crt_rand
#define read crt_read
#define sprintf crt_sprintf
#define srand crt_srand
#define strcat crt_strcat
#define strchr crt_strchr
#define strcmp crt_strcmp
#define strcpy crt_strcpy
#define stricmp crt_stricmp
#define strlen crt_strlen
#define strlwr crt_strlwr
#define strncmp crt_strncmp
#define strncpy crt_strncpy
#define strnicmp crt_strnicmp
#define strstr crt_strstr
#define strupr crt_strupr
#define time crt_time
#define write crt_write
#endif
