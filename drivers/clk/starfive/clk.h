// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2022 Starfive, Inc.
 * Author:	Yanhong Wang <yanhong.wang@starfivetech.com>
 *
 */

#ifndef __CLK_STARFIVE_H
#define __CLK_STARFIVE_H

enum starfive_pll_type {
	PLL0 = 0,
	PLL1,
	PLL2,
	PLL_MAX = PLL2
};

struct starfive_pllx_rate {
	u64 rate;
	u32 prediv;
	u32 fbdiv;
	u32 frac;
	u32 postdiv1;
	u32 dacpd;
	u32 dsmpd;
};

struct starfive_pllx_clk {
	enum starfive_pll_type type;
	const struct starfive_pllx_rate *rate_table;
	int rate_count;
	int flags;
};

extern struct starfive_pllx_clk starfive_jh7110_pll0;
extern struct starfive_pllx_clk starfive_jh7110_pll1;
extern struct starfive_pllx_clk starfive_jh7110_pll2;

struct clk *starfive_jh7110_pll(const char *name, const char *parent_name,
			    void __iomem *base, void __iomem *sysreg,
			    const struct starfive_pllx_clk *pll_clk);
#endif
