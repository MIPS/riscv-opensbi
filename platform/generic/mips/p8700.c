/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 MIPS
 *
 */

#include <platform_override.h>
#include <sbi/sbi_domain.h>
#include <sbi/sbi_error.h>
#include <sbi/sbi_hsm.h>
#include <sbi/sbi_timer.h>
#include <sbi_utils/fdt/fdt_helper.h>
#include <mips/p8700.h>
#include <mips/mips-cm.h>

extern void mips_cps_core_entry(void);

#if CLUSTERS_IN_PLATFORM > 1
static void power_up_other_cluster(u32 hartid)
{
	unsigned int stat;
	unsigned int timeout;
	bool local_p = (cpu_cluster(current_hartid()) == cpu_cluster(hartid));

	/* Power up cluster cl core 0 hart 0 */
	write_cpc_pwrup_ctl(hartid, 1, local_p);

	/* Wait for the CM to start up */
	timeout = 100;
	while (true) {
		stat = read_cpc_cm_stat_conf(hartid, local_p);
		stat = EXT(stat, CPC_Cx_STAT_CONF_SEQ_STATE);
		if (stat == CPC_Cx_STAT_CONF_SEQ_STATE_U5)
			break;

		/* Delay a little while before we start warning */
		if (timeout) {
			sbi_dprintf("Delay a little while before we start warning\n");
			timeout--;
		}
		else {
			sbi_printf("Waiting for cluster %u CM to power up... STAT_CONF=0x%x\n",
				   cpu_cluster(hartid), stat);
			break;
		}
	}
}
#endif

static int mips_hart_start(u32 hartid, ulong saddr)
{
	unsigned int stat;
	unsigned int timeout;
	bool local_p = (cpu_cluster(current_hartid()) == cpu_cluster(hartid));

	/* Hart 0 is the boot hart, and we don't use the CPC cmd to start.  */
	if (hartid == 0)
		return SBI_ENOTSUPP;

	if (cpu_hart(hartid) == 0) {
		/* Change cluster cl core co hart 0 reset base */
		write_gcr_co_reset_base(hartid,
					(unsigned long)mips_cps_core_entry, local_p);

		/* Ensure its coherency is disabled */
		write_gcr_co_coherence(hartid, 0, local_p);

		/* Start cluster cl core co hart 0 */
		write_cpc_co_vp_run(hartid, 1 << cpu_hart(hartid), local_p);

		/* Reset cluster cl core co hart 0 */
		write_cpc_co_cmd(hartid, CPC_Cx_CMD_RESET, local_p);

		timeout = 100;
		while (true) {
			stat = read_cpc_co_stat_conf(hartid, local_p);
			stat = EXT(stat, CPC_Cx_STAT_CONF_SEQ_STATE);
			if (stat == CPC_Cx_STAT_CONF_SEQ_STATE_U6)
				break;

			/* Delay a little while before we start warning */
			if (timeout) {
				sbi_timer_mdelay(10);
				timeout--;
			}
			else {
				sbi_printf("Waiting for cluster %u core %u hart %u to start... STAT_CONF=0x%x\n",
					   cpu_cluster(hartid),
					   cpu_core(hartid), cpu_hart(hartid),
					   stat);
				break;
			}
		}
	}
	else {
		write_gcr_co_reset_base(hartid,
					(unsigned long)mips_cps_core_entry, local_p);
		write_cpc_co_vp_run(hartid, 1 << cpu_hart(hartid), local_p);
	}

	return 0;
}

static int mips_hart_stop()
{
	u32 hartid = current_hartid();
	bool local_p = (cpu_cluster(current_hartid()) == cpu_cluster(hartid));

	/* Hart 0 is the boot hart, and we don't use the CPC cmd to stop.  */
	if (hartid == 0)
		return SBI_ENOTSUPP;

	write_cpc_co_vp_stop(hartid, 1 << cpu_hart(hartid), local_p);

	return 0;
}

static const struct sbi_hsm_device mips_hsm = {
	.name		= "mips_hsm",
	.hart_start	= mips_hart_start,
	.hart_stop	= mips_hart_stop,
};

static int mips_final_init(bool cold_boot, void *fdt,
			     const struct fdt_match *match)
{
	if (cold_boot)
		sbi_hsm_set_device(&mips_hsm);

	return 0;
}

static int mips_early_init(bool cold_boot, const void *fdt,
			     const struct fdt_match *match)
{
	int rc;

	if (cold_boot)
	{
#if CLUSTERS_IN_PLATFORM > 1
		int i;
		/* Power up other clusters in the platform. */
		for (i = 1; i < CLUSTERS_IN_PLATFORM; i++) {
			power_up_other_cluster(i << NEW_CLUSTER_SHIFT);
		}
#endif

		/* For the CPC mtime region, the minimum size is 0x10000. */
		rc = sbi_domain_root_add_memrange(CM_BASE, SIZE_FOR_CPC_MTIME,
						  P8700_ALIGN,
						  (SBI_DOMAIN_MEMREGION_MMIO |
						   SBI_DOMAIN_MEMREGION_M_READABLE |
						   SBI_DOMAIN_MEMREGION_M_WRITABLE));
		if (rc)
			return rc;

		/* For the APLIC and ACLINT m-mode region */
		rc = sbi_domain_root_add_memrange(AIA_BASE, SIZE_FOR_AIA_M_MODE,
						  P8700_ALIGN,
						  (SBI_DOMAIN_MEMREGION_MMIO |
						   SBI_DOMAIN_MEMREGION_M_READABLE |
						   SBI_DOMAIN_MEMREGION_M_WRITABLE));
		if (rc)
			return rc;

#if CLUSTERS_IN_PLATFORM > 1
		for (i = 0; i < CLUSTERS_IN_PLATFORM; i++) {
			/* For the CPC mtime region, the minimum size is 0x10000. */
			rc = sbi_domain_root_add_memrange(GLOBAL_CM_BASE[i], SIZE_FOR_CPC_MTIME,
							  P8700_ALIGN,
							  (SBI_DOMAIN_MEMREGION_MMIO |
							   SBI_DOMAIN_MEMREGION_M_READABLE |
							   SBI_DOMAIN_MEMREGION_M_WRITABLE));
			if (rc)
				return rc;

			/* For the APLIC and ACLINT m-mode region */
			rc = sbi_domain_root_add_memrange(AIA_BASE - CM_BASE + GLOBAL_CM_BASE[i], SIZE_FOR_AIA_M_MODE,
							  P8700_ALIGN,
							  (SBI_DOMAIN_MEMREGION_MMIO |
							   SBI_DOMAIN_MEMREGION_M_READABLE |
							   SBI_DOMAIN_MEMREGION_M_WRITABLE));
			if (rc)
				return rc;
		}
#endif
	}

	return 0;
}

void mipstvec_handler_stw();
static int mips_nascent_init(const struct fdt_match *match)
{
	u64 hartid = current_hartid();
	u64 cm_base = CM_BASE;
	int i;

	/* Coherence enable for every core */
	if (cpu_hart(hartid) == 0) {
		cm_base += (cpu_core(hartid) << CM_BASE_CORE_SHIFT);
		asm volatile("sd %0,0(%1)"::"r"(GCR_CORE_COH_EN_EN),
			      "r"(cm_base + GCR_OFF_LOCAL + GCR_CORE_COH_EN));
		asm volatile("fence");
	}

	/* Set up pmp for DRAM */
	csr_write(CSR_PMPADDR14, DRAM_PMP_ADDR);
	/* All from 0x0 */
	csr_write(CSR_PMPADDR15, 0x1fffffffffffffff);
	csr_write(CSR_PMPCFG2, ((PMP_A_NAPOT|PMP_R|PMP_W|PMP_X)<<56)|
		  ((PMP_A_NAPOT|PMP_R|PMP_W|PMP_X)<<48));
	/* Set cacheable for pmp6, uncacheable for pmp7 */
	csr_write(CSR_PMACFG2, ((u64)CCA_CACHE_DISABLE << 56)|
		  ((u64)CCA_CACHE_ENABLE << 48));
	/* Reset pmpcfg0 */
	csr_write(CSR_PMPCFG0, 0);
	/* Reset pmacfg0 */
	csr_write(CSR_PMACFG0, 0);
	asm volatile("fence");

	/* Per cluster set up */
	if (cpu_core(hartid) == 0 && cpu_hart(hartid) == 0) {
		/* Enable L2 prefetch */
		asm volatile("sw %0,0(%1)"::"r"(0xfffff110),
			      "r"(cm_base + L2_PFT_CONTROL_OFFSET));
		asm volatile("sw %0,0(%1)"::"r"(0x15ff),
			      "r"(cm_base + L2_PFT_CONTROL_B_OFFSET));
	}

	/* Per core set up */
	if (cpu_hart(hartid) == 0) {
		/* Enable load pair, store pair, and HTW */
		csr_clear(CSR_MIPSCONFIG7, (1<<12)|(1<<13)|(1<<7));

		/* Disable noRFO, misaligned load/store to have misaligned address exceptions */
		csr_set(CSR_MIPSCONFIG7, (1<<25)|(1<<9));

		/* Enable L1-D$ Prefetch */
		csr_write(CSR_MIPSCONFIG11, 0xff);

		for (i = 0; i < 8; i++) {
			csr_set(CSR_MIPSCONFIG8, 4 + 0x100 * i);
			csr_set(CSR_MIPSCONFIG9, 8);
			asm volatile("fence");
			asm volatile("fence.i");
		}
	}

	/* Per hart set up */
	csr_write(CSR_MIPSTVEC, mipstvec_handler_stw + 1);

	return 0;
}

static const struct fdt_match mips_match[] = {
	{ .compatible = "mips,boston" },
	{ },
};

const struct platform_override mips  = {
	.match_table = mips_match,
	.nascent_init = mips_nascent_init,
	.early_init = mips_early_init,
	.final_init = mips_final_init,
};
