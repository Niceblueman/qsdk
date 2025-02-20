/*
 * Copyright (c) 2014-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * DOC: reg_priv_objs.c
 * This file defines the APIs to create regulatory private PSOC and PDEV
 * objects.
 */

#include <wlan_cmn.h>
#include <reg_services_public_struct.h>
#include <wlan_objmgr_psoc_obj.h>
#include <wlan_objmgr_pdev_obj.h>
#include <qdf_lock.h>
#include "reg_priv_objs.h"
#include "reg_utils.h"
#include "reg_services_common.h"
#include "reg_build_chan_list.h"
#include "reg_host_11d.h"
#include "reg_callbacks.h"
#include "wlan_utility.h"
#ifdef CONFIG_AFC_SUPPORT
#include <cfg_ucfg_api.h>
#endif

struct wlan_regulatory_psoc_priv_obj *reg_get_psoc_obj(
		struct wlan_objmgr_psoc *psoc)
{
	struct wlan_regulatory_psoc_priv_obj *psoc_priv_obj;

	if (!psoc) {
		reg_alert("psoc is NULL");
		return NULL;
	}
	psoc_priv_obj = wlan_objmgr_psoc_get_comp_private_obj(
			psoc, WLAN_UMAC_COMP_REGULATORY);

	return psoc_priv_obj;
}

struct wlan_regulatory_pdev_priv_obj *reg_get_pdev_obj(
		struct wlan_objmgr_pdev *pdev)
{
	struct wlan_regulatory_pdev_priv_obj *pdev_reg;

	if (!pdev) {
		reg_alert("pdev is NULL");
		return NULL;
	}

	pdev_reg = wlan_objmgr_pdev_get_comp_private_obj(
			pdev, WLAN_UMAC_COMP_REGULATORY);

	return pdev_reg;
}

/*
 * reg_set_5dot9_ghz_chan_in_master_mode - Set 5.9GHz channels to operate
 * in master mode.
 * @soc_reg_obj - Pointer to soc_reg_obj.
 *
 * Return: void
 *
 */
#ifdef CONFIG_REG_CLIENT
static void
reg_set_5dot9_ghz_chan_in_master_mode(struct wlan_regulatory_psoc_priv_obj
				      *soc_reg_obj)
{
	soc_reg_obj->enable_5dot9_ghz_chan_in_master_mode = false;
}
#else
static void
reg_set_5dot9_ghz_chan_in_master_mode(struct wlan_regulatory_psoc_priv_obj
				      *soc_reg_obj)
{
	soc_reg_obj->enable_5dot9_ghz_chan_in_master_mode = true;
}
#endif

QDF_STATUS wlan_regulatory_psoc_obj_created_notification(
		struct wlan_objmgr_psoc *psoc, void *arg_list)
{
	struct wlan_regulatory_psoc_priv_obj *soc_reg_obj;
	struct regulatory_channel *mas_chan_list;
	enum channel_enum chan_enum;
	QDF_STATUS status;
	uint8_t i;
	uint8_t phy_cnt;

	soc_reg_obj = qdf_mem_common_alloc(sizeof(*soc_reg_obj));
	if (!soc_reg_obj)
		return QDF_STATUS_E_NOMEM;

	soc_reg_obj->offload_enabled = false;
	soc_reg_obj->psoc_ptr = psoc;
	soc_reg_obj->dfs_enabled = true;
	soc_reg_obj->band_capability = (BIT(REG_BAND_2G) | BIT(REG_BAND_5G) |
					BIT(REG_BAND_6G));
	soc_reg_obj->enable_11d_supp = false;
	soc_reg_obj->indoor_chan_enabled = true;
	soc_reg_obj->force_ssc_disable_indoor_channel = false;
	soc_reg_obj->vdev_cnt_11d = 0;
	soc_reg_obj->vdev_id_for_11d_scan = INVALID_VDEV_ID;
	soc_reg_obj->restart_beaconing = CH_AVOID_RULE_RESTART;
	soc_reg_obj->enable_srd_chan_in_master_mode = 0xFF;
	soc_reg_obj->enable_11d_in_world_mode = false;
	soc_reg_obj->five_dot_nine_ghz_supported = false;
	reg_set_5dot9_ghz_chan_in_master_mode(soc_reg_obj);
	soc_reg_obj->retain_nol_across_regdmn_update = false;
	soc_reg_obj->is_ext_tpc_supported = false;
	soc_reg_obj->sta_sap_scc_on_indoor_channel = true;
	soc_reg_obj->set_fcc_channel = false;
	soc_reg_obj->p2p_indoor_ch_support = false;

	for (i = 0; i < MAX_STA_VDEV_CNT; i++)
		soc_reg_obj->vdev_ids_11d[i] = INVALID_VDEV_ID;

	qdf_spinlock_create(&soc_reg_obj->cbk_list_lock);

	for (phy_cnt = 0; phy_cnt < PSOC_MAX_PHY_REG_CAP; phy_cnt++) {
		mas_chan_list =
			soc_reg_obj->mas_chan_params[phy_cnt].mas_chan_list;
		soc_reg_obj->chan_list_recvd[phy_cnt] = false;

		for (chan_enum = 0; chan_enum < NUM_CHANNELS; chan_enum++) {
			mas_chan_list[chan_enum].chan_flags |=
				REGULATORY_CHAN_DISABLED;
			mas_chan_list[chan_enum].state = CHANNEL_STATE_DISABLE;
			mas_chan_list[chan_enum].nol_chan = false;
		}
	}

	status = wlan_objmgr_psoc_component_obj_attach(
			psoc, WLAN_UMAC_COMP_REGULATORY, soc_reg_obj,
			QDF_STATUS_SUCCESS);
	if (QDF_IS_STATUS_ERROR(status)) {
		qdf_spinlock_destroy(&soc_reg_obj->cbk_list_lock);
		qdf_mem_common_free(soc_reg_obj);
		reg_err("Obj attach failed");
		return status;
	}

	reg_debug("reg psoc obj created with status %d", status);
	qdf_minidump_log(soc_reg_obj, sizeof(*soc_reg_obj), "psoc_regulatory");

	return status;
}

QDF_STATUS wlan_regulatory_psoc_obj_destroyed_notification(
	struct wlan_objmgr_psoc *psoc, void *arg_list)
{
	QDF_STATUS status;
	struct wlan_regulatory_psoc_priv_obj *psoc_priv_obj;

	psoc_priv_obj = reg_get_psoc_obj(psoc);
	if (!psoc_priv_obj) {
		reg_err_rl("NULL reg psoc priv obj");
		return QDF_STATUS_E_FAULT;
	}

	psoc_priv_obj->psoc_ptr = NULL;
	qdf_spinlock_destroy(&psoc_priv_obj->cbk_list_lock);

	status = wlan_objmgr_psoc_component_obj_detach(
			psoc, WLAN_UMAC_COMP_REGULATORY, psoc_priv_obj);

	if (status != QDF_STATUS_SUCCESS)
		reg_err_rl("psoc_priv_obj private obj detach failed");

	reg_debug("reg psoc obj detached");
	qdf_minidump_remove(psoc_priv_obj, sizeof(*psoc_priv_obj),
			    "psoc_regulatory");

	qdf_mem_common_free(psoc_priv_obj);

	return status;
}

#ifdef DISABLE_UNII_SHARED_BANDS
/**
 * reg_reset_unii_5g_bitmap() - Reset the value of unii_5g_bitmap.
 * @pdev_priv_obj: pointer to wlan_regulatory_pdev_priv_obj.
 *
 * Return : void
 */
static void
reg_reset_unii_5g_bitmap(struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj)
{
	pdev_priv_obj->unii_5g_bitmap = 0x0;
}
#else
static void inline
reg_reset_unii_5g_bitmap(struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj)
{
}
#endif

#if defined(CONFIG_BAND_6GHZ)
#if defined(CONFIG_REG_CLIENT)
/**
 * reg_init_def_client_type() - Initialize the regulatory 6G client type.
 *
 * @pdev_priv_obj: pointer to wlan_regulatory_pdev_priv_obj.
 *
 * Return : void
 */
static void
reg_init_def_client_type(struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj)
{
	pdev_priv_obj->reg_cur_6g_client_mobility_type = REG_DEFAULT_CLIENT;
}
#else
static void
reg_init_def_client_type(struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj)
{
	pdev_priv_obj->reg_cur_6g_client_mobility_type = REG_SUBORDINATE_CLIENT;
}
#endif

/**
 * reg_init_6g_vars() - Initialize the regulatory 6G variables viz.
 * AP power type, client mobility type, rnr tpe usable and unspecified ap
 * usable.
 * @pdev_priv_obj: pointer to wlan_regulatory_pdev_priv_obj.
 *
 * Return : void
 */
static void
reg_init_6g_vars(struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj)
{
	reg_set_ap_pwr_type(pdev_priv_obj);
	pdev_priv_obj->reg_rnr_tpe_usable = false;
	pdev_priv_obj->reg_unspecified_ap_usable = false;
	reg_init_def_client_type(pdev_priv_obj);
}
#else
static void
reg_init_6g_vars(struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj)
{
}
#endif

#ifdef CONFIG_AFC_SUPPORT
static void
reg_create_afc_cb_spinlock(struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj)
{
	qdf_spinlock_create(&pdev_priv_obj->afc_cb_lock);
}

static void
reg_destroy_afc_cb_spinlock(struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj)
{
	qdf_spinlock_destroy(&pdev_priv_obj->afc_cb_lock);
}

static void
reg_init_afc_vars(struct wlan_regulatory_psoc_priv_obj *psoc_priv_obj,
		  struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj)
{
	pdev_priv_obj->is_reg_noaction_on_afc_pwr_evt =
			psoc_priv_obj->is_afc_reg_noaction;
}

static inline void
reg_set_pdev_afc_dev_type(struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj,
			  struct wlan_regulatory_psoc_priv_obj *psoc_priv_obj)
{
	pdev_priv_obj->reg_afc_dev_deployment_type =
		psoc_priv_obj->reg_afc_dev_type;
}
#else
static inline void
reg_create_afc_cb_spinlock(struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj)
{
}

static inline void
reg_destroy_afc_cb_spinlock(struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj)
{
}

static void
reg_init_afc_vars(struct wlan_regulatory_psoc_priv_obj *psoc_priv_obj,
		  struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj)
{
}

static inline void
reg_set_pdev_afc_dev_type(struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj,
			  struct wlan_regulatory_psoc_priv_obj *psoc_priv_obj)
{
}
#endif

QDF_STATUS wlan_regulatory_pdev_obj_created_notification(
	struct wlan_objmgr_pdev *pdev, void *arg_list)
{
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;
	struct wlan_regulatory_psoc_priv_obj *psoc_priv_obj;
	struct wlan_psoc_host_hal_reg_capabilities_ext *reg_cap_ptr;
	struct wlan_objmgr_psoc *parent_psoc;
	uint8_t pdev_id;
	uint8_t phy_id;
	uint32_t cnt;
	uint32_t range_2g_low, range_2g_high;
	uint32_t range_5g_low, range_5g_high;
	QDF_STATUS status;
	struct wlan_lmac_if_reg_tx_ops *tx_ops;
	enum direction dir;
	wlan_objmgr_ref_dbgid dbg_id;

	pdev_priv_obj = qdf_mem_common_alloc(sizeof(*pdev_priv_obj));
	if (!pdev_priv_obj)
		return QDF_STATUS_E_NOMEM;

	parent_psoc = wlan_pdev_get_psoc(pdev);
	pdev_id = wlan_objmgr_pdev_get_pdev_id(pdev);
	tx_ops = reg_get_psoc_tx_ops(parent_psoc);

	if (tx_ops->get_phy_id_from_pdev_id)
		tx_ops->get_phy_id_from_pdev_id(parent_psoc, pdev_id, &phy_id);
	else
		phy_id = pdev_id;

	psoc_priv_obj = reg_get_psoc_obj(parent_psoc);
	if (!psoc_priv_obj) {
		reg_err("reg psoc private obj is NULL");
		qdf_mem_common_free(pdev_priv_obj);
		return QDF_STATUS_E_FAULT;
	}

	pdev_priv_obj->pdev_ptr = pdev;
	pdev_priv_obj->dfs_enabled = psoc_priv_obj->dfs_enabled;
	pdev_priv_obj->set_fcc_channel = psoc_priv_obj->set_fcc_channel;
	pdev_priv_obj->band_capability = psoc_priv_obj->band_capability;
	pdev_priv_obj->indoor_chan_enabled =
		psoc_priv_obj->indoor_chan_enabled;
	reg_set_keep_6ghz_sta_cli_connection(pdev, false);

	reg_set_pdev_afc_dev_type(pdev_priv_obj, psoc_priv_obj);

	pdev_priv_obj->en_chan_144 = true;
	reg_reset_unii_5g_bitmap(pdev_priv_obj);

	qdf_spinlock_create(&pdev_priv_obj->reg_rules_lock);
	reg_create_afc_cb_spinlock(pdev_priv_obj);

	reg_cap_ptr = psoc_priv_obj->reg_cap;
	pdev_priv_obj->force_ssc_disable_indoor_channel =
		psoc_priv_obj->force_ssc_disable_indoor_channel;
	pdev_priv_obj->sta_sap_scc_on_indoor_channel =
		psoc_priv_obj->sta_sap_scc_on_indoor_channel;
	pdev_priv_obj->p2p_indoor_ch_support =
		psoc_priv_obj->p2p_indoor_ch_support;

	for (cnt = 0; cnt < PSOC_MAX_PHY_REG_CAP; cnt++) {
		if (!reg_cap_ptr) {
			qdf_mem_common_free(pdev_priv_obj);
			reg_err("reg cap ptr is NULL");
			return QDF_STATUS_E_FAULT;
		}

		if (reg_cap_ptr->phy_id == phy_id)
			break;
		reg_cap_ptr++;
	}

	if (cnt == PSOC_MAX_PHY_REG_CAP) {
		qdf_mem_common_free(pdev_priv_obj);
		reg_err("extended capabilities not found for pdev");
		return QDF_STATUS_E_FAULT;
	}

	range_2g_low = reg_cap_ptr->low_2ghz_chan;
	range_2g_high = reg_cap_ptr->high_2ghz_chan;
	range_5g_low = reg_cap_ptr->low_5ghz_chan;
	range_5g_high = reg_cap_ptr->high_5ghz_chan;

	pdev_priv_obj->range_2g_low = range_2g_low;
	pdev_priv_obj->range_2g_high = range_2g_high;
	pdev_priv_obj->range_5g_low = range_5g_low;
	pdev_priv_obj->range_5g_high = range_5g_high;
	pdev_priv_obj->wireless_modes = reg_cap_ptr->wireless_modes;
	reg_init_6g_vars(pdev_priv_obj);
	pdev_priv_obj->chan_list_recvd =
		psoc_priv_obj->chan_list_recvd[phy_id];

	status = wlan_objmgr_pdev_component_obj_attach(
			pdev, WLAN_UMAC_COMP_REGULATORY, pdev_priv_obj,
			QDF_STATUS_SUCCESS);
	if (QDF_IS_STATUS_ERROR(status)) {
		reg_err("Obj attach failed");
		qdf_mem_common_free(pdev_priv_obj);
		return status;
	}

	if (psoc_priv_obj->offload_enabled) {
		dbg_id = WLAN_REGULATORY_NB_ID;
		dir = NORTHBOUND;
	} else {
		dbg_id = WLAN_REGULATORY_SB_ID;
		dir = SOUTHBOUND;
	}

	wlan_objmgr_pdev_get_ref(pdev, dbg_id);
	reg_propagate_mas_chan_list_to_pdev(parent_psoc, pdev, &dir);
	wlan_objmgr_pdev_release_ref(pdev, dbg_id);

	reg_init_afc_vars(psoc_priv_obj, pdev_priv_obj);

	if (!psoc_priv_obj->is_11d_offloaded)
		reg_11d_host_scan_init(parent_psoc);

	reg_debug("reg pdev obj created with status %d", status);
	wlan_minidump_log(pdev_priv_obj, sizeof(*pdev_priv_obj), parent_psoc,
			  WLAN_MD_OBJMGR_PDEV_AFC_REG, "pdev_regulatory");

	return status;
}

#ifdef CONFIG_AFC_SUPPORT
/**
 * reg_free_chan_obj() - Free the AFC chan object and chan eirp object
 * information
 * @afc_chan_info: Pointer to afc_chan_info
 *
 * Return: void
 */
static void reg_free_chan_obj(struct afc_chan_obj *afc_chan_info)
{
	if (afc_chan_info->chan_eirp_info)
		qdf_mem_free(afc_chan_info->chan_eirp_info);
}

/**
 * reg_free_afc_pwr_info() - Free the AFC power information object
 * @pdev_priv_obj: Pointer to pdev_priv_obj
 *
 * Return: void
 */
void
reg_free_afc_pwr_info(struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj)
{
	struct reg_fw_afc_power_event *power_info;
	uint8_t i;

	power_info = pdev_priv_obj->power_info;
	if (!power_info)
		return;

	if (power_info->afc_freq_info)
		qdf_mem_free(power_info->afc_freq_info);

	if (!power_info->afc_chan_info)
		return;

	for (i = 0; i < power_info->num_chan_objs; i++)
		reg_free_chan_obj(&power_info->afc_chan_info[i]);

	if (power_info->afc_chan_info)
		qdf_mem_free(power_info->afc_chan_info);

	qdf_mem_free(power_info);
	pdev_priv_obj->power_info = NULL;
}
#endif

QDF_STATUS wlan_regulatory_pdev_obj_destroyed_notification(
		struct wlan_objmgr_pdev *pdev, void *arg_list)
{
	QDF_STATUS status;
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;
	struct wlan_regulatory_psoc_priv_obj *psoc_priv_obj;
	uint32_t pdev_id;
	struct wlan_objmgr_psoc *psoc;

	pdev_priv_obj = reg_get_pdev_obj(pdev);

	pdev_id = wlan_objmgr_pdev_get_pdev_id(pdev);

	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err("reg pdev private obj is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	psoc_priv_obj = reg_get_psoc_obj(wlan_pdev_get_psoc(pdev));
	if (!IS_VALID_PSOC_REG_OBJ(psoc_priv_obj)) {
		reg_err("reg psoc private obj is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	if (!psoc_priv_obj->is_11d_offloaded)
		reg_11d_host_scan_deinit(wlan_pdev_get_psoc(pdev));

	psoc = wlan_pdev_get_psoc(pdev);
	wlan_minidump_remove(pdev_priv_obj, sizeof(*pdev_priv_obj),
			     psoc, WLAN_MD_OBJMGR_PDEV_AFC_REG,
			     "pdev_regulatory");

	reg_free_afc_pwr_info(pdev_priv_obj);
	pdev_priv_obj->pdev_ptr = NULL;

	status = wlan_objmgr_pdev_component_obj_detach(
			pdev, WLAN_UMAC_COMP_REGULATORY, pdev_priv_obj);

	if (status != QDF_STATUS_SUCCESS)
		reg_err("reg pdev private obj detach failed");

	reg_debug("reg pdev obj deleted");

	qdf_spin_lock_bh(&pdev_priv_obj->reg_rules_lock);
	reg_reset_reg_rules(&pdev_priv_obj->reg_rules);
	qdf_spin_unlock_bh(&pdev_priv_obj->reg_rules_lock);

	reg_destroy_afc_cb_spinlock(pdev_priv_obj);
	qdf_spinlock_destroy(&pdev_priv_obj->reg_rules_lock);

	qdf_mem_common_free(pdev_priv_obj);

	return status;
}
