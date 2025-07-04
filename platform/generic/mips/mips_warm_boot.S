/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 MIPS
 *
 */
       .text
       .align  12
       .globl  mips_warm_boot
mips_warm_boot:
       j       _start_warm
       .align  2
nmi_vector:
       j       _start_warm
       .align  2
cacheerr_vector:
       j       _start_warm
       .align  2
debugexc_vector:
       j       _start_warm
