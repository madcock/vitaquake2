
#ifndef __LIBRETRO_FILE_H
#define __LIBRETRO_FILE_H

#include <file/file_path.h>
#include <streams/file_stream.h>

RFILE* rfopen(const char *path, const char *mode);
int rfclose(RFILE* stream);
int64_t rftell(RFILE* stream);
int64_t rfseek(RFILE* stream, int64_t offset, int origin);
int64_t rfread(void* buffer, size_t elem_size, size_t elem_count, RFILE* stream);
char *rfgets(char *buffer, int maxCount, RFILE* stream);
int64_t rfwrite(void const* buffer, size_t elem_size, size_t elem_count, RFILE* stream);
int64_t rfflush(RFILE * stream);
int rfprintf(RFILE * stream, const char * format, ...);
int rferror(RFILE* stream);

#endif
