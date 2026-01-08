#include "lfs_w25q64.h"
#include "lfs.h"
#include <string.h>
extern lfs_t lfs;
extern struct lfs_config lfs_cfg;

/*
 * Block-device hooks bridging littlefs <-> W25Q128
 * Update the function names if your driver exposes different symbols.
 */
static int bd_read(const struct lfs_config *c, lfs_block_t block, lfs_off_t off,
                   void *buffer, lfs_size_t size)
{
    uint32_t addr = (uint32_t)block * c->block_size + off;
    W25Q64_Read(addr, (uint8_t*)buffer, (size_t)size);
    return 0;
}

static int bd_prog(const struct lfs_config *c, lfs_block_t block, lfs_off_t off,
                   const void *buffer, lfs_size_t size)
{
    uint32_t addr = (uint32_t)block * c->block_size + off;
    /* The driver should split across 256B page boundaries internally */
    W25Q64_PageProgram(addr, (const uint8_t*)buffer, (size_t)size);
    return 0;
}

static int bd_erase(const struct lfs_config *c, lfs_block_t block)
{
    uint32_t addr = (uint32_t)block * c->block_size;
    W25Q64_SectorErase4K(addr);
    return 0;
}

static int bd_sync(const struct lfs_config *c)
{
    (void)c; /* NOR typically needs no explicit sync */
    return 0;
}

void LFS_W25Q64_InitConfig(struct lfs_config *cfg)
{
    memset(cfg, 0, sizeof(*cfg));

    /* I/O hooks */
    cfg->read  = bd_read;
    cfg->prog  = bd_prog;
    cfg->erase = bd_erase;
    cfg->sync  = bd_sync;

    /* Geometry */
    cfg->read_size      = LFS_W25Q128_READ_SIZE;
    cfg->prog_size      = LFS_W25Q128_PROG_SIZE;
    cfg->block_size     = LFS_W25Q128_BLOCK_SIZE;
    cfg->block_count    = LFS_W25Q128_BLOCK_COUNT;

    /* Tunables */
    cfg->cache_size     = LFS_W25Q128_CACHE_SIZE;
    cfg->lookahead_size = LFS_W25Q128_LOOKAHEAD;
    cfg->block_cycles   = LFS_W25Q128_BLOCK_CYCLES;

    /* Optional compile-time safety checks (uncomment if desired) */
    /*
    _Static_assert(LFS_W25Q128_CACHE_SIZE % LFS_W25Q128_READ_SIZE == 0, "cache%read");
    _Static_assert(LFS_W25Q128_CACHE_SIZE % LFS_W25Q128_PROG_SIZE == 0, "cache%prog");
    _Static_assert(LFS_W25Q128_BLOCK_SIZE % LFS_W25Q128_CACHE_SIZE == 0, "cache|block");
    _Static_assert((LFS_W25Q128_LOOKAHEAD % 8) == 0, "lookahead%8");
    */
}

int LFS_W25Q64_Mount(lfs_t *lfs, const struct lfs_config *cfg)
{
    return lfs_mount(lfs, cfg);
}

int LFS_W25Q64_FormatAndMount(lfs_t *lfs, const struct lfs_config *cfg)
{
    int rc = lfs_format(lfs, cfg);
    if (rc) return rc;
    return lfs_mount(lfs, cfg);
}

void LFS_W25Q64_Unmount(lfs_t *lfs)
{
    (void)lfs_unmount(lfs);
}

uint8_t FS_IsNearFull(uint32_t reserve_blocks)
{
    lfs_ssize_t used = lfs_fs_size(&lfs);
    uint32_t total = lfs_cfg.block_count;
    if (used < 0 || total == 0) return 0; // conservative: not full
    return ((uint32_t)used >= (total - reserve_blocks)) ? 1 : 0;
}
