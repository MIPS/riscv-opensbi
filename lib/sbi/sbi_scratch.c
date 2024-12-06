 /*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Western Digital Corporation or its affiliates.
 *
 * Authors:
 *   Anup Patel <anup.patel@wdc.com>
 */

#include <sbi/riscv_locks.h>
#include <sbi/sbi_hart.h>
#include <sbi/sbi_hartmask.h>
#include <sbi/sbi_platform.h>
#include <sbi/sbi_scratch.h>
#include <sbi/sbi_string.h>

/* Minimum size and alignment of scratch allocations */
#define SCRATCH_ALLOC_ALIGN 64

u32 sbi_scratch_hart_count;
u32 hartindex_to_hartid_table[SBI_HARTMASK_MAX_BITS] = { [0 ... SBI_HARTMASK_MAX_BITS-1] = -1U };
struct sbi_scratch *hartindex_to_scratch_table[SBI_HARTMASK_MAX_BITS];

static spinlock_t extra_lock = SPIN_LOCK_INITIALIZER;
static unsigned long extra_offset = SBI_SCRATCH_EXTRA_SPACE_OFFSET;

u32 sbi_hartid_to_hartindex(u32 hartid)
{
	sbi_for_each_hartindex(i)
		if (hartindex_to_hartid_table[i] == hartid)
			return i;

	return -1U;
}

typedef struct sbi_scratch *(*hartid2scratch)(ulong hartid, ulong hartindex);

int sbi_scratch_init(struct sbi_scratch *scratch)
{
	u32 h, hart_count;
	const struct sbi_platform *plat = sbi_platform_ptr(scratch);

	hart_count = plat->hart_count;
	if (hart_count > SBI_HARTMASK_MAX_BITS)
		hart_count = SBI_HARTMASK_MAX_BITS;
	sbi_scratch_hart_count = hart_count;

	sbi_for_each_hartindex(i) {
		h = (plat->hart_index2id) ? plat->hart_index2id[i] : i;
		hartindex_to_hartid_table[i] = h;
		hartindex_to_scratch_table[i] =
			((hartid2scratch)scratch->hartid_to_scratch)(h, i);
	}

	return 0;
}

unsigned long sbi_scratch_alloc_offset(unsigned long size)
{
	void *ptr;
	unsigned long ret = 0;
	struct sbi_scratch *rscratch;

	/*
	 * We have a simple brain-dead allocator which never expects
	 * anything to be free-ed hence it keeps incrementing the
	 * next allocation offset until it runs-out of space.
	 *
	 * In future, we will have more sophisticated allocator which
	 * will allow us to re-claim free-ed space.
	 */

	if (!size)
		return 0;

	/*
	 * We let the allocation align to 64 bytes to avoid livelock on
	 * certain platforms due to atomic variables from the same cache line.
	 */
	size += SCRATCH_ALLOC_ALIGN - 1;
	size &= ~((unsigned long)SCRATCH_ALLOC_ALIGN - 1);

	spin_lock(&extra_lock);

	if (SBI_SCRATCH_SIZE < (extra_offset + size))
		goto done;

	ret = extra_offset;
	extra_offset += size;

done:
	spin_unlock(&extra_lock);

	if (ret) {
		sbi_for_each_hartindex(i) {
			rscratch = sbi_hartindex_to_scratch(i);
			if (!rscratch)
				continue;
			ptr = sbi_scratch_offset_ptr(rscratch, ret);
			sbi_memset(ptr, 0, size);
		}
	}

	return ret;
}

void sbi_scratch_free_offset(unsigned long offset)
{
	if ((offset < SBI_SCRATCH_EXTRA_SPACE_OFFSET) ||
	    (SBI_SCRATCH_SIZE <= offset))
		return;

	/*
	 * We don't actually free-up because it's a simple
	 * brain-dead allocator.
	 */
}

unsigned long sbi_scratch_used_space(void)
{
	unsigned long ret = 0;

	spin_lock(&extra_lock);
	ret = extra_offset;
	spin_unlock(&extra_lock);

	return ret;
}
