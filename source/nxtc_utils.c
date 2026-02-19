/*
 * nxtc_utils.c
 *
 * Copyright (c) 2025-2026, DarkMatterCore <pabloacurielz@gmail.com>.
 *
 * This file is part of libnxtc (https://github.com/DarkMatterCore/libnxtc).
 */

#include "nxtc_utils.h"

FsFileSystem *g_sdCardFileSystem = NULL;

bool nxtcUtilsCommitSdCardFileSystemChanges(void)
{
    return (g_sdCardFileSystem ? R_SUCCEEDED(fsFsCommit(g_sdCardFileSystem)) : false);
}

void *nxtcUtilsAlignedAlloc(size_t alignment, size_t size)
{
    if (!alignment || !IS_POWER_OF_TWO(alignment) || (alignment % sizeof(void*)) != 0 || !size) return NULL;

    if (!IS_ALIGNED(size, alignment)) size = ALIGN_UP(size, alignment);

    return aligned_alloc(alignment, size);
}

void nxtcUtilsTrimString(char *str)
{
    size_t strsize = 0;
    char *start = NULL, *end = NULL;

    if (!str || !(strsize = strlen(str))) return;

    start = str;
    end = (start + strsize);

    while(--end >= start)
    {
        if (!isspace((unsigned char)*end)) break;
    }

    *(++end) = '\0';

    while(isspace((unsigned char)*start)) start++;

    if (start != str) memmove(str, start, end - start + 1);
}

bool nxtcUtilsZlibDecompress(void* dst, size_t dst_sz, const void* src, size_t src_sz, int wbits)
{
    if (!dst || !dst_sz || !src || src_sz >= dst_sz)
    {
        NXTC_LOG_MSG("Invalid parameters!");
        return false;
    }

    z_stream zstrm = {0};
    int ret = Z_OK;
    bool success = false;

    /* Setup zlib stream settings. */
    zstrm.next_in = (z_const Bytef*)src;
    zstrm.avail_in = (uInt)src_sz;
    zstrm.next_out = (Bytef*)dst;
    zstrm.avail_out = (uInt)dst_sz;

    /* Decompress zlib stream. */
    ret = inflateInit2(&zstrm, wbits);
    if (ret != Z_OK)
    {
        NXTC_LOG_MSG("inflateInit2() failed! (%d) (%s).", ret, zstrm.msg);
        goto end;
    }

    ret = inflate(&zstrm, Z_FINISH);
    if (ret != Z_STREAM_END)
    {
        NXTC_LOG_MSG("inflate() failed! (%d) (%s).", ret, zstrm.msg);
        goto end;
    }

    ret = inflateEnd(&zstrm);
    if (ret != Z_OK) NXTC_LOG_MSG("inflateEnd() failed! (%d) (%s).", ret, zstrm.msg);

    /* Update flag. */
    success = true;

end:
    return success;
}
