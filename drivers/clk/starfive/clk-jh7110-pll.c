// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2022 StarFive Technology Co., Ltd.
 *
 * Author:	Yanhong Wang <yanhong.wang@starfivetech.com>
 */

#include <common.h>
#include <asm/io.h>
#include <malloc.h>
#include <clk-uclass.h>
#include <div64.h>
#include <dm/device.h>
#include <linux/bitops.h>
#include <linux/clk-provider.h>
#include <linux/delay.h>
#include <linux/err.h>

#include "clk.h"

#define UBOOT_DM_CLK_JH7110_PLLx "jh7110_clk_pllx"

#define PLL0_DACPD_MASK	BIT(24)
#define PLL0_DSMPD_MASK	BIT(25)
#define PLL0_FBDIV_MASK	GENMASK(11, 0)
#define PLL0_FRAC_MASK		GENMASK(23, 0)
#define PLL0_PD_MASK		BIT(27)
#define PLL0_POSTDIV1_MASK	GENMASK(29, 28)
#define PLL0_PREDIV_MASK	GENMASK(5, 0)
#define PLL1_DACPD_MASK	BIT(15)
#define PLL1_DSMPD_MASK	BIT(16)
#define PLL1_FBDIV_MASK	GENMASK(28, 17)
#define PLL1_FRAC_MASK		GENMASK(23, 0)
#define PLL1_PD_MASK		BIT(27)
#define PLL1_POSTDIV1_MASK	GENMASK(29, 28)
#define PLL1_PREDIV_MASK	GENMASK(5, 0)
#define PLL2_DACPD_MASK	BIT(15)
#define PLL2_DSMPD_MASK	BIT(16)
#define PLL2_FBDIV_MASK	GENMASK(28, 17)
#define PLL2_FRAC_MASK		GENMASK(23, 0)
#define PLL2_PD_MASK		BIT(27)
#define PLL2_POSTDIV1_MASK	GENMASK(29, 28)
#define PLL2_PREDIV_MASK	GENMASK(5, 0)

#define PLL0_DACPD_OFFSET	0x18
#define PLL0_DSMPD_OFFSET	0x18
#define PLL0_FBDIV_OFFSET	0x1C
#define PLL0_FRAC_OFFSET	0x20
#define PLL0_PD_OFFSET		0x20
#define PLL0_POSTDIV1_OFFSET	0x20
#define PLL0_PREDIV_OFFSET	0x24
#define PLL1_DACPD_OFFSET	0x24
#define PLL1_DSMPD_OFFSET	0x24
#define PLL1_FBDIV_OFFSET	0x24
#define PLL1_FRAC_OFFSET	0x28
#define PLL1_PD_OFFSET		0x28
#define PLL1_POSTDIV1_OFFSET	0x28
#define PLL1_PREDIV_OFFSET	0x2c
#define PLL2_DACPD_OFFSET	0x2c
#define PLL2_DSMPD_OFFSET	0x2c
#define PLL2_FBDIV_OFFSET	0x2c
#define PLL2_FRAC_OFFSET	0x30
#define PLL2_PD_OFFSET		0x30
#define PLL2_POSTDIV1_OFFSET	0x30
#define PLL2_PREDIV_OFFSET	0x34

#define PLL_PD_OFF		1
#define PLL_PD_ON		0

#define CLK_DDR_BUS_MASK	GENMASK(29, 24)
#define CLK_DDR_BUS_OFFSET	0xAC
#define CLK_DDR_BUS_OSC_DIV2	0
#define CLK_DDR_BUS_PLL1_DIV2	1
#define CLK_DDR_BUS_PLL1_DIV4	2
#define CLK_DDR_BUS_PLL1_DIV8	3

struct clk_jh7110_pllx {
	struct clk		clk;
	void __iomem	*base;
	void __iomem	*sysreg;
	enum starfive_pll_type	type;
	const struct starfive_pllx_rate *rate_table;
	int rate_count;
};

#define getbits_le32(addr, mask) ((in_le32(addr)&(mask)) >> __ffs(mask))

#define GET_PLL(index, type) \
		getbits_le32((ulong)pll->base + index##_##type##_OFFSET, index##_##type##_MASK)

#define SET_PLL(index, type, val) \
		clrsetbits_le32((ulong)pll->base + index##_##type##_OFFSET, \
			index##_##type##_MASK, \
			((val) << __ffs(index##_##type##_MASK)) & index##_##type##_MASK)

#define CLK_DDR_REG_SET(type, val) \
		clrsetbits_le32((ulong)pll->sysreg + CLK_DDR_##type##_OFFSET, \
			CLK_DDR_##type##_MASK, \
			((val) << __ffs(CLK_DDR_##type##_MASK)) & CLK_DDR_##type##_MASK)

#define PLLX_RATE(_rate, _pd, _fd, _pd1, _da, _ds)	\
	{						\
		.rate		= (_rate),	\
		.prediv		= (_pd),	\
		.fbdiv		= (_fd),	\
		.postdiv1	= (_pd1),	\
		.dacpd		= (_da),	\
		.dsmpd		= (_ds),	\
	}

#define to_clk_pllx(_clk) container_of(_clk, struct clk_jh7110_pllx, clk)

static const struct starfive_pllx_rate jh7110_pll0_tbl[] = {
	PLLX_RATE(375000000UL, 8, 125, 1, 1, 1),
	PLLX_RATE(500000000UL, 6, 125, 1, 1, 1),
	PLLX_RATE(625000000UL, 24, 625, 1, 1, 1),
	PLLX_RATE(750000000UL, 4, 125, 1, 1, 1),
	PLLX_RATE(875000000UL, 24, 875, 1, 1, 1),
	PLLX_RATE(1000000000UL, 3, 125, 1, 1, 1),
	PLLX_RATE(1250000000UL, 12, 625, 1, 1, 1),
	PLLX_RATE(1375000000UL, 24, 1375, 1, 1, 1),
	PLLX_RATE(1500000000UL, 2, 125, 1, 1, 1),
	PLLX_RATE(1625000000UL, 24, 1625, 1, 1, 1),
	PLLX_RATE(1750000000UL, 12, 875, 1, 1, 1),
	PLLX_RATE(1800000000UL, 3, 225, 1, 1, 1),
};

static const struct starfive_pllx_rate jh7110_pll1_tbl[] = {
	PLLX_RATE(1066000000UL, 12, 533, 1, 1, 1),
	PLLX_RATE(1200000000UL, 1, 50, 1, 1, 1),
	PLLX_RATE(1400000000UL, 6, 350, 1, 1, 1),
	PLLX_RATE(1600000000UL, 3, 200, 1, 1, 1),
};

static const struct starfive_pllx_rate jh7110_pll2_tbl[] = {
	PLLX_RATE(1228800000UL, 15, 768, 1, 1, 1),
	PLLX_RATE(1188000000UL, 2, 99, 1, 1, 1),
};

struct starfive_pllx_clk starfive_jh7110_pll0 __initdata = {
	.type = PLL0,
	.rate_table = jh7110_pll0_tbl,
	.rate_count = ARRAY_SIZE(jh7110_pll0_tbl),
};
EXPORT_SYMBOL_GPL(starfive_jh7110_pll0);

struct starfive_pllx_clk starfive_jh7110_pll1 __initdata = {
	.type = PLL1,
	.rate_table = jh7110_pll1_tbl,
	.rate_count = ARRAY_SIZE(jh7110_pll1_tbl),
};
EXPORT_SYMBOL_GPL(starfive_jh7110_pll1);

struct starfive_pllx_clk starfive_jh7110_pll2 __initdata = {
	.type = PLL2,
	.rate_table = jh7110_pll2_tbl,
	.rate_count = ARRAY_SIZE(jh7110_pll2_tbl),
};
EXPORT_SYMBOL_GPL(starfive_jh7110_pll2);

static const struct starfive_pllx_rate *jh7110_get_pll_settings(
		struct clk_jh7110_pllx *pll, unsigned long rate)
{
	const struct starfive_pllx_rate *rate_table = pll->rate_table;
	int i;

	for (i = 0; i < pll->rate_count; i++)
		if (rate == rate_table[i].rate)
			return &rate_table[i];

	return NULL;
}

static void jh7110_pll_set_rate(struct clk_jh7110_pllx *pll,
							const struct starfive_pllx_rate *rate)
{
	switch (pll->type) {
	case PLL0:
		SET_PLL(PLL0, PD, PLL_PD_OFF);
		SET_PLL(PLL0, DACPD, rate->dacpd);
		SET_PLL(PLL0, DSMPD, rate->dsmpd);
		SET_PLL(PLL0, PREDIV, rate->prediv);
		SET_PLL(PLL0, FBDIV, rate->fbdiv);
		SET_PLL(PLL0, POSTDIV1, rate->postdiv1 >> 1);
		SET_PLL(PLL0, PD, PLL_PD_ON);
		break;

	case PLL1:
		CLK_DDR_REG_SET(BUS, CLK_DDR_BUS_OSC_DIV2);
		SET_PLL(PLL1, PD, PLL_PD_OFF);
		SET_PLL(PLL1, DACPD, rate->dacpd);
		SET_PLL(PLL1, DSMPD, rate->dsmpd);
		SET_PLL(PLL1, PREDIV, rate->prediv);
		SET_PLL(PLL1, FBDIV, rate->fbdiv);
		SET_PLL(PLL1, POSTDIV1, rate->postdiv1 >> 1);
		SET_PLL(PLL1, PD, PLL_PD_ON);
		udelay(100);
		CLK_DDR_REG_SET(BUS, CLK_DDR_BUS_PLL1_DIV2);
		break;

	case PLL2:
		SET_PLL(PLL2, PD, PLL_PD_OFF);
		SET_PLL(PLL2, DACPD, rate->dacpd);
		SET_PLL(PLL2, DSMPD, rate->dsmpd);
		SET_PLL(PLL2, PREDIV, rate->prediv);
		SET_PLL(PLL2, FBDIV, rate->fbdiv);
		SET_PLL(PLL2, POSTDIV1, rate->postdiv1 >> 1);
		SET_PLL(PLL2, PD, PLL_PD_ON);
		break;

	default:
		break;
	}
}

static ulong jh7110_pllx_recalc_rate(struct clk *clk)
{
	struct clk_jh7110_pllx *pll = to_clk_pllx(dev_get_clk_ptr(clk->dev));
	u64 refclk = clk_get_parent_rate(clk);
	u32 dacpd, dsmpd;
	u32 prediv, fbdiv, postdiv1;
	u64 frac;

	switch (pll->type) {
	case PLL0:
		dacpd = GET_PLL(PLL0, DACPD);
		dsmpd = GET_PLL(PLL0, DSMPD);
		prediv = GET_PLL(PLL0, PREDIV);
		fbdiv = GET_PLL(PLL0, FBDIV);
		postdiv1 = 1 << GET_PLL(PLL0, POSTDIV1);
		frac = (u64)GET_PLL(PLL0, FRAC);
		break;

	case PLL1:
		dacpd = GET_PLL(PLL1, DACPD);
		dsmpd = GET_PLL(PLL1, DSMPD);
		prediv = GET_PLL(PLL1, PREDIV);
		fbdiv = GET_PLL(PLL1, FBDIV);
		postdiv1 = 1 << GET_PLL(PLL1, POSTDIV1);
		frac = (u64)GET_PLL(PLL1, FRAC);
		break;

	case PLL2:
		dacpd = GET_PLL(PLL2, DACPD);
		dsmpd = GET_PLL(PLL2, DSMPD);
		prediv = GET_PLL(PLL2, PREDIV);
		fbdiv = GET_PLL(PLL2, FBDIV);
		postdiv1 = 1 << GET_PLL(PLL2, POSTDIV1);
		frac = (u64)GET_PLL(PLL2, FRAC);
		break;

	default:
		debug("Unsupported pll type %d.\n", pll->type);
		return 0;
	}

	/* Integer Mode or Fraction Mode */
	if ((dacpd == 1) && (dsmpd == 1))
		frac = 0;
	else if ((dacpd == 0 && dsmpd == 0))
		do_div(frac, 1 << 24);
	else
		return -EINVAL;

	refclk *= (fbdiv + frac);
	do_div(refclk, prediv * postdiv1);

	return refclk;
}

static ulong jh7110_pllx_set_rate(struct clk *clk, ulong drate)
{
	struct clk_jh7110_pllx *pll = to_clk_pllx(dev_get_clk_ptr(clk->dev));
	const struct starfive_pllx_rate *rate;

	switch (pll->type) {
	case PLL0:
	case PLL1:
	case PLL2:
		break;

	default:
		debug("%s: Unknown pll clk type\n", __func__);
		return -EINVAL;
	};

	rate = jh7110_get_pll_settings(pll, drate);
	if (!rate)
		return -EINVAL;

	jh7110_pll_set_rate(pll, rate);

	return jh7110_pllx_recalc_rate(clk);
}

#if IS_ENABLED(CONFIG_SPL_BUILD)
static void jh7110_pllx_init_rate(struct clk_jh7110_pllx *pll, ulong drate)
{
	const struct starfive_pllx_rate *rate;

	rate = jh7110_get_pll_settings(pll, drate);
	if (!rate)
		return;

	jh7110_pll_set_rate(pll, rate);
}
#endif

static const struct clk_ops clk_jh7110_ops = {
	.set_rate	= jh7110_pllx_set_rate,
	.get_rate	= jh7110_pllx_recalc_rate,
};

struct clk *starfive_jh7110_pll(const char *name, const char *parent_name,
			    void __iomem *base, void __iomem *sysreg,
			    const struct starfive_pllx_clk *pll_clk)
{
	struct clk_jh7110_pllx *pll;
	struct clk *clk;
	char *type_name;
	int ret;

	switch (pll_clk->type) {
	case PLL0:
	case PLL1:
	case PLL2:
		type_name = UBOOT_DM_CLK_JH7110_PLLx;
		break;

	default:
		return ERR_PTR(-EINVAL);
	};

	pll = kzalloc(sizeof(*pll), GFP_KERNEL);
	if (!pll)
		return ERR_PTR(-ENOMEM);

	pll->base = base;
	pll->sysreg = sysreg;
	pll->type = pll_clk->type;
	pll->rate_table = pll_clk->rate_table;
	pll->rate_count = pll_clk->rate_count;

	clk = &pll->clk;
	ret = clk_register(clk, type_name, name, parent_name);
	if (ret) {
		kfree(pll);
		return ERR_PTR(ret);
	}

#if IS_ENABLED(CONFIG_SPL_BUILD)
	if (pll->type == PLL0)
		jh7110_pllx_init_rate(pll, 1250000000);

	if (pll->type == PLL2)
		jh7110_pllx_init_rate(pll, 1188000000);
#endif

	return clk;
}

U_BOOT_DRIVER(jh7110_clk_pllx) = {
	.name	= UBOOT_DM_CLK_JH7110_PLLx,
	.id	= UCLASS_CLK,
	.ops	= &clk_jh7110_ops,
};
