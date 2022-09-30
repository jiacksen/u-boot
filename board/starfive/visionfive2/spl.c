// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2022 StarFive Technology Co., Ltd.
 * Author:	Yanhong Wang<yanhong.wang@starfivetech.com>
 *
 */

#include <common.h>
#include <asm/arch/jh7110-regs.h>
#include <asm/arch/spl.h>
#include <asm/io.h>
#include <log.h>
#include <spl.h>

#define JH7110_CLK_CPU_ROOT_OFFSET		0x0U
#define JH7110_CLK_CPU_ROOT_SHIFT		24
#define JH7110_CLK_CPU_ROOT_MASK		GENMASK(29, 24)
#define JH7110_CLK_BUS_ROOT_OFFSET		0x14U
#define JH7110_CLK_BUS_ROOT_SHIFT		24
#define JH7110_CLK_BUS_ROOT_MASK		GENMASK(29, 24)
#define JH7110_CLK_PERH_ROOT_OFFSET		0x10U
#define JH7110_CLK_PERH_ROOT_SHIFT		24
#define JH7110_CLK_PERH_ROOT_MASK		GENMASK(29, 24)
#define JH7110_CLK_NOC_BUS_STG_AXI_OFFSET	0x180U
#define JH7110_CLK_NOC_BUS_STG_AXI_SHIFT	31
#define JH7110_CLK_NOC_BUS_STG_AXI_MASK	BIT(31)
#define JH7110_CLK_AON_APB_FUNC_OFFSET		0x4U
#define JH7110_CLK_AON_APB_FUNC_SHIFT		24
#define JH7110_CLK_AON_APB_FUNC_MASK		GENMASK(29, 24)
#define JH7110_CLK_QSPI_REF_OFFSET		0x168U
#define JH7110_CLK_QSPI_REF_SHIFT		24
#define JH7110_CLK_QSPI_REF_MASK		GENMASK(29, 24)


#define SET_DIV(type, val) \
	clrsetbits_le32(JH7110_SYS_CRG + JH7110_CLK_##type##_OFFSET, \
		JH7110_CLK_##type##_MASK, \
		((val) << JH7110_CLK_##type##_SHIFT) & JH7110_CLK_##type##_MASK)

int spl_board_init_f(void)
{
	int ret;

	ret = spl_soc_init();
	if (ret) {
		debug("JH7110 SPL init failed: %d\n", ret);
		return ret;
	}

	return 0;
}

u32 spl_boot_device(void)
{
	u32 mode;

	mode = in_le32(JH7110_BOOT_MODE_SELECT_REG)
				& JH7110_BOOT_MODE_SELECT_MASK;
	switch (mode) {
	case 0:
		return BOOT_DEVICE_SPI;

	case 1:
		return BOOT_DEVICE_MMC2;

	case 2:
		return BOOT_DEVICE_MMC1;

	case 3:
		return BOOT_DEVICE_UART;

	default:
		debug("Unsupported boot device 0x%x.\n", mode);
		return BOOT_DEVICE_NONE;
	}
}

void board_init_f(ulong dummy)
{
	int ret;

	ret = spl_early_init();
	if (ret)
		panic("spl_early_init() failed: %d\n", ret);

	riscv_cpu_setup(NULL, NULL);
	preloader_console_init();

	/* select clk_pll0 by default */
	SET_DIV(CPU_ROOT, 1);

	/* select clk_pll2 by default */
	SET_DIV(BUS_ROOT, 1);

	/* select clk_pll2 by default */
	SET_DIV(PERH_ROOT, 1);

	SET_DIV(NOC_BUS_STG_AXI, 1);

	clrsetbits_le32(JH7110_AON_CRG + JH7110_CLK_AON_APB_FUNC_OFFSET,
		JH7110_CLK_AON_APB_FUNC_MASK,
		BIT(JH7110_CLK_AON_APB_FUNC_SHIFT));

	/* select clk_pll0 by default */
	SET_DIV(QSPI_REF, 1);

	ret = spl_board_init_f();
	if (ret) {
		debug("spl_board_init_f init failed: %d\n", ret);
		return;
	}
}

#ifdef CONFIG_SPL_LOAD_FIT
int board_fit_config_name_match(const char *name)
{
	/* boot using first FIT config */
	return 0;
}
#endif
