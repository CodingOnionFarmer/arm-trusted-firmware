/*
 * Copyright 2024 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <common/debug.h>
#include <drivers/arm/gicv3.h>
#include <lib/xlat_tables/xlat_tables_v2.h>
#include <plat/common/platform.h>
#include <plat_console.h>

#include <s32cc-bl-common.h>

static entry_point_info_t bl33_image_ep_info;

static unsigned int s32g2_mpidr_to_core_pos(unsigned long mpidr);

static uint32_t get_spsr_for_bl33_entry(void)
{
	unsigned long mode = MODE_EL1;
	uint32_t spsr;

	spsr = SPSR_64(mode, MODE_SP_ELX, DISABLE_ALL_EXCEPTIONS);

	return spsr;
}

void bl31_early_platform_setup2(u_register_t arg0, u_register_t arg1,
				u_register_t arg2, u_register_t arg3)
{
	SET_PARAM_HEAD(&bl33_image_ep_info, PARAM_EP, VERSION_1, 0);
	bl33_image_ep_info.pc = BL33_BASE;
	bl33_image_ep_info.spsr = get_spsr_for_bl33_entry();
	SET_SECURITY_STATE(bl33_image_ep_info.h.attr, NON_SECURE);
}

void bl31_plat_arch_setup(void)
{
	int ret;

	ret = s32cc_bl_mmu_setup();
	if (ret != 0) {
		panic();
	}

	console_s32g2_register();
}

struct entry_point_info *bl31_plat_get_next_image_ep_info(uint32_t type)
{
	return &bl33_image_ep_info;
}

static int mmap_gic(const gicv3_driver_data_t *gic_data)
{
	size_t gicr_size;
	int ret;

	ret = mmap_add_dynamic_region(gic_data->gicd_base,
				      gic_data->gicd_base,
				      PAGE_SIZE_64KB,
				      MT_DEVICE | MT_RW | MT_SECURE);
	if (ret != 0) {
		return ret;
	}

	gicr_size = gicv3_redist_size(0x0U);
	ret = mmap_add_dynamic_region(gic_data->gicr_base,
				      gic_data->gicr_base,
				      gicr_size * gic_data->rdistif_num,
				      MT_DEVICE | MT_RW | MT_SECURE);
	if (ret != 0) {
		return ret;
	}

	return 0;
}

void bl31_platform_setup(void)
{
	static uintptr_t rdistif_base_addrs[PLATFORM_CORE_COUNT];
	static gicv3_driver_data_t plat_gic_data = {
		.gicd_base = PLAT_GICD_BASE,
		.gicr_base = PLAT_GICR_BASE,
		.rdistif_num = PLATFORM_CORE_COUNT,
		.rdistif_base_addrs = rdistif_base_addrs,
		.mpidr_to_core_pos = s32g2_mpidr_to_core_pos,
	};
	unsigned int pos = plat_my_core_pos();
	int ret;

	ret = mmap_gic(&plat_gic_data);
	if (ret != 0) {
		panic();
	}

	gicv3_driver_init(&plat_gic_data);
	gicv3_distif_init();
	gicv3_rdistif_init(pos);
	gicv3_cpuif_enable(pos);
}

static unsigned int s32g2_mpidr_to_core_pos(unsigned long mpidr)
{
	int core;

	core = plat_core_pos_by_mpidr(mpidr);
	if (core < 0) {
		return 0;
	}

	return (unsigned int)core;
}

