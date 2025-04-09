/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 MIPS
 *
 */

#include <sbi/riscv_encoding.h>
#include <mips/p8700.h>

	.text
	.align	12
	.globl	mips_cps_core_entry
mips_cps_core_entry:
	j	1f

	.align	2
nmi_vector:
	j	1f

	.align	2
cacheerr_vector:
	j	1f

	.align	2
debugexc_vector:
	j	1f

	.align	2
1:
	li	x1, 0
	li	x2, 0
	li	x3, 0
	li	x4, 0
	li	x5, 0
	li	x6, 0
	li	x7, 0
	li	x8, 0
	li	x9, 0
	li	x10, 0
#ifdef FW_FDT_PATH
	li	x11, 0
#endif
	li	x12, 0
	li	x13, 0
	li	x14, 0
	li	x15, 0
	li	x16, 0
	li	x17, 0
	li	x18, 0
	li	x19, 0
	li	x20, 0
	li	x21, 0
	li	x22, 0
	li	x23, 0
	li	x24, 0
	li	x25, 0
	li	x26, 0
	li	x27, 0
	li	x28, 0
	li	x29, 0
	li	x30, 0
	li	x31, 0

	/* a0 has mhartid */
	csrr	a0, CSR_MHARTID

	/* Let hart 0 jump to _start */
	beqz	a0, 1f
	lla	t0, _start_warm
	jr	t0
1:
	lla	t0, _start
	jr	t0

