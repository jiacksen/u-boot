// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2022 StarFive Technology Co., Ltd.
 * Author:	Yanhong Wang<yanhong.wang@starfivetech.com>
 *
 */

#include <asm/csr.h>
#include <dm.h>
#include <log.h>

#define CSR_U74_FEATURE_DISABLE	0x7c1

int spl_soc_init(void)
{
	int ret;
	struct udevice *dev;

	/* DDR init */
	ret = uclass_get_device(UCLASS_RAM, 0, &dev);
	if (ret) {
		debug("DRAM init failed: %d\n", ret);
		return ret;
	}

	/* flash init */
	ret = uclass_get_device(UCLASS_SPI_FLASH, 0, &dev);
	if (ret) {
		debug("SPI init failed: %d\n", ret);
		return ret;
	}

	return 0;
}

void harts_early_init(void)
{
	/*
	 * Feature Disable CSR
	 *
	 * Clear feature disable CSR to '0' to turn on all features for
	 * each core. This operation must be in M-mode.
	 */
	if (CONFIG_IS_ENABLED(RISCV_MMODE))
		csr_write(CSR_U74_FEATURE_DISABLE, 0);

	/* clear L2 LIM  memory
	 * set __bss_end to 0x81FFFFF region to zero
	 */
	asm volatile (
		"la t1, __bss_end\n\t"
		"li t2, 0x81FFFFF\n\t"
		"spl_clear_l2im:\n\t"
		"sd zero, 0(t1)\n\t"
		"addi t1, t1, 8\n\t"
		"blt t1, t2, spl_clear_l2im\n\t");
}
