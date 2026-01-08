#ifndef LFS_W25Q128_H
#define LFS_W25Q128_H

#include <stdint.h>
#include <stddef.h>
#include "lfs.h"
// Update this include to match the header name of your 128-Mbit driver
#include "w25q64.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * LittleFS <-> Winbond W25Q128 (128 Mbit = 16 MiB) bridge
 * Geometry assumptions (per W25Q128JV/FV family):
 *  - Erase sector: 4096 bytes
 *  - Page program size: 256 bytes
 *  - Total capacity: 16 MiB -> 4096 sectors
 *
 * You can override these macros at compile time if needed.
 */
#ifndef LFS_W25Q128_BLOCK_SIZE
#define LFS_W25Q128_BLOCK_SIZE     4096u   /* 4 KiB erase sector */
#endif
#ifndef LFS_W25Q128_BLOCK_COUNT
#define LFS_W25Q128_BLOCK_COUNT    4096u   /* 16 MiB / 4 KiB */
#endif
#ifndef LFS_W25Q128_PROG_SIZE
#define LFS_W25Q128_PROG_SIZE       256u   /* page program granularity */
#endif
#ifndef LFS_W25Q128_READ_SIZE
#define LFS_W25Q128_READ_SIZE       256u   /* align reads to page */
#endif
#ifndef LFS_W25Q128_CACHE_SIZE
#define LFS_W25Q128_CACHE_SIZE      256u   /* multiple of read/prog; factor of block_size */
#endif
#ifndef LFS_W25Q128_LOOKAHEAD
#define LFS_W25Q128_LOOKAHEAD       256u   /* multiple of 8; 256B->window 2048 blocks */
#endif
#ifndef LFS_W25Q128_BLOCK_CYCLES
#define LFS_W25Q128_BLOCK_CYCLES    500    /* 100..1000 typical */
#endif

/* API */
void LFS_W25Q64_InitConfig(struct lfs_config *cfg);
int  LFS_W25Q64_Mount(lfs_t *lfs, const struct lfs_config *cfg);
int  LFS_W25Q64_FormatAndMount(lfs_t *lfs, const struct lfs_config *cfg);
void LFS_W25Q64_Unmount(lfs_t *lfs);
// Returns 1 if filesystem is near full (used >= total - reserve_blocks), else 0.
// Requires that littlefs is already mounted on global 'lfs' with valid 'lfs_cfg'.
uint8_t FS_IsNearFull(uint32_t reserve_blocks);

#ifdef __cplusplus
}
#endif

#endif /* LFS_W25Q128_H */

