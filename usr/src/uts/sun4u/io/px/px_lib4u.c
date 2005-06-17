/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#pragma ident	"%Z%%M%	%I%	%E% SMI"

#include <sys/types.h>
#include <sys/kmem.h>
#include <sys/conf.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/fm/protocol.h>
#include <sys/fm/util.h>
#include <sys/modctl.h>
#include <sys/disp.h>
#include <sys/stat.h>
#include <sys/ddi_impldefs.h>
#include <sys/vmem.h>
#include <sys/iommutsb.h>
#include <sys/cpuvar.h>
#include <sys/ivintr.h>
#include <px_obj.h>
#include <pcie_pwr.h>
#include "px_tools_var.h"
#include <px_regs.h>
#include <px_csr.h>
#include <sys/machsystm.h>
#include "px_lib4u.h"
#include "px_err.h"

#pragma weak jbus_stst_order

extern void jbus_stst_order();

ulong_t px_mmu_dvma_end = 0xfffffffful;
uint_t px_ranges_phi_mask = 0xfffffffful;

static int px_goto_l23ready(px_t *px_p);
static uint32_t px_identity_chip(px_t *px_p);
static void px_lib_clr_errs(px_t *px_p, px_pec_t *pec_p);

/*
 * px_lib_map_registers
 *
 * This function is called from the attach routine to map the registers
 * accessed by this driver.
 *
 * used by: px_attach()
 *
 * return value: DDI_FAILURE on failure
 */
int
px_lib_map_regs(pxu_t *pxu_p, dev_info_t *dip)
{
	ddi_device_acc_attr_t	attr;
	px_reg_bank_t		reg_bank = PX_REG_CSR;

	DBG(DBG_ATTACH, dip, "px_lib_map_regs: pxu_p:0x%p, dip 0x%p\n",
		pxu_p, dip);

	attr.devacc_attr_version = DDI_DEVICE_ATTR_V0;
	attr.devacc_attr_dataorder = DDI_STRICTORDER_ACC;
	attr.devacc_attr_endian_flags = DDI_NEVERSWAP_ACC;

	/*
	 * PCI CSR Base
	 */
	if (ddi_regs_map_setup(dip, reg_bank, &pxu_p->px_address[reg_bank],
	    0, 0, &attr, &pxu_p->px_ac[reg_bank]) != DDI_SUCCESS) {
		goto fail;
	}

	reg_bank++;

	/*
	 * XBUS CSR Base
	 */
	if (ddi_regs_map_setup(dip, reg_bank, &pxu_p->px_address[reg_bank],
	    0, 0, &attr, &pxu_p->px_ac[reg_bank]) != DDI_SUCCESS) {
		goto fail;
	}

	pxu_p->px_address[reg_bank] -= FIRE_CONTROL_STATUS;

done:
	for (; reg_bank >= PX_REG_CSR; reg_bank--) {
		DBG(DBG_ATTACH, dip, "reg_bank 0x%x address 0x%p\n",
		    reg_bank, pxu_p->px_address[reg_bank]);
	}

	return (DDI_SUCCESS);

fail:
	cmn_err(CE_WARN, "%s%d: unable to map reg entry %d\n",
	    ddi_driver_name(dip), ddi_get_instance(dip), reg_bank);

	for (reg_bank--; reg_bank >= PX_REG_CSR; reg_bank--) {
		pxu_p->px_address[reg_bank] = NULL;
		ddi_regs_map_free(&pxu_p->px_ac[reg_bank]);
	}

	return (DDI_FAILURE);
}

/*
 * px_lib_unmap_regs:
 *
 * This routine unmaps the registers mapped by map_px_registers.
 *
 * used by: px_detach(), and error conditions in px_attach()
 *
 * return value: none
 */
void
px_lib_unmap_regs(pxu_t *pxu_p)
{
	int i;

	for (i = 0; i < PX_REG_MAX; i++) {
		if (pxu_p->px_ac[i])
			ddi_regs_map_free(&pxu_p->px_ac[i]);
	}
}

int
px_lib_dev_init(dev_info_t *dip, devhandle_t *dev_hdl)
{
	px_t		*px_p = DIP_TO_STATE(dip);
	caddr_t		xbc_csr_base, csr_base;
	px_dvma_range_prop_t	px_dvma_range;
	uint32_t	chip_id;
	pxu_t		*pxu_p;

	DBG(DBG_ATTACH, dip, "px_lib_dev_init: dip 0x%p\n", dip);

	if ((chip_id = px_identity_chip(px_p)) == PX_CHIP_UNIDENTIFIED)
		return (DDI_FAILURE);

	switch (chip_id) {
	case FIRE_VER_10:
		DBG(DBG_ATTACH, dip, "FIRE Hardware Version 1.0\n");
		break;
	case FIRE_VER_20:
		DBG(DBG_ATTACH, dip, "FIRE Hardware Version 2.0\n");
		break;
	default:
		cmn_err(CE_WARN, "%s%d: FIRE Hardware Version Unknown\n",
		    ddi_driver_name(dip), ddi_get_instance(dip));
		return (DDI_FAILURE);
	}

	/*
	 * Allocate platform specific structure and link it to
	 * the px state structure.
	 */
	pxu_p = kmem_zalloc(sizeof (pxu_t), KM_SLEEP);
	pxu_p->chip_id = chip_id;
	pxu_p->portid  = ddi_getprop(DDI_DEV_T_ANY, dip, DDI_PROP_DONTPASS,
	    "portid", -1);

	/* Map in the registers */
	if (px_lib_map_regs(pxu_p, dip) == DDI_FAILURE) {
		kmem_free(pxu_p, sizeof (pxu_t));

		return (DDI_FAILURE);
	}

	xbc_csr_base = (caddr_t)pxu_p->px_address[PX_REG_XBC];
	csr_base = (caddr_t)pxu_p->px_address[PX_REG_CSR];

	pxu_p->tsb_cookie = iommu_tsb_alloc(pxu_p->portid);
	pxu_p->tsb_size = iommu_tsb_cookie_to_size(pxu_p->tsb_cookie);
	pxu_p->tsb_vaddr = iommu_tsb_cookie_to_va(pxu_p->tsb_cookie);

	/*
	 * Create "virtual-dma" property to support child devices
	 * needing to know DVMA range.
	 */
	px_dvma_range.dvma_base = (uint32_t)px_mmu_dvma_end + 1
	    - ((pxu_p->tsb_size >> 3) << MMU_PAGE_SHIFT);
	px_dvma_range.dvma_len = (uint32_t)
	    px_mmu_dvma_end - px_dvma_range.dvma_base + 1;

	(void) ddi_prop_create(DDI_DEV_T_NONE, dip, DDI_PROP_CANSLEEP,
		"virtual-dma", (caddr_t)&px_dvma_range,
		sizeof (px_dvma_range_prop_t));
	/*
	 * Initilize all fire hardware specific blocks.
	 */
	hvio_cb_init(xbc_csr_base, pxu_p);
	hvio_ib_init(csr_base, pxu_p);
	hvio_pec_init(csr_base, pxu_p);
	hvio_mmu_init(csr_base, pxu_p);

	px_p->px_plat_p = (void *)pxu_p;

	/*
	 * Initialize all the interrupt handlers
	 */
	px_err_reg_enable(px_p, PX_ERR_JBC);
	px_err_reg_enable(px_p, PX_ERR_MMU);
	px_err_reg_enable(px_p, PX_ERR_IMU);
	px_err_reg_enable(px_p, PX_ERR_TLU_UE);
	px_err_reg_enable(px_p, PX_ERR_TLU_CE);
	px_err_reg_enable(px_p, PX_ERR_TLU_OE);
	px_err_reg_enable(px_p, PX_ERR_ILU);
	px_err_reg_enable(px_p, PX_ERR_LPU_LINK);
	px_err_reg_enable(px_p, PX_ERR_LPU_PHY);
	px_err_reg_enable(px_p, PX_ERR_LPU_RX);
	px_err_reg_enable(px_p, PX_ERR_LPU_TX);
	px_err_reg_enable(px_p, PX_ERR_LPU_LTSSM);
	px_err_reg_enable(px_p, PX_ERR_LPU_GIGABLZ);

	/* Initilize device handle */
	*dev_hdl = (devhandle_t)csr_base;

	DBG(DBG_ATTACH, dip, "px_lib_dev_init: dev_hdl 0x%llx\n", *dev_hdl);

	return (DDI_SUCCESS);
}

int
px_lib_dev_fini(dev_info_t *dip)
{
	px_t	*px_p = DIP_TO_STATE(dip);
	pxu_t	*pxu_p = (pxu_t *)px_p->px_plat_p;

	DBG(DBG_DETACH, dip, "px_lib_dev_fini: dip 0x%p\n", dip);

	/*
	 * Deinitialize all the interrupt handlers
	 */
	px_err_reg_disable(px_p, PX_ERR_JBC);
	px_err_reg_disable(px_p, PX_ERR_MMU);
	px_err_reg_disable(px_p, PX_ERR_IMU);
	px_err_reg_disable(px_p, PX_ERR_TLU_UE);
	px_err_reg_disable(px_p, PX_ERR_TLU_CE);
	px_err_reg_disable(px_p, PX_ERR_TLU_OE);
	px_err_reg_disable(px_p, PX_ERR_ILU);
	px_err_reg_disable(px_p, PX_ERR_LPU_LINK);
	px_err_reg_disable(px_p, PX_ERR_LPU_PHY);
	px_err_reg_disable(px_p, PX_ERR_LPU_RX);
	px_err_reg_disable(px_p, PX_ERR_LPU_TX);
	px_err_reg_disable(px_p, PX_ERR_LPU_LTSSM);
	px_err_reg_disable(px_p, PX_ERR_LPU_GIGABLZ);

	iommu_tsb_free(pxu_p->tsb_cookie);

	px_lib_unmap_regs((pxu_t *)px_p->px_plat_p);
	kmem_free(px_p->px_plat_p, sizeof (pxu_t));
	px_p->px_plat_p = NULL;

	return (DDI_SUCCESS);
}

/*ARGSUSED*/
int
px_lib_intr_devino_to_sysino(dev_info_t *dip, devino_t devino,
    sysino_t *sysino)
{
	px_t	*px_p = DIP_TO_STATE(dip);
	pxu_t	*pxu_p = (pxu_t *)px_p->px_plat_p;
	uint64_t	ret;

	DBG(DBG_LIB_INT, dip, "px_lib_intr_devino_to_sysino: dip 0x%p "
	    "devino 0x%x\n", dip, devino);

	if ((ret = hvio_intr_devino_to_sysino(DIP_TO_HANDLE(dip),
	    pxu_p, devino, sysino)) != H_EOK) {
		DBG(DBG_LIB_INT, dip,
		    "hvio_intr_devino_to_sysino failed, ret 0x%lx\n", ret);
		return (DDI_FAILURE);
	}

	DBG(DBG_LIB_INT, dip, "px_lib_intr_devino_to_sysino: sysino 0x%llx\n",
	    *sysino);

	return (DDI_SUCCESS);
}

/*ARGSUSED*/
int
px_lib_intr_getvalid(dev_info_t *dip, sysino_t sysino,
    intr_valid_state_t *intr_valid_state)
{
	uint64_t	ret;

	DBG(DBG_LIB_INT, dip, "px_lib_intr_getvalid: dip 0x%p sysino 0x%llx\n",
	    dip, sysino);

	if ((ret = hvio_intr_getvalid(DIP_TO_HANDLE(dip),
	    sysino, intr_valid_state)) != H_EOK) {
		DBG(DBG_LIB_INT, dip, "hvio_intr_getvalid failed, ret 0x%lx\n",
		    ret);
		return (DDI_FAILURE);
	}

	DBG(DBG_LIB_INT, dip, "px_lib_intr_getvalid: intr_valid_state 0x%x\n",
	    *intr_valid_state);

	return (DDI_SUCCESS);
}

/*ARGSUSED*/
int
px_lib_intr_setvalid(dev_info_t *dip, sysino_t sysino,
    intr_valid_state_t intr_valid_state)
{
	uint64_t	ret;

	DBG(DBG_LIB_INT, dip, "px_lib_intr_setvalid: dip 0x%p sysino 0x%llx "
	    "intr_valid_state 0x%x\n", dip, sysino, intr_valid_state);

	if ((ret = hvio_intr_setvalid(DIP_TO_HANDLE(dip),
	    sysino, intr_valid_state)) != H_EOK) {
		DBG(DBG_LIB_INT, dip, "hvio_intr_setvalid failed, ret 0x%lx\n",
		    ret);
		return (DDI_FAILURE);
	}

	return (DDI_SUCCESS);
}

/*ARGSUSED*/
int
px_lib_intr_getstate(dev_info_t *dip, sysino_t sysino,
    intr_state_t *intr_state)
{
	uint64_t	ret;

	DBG(DBG_LIB_INT, dip, "px_lib_intr_getstate: dip 0x%p sysino 0x%llx\n",
	    dip, sysino);

	if ((ret = hvio_intr_getstate(DIP_TO_HANDLE(dip),
	    sysino, intr_state)) != H_EOK) {
		DBG(DBG_LIB_INT, dip, "hvio_intr_getstate failed, ret 0x%lx\n",
		    ret);
		return (DDI_FAILURE);
	}

	DBG(DBG_LIB_INT, dip, "px_lib_intr_getstate: intr_state 0x%x\n",
	    *intr_state);

	return (DDI_SUCCESS);
}

/*ARGSUSED*/
int
px_lib_intr_setstate(dev_info_t *dip, sysino_t sysino,
    intr_state_t intr_state)
{
	uint64_t	ret;

	DBG(DBG_LIB_INT, dip, "px_lib_intr_setstate: dip 0x%p sysino 0x%llx "
	    "intr_state 0x%x\n", dip, sysino, intr_state);

	if ((ret = hvio_intr_setstate(DIP_TO_HANDLE(dip),
	    sysino, intr_state)) != H_EOK) {
		DBG(DBG_LIB_INT, dip, "hvio_intr_setstate failed, ret 0x%lx\n",
		    ret);
		return (DDI_FAILURE);
	}

	return (DDI_SUCCESS);
}

/*ARGSUSED*/
int
px_lib_intr_gettarget(dev_info_t *dip, sysino_t sysino, cpuid_t *cpuid)
{
	uint64_t	ret;

	DBG(DBG_LIB_INT, dip, "px_lib_intr_gettarget: dip 0x%p sysino 0x%llx\n",
	    dip, sysino);

	if ((ret = hvio_intr_gettarget(DIP_TO_HANDLE(dip),
	    sysino, cpuid)) != H_EOK) {
		DBG(DBG_LIB_INT, dip, "hvio_intr_gettarget failed, ret 0x%lx\n",
		    ret);
		return (DDI_FAILURE);
	}

	DBG(DBG_LIB_INT, dip, "px_lib_intr_gettarget: cpuid 0x%x\n", cpuid);

	return (DDI_SUCCESS);
}

/*ARGSUSED*/
int
px_lib_intr_settarget(dev_info_t *dip, sysino_t sysino, cpuid_t cpuid)
{
	uint64_t	ret;

	DBG(DBG_LIB_INT, dip, "px_lib_intr_settarget: dip 0x%p sysino 0x%llx "
	    "cpuid 0x%x\n", dip, sysino, cpuid);

	if ((ret = hvio_intr_settarget(DIP_TO_HANDLE(dip),
	    sysino, cpuid)) != H_EOK) {
		DBG(DBG_LIB_INT, dip, "hvio_intr_settarget failed, ret 0x%lx\n",
		    ret);
		return (DDI_FAILURE);
	}

	return (DDI_SUCCESS);
}

/*ARGSUSED*/
int
px_lib_intr_reset(dev_info_t *dip)
{
	devino_t	ino;
	sysino_t	sysino;

	DBG(DBG_LIB_INT, dip, "px_lib_intr_reset: dip 0x%p\n", dip);

	/* Reset all Interrupts */
	for (ino = 0; ino < INTERRUPT_MAPPING_ENTRIES; ino++) {
		if (px_lib_intr_devino_to_sysino(dip, ino,
		    &sysino) != DDI_SUCCESS)
			return (BF_FATAL);

		if (px_lib_intr_setstate(dip, sysino,
		    INTR_IDLE_STATE) != DDI_SUCCESS)
			return (BF_FATAL);
	}

	return (BF_NONE);
}

/*ARGSUSED*/
int
px_lib_iommu_map(dev_info_t *dip, tsbid_t tsbid, pages_t pages,
    io_attributes_t io_attributes, void *addr, size_t pfn_index,
    int flag)
{
	px_t		*px_p = DIP_TO_STATE(dip);
	pxu_t		*pxu_p = (pxu_t *)px_p->px_plat_p;
	uint64_t	ret;

	DBG(DBG_LIB_DMA, dip, "px_lib_iommu_map: dip 0x%p tsbid 0x%llx "
	    "pages 0x%x atrr 0x%x addr 0x%p pfn_index 0x%llx, flag 0x%x\n",
	    dip, tsbid, pages, io_attributes, addr, pfn_index, flag);

	if ((ret = hvio_iommu_map(px_p->px_dev_hdl, pxu_p, tsbid, pages,
	    io_attributes, addr, pfn_index, flag)) != H_EOK) {
		DBG(DBG_LIB_DMA, dip,
		    "px_lib_iommu_map failed, ret 0x%lx\n", ret);
		return (DDI_FAILURE);
	}

	return (DDI_SUCCESS);
}

/*ARGSUSED*/
int
px_lib_iommu_demap(dev_info_t *dip, tsbid_t tsbid, pages_t pages)
{
	px_t		*px_p = DIP_TO_STATE(dip);
	pxu_t		*pxu_p = (pxu_t *)px_p->px_plat_p;
	uint64_t	ret;

	DBG(DBG_LIB_DMA, dip, "px_lib_iommu_demap: dip 0x%p tsbid 0x%llx "
	    "pages 0x%x\n", dip, tsbid, pages);

	if ((ret = hvio_iommu_demap(px_p->px_dev_hdl, pxu_p, tsbid, pages))
	    != H_EOK) {
		DBG(DBG_LIB_DMA, dip,
		    "px_lib_iommu_demap failed, ret 0x%lx\n", ret);

		return (DDI_FAILURE);
	}

	return (DDI_SUCCESS);
}

/*ARGSUSED*/
int
px_lib_iommu_getmap(dev_info_t *dip, tsbid_t tsbid,
    io_attributes_t *attributes_p, r_addr_t *r_addr_p)
{
	px_t	*px_p = DIP_TO_STATE(dip);
	pxu_t	*pxu_p = (pxu_t *)px_p->px_plat_p;
	uint64_t	ret;

	DBG(DBG_LIB_DMA, dip, "px_lib_iommu_getmap: dip 0x%p tsbid 0x%llx\n",
	    dip, tsbid);

	if ((ret = hvio_iommu_getmap(DIP_TO_HANDLE(dip), pxu_p, tsbid,
	    attributes_p, r_addr_p)) != H_EOK) {
		DBG(DBG_LIB_DMA, dip,
		    "hvio_iommu_getmap failed, ret 0x%lx\n", ret);

		return ((ret == H_ENOMAP) ? DDI_DMA_NOMAPPING:DDI_FAILURE);
	}

	DBG(DBG_LIB_DMA, dip, "px_lib_iommu_getmap: attr 0x%x r_addr 0x%llx\n",
	    *attributes_p, *r_addr_p);

	return (DDI_SUCCESS);
}


/*
 * Checks dma attributes against system bypass ranges
 * The bypass range is determined by the hardware. Return them so the
 * common code can do generic checking against them.
 */
/*ARGSUSED*/
int
px_lib_dma_bypass_rngchk(ddi_dma_attr_t *attrp, uint64_t *lo_p, uint64_t *hi_p)
{
	*lo_p = MMU_BYPASS_BASE;
	*hi_p = MMU_BYPASS_END;

	return (DDI_SUCCESS);
}


/*ARGSUSED*/
int
px_lib_iommu_getbypass(dev_info_t *dip, r_addr_t ra,
    io_attributes_t io_attributes, io_addr_t *io_addr_p)
{
	uint64_t	ret;

	DBG(DBG_LIB_DMA, dip, "px_lib_iommu_getbypass: dip 0x%p ra 0x%llx "
	    "attr 0x%x\n", dip, ra, io_attributes);

	if ((ret = hvio_iommu_getbypass(DIP_TO_HANDLE(dip), ra,
	    io_attributes, io_addr_p)) != H_EOK) {
		DBG(DBG_LIB_DMA, dip,
		    "hvio_iommu_getbypass failed, ret 0x%lx\n", ret);
		return (DDI_FAILURE);
	}

	DBG(DBG_LIB_DMA, dip, "px_lib_iommu_getbypass: io_addr 0x%llx\n",
	    *io_addr_p);

	return (DDI_SUCCESS);
}

/*
 * bus dma sync entry point.
 */
/*ARGSUSED*/
int
px_lib_dma_sync(dev_info_t *dip, dev_info_t *rdip, ddi_dma_handle_t handle,
	off_t off, size_t len, uint_t cache_flags)
{
	ddi_dma_impl_t *mp = (ddi_dma_impl_t *)handle;

	DBG(DBG_LIB_DMA, dip, "px_lib_dma_sync: dip 0x%p rdip 0x%p "
	    "handle 0x%llx off 0x%x len 0x%x flags 0x%x\n",
	    dip, rdip, handle, off, len, cache_flags);

	/*
	 * jbus_stst_order is found only in certain cpu modules.
	 * Just return success if not present.
	 */
	if (&jbus_stst_order == NULL)
		return (DDI_SUCCESS);

	if (!(mp->dmai_flags & DMAI_FLAGS_INUSE)) {
		cmn_err(CE_WARN, "%s%d: Unbound dma handle %p.",
		    ddi_driver_name(rdip), ddi_get_instance(rdip), (void *)mp);

		return (DDI_FAILURE);
	}

	if (mp->dmai_flags & DMAI_FLAGS_NOSYNC)
		return (DDI_SUCCESS);

	/*
	 * No flush needed when sending data from memory to device.
	 * Nothing to do to "sync" memory to what device would already see.
	 */
	if (!(mp->dmai_rflags & DDI_DMA_READ) ||
	    ((cache_flags & PX_DMA_SYNC_DDI_FLAGS) == DDI_DMA_SYNC_FORDEV))
		return (DDI_SUCCESS);

	/*
	 * Perform necessary cpu workaround to ensure jbus ordering.
	 * CPU's internal "invalidate FIFOs" are flushed.
	 */

#if !defined(lint)
	kpreempt_disable();
#endif
	jbus_stst_order();
#if !defined(lint)
	kpreempt_enable();
#endif
	return (DDI_SUCCESS);
}

/*
 * MSIQ Functions:
 */
/*ARGSUSED*/
int
px_lib_msiq_init(dev_info_t *dip)
{
	px_t		*px_p = DIP_TO_STATE(dip);
	pxu_t		*pxu_p = (pxu_t *)px_p->px_plat_p;
	px_msiq_state_t	*msiq_state_p = &px_p->px_ib_p->ib_msiq_state;
	caddr_t		msiq_addr;
	px_dvma_addr_t	pg_index;
	size_t		size;
	int		ret;

	DBG(DBG_LIB_MSIQ, dip, "px_lib_msiq_init: dip 0x%p\n", dip);

	/*
	 * Map the EQ memory into the Fire MMU (has to be 512KB aligned)
	 * and then initialize the base address register.
	 *
	 * Allocate entries from Fire IOMMU so that the resulting address
	 * is properly aligned.  Calculate the index of the first allocated
	 * entry.  Note: The size of the mapping is assumed to be a multiple
	 * of the page size.
	 */
	msiq_addr = (caddr_t)(((uint64_t)msiq_state_p->msiq_buf_p +
	    (MMU_PAGE_SIZE - 1)) >> MMU_PAGE_SHIFT << MMU_PAGE_SHIFT);

	size = msiq_state_p->msiq_cnt *
	    msiq_state_p->msiq_rec_cnt * sizeof (msiq_rec_t);

	pxu_p->msiq_mapped_p = vmem_xalloc(px_p->px_mmu_p->mmu_dvma_map,
	    size, (512 * 1024), 0, 0, NULL, NULL, VM_NOSLEEP | VM_BESTFIT);

	if (pxu_p->msiq_mapped_p == NULL)
		return (DDI_FAILURE);

	pg_index = MMU_PAGE_INDEX(px_p->px_mmu_p,
	    MMU_BTOP((ulong_t)pxu_p->msiq_mapped_p));

	if ((ret = px_lib_iommu_map(px_p->px_dip, PCI_TSBID(0, pg_index),
	    MMU_BTOP(size), PCI_MAP_ATTR_WRITE, (void *)msiq_addr, 0,
	    MMU_MAP_BUF)) != DDI_SUCCESS) {
		DBG(DBG_LIB_MSIQ, dip,
		    "hvio_msiq_init failed, ret 0x%lx\n", ret);

		(void) px_lib_msiq_fini(dip);
		return (DDI_FAILURE);
	}

	(void) hvio_msiq_init(DIP_TO_HANDLE(dip), pxu_p);

	return (DDI_SUCCESS);
}

/*ARGSUSED*/
int
px_lib_msiq_fini(dev_info_t *dip)
{
	px_t		*px_p = DIP_TO_STATE(dip);
	pxu_t		*pxu_p = (pxu_t *)px_p->px_plat_p;
	px_msiq_state_t	*msiq_state_p = &px_p->px_ib_p->ib_msiq_state;
	px_dvma_addr_t	pg_index;
	size_t		size;

	DBG(DBG_LIB_MSIQ, dip, "px_lib_msiq_fini: dip 0x%p\n", dip);

	/*
	 * Unmap and free the EQ memory that had been mapped
	 * into the Fire IOMMU.
	 */
	size = msiq_state_p->msiq_cnt *
	    msiq_state_p->msiq_rec_cnt * sizeof (msiq_rec_t);

	pg_index = MMU_PAGE_INDEX(px_p->px_mmu_p,
	    MMU_BTOP((ulong_t)pxu_p->msiq_mapped_p));

	(void) px_lib_iommu_demap(px_p->px_dip,
	    PCI_TSBID(0, pg_index), MMU_BTOP(size));

	/* Free the entries from the Fire MMU */
	vmem_xfree(px_p->px_mmu_p->mmu_dvma_map,
	    (void *)pxu_p->msiq_mapped_p, size);

	return (DDI_SUCCESS);
}

/*ARGSUSED*/
int
px_lib_msiq_info(dev_info_t *dip, msiqid_t msiq_id, r_addr_t *ra_p,
    uint_t *msiq_rec_cnt_p)
{
	px_t		*px_p = DIP_TO_STATE(dip);
	px_msiq_state_t	*msiq_state_p = &px_p->px_ib_p->ib_msiq_state;
	uint64_t	*msiq_addr;
	size_t		msiq_size;

	DBG(DBG_LIB_MSIQ, dip, "px_msiq_info: dip 0x%p msiq_id 0x%x\n",
	    dip, msiq_id);

	msiq_addr = (uint64_t *)(((uint64_t)msiq_state_p->msiq_buf_p +
	    (MMU_PAGE_SIZE - 1)) >> MMU_PAGE_SHIFT << MMU_PAGE_SHIFT);
	msiq_size = msiq_state_p->msiq_rec_cnt * sizeof (msiq_rec_t);
	ra_p = (r_addr_t *)((caddr_t)msiq_addr + (msiq_id * msiq_size));

	*msiq_rec_cnt_p = msiq_state_p->msiq_rec_cnt;

	DBG(DBG_LIB_MSIQ, dip, "px_msiq_info: ra_p 0x%p msiq_rec_cnt 0x%x\n",
	    ra_p, *msiq_rec_cnt_p);

	return (DDI_SUCCESS);
}

/*ARGSUSED*/
int
px_lib_msiq_getvalid(dev_info_t *dip, msiqid_t msiq_id,
    pci_msiq_valid_state_t *msiq_valid_state)
{
	uint64_t	ret;

	DBG(DBG_LIB_MSIQ, dip, "px_lib_msiq_getvalid: dip 0x%p msiq_id 0x%x\n",
	    dip, msiq_id);

	if ((ret = hvio_msiq_getvalid(DIP_TO_HANDLE(dip),
	    msiq_id, msiq_valid_state)) != H_EOK) {
		DBG(DBG_LIB_MSIQ, dip,
		    "hvio_msiq_getvalid failed, ret 0x%lx\n", ret);
		return (DDI_FAILURE);
	}

	DBG(DBG_LIB_MSIQ, dip, "px_lib_msiq_getvalid: msiq_valid_state 0x%x\n",
	    *msiq_valid_state);

	return (DDI_SUCCESS);
}

/*ARGSUSED*/
int
px_lib_msiq_setvalid(dev_info_t *dip, msiqid_t msiq_id,
    pci_msiq_valid_state_t msiq_valid_state)
{
	uint64_t	ret;

	DBG(DBG_LIB_MSIQ, dip, "px_lib_msiq_setvalid: dip 0x%p msiq_id 0x%x "
	    "msiq_valid_state 0x%x\n", dip, msiq_id, msiq_valid_state);

	if ((ret = hvio_msiq_setvalid(DIP_TO_HANDLE(dip),
	    msiq_id, msiq_valid_state)) != H_EOK) {
		DBG(DBG_LIB_MSIQ, dip,
		    "hvio_msiq_setvalid failed, ret 0x%lx\n", ret);
		return (DDI_FAILURE);
	}

	return (DDI_SUCCESS);
}

/*ARGSUSED*/
int
px_lib_msiq_getstate(dev_info_t *dip, msiqid_t msiq_id,
    pci_msiq_state_t *msiq_state)
{
	uint64_t	ret;

	DBG(DBG_LIB_MSIQ, dip, "px_lib_msiq_getstate: dip 0x%p msiq_id 0x%x\n",
	    dip, msiq_id);

	if ((ret = hvio_msiq_getstate(DIP_TO_HANDLE(dip),
	    msiq_id, msiq_state)) != H_EOK) {
		DBG(DBG_LIB_MSIQ, dip,
		    "hvio_msiq_getstate failed, ret 0x%lx\n", ret);
		return (DDI_FAILURE);
	}

	DBG(DBG_LIB_MSIQ, dip, "px_lib_msiq_getstate: msiq_state 0x%x\n",
	    *msiq_state);

	return (DDI_SUCCESS);
}

/*ARGSUSED*/
int
px_lib_msiq_setstate(dev_info_t *dip, msiqid_t msiq_id,
    pci_msiq_state_t msiq_state)
{
	uint64_t	ret;

	DBG(DBG_LIB_MSIQ, dip, "px_lib_msiq_setstate: dip 0x%p msiq_id 0x%x "
	    "msiq_state 0x%x\n", dip, msiq_id, msiq_state);

	if ((ret = hvio_msiq_setstate(DIP_TO_HANDLE(dip),
	    msiq_id, msiq_state)) != H_EOK) {
		DBG(DBG_LIB_MSIQ, dip,
		    "hvio_msiq_setstate failed, ret 0x%lx\n", ret);
		return (DDI_FAILURE);
	}

	return (DDI_SUCCESS);
}

/*ARGSUSED*/
int
px_lib_msiq_gethead(dev_info_t *dip, msiqid_t msiq_id,
    msiqhead_t *msiq_head)
{
	uint64_t	ret;

	DBG(DBG_LIB_MSIQ, dip, "px_lib_msiq_gethead: dip 0x%p msiq_id 0x%x\n",
	    dip, msiq_id);

	if ((ret = hvio_msiq_gethead(DIP_TO_HANDLE(dip),
	    msiq_id, msiq_head)) != H_EOK) {
		DBG(DBG_LIB_MSIQ, dip,
		    "hvio_msiq_gethead failed, ret 0x%lx\n", ret);
		return (DDI_FAILURE);
	}

	DBG(DBG_LIB_MSIQ, dip, "px_lib_msiq_gethead: msiq_head 0x%x\n",
	    *msiq_head);

	return (DDI_SUCCESS);
}

/*ARGSUSED*/
int
px_lib_msiq_sethead(dev_info_t *dip, msiqid_t msiq_id,
    msiqhead_t msiq_head)
{
	uint64_t	ret;

	DBG(DBG_LIB_MSIQ, dip, "px_lib_msiq_sethead: dip 0x%p msiq_id 0x%x "
	    "msiq_head 0x%x\n", dip, msiq_id, msiq_head);

	if ((ret = hvio_msiq_sethead(DIP_TO_HANDLE(dip),
	    msiq_id, msiq_head)) != H_EOK) {
		DBG(DBG_LIB_MSIQ, dip,
		    "hvio_msiq_sethead failed, ret 0x%lx\n", ret);
		return (DDI_FAILURE);
	}

	return (DDI_SUCCESS);
}

/*ARGSUSED*/
int
px_lib_msiq_gettail(dev_info_t *dip, msiqid_t msiq_id,
    msiqtail_t *msiq_tail)
{
	uint64_t	ret;

	DBG(DBG_LIB_MSIQ, dip, "px_lib_msiq_gettail: dip 0x%p msiq_id 0x%x\n",
	    dip, msiq_id);

	if ((ret = hvio_msiq_gettail(DIP_TO_HANDLE(dip),
	    msiq_id, msiq_tail)) != H_EOK) {
		DBG(DBG_LIB_MSIQ, dip,
		    "hvio_msiq_gettail failed, ret 0x%lx\n", ret);
		return (DDI_FAILURE);
	}

	DBG(DBG_LIB_MSIQ, dip, "px_lib_msiq_gettail: msiq_tail 0x%x\n",
	    *msiq_tail);

	return (DDI_SUCCESS);
}

/*ARGSUSED*/
void
px_lib_get_msiq_rec(dev_info_t *dip, px_msiq_t *msiq_p, msiq_rec_t *msiq_rec_p)
{
	px_t		*px_p = DIP_TO_STATE(dip);
	pxu_t		*pxu_p = (pxu_t *)px_p->px_plat_p;
	eq_rec_t	*eq_rec_p = (eq_rec_t *)msiq_p->msiq_curr;

	DBG(DBG_LIB_MSIQ, dip, "px_lib_get_msiq_rec: dip 0x%p eq_rec_p 0x%p\n",
	    dip, eq_rec_p);

	if (!eq_rec_p->eq_rec_rid) {
		/* Set msiq_rec_rid to zero */
		msiq_rec_p->msiq_rec_rid = 0;

		return;
	}

	DBG(DBG_LIB_MSIQ, dip, "px_lib_get_msiq_rec: EQ RECORD, "
	    "eq_rec_rid 0x%llx eq_rec_fmt_type 0x%llx "
	    "eq_rec_len 0x%llx eq_rec_addr0 0x%llx "
	    "eq_rec_addr1 0x%llx eq_rec_data0 0x%llx "
	    "eq_rec_data1 0x%llx\n", eq_rec_p->eq_rec_rid,
	    eq_rec_p->eq_rec_fmt_type, eq_rec_p->eq_rec_len,
	    eq_rec_p->eq_rec_addr0, eq_rec_p->eq_rec_addr1,
	    eq_rec_p->eq_rec_data0, eq_rec_p->eq_rec_data1);

	/*
	 * Only upper 4 bits of eq_rec_fmt_type is used
	 * to identify the EQ record type.
	 */
	switch (eq_rec_p->eq_rec_fmt_type >> 3) {
	case EQ_REC_MSI32:
		msiq_rec_p->msiq_rec_type = MSI32_REC;

		if (pxu_p->chip_id == FIRE_VER_10) {
			msiq_rec_p->msiq_rec_data.msi.msi_data =
			    (eq_rec_p->eq_rec_data0 & 0xFF) << 8 |
			    (eq_rec_p->eq_rec_data0 & 0xFF00) >> 8;
		} else {
			/* Default case is FIRE2.0 */
			msiq_rec_p->msiq_rec_data.msi.msi_data =
			    eq_rec_p->eq_rec_data0;
		}

		break;
	case EQ_REC_MSI64:
		msiq_rec_p->msiq_rec_type = MSI64_REC;

		if (pxu_p->chip_id == FIRE_VER_10) {
			msiq_rec_p->msiq_rec_data.msi.msi_data =
			    (eq_rec_p->eq_rec_data0 & 0xFF) << 8 |
			    (eq_rec_p->eq_rec_data0 & 0xFF00) >> 8;
		} else {
			/* Default case is FIRE2.0 */
			msiq_rec_p->msiq_rec_data.msi.msi_data =
			    eq_rec_p->eq_rec_data0;
		}

		break;
	case EQ_REC_MSG:
		msiq_rec_p->msiq_rec_type = MSG_REC;

		msiq_rec_p->msiq_rec_data.msg.msg_route =
		    eq_rec_p->eq_rec_fmt_type & 7;
		msiq_rec_p->msiq_rec_data.msg.msg_targ = eq_rec_p->eq_rec_rid;
		msiq_rec_p->msiq_rec_data.msg.msg_code = eq_rec_p->eq_rec_data0;
		break;
	default:
		cmn_err(CE_WARN, "%s%d: px_lib_get_msiq_rec: "
		    "0x%lx is an unknown EQ record type",
		    ddi_driver_name(dip), ddi_get_instance(dip),
		    eq_rec_p->eq_rec_fmt_type);
		break;
	}

	msiq_rec_p->msiq_rec_rid = eq_rec_p->eq_rec_rid;
	msiq_rec_p->msiq_rec_msi_addr = ((eq_rec_p->eq_rec_addr1 << 16) |
	    (eq_rec_p->eq_rec_addr0 << 2));

	/* Zero out eq_rec_rid field */
	eq_rec_p->eq_rec_rid = 0;
}

/*
 * MSI Functions:
 */
/*ARGSUSED*/
int
px_lib_msi_init(dev_info_t *dip)
{
	px_t		*px_p = DIP_TO_STATE(dip);
	px_msi_state_t	*msi_state_p = &px_p->px_ib_p->ib_msi_state;
	uint64_t	ret;

	DBG(DBG_LIB_MSI, dip, "px_lib_msi_init: dip 0x%p\n", dip);

	if ((ret = hvio_msi_init(DIP_TO_HANDLE(dip),
	    msi_state_p->msi_addr32, msi_state_p->msi_addr64)) != H_EOK) {
		DBG(DBG_LIB_MSIQ, dip, "px_lib_msi_init failed, ret 0x%lx\n",
		    ret);
		return (DDI_FAILURE);
	}

	return (DDI_SUCCESS);
}

/*ARGSUSED*/
int
px_lib_msi_getmsiq(dev_info_t *dip, msinum_t msi_num,
    msiqid_t *msiq_id)
{
	uint64_t	ret;

	DBG(DBG_LIB_MSI, dip, "px_lib_msi_getmsiq: dip 0x%p msi_num 0x%x\n",
	    dip, msi_num);

	if ((ret = hvio_msi_getmsiq(DIP_TO_HANDLE(dip),
	    msi_num, msiq_id)) != H_EOK) {
		DBG(DBG_LIB_MSI, dip,
		    "hvio_msi_getmsiq failed, ret 0x%lx\n", ret);
		return (DDI_FAILURE);
	}

	DBG(DBG_LIB_MSI, dip, "px_lib_msi_getmsiq: msiq_id 0x%x\n",
	    *msiq_id);

	return (DDI_SUCCESS);
}

/*ARGSUSED*/
int
px_lib_msi_setmsiq(dev_info_t *dip, msinum_t msi_num,
    msiqid_t msiq_id, msi_type_t msitype)
{
	uint64_t	ret;

	DBG(DBG_LIB_MSI, dip, "px_lib_msi_setmsiq: dip 0x%p msi_num 0x%x "
	    "msq_id 0x%x\n", dip, msi_num, msiq_id);

	if ((ret = hvio_msi_setmsiq(DIP_TO_HANDLE(dip),
	    msi_num, msiq_id)) != H_EOK) {
		DBG(DBG_LIB_MSI, dip,
		    "hvio_msi_setmsiq failed, ret 0x%lx\n", ret);
		return (DDI_FAILURE);
	}

	return (DDI_SUCCESS);
}

/*ARGSUSED*/
int
px_lib_msi_getvalid(dev_info_t *dip, msinum_t msi_num,
    pci_msi_valid_state_t *msi_valid_state)
{
	uint64_t	ret;

	DBG(DBG_LIB_MSI, dip, "px_lib_msi_getvalid: dip 0x%p msi_num 0x%x\n",
	    dip, msi_num);

	if ((ret = hvio_msi_getvalid(DIP_TO_HANDLE(dip),
	    msi_num, msi_valid_state)) != H_EOK) {
		DBG(DBG_LIB_MSI, dip,
		    "hvio_msi_getvalid failed, ret 0x%lx\n", ret);
		return (DDI_FAILURE);
	}

	DBG(DBG_LIB_MSI, dip, "px_lib_msi_getvalid: msiq_id 0x%x\n",
	    *msi_valid_state);

	return (DDI_SUCCESS);
}

/*ARGSUSED*/
int
px_lib_msi_setvalid(dev_info_t *dip, msinum_t msi_num,
    pci_msi_valid_state_t msi_valid_state)
{
	uint64_t	ret;

	DBG(DBG_LIB_MSI, dip, "px_lib_msi_setvalid: dip 0x%p msi_num 0x%x "
	    "msi_valid_state 0x%x\n", dip, msi_num, msi_valid_state);

	if ((ret = hvio_msi_setvalid(DIP_TO_HANDLE(dip),
	    msi_num, msi_valid_state)) != H_EOK) {
		DBG(DBG_LIB_MSI, dip,
		    "hvio_msi_setvalid failed, ret 0x%lx\n", ret);
		return (DDI_FAILURE);
	}

	return (DDI_SUCCESS);
}

/*ARGSUSED*/
int
px_lib_msi_getstate(dev_info_t *dip, msinum_t msi_num,
    pci_msi_state_t *msi_state)
{
	uint64_t	ret;

	DBG(DBG_LIB_MSI, dip, "px_lib_msi_getstate: dip 0x%p msi_num 0x%x\n",
	    dip, msi_num);

	if ((ret = hvio_msi_getstate(DIP_TO_HANDLE(dip),
	    msi_num, msi_state)) != H_EOK) {
		DBG(DBG_LIB_MSI, dip,
		    "hvio_msi_getstate failed, ret 0x%lx\n", ret);
		return (DDI_FAILURE);
	}

	DBG(DBG_LIB_MSI, dip, "px_lib_msi_getstate: msi_state 0x%x\n",
	    *msi_state);

	return (DDI_SUCCESS);
}

/*ARGSUSED*/
int
px_lib_msi_setstate(dev_info_t *dip, msinum_t msi_num,
    pci_msi_state_t msi_state)
{
	uint64_t	ret;

	DBG(DBG_LIB_MSI, dip, "px_lib_msi_setstate: dip 0x%p msi_num 0x%x "
	    "msi_state 0x%x\n", dip, msi_num, msi_state);

	if ((ret = hvio_msi_setstate(DIP_TO_HANDLE(dip),
	    msi_num, msi_state)) != H_EOK) {
		DBG(DBG_LIB_MSI, dip,
		    "hvio_msi_setstate failed, ret 0x%lx\n", ret);
		return (DDI_FAILURE);
	}

	return (DDI_SUCCESS);
}

/*
 * MSG Functions:
 */
/*ARGSUSED*/
int
px_lib_msg_getmsiq(dev_info_t *dip, pcie_msg_type_t msg_type,
    msiqid_t *msiq_id)
{
	uint64_t	ret;

	DBG(DBG_LIB_MSG, dip, "px_lib_msg_getmsiq: dip 0x%p msg_type 0x%x\n",
	    dip, msg_type);

	if ((ret = hvio_msg_getmsiq(DIP_TO_HANDLE(dip),
	    msg_type, msiq_id)) != H_EOK) {
		DBG(DBG_LIB_MSG, dip,
		    "hvio_msg_getmsiq failed, ret 0x%lx\n", ret);
		return (DDI_FAILURE);
	}

	DBG(DBG_LIB_MSI, dip, "px_lib_msg_getmsiq: msiq_id 0x%x\n",
	    *msiq_id);

	return (DDI_SUCCESS);
}

/*ARGSUSED*/
int
px_lib_msg_setmsiq(dev_info_t *dip, pcie_msg_type_t msg_type,
    msiqid_t msiq_id)
{
	uint64_t	ret;

	DBG(DBG_LIB_MSG, dip, "px_lib_msi_setstate: dip 0x%p msg_type 0x%x "
	    "msiq_id 0x%x\n", dip, msg_type, msiq_id);

	if ((ret = hvio_msg_setmsiq(DIP_TO_HANDLE(dip),
	    msg_type, msiq_id)) != H_EOK) {
		DBG(DBG_LIB_MSG, dip,
		    "hvio_msg_setmsiq failed, ret 0x%lx\n", ret);
		return (DDI_FAILURE);
	}

	return (DDI_SUCCESS);
}

/*ARGSUSED*/
int
px_lib_msg_getvalid(dev_info_t *dip, pcie_msg_type_t msg_type,
    pcie_msg_valid_state_t *msg_valid_state)
{
	uint64_t	ret;

	DBG(DBG_LIB_MSG, dip, "px_lib_msg_getvalid: dip 0x%p msg_type 0x%x\n",
	    dip, msg_type);

	if ((ret = hvio_msg_getvalid(DIP_TO_HANDLE(dip), msg_type,
	    msg_valid_state)) != H_EOK) {
		DBG(DBG_LIB_MSG, dip,
		    "hvio_msg_getvalid failed, ret 0x%lx\n", ret);
		return (DDI_FAILURE);
	}

	DBG(DBG_LIB_MSI, dip, "px_lib_msg_getvalid: msg_valid_state 0x%x\n",
	    *msg_valid_state);

	return (DDI_SUCCESS);
}

/*ARGSUSED*/
int
px_lib_msg_setvalid(dev_info_t *dip, pcie_msg_type_t msg_type,
    pcie_msg_valid_state_t msg_valid_state)
{
	uint64_t	ret;

	DBG(DBG_LIB_MSG, dip, "px_lib_msg_setvalid: dip 0x%p msg_type 0x%x "
	    "msg_valid_state 0x%x\n", dip, msg_type, msg_valid_state);

	if ((ret = hvio_msg_setvalid(DIP_TO_HANDLE(dip), msg_type,
	    msg_valid_state)) != H_EOK) {
		DBG(DBG_LIB_MSG, dip,
		    "hvio_msg_setvalid failed, ret 0x%lx\n", ret);
		return (DDI_FAILURE);
	}

	return (DDI_SUCCESS);
}

/*
 * Suspend/Resume Functions:
 * Currently unsupported by hypervisor
 */
int
px_lib_suspend(dev_info_t *dip)
{
	px_t		*px_p = DIP_TO_STATE(dip);
	pxu_t		*pxu_p = (pxu_t *)px_p->px_plat_p;
	devhandle_t	dev_hdl, xbus_dev_hdl;
	uint64_t	ret;

	DBG(DBG_DETACH, dip, "px_lib_suspend: dip 0x%p\n", dip);

	dev_hdl = (devhandle_t)pxu_p->px_address[PX_REG_CSR];
	xbus_dev_hdl = (devhandle_t)pxu_p->px_address[PX_REG_XBC];

	if ((ret = hvio_suspend(dev_hdl, pxu_p)) == H_EOK) {
		px_p->px_cb_p->xbc_attachcnt--;
		if (px_p->px_cb_p->xbc_attachcnt == 0)
			if ((ret = hvio_cb_suspend(xbus_dev_hdl, pxu_p))
			    != H_EOK)
				px_p->px_cb_p->xbc_attachcnt++;
	}

	return ((ret != H_EOK) ? DDI_FAILURE: DDI_SUCCESS);
}

void
px_lib_resume(dev_info_t *dip)
{
	px_t		*px_p = DIP_TO_STATE(dip);
	pxu_t		*pxu_p = (pxu_t *)px_p->px_plat_p;
	devhandle_t	dev_hdl, xbus_dev_hdl;
	devino_t	pec_ino = px_p->px_inos[PX_INTR_PEC];
	devino_t	xbc_ino = px_p->px_inos[PX_INTR_XBC];

	DBG(DBG_ATTACH, dip, "px_lib_resume: dip 0x%p\n", dip);

	dev_hdl = (devhandle_t)pxu_p->px_address[PX_REG_CSR];
	xbus_dev_hdl = (devhandle_t)pxu_p->px_address[PX_REG_XBC];

	px_p->px_cb_p->xbc_attachcnt++;
	if (px_p->px_cb_p->xbc_attachcnt == 1)
		hvio_cb_resume(dev_hdl, xbus_dev_hdl, xbc_ino, pxu_p);
	hvio_resume(dev_hdl, pec_ino, pxu_p);
}

/*
 * PCI tool Functions:
 * Currently unsupported by hypervisor
 */
/*ARGSUSED*/
int
px_lib_tools_dev_reg_ops(dev_info_t *dip, void *arg, int cmd, int mode)
{
	px_t *px_p = DIP_TO_STATE(dip);

	DBG(DBG_TOOLS, dip, "px_lib_tools_dev_reg_ops: dip 0x%p arg 0x%p "
	    "cmd 0x%x mode 0x%x\n", dip, arg, cmd, mode);

	return (px_dev_reg_ops(dip, arg, cmd, mode, px_p));
}

/*ARGSUSED*/
int
px_lib_tools_bus_reg_ops(dev_info_t *dip, void *arg, int cmd, int mode)
{
	DBG(DBG_TOOLS, dip, "px_lib_tools_bus_reg_ops: dip 0x%p arg 0x%p "
	    "cmd 0x%x mode 0x%x\n", dip, arg, cmd, mode);

	return (px_bus_reg_ops(dip, arg, cmd, mode));
}

/*ARGSUSED*/
int
px_lib_tools_intr_admn(dev_info_t *dip, void *arg, int cmd, int mode)
{
	px_t *px_p = DIP_TO_STATE(dip);

	DBG(DBG_TOOLS, dip, "px_lib_tools_intr_admn: dip 0x%p arg 0x%p "
	    "cmd 0x%x mode 0x%x\n", dip, arg, cmd, mode);

	return (px_intr_admn(dip, arg, cmd, mode, px_p));
}

/*
 * Misc Functions:
 * Currently unsupported by hypervisor
 */
uint64_t
px_lib_get_cb(dev_info_t *dip)
{
	px_t	*px_p = DIP_TO_STATE(dip);
	pxu_t	*pxu_p = (pxu_t *)px_p->px_plat_p;

	return (CSR_XR((caddr_t)pxu_p->px_address[PX_REG_XBC], JBUS_SCRATCH_1));
}

void
px_lib_set_cb(dev_info_t *dip, uint64_t val)
{
	px_t	*px_p = DIP_TO_STATE(dip);
	pxu_t	*pxu_p = (pxu_t *)px_p->px_plat_p;

	CSR_XS((caddr_t)pxu_p->px_address[PX_REG_XBC], JBUS_SCRATCH_1, val);
}

/*ARGSUSED*/
int
px_lib_map_vconfig(dev_info_t *dip,
	ddi_map_req_t *mp, pci_config_offset_t off,
		pci_regspec_t *rp, caddr_t *addrp)
{
	/*
	 * No special config space access services in this layer.
	 */
	return (DDI_FAILURE);
}

static void
px_lib_clr_errs(px_t *px_p, px_pec_t *pec_p)
{
	dev_info_t	*rpdip = px_p->px_dip;
	px_cb_t		*cb_p = px_p->px_cb_p;
	int		err = PX_OK, ret;
	int		acctype = pec_p->pec_safeacc_type;
	ddi_fm_error_t	derr;

	/* Create the derr */
	bzero(&derr, sizeof (ddi_fm_error_t));
	derr.fme_version = DDI_FME_VERSION;
	derr.fme_ena = fm_ena_generate(0, FM_ENA_FMT1);
	derr.fme_flag = acctype;

	if (acctype == DDI_FM_ERR_EXPECTED) {
		derr.fme_status = DDI_FM_NONFATAL;
		ndi_fm_acc_err_set(pec_p->pec_acc_hdl, &derr);
	}

	mutex_enter(&cb_p->xbc_fm_mutex);

	/* send ereport/handle/clear fire registers */
	err = px_err_handle(px_p, &derr, PX_LIB_CALL, B_TRUE);

	/* Check all child devices for errors */
	ret = ndi_fm_handler_dispatch(rpdip, NULL, &derr);

	mutex_exit(&cb_p->xbc_fm_mutex);

	/*
	 * PX_FATAL_HW indicates a condition recovered from Fatal-Reset,
	 * therefore it does not cause panic.
	 */
	if ((err & (PX_FATAL_GOS | PX_FATAL_SW)) || (ret == DDI_FM_FATAL))
		fm_panic("Fatal System Port Error has occurred\n");
}

#ifdef  DEBUG
int	px_peekfault_cnt = 0;
int	px_pokefault_cnt = 0;
#endif  /* DEBUG */

/*ARGSUSED*/
static int
px_lib_do_poke(dev_info_t *dip, dev_info_t *rdip,
    peekpoke_ctlops_t *in_args)
{
	px_t *px_p = DIP_TO_STATE(dip);
	px_pec_t *pec_p = px_p->px_pec_p;
	int err = DDI_SUCCESS;
	on_trap_data_t otd;

	mutex_enter(&pec_p->pec_pokefault_mutex);
	pec_p->pec_ontrap_data = &otd;
	pec_p->pec_safeacc_type = DDI_FM_ERR_POKE;

	/* Set up protected environment. */
	if (!on_trap(&otd, OT_DATA_ACCESS)) {
		uintptr_t tramp = otd.ot_trampoline;

		otd.ot_trampoline = (uintptr_t)&poke_fault;
		err = do_poke(in_args->size, (void *)in_args->dev_addr,
		    (void *)in_args->host_addr);
		otd.ot_trampoline = tramp;
	} else
		err = DDI_FAILURE;

	px_lib_clr_errs(px_p, pec_p);

	if (otd.ot_trap & OT_DATA_ACCESS)
		err = DDI_FAILURE;

	/* Take down protected environment. */
	no_trap();

	pec_p->pec_ontrap_data = NULL;
	pec_p->pec_safeacc_type = DDI_FM_ERR_UNEXPECTED;
	mutex_exit(&pec_p->pec_pokefault_mutex);

#ifdef  DEBUG
	if (err == DDI_FAILURE)
		px_pokefault_cnt++;
#endif
	return (err);
}

/*ARGSUSED*/
static int
px_lib_do_caut_put(dev_info_t *dip, dev_info_t *rdip,
    peekpoke_ctlops_t *cautacc_ctlops_arg)
{
	size_t size = cautacc_ctlops_arg->size;
	uintptr_t dev_addr = cautacc_ctlops_arg->dev_addr;
	uintptr_t host_addr = cautacc_ctlops_arg->host_addr;
	ddi_acc_impl_t *hp = (ddi_acc_impl_t *)cautacc_ctlops_arg->handle;
	size_t repcount = cautacc_ctlops_arg->repcount;
	uint_t flags = cautacc_ctlops_arg->flags;

	px_t *px_p = DIP_TO_STATE(dip);
	px_pec_t *pec_p = px_p->px_pec_p;
	int err = DDI_SUCCESS;

	/*
	 * Note that i_ndi_busop_access_enter ends up grabbing the pokefault
	 * mutex.
	 */
	i_ndi_busop_access_enter(hp->ahi_common.ah_dip, (ddi_acc_handle_t)hp);

	pec_p->pec_ontrap_data = (on_trap_data_t *)hp->ahi_err->err_ontrap;
	pec_p->pec_safeacc_type = DDI_FM_ERR_EXPECTED;
	hp->ahi_err->err_expected = DDI_FM_ERR_EXPECTED;

	if (!i_ddi_ontrap((ddi_acc_handle_t)hp)) {
		for (; repcount; repcount--) {
			switch (size) {

			case sizeof (uint8_t):
				i_ddi_put8(hp, (uint8_t *)dev_addr,
				    *(uint8_t *)host_addr);
				break;

			case sizeof (uint16_t):
				i_ddi_put16(hp, (uint16_t *)dev_addr,
				    *(uint16_t *)host_addr);
				break;

			case sizeof (uint32_t):
				i_ddi_put32(hp, (uint32_t *)dev_addr,
				    *(uint32_t *)host_addr);
				break;

			case sizeof (uint64_t):
				i_ddi_put64(hp, (uint64_t *)dev_addr,
				    *(uint64_t *)host_addr);
				break;
			}

			host_addr += size;

			if (flags == DDI_DEV_AUTOINCR)
				dev_addr += size;

			px_lib_clr_errs(px_p, pec_p);

			if (pec_p->pec_ontrap_data->ot_trap & OT_DATA_ACCESS) {
				err = DDI_FAILURE;
#ifdef  DEBUG
				px_pokefault_cnt++;
#endif
				break;
			}
		}
	}

	i_ddi_notrap((ddi_acc_handle_t)hp);
	pec_p->pec_ontrap_data = NULL;
	pec_p->pec_safeacc_type = DDI_FM_ERR_UNEXPECTED;
	i_ndi_busop_access_exit(hp->ahi_common.ah_dip, (ddi_acc_handle_t)hp);
	hp->ahi_err->err_expected = DDI_FM_ERR_UNEXPECTED;

	return (err);
}


int
px_lib_ctlops_poke(dev_info_t *dip, dev_info_t *rdip,
    peekpoke_ctlops_t *in_args)
{
	return (in_args->handle ? px_lib_do_caut_put(dip, rdip, in_args) :
	    px_lib_do_poke(dip, rdip, in_args));
}


/*ARGSUSED*/
static int
px_lib_do_peek(dev_info_t *dip, peekpoke_ctlops_t *in_args)
{
	px_t *px_p = DIP_TO_STATE(dip);
	px_pec_t *pec_p = px_p->px_pec_p;
	int err = DDI_SUCCESS;
	on_trap_data_t otd;

	mutex_enter(&pec_p->pec_pokefault_mutex);
	pec_p->pec_safeacc_type = DDI_FM_ERR_PEEK;

	if (!on_trap(&otd, OT_DATA_ACCESS)) {
		uintptr_t tramp = otd.ot_trampoline;

		otd.ot_trampoline = (uintptr_t)&peek_fault;
		err = do_peek(in_args->size, (void *)in_args->dev_addr,
		    (void *)in_args->host_addr);
		otd.ot_trampoline = tramp;
	} else
		err = DDI_FAILURE;

	no_trap();
	pec_p->pec_safeacc_type = DDI_FM_ERR_UNEXPECTED;
	mutex_exit(&pec_p->pec_pokefault_mutex);

#ifdef  DEBUG
	if (err == DDI_FAILURE)
		px_peekfault_cnt++;
#endif
	return (err);
}


static int
px_lib_do_caut_get(dev_info_t *dip, peekpoke_ctlops_t *cautacc_ctlops_arg)
{
	size_t size = cautacc_ctlops_arg->size;
	uintptr_t dev_addr = cautacc_ctlops_arg->dev_addr;
	uintptr_t host_addr = cautacc_ctlops_arg->host_addr;
	ddi_acc_impl_t *hp = (ddi_acc_impl_t *)cautacc_ctlops_arg->handle;
	size_t repcount = cautacc_ctlops_arg->repcount;
	uint_t flags = cautacc_ctlops_arg->flags;

	px_t *px_p = DIP_TO_STATE(dip);
	px_pec_t *pec_p = px_p->px_pec_p;
	int err = DDI_SUCCESS;

	/*
	 * Note that i_ndi_busop_access_enter ends up grabbing the pokefault
	 * mutex.
	 */
	i_ndi_busop_access_enter(hp->ahi_common.ah_dip, (ddi_acc_handle_t)hp);

	pec_p->pec_ontrap_data = (on_trap_data_t *)hp->ahi_err->err_ontrap;
	pec_p->pec_safeacc_type = DDI_FM_ERR_EXPECTED;
	hp->ahi_err->err_expected = DDI_FM_ERR_EXPECTED;

	if (repcount == 1) {
		if (!i_ddi_ontrap((ddi_acc_handle_t)hp)) {
			i_ddi_caut_get(size, (void *)dev_addr,
			    (void *)host_addr);
		} else {
			int i;
			uint8_t *ff_addr = (uint8_t *)host_addr;
			for (i = 0; i < size; i++)
				*ff_addr++ = 0xff;

			err = DDI_FAILURE;
#ifdef  DEBUG
			px_peekfault_cnt++;
#endif
		}
	} else {
		if (!i_ddi_ontrap((ddi_acc_handle_t)hp)) {
			for (; repcount; repcount--) {
				i_ddi_caut_get(size, (void *)dev_addr,
				    (void *)host_addr);

				host_addr += size;

				if (flags == DDI_DEV_AUTOINCR)
					dev_addr += size;
			}
		} else {
			err = DDI_FAILURE;
#ifdef  DEBUG
			px_peekfault_cnt++;
#endif
		}
	}

	i_ddi_notrap((ddi_acc_handle_t)hp);
	pec_p->pec_ontrap_data = NULL;
	pec_p->pec_safeacc_type = DDI_FM_ERR_UNEXPECTED;
	i_ndi_busop_access_exit(hp->ahi_common.ah_dip, (ddi_acc_handle_t)hp);
	hp->ahi_err->err_expected = DDI_FM_ERR_UNEXPECTED;

	return (err);
}

/*ARGSUSED*/
int
px_lib_ctlops_peek(dev_info_t *dip, dev_info_t *rdip,
    peekpoke_ctlops_t *in_args, void *result)
{
	result = (void *)in_args->host_addr;
	return (in_args->handle ? px_lib_do_caut_get(dip, in_args) :
	    px_lib_do_peek(dip, in_args));
}
/*
 * implements PPM interface
 */
int
px_lib_pmctl(int cmd, px_t *px_p)
{
	ASSERT((cmd & ~PPMREQ_MASK) == PPMREQ);
	switch (cmd) {
	case PPMREQ_PRE_PWR_OFF:
		/*
		 * Currently there is no device power management for
		 * the root complex (fire). When there is we need to make
		 * sure that it is at full power before trying to send the
		 * PME_Turn_Off message.
		 */
		DBG(DBG_PWR, px_p->px_dip,
		    "ioctl: request to send PME_Turn_Off\n");
		return (px_goto_l23ready(px_p));

	case PPMREQ_PRE_PWR_ON:
	case PPMREQ_POST_PWR_ON:
		/* code to be written for Fire 2.0. return failure for now */
		return (DDI_FAILURE);

	default:
		return (DDI_FAILURE);
	}
}

/*
 * sends PME_Turn_Off message to put the link in L2/L3 ready state.
 * called by px_ioctl.
 * returns DDI_SUCCESS or DDI_FAILURE
 * 1. Wait for link to be in L1 state (link status reg)
 * 2. write to PME_Turn_off reg to boradcast
 * 3. set timeout
 * 4. If timeout, return failure.
 * 5. If PM_TO_Ack, wait till link is in L2/L3 ready
 */
static int
px_goto_l23ready(px_t *px_p)
{
	pcie_pwr_t	*pwr_p;
	pxu_t		*pxu_p = (pxu_t *)px_p->px_plat_p;
	caddr_t	csr_base = (caddr_t)pxu_p->px_address[PX_REG_CSR];
	int		ret = DDI_SUCCESS;
	clock_t		end, timeleft;

	/* If no PM info, return failure */
	if (!PCIE_PMINFO(px_p->px_dip) ||
	    !(pwr_p = PCIE_NEXUS_PMINFO(px_p->px_dip)))
		return (DDI_FAILURE);

	mutex_enter(&pwr_p->pwr_lock);
	mutex_enter(&pwr_p->pwr_intr_lock);
	/* Clear the PME_To_ACK receieved flag */
	pwr_p->pwr_flags &= ~PCIE_PMETOACK_RECVD;
	if (px_send_pme_turnoff(csr_base) != DDI_SUCCESS) {
		ret = DDI_FAILURE;
		goto l23ready_done;
	}
	pwr_p->pwr_flags |= PCIE_PME_TURNOFF_PENDING;

	end = ddi_get_lbolt() + drv_usectohz(px_pme_to_ack_timeout);
	while (!(pwr_p->pwr_flags & PCIE_PMETOACK_RECVD)) {
		timeleft = cv_timedwait(&pwr_p->pwr_cv,
		    &pwr_p->pwr_intr_lock, end);
		/*
		 * if cv_timedwait returns -1, it is either
		 * 1) timed out or
		 * 2) there was a pre-mature wakeup but by the time
		 * cv_timedwait is called again end < lbolt i.e.
		 * end is in the past.
		 * 3) By the time we make first cv_timedwait call,
		 * end < lbolt is true.
		 */
		if (timeleft == -1)
			break;
	}
	if (!(pwr_p->pwr_flags & PCIE_PMETOACK_RECVD)) {
		/*
		 * Either timedout or interrupt didn't get a
		 * chance to grab the mutex and set the flag.
		 * release the mutex and delay for sometime.
		 * This will 1) give a chance for interrupt to
		 * set the flag 2) creates a delay between two
		 * consequetive requests.
		 */
		mutex_exit(&pwr_p->pwr_intr_lock);
		delay(5);
		mutex_enter(&pwr_p->pwr_intr_lock);
		if (!(pwr_p->pwr_flags & PCIE_PMETOACK_RECVD)) {
			ret = DDI_FAILURE;
			DBG(DBG_PWR, px_p->px_dip, " Timed out while waiting"
			    " for PME_TO_ACK\n");
		}
	}
	/* PME_To_ACK receieved */
	pwr_p->pwr_flags &= ~(PCIE_PME_TURNOFF_PENDING | PCIE_PMETOACK_RECVD);

	/* TBD: wait till link is in L2/L3 ready (link status reg) */

l23ready_done:
	mutex_exit(&pwr_p->pwr_intr_lock);
	mutex_exit(&pwr_p->pwr_lock);
	return (ret);
}


/*
 * Extract the drivers binding name to identify which chip we're binding to.
 * Whenever a new bus bridge is created, the driver alias entry should be
 * added here to identify the device if needed.  If a device isn't added,
 * the identity defaults to PX_CHIP_UNIDENTIFIED.
 */
static uint32_t
px_identity_chip(px_t *px_p)
{
	dev_info_t	*dip = px_p->px_dip;
	char		*name = ddi_binding_name(dip);
	uint32_t	revision = 0;

	revision = ddi_prop_get_int(DDI_DEV_T_ANY, dip, DDI_PROP_DONTPASS,
	    "module-revision#", 0);

	/* Check for Fire driver binding name */
	if (strcmp(name, "pci108e,80f0") == 0) {
		DBG(DBG_ATTACH, dip, "px_identity_chip: %s%d: "
		    "name %s module-revision %d\n", ddi_driver_name(dip),
		    ddi_get_instance(dip), name, revision);

		return (PX_CHIP_ID(PX_CHIP_FIRE, revision, 0x00));
	}

	DBG(DBG_ATTACH, dip, "%s%d: Unknown PCI Express Host bridge %s %x\n",
	    ddi_driver_name(dip), ddi_get_instance(dip), name, revision);

	return (PX_CHIP_UNIDENTIFIED);
}

int
px_err_add_intr(px_fault_t *px_fault_p)
{
	dev_info_t	*dip = px_fault_p->px_fh_dip;
	px_t		*px_p = DIP_TO_STATE(dip);

	VERIFY(add_ivintr(px_fault_p->px_fh_sysino, PX_ERR_PIL,
		px_fault_p->px_err_func, (caddr_t)px_fault_p, NULL) == 0);

	px_ib_intr_enable(px_p, intr_dist_cpuid(), px_fault_p->px_intr_ino);

	return (DDI_SUCCESS);
}

void
px_err_rem_intr(px_fault_t *px_fault_p)
{
	dev_info_t	*dip = px_fault_p->px_fh_dip;
	px_t		*px_p = DIP_TO_STATE(dip);

	rem_ivintr(px_fault_p->px_fh_sysino, NULL);

	px_ib_intr_disable(px_p->px_ib_p, px_fault_p->px_intr_ino,
		IB_INTR_WAIT);
}

#ifdef FMA
void
px_fill_rc_status(px_fault_t *px_fault_p, pciex_rc_error_regs_t *rc_status)
{
	/* populate the rc_status by reading the registers - TBD */
}
#endif /* FMA */
