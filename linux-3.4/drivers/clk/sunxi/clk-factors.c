/*
 * Copyright (C) 2013 Allwinnertech, kevin.z.m <kevin@allwinnertech.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Adjustable factor-based clock implementation
 */

#include <linux/clk-provider.h>
#include <linux/clk-private.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/string.h>

#include <linux/delay.h>

#include "clk-sunxi.h"
#include "clk-factors.h"


static int sunxi_clk_fators_enable(struct clk_hw *hw)
{
	struct sunxi_clk_factors *factor = to_clk_factor(hw);
    struct sunxi_clk_factors_config *config = factor->config;
    unsigned long reg = factor_readl(factor,factor->reg);
	unsigned int loop = 500;
    if(config->sdmwidth)
    {
        factor_writel(factor,config->sdmval, (void __iomem *)config->sdmpat);
        reg = SET_BITS(config->sdmshift, config->sdmwidth, reg, 1);
    }

	//enable the register
	reg = SET_BITS(config->enshift, 1, reg, 1);

	if(config->updshift) //update for pll_ddr register
		reg = SET_BITS(config->updshift, 1, reg, 1);

	factor_writel(factor,reg, factor->reg);


    while(loop--)
    {
        reg = factor_readl(factor,factor->lock_reg);
        if(GET_BITS(factor->lock_bit, 1, reg))
             break;
        else
            udelay(1);
    }
    if(!loop)
#if (defined CONFIG_FPGA_V4_PLATFORM) || (defined CONFIG_FPGA_V7_PLATFORM)
    printk("clk %s wait lock timeout\n",hw->clk->name);
#else
    WARN(1, "clk %s wait lock timeout\n",hw->clk->name);
#endif
    return 0;
}

static void sunxi_clk_fators_disable(struct clk_hw *hw)
{
	struct sunxi_clk_factors *factor = to_clk_factor(hw);
    struct sunxi_clk_factors_config *config = factor->config;
    unsigned long reg = factor_readl(factor,factor->reg);

    if(config->sdmwidth)
        reg = SET_BITS(config->sdmshift, config->sdmwidth, reg, 0);
	if(config->updshift) //update for pll_ddr register
		reg = SET_BITS(config->updshift, 1, reg, 1);

    reg = SET_BITS(config->enshift, 1, reg, 0);
    factor_writel(factor,reg, factor->reg);
}

static int sunxi_clk_fators_is_enabled(struct clk_hw *hw)
{
    unsigned long val;
	struct sunxi_clk_factors *factor = to_clk_factor(hw);
    struct sunxi_clk_factors_config *config = factor->config;
    unsigned long reg = factor_readl(factor,factor->reg);

    val = GET_BITS(config->enshift, 1, reg);
    return val ? 1 : 0;
}

static unsigned long sunxi_clk_factors_recalc_rate(struct clk_hw *hw, unsigned long parent_rate)
{
    unsigned long reg;
    struct clk_factors_value factor_val;
	struct sunxi_clk_factors *factor = to_clk_factor(hw);
    struct sunxi_clk_factors_config *config = factor->config;

    if(!factor->calc_rate)
        return 0;

    reg = factor_readl(factor,factor->reg);
    if(config->nwidth)
        factor_val.factorn = GET_BITS(config->nshift, config->nwidth, reg);
    else
        factor_val.factorn = 0xffff;

    if(config->kwidth)
        factor_val.factork = GET_BITS(config->kshift, config->kwidth, reg);
    else
        factor_val.factork = 0xffff;

    if(config->mwidth)
        factor_val.factorm = GET_BITS(config->mshift, config->mwidth, reg);
    else
        factor_val.factorm = 0xffff;

    if(config->pwidth)
        factor_val.factorp = GET_BITS(config->pshift, config->pwidth, reg);
    else
        factor_val.factorp = 0xffff;

    if(config->d1width)
        factor_val.factord1 = GET_BITS(config->d1shift, config->d1width, reg);
    else
        factor_val.factord1 = 0xffff;

    if(config->d2width)
        factor_val.factord2 = GET_BITS(config->d2shift, config->d2width, reg);
    else
        factor_val.factord2 = 0xffff;

    if(config->frac) {
        factor_val.frac_mode = GET_BITS(config->modeshift, 1, reg);
        factor_val.frac_freq = GET_BITS(config->outshift, 1, reg);
    } else {
        factor_val.frac_mode = 0xffff;
        factor_val.frac_freq = 0xffff;
    }

    return factor->calc_rate(parent_rate, &factor_val);
}

static long sunxi_clk_factors_round_rate(struct clk_hw *hw, unsigned long rate, unsigned long *prate)
{
    struct clk_factors_value factor_val;
	struct sunxi_clk_factors *factor = to_clk_factor(hw);

    if(!factor->get_factors || !factor->calc_rate)
        return rate;

    factor->get_factors(rate, *prate, &factor_val);
    return factor->calc_rate(*prate, &factor_val);
}

static int sunxi_clk_factors_set_rate(struct clk_hw *hw, unsigned long rate, unsigned long parent_rate)
{
    unsigned long reg;
    struct clk_factors_value factor_val;
    unsigned int loop = 500;
	struct sunxi_clk_factors *factor = to_clk_factor(hw);
    struct sunxi_clk_factors_config *config = factor->config;

    if(!factor->get_factors)
        return 0;

    factor->get_factors(rate, parent_rate, &factor_val);

    reg = factor_readl(factor,factor->reg);

    if(config->sdmwidth)
    {
        factor_writel(factor,config->sdmval, (void __iomem *)config->sdmpat);
        reg = SET_BITS(config->sdmshift, config->sdmwidth, reg, 1);
    }

    if(config->nwidth)
        reg = SET_BITS(config->nshift, config->nwidth, reg, factor_val.factorn);

    if(config->kwidth)
        reg = SET_BITS(config->kshift, config->kwidth, reg, factor_val.factork);

    if(config->mwidth)
        reg = SET_BITS(config->mshift, config->mwidth, reg, factor_val.factorm);

    if(config->pwidth)
        reg = SET_BITS(config->pshift, config->pwidth, reg, factor_val.factorp);

    if(config->d1width)
        reg = SET_BITS(config->d1shift, config->d1width, reg, factor_val.factord1);

    if(config->d2width)
        reg = SET_BITS(config->d2shift, config->d2width, reg, factor_val.factord2);

    if(config->frac) {
        reg = SET_BITS(config->modeshift, 1, reg, factor_val.frac_mode);
        reg = SET_BITS(config->outshift, 1, reg, factor_val.frac_freq);
    }

	if(config->updshift) //update for pll_ddr register
		reg = SET_BITS(config->updshift, 1, reg, 1);

    factor_writel(factor,reg, factor->reg);
#ifndef CONFIG_SUNXI_CLK_DUMMY_DEBUG
    if(GET_BITS(config->enshift, 1, reg))
    {
        while(loop--)
        {
            reg = factor_readl(factor,factor->lock_reg);
            if(GET_BITS(factor->lock_bit, 1, reg))
                break;
            else
               udelay(1);
        }
        if(!loop)
#if (defined CONFIG_FPGA_V4_PLATFORM) || (defined CONFIG_FPGA_V7_PLATFORM)
        printk("clk %s wait lock timeout\n",hw->clk->name);
#else
        WARN(1, "clk %s wait lock timeout\n",hw->clk->name);
#endif
    }
#endif
    return 0;
}


static const struct clk_ops clk_factors_ops = {
    .enable = sunxi_clk_fators_enable,
    .disable = sunxi_clk_fators_disable,
    .is_enabled = sunxi_clk_fators_is_enabled,

    .recalc_rate = sunxi_clk_factors_recalc_rate,
    .round_rate = sunxi_clk_factors_round_rate,
    .set_rate = sunxi_clk_factors_set_rate,
};
void sunxi_clk_get_factors_ops(struct clk_ops* ops)
{
    memcpy(ops,&clk_factors_ops,sizeof(clk_factors_ops));
}
/**
 * clk_register_factors - register a factors clock with
 * the clock framework
 * @dev: device registering this clock
 * @name: name of this clock
 * @parent_name: name of clock's parent
 * @flags: framework-specific flags
 * @reg: register address to adjust factors
 * @config: shift and width of factors n, k, m, p, div1 and div2
 * @get_factors: function to calculate the factors for a given frequency
 * @lock: shared register lock for this clock
 */
struct clk *sunxi_clk_register_factors(struct device *dev,void __iomem *base,spinlock_t *lock,struct factor_init_data* init_data)
{
    struct sunxi_clk_factors *factors;
    struct clk *clk;
    struct clk_init_data init;

    /* allocate the factors */
    factors = kzalloc(sizeof(struct sunxi_clk_factors), GFP_KERNEL);
    if (!factors) {
        pr_err("%s: could not allocate factors clk\n", __func__);
        return ERR_PTR(-ENOMEM);
    }

#ifdef __SUNXI_ALL_CLK_IGNORE_UNUSED__
		init_data->flags |= CLK_IGNORE_UNUSED;
#endif
    init.name = init_data->name;
    init.ops = init_data->priv_ops?(init_data->priv_ops):(&clk_factors_ops);
    factors->priv_regops = init_data->priv_regops?(init_data->priv_regops):NULL;
    init.flags =init_data->flags;
    init.parent_names = init_data->parent_names;
    init.num_parents = init_data->num_parents;

    /* struct clk_factors assignments */
    factors->reg = base + init_data->reg;
    factors->lock_reg = base + init_data->lock_reg;   
    factors->lock_bit = init_data->lock_bit; 
    factors->config = init_data->config;
    factors->config->sdmpat = (unsigned int __force)(base + factors->config->sdmpat);
    factors->lock = lock;
    factors->hw.init = &init;
    factors->get_factors = init_data->get_factors;
    factors->calc_rate = init_data->calc_rate;

    /* register the clock */
    clk = clk_register(dev, &factors->hw);

    if (IS_ERR(clk))
        kfree(factors);

    return clk;
}
int sunxi_clk_get_common_factors(struct sunxi_clk_factors_config* f_config,struct clk_factors_value *factor, struct sunxi_clk_factor_freq table[],unsigned long index,unsigned long tbl_size)
{
    if(index >=tbl_size/sizeof(struct sunxi_clk_factor_freq))
      return -1;
    factor->factorn = (table[index].factor >>f_config->nshift)&((1<<(f_config->nwidth))-1);
    factor->factork = (table[index].factor >>f_config->kshift)&((1<<(f_config->kwidth))-1);
    factor->factorm = (table[index].factor >>f_config->mshift)&((1<<(f_config->mwidth))-1);
    factor->factorp = (table[index].factor >>f_config->pshift)&((1<<(f_config->pwidth))-1);   
    factor->factord1 = (table[index].factor >>f_config->d1shift)&((1<<(f_config->d1width))-1); 
    factor->factord2 = (table[index].factor >>f_config->d2shift)&((1<<(f_config->d2width))-1);
    if(f_config->frac)
    {
      factor->frac_mode = (table[index].factor >>f_config->modeshift)&1;
      factor->frac_freq = (table[index].factor >>f_config->outshift)&1;
    }
    return 0;
}

static int sunxi_clk_freq_search(struct sunxi_clk_factor_freq tbl[],unsigned long freq,int low,int high)
{
  int mid;
  unsigned long checkfreq;
  if(low > high)
    return (high==-1)? 0: high;

  mid = (low + high)/2;
  checkfreq = tbl[mid].freq/1000000;
  if( checkfreq == freq)
    return mid;
  else if(checkfreq > freq)
    return sunxi_clk_freq_search(tbl,freq,low,mid -1);
  else
    return sunxi_clk_freq_search(tbl,freq,mid + 1,high);
}
static int sunxi_clk_freq_find(struct sunxi_clk_factor_freq tbl[],unsigned long n,unsigned long freq)
{
  int delta1,delta2;
  int i = sunxi_clk_freq_search(tbl,freq,0,n-1);
  if(i != n-1)
  {
    delta1 = (freq > tbl[i].freq/1000000)?(freq -tbl[i].freq/1000000):(tbl[i].freq/1000000-freq);
    delta2 = (freq > tbl[i+1].freq/1000000)?(freq -tbl[i+1].freq/1000000):(tbl[i+1].freq/1000000-freq);
    if(delta2 < delta1)
        i++;
  }
  return i;
}
int sunxi_clk_get_common_factors_search(struct sunxi_clk_factors_config* f_config,struct clk_factors_value *factor, struct sunxi_clk_factor_freq table[],unsigned long index,unsigned long tbl_count)
{
    int i=sunxi_clk_freq_find(table,tbl_count,index);   
    if(i >=tbl_count)
      return -1;
    factor->factorn = (table[i].factor >>f_config->nshift)&((1<<(f_config->nwidth))-1);
    factor->factork = (table[i].factor >>f_config->kshift)&((1<<(f_config->kwidth))-1);
    factor->factorm = (table[i].factor >>f_config->mshift)&((1<<(f_config->mwidth))-1);
    factor->factorp = (table[i].factor >>f_config->pshift)&((1<<(f_config->pwidth))-1);   
    factor->factord1 = (table[i].factor >>f_config->d1shift)&((1<<(f_config->d1width))-1); 
    factor->factord2 = (table[i].factor >>f_config->d2shift)&((1<<(f_config->d2width))-1);
    if(f_config->frac)
    {
      factor->frac_mode = (table[i].factor >>f_config->modeshift)&1;
      factor->frac_freq = (table[i].factor >>f_config->outshift)&1;
    }
    return 0;
}