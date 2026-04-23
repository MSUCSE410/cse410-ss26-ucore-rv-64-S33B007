#ifndef STAT_H
#define STAT_H

#include "types.h"

#define ST_DIR  0x040000
#define ST_FILE 0x100000

typedef struct {
	uint64 dev;    // drive number of the disk where the file is located
	uint64 ino;    // inode number
	uint32 mode;   // file type
	uint32 nlink;  // number of hard links
	uint64 pad[7]; // for compatibility, can be ignored
} Stat;

#endif // STAT_H