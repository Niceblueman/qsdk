/* Copyright (c) 2015-2018, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/firmware.h>
#include <linux/module.h>
#include <linux/ktime.h>
#include <linux/mutex.h>
#include <linux/soc/qcom/qmi.h>
#include <linux/of.h>

#include "bus/bus.h"
#ifdef CNSS_DEBUG_SUPPORT
#include "debug/debug.h"
#endif
#include "../main.h"
#include "qmi/qmi.h"
#if defined CNSS_PCI_SUPPORT
#include "pci/pci.h"
#endif
#include "cnss_common/cnss_common.h"
#include "qmi/cnss_plat_ipc_qmi.h"
#include "genl/genl.h"

#define WLFW_SERVICE_INS_ID_V01		1
#define WLFW_CLIENT_ID			0x4b4e454c
#define MAX_BDF_FILE_NAME		64
#define BDF_FILE_NAME_PREFIX		"bdwlan"
#define ELF_BDF_FILE_NAME		"bdwlan.elf"
#define ELF_BDF_FILE_NAME_PREFIX	"bdwlan.e"
#define BIN_BDF_FILE_NAME		"bdwlan.bin"
#define BIN_BDF_FILE_NAME_PREFIX	"bdwlan.b"
#define DEFAULT_BDF_FILE_NAME		"bdwlan.bin"
#define BDF_WIN_FILE_NAME_PREFIX	"bdwlan.b"
#define DEFAULT_REGDB_FILE_NAME		"regdb.bin"
#define REGDB_WIN_FILE_NAME_PREFIX	"regdb.b"
#define DUMMY_BDF_FILE_NAME		"bdwlan.dmy"
#define HDS_FILE_NAME			"hds.bin"
#define FW_INI_CFG_FILE_NAME		"fw_ini_cfg.bin"
#define DEFAULT_RXGAINLUT_FILE_NAME	"rxgainlut.bin"
#define RXGAINLUT_WIN_FILE_NAME_PREFIX	"rxgainlut.b"
#define FW_INI_FILE_NAME_LEN		100

#define DEFAULT_CAL_FILE_NAME		"caldata.bin"
#define CAL_FILE_NAME_PREFIX		"caldata.b"
#define DEFAULT_CAL_FILE_PREFIX         "caldata_"
#define DEFAULT_CAL_FILE_SUFFIX         ".bin"
#define FTM_CONF_FILE_PATH		"/tmp/ftm.conf"

#define QMI_MSG_REQ_STR			"REQ"
#define QMI_MSG_RESP_STR		"RESP"
#define QMI_MSG_IND_STR			"IND"
#define QMI_MSG_INVALID			"INVL"

/* Bit 8 -> 0 -> TYPE IND
 * Bit 8 -> 1 -> TYPE REQ
 * Bit 9 -> 1 -> TYPE RESP
 */
#define QMI_TYPE_IND			0x0
#define QMI_TYPE_REQ			0x100
#define QMI_TYPE_RESP			0x200

#define QMI_WLFW_TIMEOUT_MS		(plat_priv->ctrl_params.qmi_timeout)

#define QMI_WLFW_TIMEOUT_JF		msecs_to_jiffies(QMI_WLFW_TIMEOUT_MS)
#define COEX_TIMEOUT			QMI_WLFW_TIMEOUT_JF
#define IMS_TIMEOUT                     QMI_WLFW_TIMEOUT_JF

#define QMI_WLFW_MAX_RECV_BUF_SIZE	SZ_8K

#define MAX_QDSS_CONFIG_FILE_NAME	64
#define QDSS_CONFIG_FILE_PREFIX		"qdss_trace_config"
#define QDSS_CONFIG_FILE_SUFFIX		".bin"

unsigned int pci_fw_mem_mode = 0xFF;
module_param(pci_fw_mem_mode, uint, 0600);
MODULE_PARM_DESC(pci_fw_mem_mode, "pci_fw_mem_mode");

unsigned int qca8074_fw_mem_mode = 0xFF;
module_param(qca8074_fw_mem_mode, uint, 0600);
MODULE_PARM_DESC(qca8074_fw_mem_mode, "qca8074_fw_mem_mode");

unsigned int num_wlan_clients;
module_param(num_wlan_clients, uint, 0600);
MODULE_PARM_DESC(num_wlan_clients, "num_wlan_clients");

unsigned int num_wlan_vaps;
module_param(num_wlan_vaps, uint, 0600);
MODULE_PARM_DESC(num_wlan_vaps, "num_wlan_vaps");

struct qmi_history qmi_log[QMI_HISTORY_SIZE];
int qmi_history_index;
DEFINE_SPINLOCK(qmi_log_spinlock);
struct wlfw_request_mem_ind_msg_v01 ind_message = {0};

struct device_name_string {
	const char *device_name;
	u16 instance_id;
};

struct qmi_msg_string {
	char *qmi_id_str;
	u16 qmi_msg_id;
};

static struct device_name_string device_name_table[] = {
	{ "QCA8074", QCA8074_DEVICE_ID },
	{ "QCA8074V2", QCA8074V2_DEVICE_ID },
	{ "QCA6018", QCA6018_DEVICE_ID },
	{ "QCA5018", QCA5018_DEVICE_ID },
	{ "QCA9574", QCA9574_DEVICE_ID },
	{ "QCA5332", QCA5332_DEVICE_ID },
	{ "QCN9000_0", QCN9000_0+FW_ID_BASE },
	{ "QCN9000_1", QCN9000_1+FW_ID_BASE },
	{ "QCN9000_2", QCN9000_2+FW_ID_BASE },
	{ "QCN9000_3", QCN9000_3+FW_ID_BASE },
	{ "QCN9224_0", QCN9224_0+FW_ID_BASE },
	{ "QCN9224_1", QCN9224_1+FW_ID_BASE },
	{ "QCN9224_2", QCN9224_2+FW_ID_BASE },
	{ "QCN9224_3", QCN9224_3+FW_ID_BASE },
	{ "QCN6122_0", USERPD_0+WLFW_SERVICE_INS_ID_V01_QCN6122 },
	{ "QCN6122_1", USERPD_1+WLFW_SERVICE_INS_ID_V01_QCN6122 },
	{ "QCN9160_0", USERPD_0+WLFW_SERVICE_INS_ID_V01_QCN9160 },
	{ "QCN9160_1", USERPD_1+WLFW_SERVICE_INS_ID_V01_QCN9160 },
	{ "QCN9160_2", USERPD_2+WLFW_SERVICE_INS_ID_V01_QCN9160 },
	{ "QCN6432_0", USERPD_0+WLFW_SERVICE_INS_ID_V01_QCN6432 },
	{ "QCN6432_1", USERPD_1+WLFW_SERVICE_INS_ID_V01_QCN6432 },
	{ "QCA5424", QCA5424_DEVICE_ID },
	{ "UNKNOWN", 0 },
};

static struct qmi_msg_string qmi_str_table[] = {
	{ "SYS_RSTRT_", QMI_WLFW_SUBSYS_RESTART_LEVEL_REQ_V01 },
	{ "TGT_CAP_", QMI_WLFW_CAP_REQ_V01 },
	{ "CAL_RPRT_", QMI_WLFW_CAL_REPORT_REQ_V01 },
	{ "IND_REG_", QMI_WLFW_IND_REGISTER_REQ_V01 },
	{ "DYN_MASK_", QMI_WLFW_DYNAMIC_FEATURE_MASK_REQ_V01 },
	{ "AUX_UC_INFO_", QMI_WLFW_AUX_UC_INFO_REQ_V01 },
	{ "FW_RDY_", QMI_WLFW_FW_READY_IND_V01 },
	{ "CAL_UPDTE_", QMI_WLFW_CAL_UPDATE_REQ_V01 },
	{ "PHY_CAP_", QMI_WLFW_PHY_CAP_REQ_V01 },
	{ "FW_MEM_REQ_", QMI_WLFW_REQUEST_MEM_IND_V01 },
	{ "QDSS_MODE_", QMI_WLFW_QDSS_TRACE_MODE_REQ_V01 },
	{ "CAL_DNDL_", QMI_WLFW_CAL_DOWNLOAD_REQ_V01 },
	{ "M3_INFO_", QMI_WLFW_M3_INFO_REQ_V01 },
	{ "PCI_GEN_SWT_", QMI_WLFW_PCIE_GEN_SWITCH_REQ_V01 },
	{ "CAL_UPDTE_", QMI_WLFW_INITIATE_CAL_UPDATE_IND_V01 },
	{ "MEM_INFO_", QMI_WLFW_RESPOND_MEM_REQ_V01 },
	{ "MSA_RDY_", QMI_WLFW_MSA_READY_IND_V01 },
	{ "WLFW_MODE_", QMI_WLFW_WLAN_MODE_REQ_V01 },
	{ "REJUVNTE_", QMI_WLFW_REJUVENATE_IND_V01 },
	{ "ATHD_WRT_", QMI_WLFW_ATHDIAG_WRITE_REQ_V01 },
	{ "SOC_WAKE_", QMI_WLFW_SOC_WAKE_REQ_V01 },
	{ "FW_PIN_RSLT_", QMI_WLFW_PIN_CONNECT_RESULT_IND_V01 },
	{ "QDSS_SAVE_", QMI_WLFW_QDSS_TRACE_SAVE_IND_V01 },
	{ "SHUTDWN_", QMI_WLFW_SHUTDOWN_REQ_V01 },
	{ "VBATT_", QMI_WLFW_VBATT_REQ_V01 },
	{ "PCI_LNK_CTRL_", QMI_WLFW_PCIE_LINK_CTRL_REQ_V01 },
	{ "MAC_ADDR_", QMI_WLFW_MAC_ADDR_REQ_V01 },
	{ "WLAN_CFG_", QMI_WLFW_WLAN_CFG_REQ_V01 },
	{ "ANT_GRNT_", QMI_WLFW_ANTENNA_GRANT_REQ_V01 },
	{ "BDF_DNLD_", QMI_WLFW_BDF_DOWNLOAD_REQ_V01 },
	{ "FW_MEM_RDY_", QMI_WLFW_FW_MEM_READY_IND_V01 },
	{ "HW_INIT_CFG_", QMI_WLFW_WLAN_HW_INIT_CFG_REQ_V01 },
	{ "RSPND_GET_INF_", QMI_WLFW_RESPOND_GET_INFO_IND_V01 },
	{ "QDSS_TRACE_", QMI_WLFW_QDSS_TRACE_DATA_REQ_V01 },
	{ "QDSS_MEM_INFO_", QMI_WLFW_QDSS_TRACE_MEM_INFO_REQ_V01 },
	{ "ANT_SWT_", QMI_WLFW_ANTENNA_SWITCH_REQ_V01 },
	{ "QDSS_REQ_MEM_", QMI_WLFW_QDSS_TRACE_REQ_MEM_IND_V01 },
	{ "INIT_CAL_DNLD_", QMI_WLFW_INITIATE_CAL_DOWNLOAD_IND_V01 },
	{ "INI_MSG_", QMI_WLFW_INI_REQ_V01 },
	{ "M3_DMP_UPLD_", QMI_WLFW_M3_DUMP_UPLOAD_SEGMENTS_REQ_IND_V01 },
	{ "MSD_RDY_", QMI_WLFW_MSA_READY_REQ_V01 },
	{ "M3_DMP_UPLD_DONE_", QMI_WLFW_M3_DUMP_UPLOAD_DONE_REQ_V01 },
	{ "REJUVNTE_ACK_", QMI_WLFW_REJUVENATE_ACK_REQ_V01 },
	{ "DEV_INF_", QMI_WLFW_DEVICE_INFO_REQ_V01 },
	{ "MSA_INF_", QMI_WLFW_MSA_INFO_REQ_V01 },
	{ "HOST_CAP_", QMI_WLFW_HOST_CAP_REQ_V01 },
	{ "QDSS_CONF_DNLD_", QMI_WLFW_QDSS_TRACE_CONFIG_DOWNLOAD_REQ_V01 },
	{ "GET_INFO_", QMI_WLFW_GET_INFO_REQ_V01 },
	{ "CAL_DONE_", QMI_WLFW_CAL_DONE_IND_V01 },
	{ "M3_DUMP_UPLD_REQ_", QMI_WLFW_M3_DUMP_UPLOAD_REQ_IND_V01 },
	{ "FW_INIT_DONE_", QMI_WLFW_FW_INIT_DONE_IND_V01 },
	{ "PWR_SAVE_", QMI_WLFW_POWER_SAVE_REQ_V01 },
	{ "X0_CAL_", QMI_WLFW_XO_CAL_IND_V01 },
	{ "ATHD_READ_", QMI_WLFW_ATHDIAG_READ_REQ_V01 },
	{ "WFC_CALL_TWT_", QMI_WLFW_WFC_CALL_TWT_CONFIG_IND_V01 },
	{ "WFC_CALL_STAT_", QMI_WLFW_WFC_CALL_STATUS_REQ_V01 },
	{ "INI_DNLD_", QMI_WLFW_INI_FILE_DOWNLOAD_REQ_V01 },
	{ "QDSS_FREE_", QMI_WLFW_QDSS_TRACE_FREE_IND_V01 },
	{ "QDSS_MEM_RDY_", QMI_WLFW_QDSS_MEM_READY_IND_V01 },
	{ "MLO_WSI_REMAP_", QMI_WLFW_MLO_RECONFIG_INFO_REQ_V01 },
	{ "UNKNOWN_", 0 },
};

const char *get_device_name_from_ins_id(u8 instance_id)
{
	struct device_name_string *table = device_name_table;
	struct cnss_plat_data *plat_priv = NULL;
	u16 ahb_instance_id = 0;

	if (instance_id == WLFW_SERVICE_INS_ID_V01_QCA8074) {
		plat_priv = cnss_get_plat_priv_by_instance_id(instance_id);
		ahb_instance_id = plat_priv->device_id;
		while (table->instance_id != 0) {
			if (ahb_instance_id == table->instance_id)
				break;
			table++;
		}
	} else {
		while (table->instance_id != 0) {
			if (instance_id == table->instance_id)
				break;
			table++;
		}
	}
	return table->device_name;
}

const char *get_msg_name_from_qmi_msg_id(u16 msg_id)
{
	struct qmi_msg_string *table = qmi_str_table;

	while (table->qmi_msg_id != 0) {
		if (msg_id == table->qmi_msg_id)
			break;
		table++;
	}
	return table->qmi_id_str;
}

void cnss_dump_qmi_history(void)
{
	int i;

	pr_err("qmi_history_index [%d]\n", ((qmi_history_index - 1) &
		(QMI_HISTORY_SIZE - 1)));
	for (i = 0; i < QMI_HISTORY_SIZE; i++) {
		if (qmi_log[i].msg_id)
			pr_err(
			"qmi_history[%d]:tstamp[%llu] ins_id [0x%x : %s] msg_id [0x%x : %s%s] err[%d] resp_err[%d]\n",
			i, qmi_log[i].timestamp,
			qmi_log[i].instance_id,
			get_device_name_from_ins_id(qmi_log[i].instance_id),
			qmi_log[i].msg_id,
			get_msg_name_from_qmi_msg_id(qmi_log[i].msg_id),
			qmi_log[i].msg_type,
			qmi_log[i].error_msg,
			qmi_log[i].resp_err_msg);
	}
}
EXPORT_SYMBOL(cnss_dump_qmi_history);

void qmi_record(u8 instance_id, u16 msg_id, s8 error_msg, s8 resp_err_msg)
{
	int qmi_msg_type = 0;

	spin_lock(&qmi_log_spinlock);
	qmi_log[qmi_history_index].instance_id = instance_id;

	qmi_msg_type = (msg_id & (QMI_TYPE_REQ | QMI_TYPE_RESP));
	msg_id = (msg_id & 0xFF);
	qmi_log[qmi_history_index].msg_id = msg_id;

	if (resp_err_msg != 0 || qmi_msg_type == QMI_TYPE_RESP)
		strlcpy(qmi_log[qmi_history_index].msg_type,
			QMI_MSG_RESP_STR, (sizeof(QMI_MSG_RESP_STR)));
	else if (error_msg != 0 || qmi_msg_type == QMI_TYPE_REQ)
		strlcpy(qmi_log[qmi_history_index].msg_type,
			QMI_MSG_REQ_STR, (sizeof(QMI_MSG_REQ_STR)));
	else if (qmi_msg_type == QMI_TYPE_IND)
		strlcpy(qmi_log[qmi_history_index].msg_type,
			QMI_MSG_IND_STR, (sizeof(QMI_MSG_IND_STR)));
	else
		strlcpy(qmi_log[qmi_history_index].msg_type,
			QMI_MSG_INVALID, (sizeof(QMI_MSG_INVALID)));

	if (error_msg < 0 || resp_err_msg != 0)
		qmi_log[qmi_history_index].error_msg = error_msg;
	else
		qmi_log[qmi_history_index].error_msg = 0;

	qmi_log[qmi_history_index].resp_err_msg = resp_err_msg;
	qmi_log[qmi_history_index++].timestamp = ktime_to_ms(ktime_get());

	qmi_history_index &= (QMI_HISTORY_SIZE - 1);
	spin_unlock(&qmi_log_spinlock);
}

static char *cnss_qmi_mode_to_str(enum cnss_driver_mode mode)
{
	switch (mode) {
	case CNSS_MISSION:
		return "MISSION";
	case CNSS_FTM:
		return "FTM";
	case CNSS_EPPING:
		return "EPPING";
	case CNSS_WALTEST:
		return "WALTEST";
	case CNSS_OFF:
		return "OFF";
	case CNSS_CCPM:
		return "CCPM";
	case CNSS_QVIT:
		return "QVIT";
	case CNSS_CALIBRATION:
		return "COLDBOOT CALIBRATION";
	case QMI_WLFW_FTM_CALIBRATION_V01:
		return "FTM COLDBOOT CALIBRATION";
	default:
		return "UNKNOWN";
	}
};

static bool cnss_check_path_exists(const char *path)
{
	struct file *filp = NULL;
	struct cnss_plat_data *plat_priv = NULL;

	if (!path)
		return false;

	filp = filp_open(path, O_RDONLY, 00644);
	if (IS_ERR(filp)) {
		cnss_pr_err("Path %s doesn't exist", path);
		return false;
	}

	filp_close(filp, NULL);
	return true;
}

static int cnss_wlfw_ind_register_send_sync(struct cnss_plat_data *plat_priv)
{
	struct wlfw_ind_register_req_msg_v01 *req;
	struct wlfw_ind_register_resp_msg_v01 *resp;
	struct qmi_txn txn;
	int ret = 0;
	int resp_error_msg = 0;

	cnss_pr_dbg("Sending indication register message, state: 0x%lx\n",
		    plat_priv->driver_state);

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	resp = kzalloc(sizeof(*resp), GFP_KERNEL);
	if (!resp) {
		kfree(req);
		return -ENOMEM;
	}

	req->client_id_valid = 1;
	req->client_id = WLFW_CLIENT_ID;
	req->fw_ready_enable_valid = 1;
	req->fw_ready_enable = 1;
	req->request_mem_enable_valid = 1;
	req->request_mem_enable = 1;
	req->fw_mem_ready_enable_valid = 1;
	req->fw_mem_ready_enable = 1;
	req->fw_init_done_enable_valid = 1;
	req->fw_init_done_enable = 1;
	req->pin_connect_result_enable_valid = 0;
	req->pin_connect_result_enable = 0;
	req->cal_done_enable_valid = 1;
	req->cal_done_enable = 1;
	req->qdss_trace_req_mem_enable_valid = 1;
	req->qdss_trace_req_mem_enable = 1;
	req->qdss_trace_save_enable_valid = 1;
	req->qdss_trace_save_enable = 1;
	req->qdss_trace_free_enable_valid = 1;
	req->qdss_trace_free_enable = 1;
	req->m3_dump_upload_req_enable_valid = 1;
	req->m3_dump_upload_req_enable = 1;
	req->qdss_mem_ready_enable_valid = 1;
	req->qdss_mem_ready_enable = 1;

	qmi_record(plat_priv->wlfw_service_instance_id,
		   (QMI_TYPE_REQ | QMI_WLFW_IND_REGISTER_REQ_V01), ret,
		   resp_error_msg);
	ret = qmi_txn_init(&plat_priv->qmi_wlfw, &txn,
			   wlfw_ind_register_resp_msg_v01_ei, resp);
	if (ret < 0) {
		cnss_pr_err("Failed to initialize txn for indication register request, err: %d\n",
			    ret);
		goto out;
	}

	ret = qmi_send_request(&plat_priv->qmi_wlfw, NULL, &txn,
			       QMI_WLFW_IND_REGISTER_REQ_V01,
			       WLFW_IND_REGISTER_REQ_MSG_V01_MAX_MSG_LEN,
			       wlfw_ind_register_req_msg_v01_ei, req);
	if (ret < 0) {
		qmi_txn_cancel(&txn);
		cnss_pr_err("Failed to send indication register request, err: %d\n",
			    ret);
		goto out;
	}

	ret = qmi_txn_wait(&txn, QMI_WLFW_TIMEOUT_JF);
	if (ret < 0) {
		resp_error_msg = -QMI_RESULT_FAILURE_V01;
		cnss_pr_err("Failed to wait for response of indication register request, err: %d\n",
			    ret);
		goto out;
	}

	if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
		cnss_pr_err("Indication register request failed, result: %d, err: %d\n",
			    resp->resp.result, resp->resp.error);
		ret = -resp->resp.result;
		resp_error_msg = resp->resp.error;
		goto out;
	}

	if (resp->fw_status_valid) {
		if (resp->fw_status & QMI_WLFW_ALREADY_REGISTERED_V01) {
			ret = -EALREADY;
			goto qmi_registered;
		}
	}

	kfree(req);
	kfree(resp);
	qmi_record(plat_priv->wlfw_service_instance_id,
		   (QMI_TYPE_RESP | QMI_WLFW_IND_REGISTER_REQ_V01), ret,
		   resp_error_msg);
	return 0;
out:
	qmi_record(plat_priv->wlfw_service_instance_id,
		   (QMI_WLFW_IND_REGISTER_REQ_V01), ret,
		   resp_error_msg);
	CNSS_ASSERT(0);

qmi_registered:
	kfree(req);
	kfree(resp);
	return ret;
}

static int cnss_wlfw_ini_file_send_sync(struct cnss_plat_data *plat_priv,
					enum wlfw_ini_file_type_v01 file_type)
{
	struct wlfw_ini_file_download_req_msg_v01 *req;
	struct wlfw_ini_file_download_resp_msg_v01 *resp;
	struct qmi_txn txn;
	int ret = 0;
	int resp_error_msg = 0;
	const struct firmware *fw = NULL;
	char filename[FW_INI_FILE_NAME_LEN] = {0};
	const u8 *temp;
	unsigned int remaining;

	cnss_pr_info("FW File %u download\n", file_type);

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	resp = kzalloc(sizeof(*resp), GFP_KERNEL);
	if (!resp) {
		kfree(req);
		return -ENOMEM;
	}
	switch (file_type) {
	case WLFW_INI_CFG_FILE_V01:
		snprintf(filename, sizeof(filename), "%s" FW_INI_CFG_FILE_NAME,
			 cnss_get_fw_path(plat_priv));
		break;
	default:
		cnss_pr_err("Invalid file type: %u\n", file_type);
		ret = -EINVAL;
		goto err;
	}

	/* Fetch the file */
	ret = request_firmware_direct(&fw, filename, &plat_priv->plat_dev->dev);
	if (ret) {
		cnss_pr_err("Failed to get FW file %s (%d)",
			    filename, ret);
		goto err;
	}

	temp = fw->data;
	remaining = fw->size;
	qmi_record(plat_priv->wlfw_service_instance_id,
		  (QMI_TYPE_REQ | QMI_WLFW_INI_FILE_DOWNLOAD_REQ_V01), ret,
		  resp_error_msg);

	while (remaining) {
		req->file_type_valid = 1;
		req->file_type = file_type;
		req->total_size_valid = 1;
		req->total_size = remaining;
		req->seg_id_valid = 1;
		req->data_valid = 1;
		req->end_valid = 1;

		if (remaining > QMI_WLFW_MAX_DATA_SIZE_V01) {
			req->data_len = QMI_WLFW_MAX_DATA_SIZE_V01;
		} else {
			req->data_len = remaining;
			req->end = 1;
		}

		memcpy(req->data, temp, req->data_len);

		ret = qmi_txn_init(&plat_priv->qmi_wlfw, &txn,
				   wlfw_ini_file_download_resp_msg_v01_ei,
				   resp);
		if (ret < 0) {
			cnss_pr_err("Failed to initialize txn for FW file download request, err: %d\n",
				    ret);
			goto err;
		}

		ret = qmi_send_request
			(&plat_priv->qmi_wlfw, NULL, &txn,
			 QMI_WLFW_INI_FILE_DOWNLOAD_REQ_V01,
			 WLFW_INI_FILE_DOWNLOAD_REQ_MSG_V01_MAX_MSG_LEN,
			 wlfw_ini_file_download_req_msg_v01_ei, req);
		if (ret < 0) {
			qmi_txn_cancel(&txn);
			cnss_pr_err("Failed to send FW File download request, err: %d\n",
				    ret);
			goto err;
		}

		ret = qmi_txn_wait(&txn, QMI_WLFW_TIMEOUT_JF);
		if (ret < 0) {
			resp_error_msg = -QMI_RESULT_FAILURE_V01;
			cnss_pr_err("Failed to wait for response of FW File download request, err: %d\n",
				    ret);
			goto err;
		}

		if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
			cnss_pr_err("FW file download request failed, result: %d, err: %d\n",
				    resp->resp.result, resp->resp.error);
			ret = -resp->resp.result;
			resp_error_msg = resp->resp.error;
			goto err;
		}

		remaining -= req->data_len;
		temp += req->data_len;
		req->seg_id++;
	}

	release_firmware(fw);

	qmi_record(plat_priv->wlfw_service_instance_id,
		  (QMI_TYPE_RESP | QMI_WLFW_INI_FILE_DOWNLOAD_REQ_V01), ret,
		  resp_error_msg);
	kfree(req);
	kfree(resp);
	return 0;

err:
	if (fw)
		release_firmware(fw);

	kfree(req);
	kfree(resp);
	qmi_record(plat_priv->wlfw_service_instance_id,
		  (QMI_WLFW_INI_FILE_DOWNLOAD_REQ_V01), ret,
		  resp_error_msg);
	CNSS_ASSERT(0);

	return ret;
}

static void cnss_mlo_config_fill_req(
				struct cnss_plat_data *plat_priv,
				struct mlo_chip_v2_info_s_v01 *v2, int i)
{
	struct cnss_mlo_chip_info *mlo_chip_info;
	struct cnss_plat_data *adj_plat_priv = NULL;
	struct cnss_mlo_chip_info *adj_ch_info;
	struct mlo_chip_info_s_v01 *adj_ch;
	int ch_idx = 0, local_links = 0;
	int j, k;

	mlo_chip_info = &plat_priv->mlo_group_info->chip_info[i];

	v2->mlo_chip_info.chip_id = mlo_chip_info->chip_id;
	v2->mlo_chip_info.num_local_links = mlo_chip_info->num_local_links;

	for (j = 0; j < CNSS_MAX_LINKS_PER_CHIP; j++) {
		v2->mlo_chip_info.hw_link_id[j] = mlo_chip_info->hw_link_ids[j];
		v2->mlo_chip_info.valid_mlo_link_id[j] =
					mlo_chip_info->valid_link_ids[j];
	}
	v2->adj_mlo_num_chips = mlo_chip_info->num_adj_chips;

	for (j = 0; j < v2->adj_mlo_num_chips; j++) {
		ch_idx = mlo_chip_info->adj_chip_ids[j];
		adj_plat_priv = cnss_get_plat_priv_by_chip_id(ch_idx);
		if (adj_plat_priv)
			adj_ch_info = adj_plat_priv->mlo_chip_info;
		else
			continue;

		adj_ch = &v2->adj_mlo_chip_info[j];
		adj_ch->chip_id = adj_ch_info->chip_id;
		adj_ch->num_local_links = adj_ch_info->num_local_links;

		local_links = adj_ch->num_local_links;
		for (k = 0; k < local_links; k++) {
			adj_ch->hw_link_id[k] = adj_ch_info->hw_link_ids[k];
			adj_ch->valid_mlo_link_id[k] =
						adj_ch_info->valid_link_ids[k];
		}
	}
}

int cnss_wlfw_mlo_wsi_remap_send_sync(struct cnss_plat_data *plat_priv)
{
	struct wlfw_mlo_reconfig_info_req_msg_v01 *req;
	struct wlfw_mlo_reconfig_info_resp_msg_v01 *resp;
	struct mlo_chip_v2_info_s_v01 *v2;
	struct qmi_txn txn;
	int ret = 0, i;
	int resp_error_msg = 0;

	cnss_pr_dbg("Sending MLO Reconfig message, state: 0x%lx\n",
		    plat_priv->driver_state);

	if (!plat_priv)
		return -ENODEV;

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	resp = kzalloc(sizeof(*resp), GFP_KERNEL);
	if (!resp) {
		kfree(req);
		return -ENOMEM;
	}

	req->mlo_capable_valid = 1;
	req->mlo_capable = 1;

	req->mlo_chip_id = plat_priv->mlo_chip_info->chip_id;
	req->mlo_chip_id_valid = 1;

	req->mlo_group_id = plat_priv->mlo_group_info->group_id;
	req->mlo_group_id_valid = 1;

	req->max_mlo_peer_valid = 1;
	req->max_mlo_peer = plat_priv->mlo_group_info->max_num_peers;

	req->mlo_num_chips_valid = 1;
	req->mlo_num_chips = plat_priv->mlo_group_info->num_chips;

	req->mlo_chip_info_valid = 0;
	req->mlo_chip_v2_info_valid = 1;
	for (i = 0; i < req->mlo_num_chips; i++) {
		v2 = &req->mlo_chip_v2_info[i];
		cnss_mlo_config_fill_req(plat_priv, v2, i);
	}

	qmi_record(plat_priv->wlfw_service_instance_id,
		  (QMI_TYPE_REQ | QMI_WLFW_MLO_RECONFIG_INFO_REQ_V01), ret,
		  resp_error_msg);

	ret = qmi_txn_init(&plat_priv->qmi_wlfw, &txn,
			   wlfw_mlo_reconfig_info_resp_msg_v01_ei, resp);
	if (ret < 0) {
		cnss_pr_err("Failed to initialize txn for MLO Reconfig request, err: %d\n",
			    ret);
		goto out;
	}

	ret = qmi_send_request(&plat_priv->qmi_wlfw, NULL, &txn,
			       QMI_WLFW_MLO_RECONFIG_INFO_REQ_V01,
			       WLFW_MLO_RECONFIG_INFO_REQ_MSG_V01_MAX_MSG_LEN,
			       wlfw_mlo_reconfig_info_req_msg_v01_ei, req);
	if (ret < 0) {
		qmi_txn_cancel(&txn);
		cnss_pr_err("Failed to send MLO Reconfig request, err: %d\n",
			    ret);
		goto out;
	}

	ret = qmi_txn_wait(&txn, QMI_WLFW_TIMEOUT_JF);
	if (ret < 0) {
		resp_error_msg = -QMI_RESULT_FAILURE_V01;
		cnss_pr_err("Failed to wait for response of MLO Reconfig request, err: %d\n",
			    ret);
		goto out;
	}

	if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
		cnss_pr_err("MLO Reconfig request failed, result: %d, err: %d\n",
			    resp->resp.result, resp->resp.error);
		ret = -resp->resp.result;
		resp_error_msg = resp->resp.error;
		goto out;
	}
	qmi_record(plat_priv->wlfw_service_instance_id,
		  (QMI_TYPE_RESP | QMI_WLFW_MLO_RECONFIG_INFO_RESP_V01), ret,
		  resp_error_msg);

	kfree(req);
	kfree(resp);
	return 0;

out:
	qmi_record(plat_priv->wlfw_service_instance_id,
		  (QMI_WLFW_MLO_RECONFIG_INFO_REQ_V01), ret,
		  resp_error_msg);
	CNSS_ASSERT(0);
	kfree(req);
	kfree(resp);
	return ret;
}

static int cnss_wlfw_host_cap_send_sync(struct cnss_plat_data *plat_priv)
{
	struct wlfw_host_cap_req_msg_v01 *req;
	struct wlfw_host_cap_resp_msg_v01 *resp;
	struct mlo_chip_v2_info_s_v01 *v2;
	struct qmi_txn txn;
	int ret = 0, i;
	int resp_error_msg = 0;
	const char *model = NULL;
	struct device_node *root;
	struct device *dev = &plat_priv->plat_dev->dev;
	const struct firmware *fw;
	char filename[FW_INI_FILE_NAME_LEN] = {0};

	cnss_pr_dbg("Sending host capability message, state: 0x%lx\n",
		    plat_priv->driver_state);

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	resp = kzalloc(sizeof(*resp), GFP_KERNEL);
	if (!resp) {
		kfree(req);
		return -ENOMEM;
	}

	/* CNSS2 is the only client to FW as per the single QMI client model.
	 * If cnss-daemon support is present, then it will be QMI client to
	 * CNSS2.
	 */
	req->num_clients_valid = 1;
	req->num_clients = 1;
	cnss_pr_dbg("Number of QMI clients are %d\n", req->num_clients);

	/* Check whether FW INI CFG file is present or not */
	snprintf(filename, sizeof(filename), "%s" FW_INI_CFG_FILE_NAME,
		 cnss_get_fw_path(plat_priv));
	ret = request_firmware_direct(&fw, filename, &plat_priv->plat_dev->dev);
	if (!ret) {
		/* File is present, set the corresponding flag */
		cnss_pr_info("FW INI CFG file %s is present\n", filename);
		req->fw_ini_cfg_support_valid = 1;
		req->fw_ini_cfg_support = 1;
	}

	plat_priv->fw_ini_cfg_support = !!req->fw_ini_cfg_support;
	release_firmware(fw);

	req->mem_cfg_mode = plat_priv->tgt_mem_cfg_mode;
	req->mem_cfg_mode_valid = 1;
	cnss_pr_info("device_id : 0x%lx mem mode : [%d]\n",
		     plat_priv->device_id,
		     plat_priv->tgt_mem_cfg_mode);

	req->bdf_support_valid = 1;
	req->bdf_support = 1;

	req->m3_support_valid = 0;
	req->m3_support = 0;

	req->m3_cache_support_valid = 0;
	req->m3_cache_support = 0;

	req->cal_done_valid = 1;
	req->cal_done = plat_priv->cal_done;
	cnss_pr_dbg("Calibration done is %d\n", plat_priv->cal_done);

	root = of_find_node_by_path("/");
	if (root) {
		model = of_get_property(root, "model", NULL);
		if (model) {
			req->platform_name_valid = 1;
			strlcpy(req->platform_name, model,
				QMI_WLFW_MAX_PLATFORM_NAME_LEN_V01);
			cnss_pr_info("platform name: %s", req->platform_name);
		}
	}

	if (!of_property_read_u32(dev->of_node, "gpios-len", &req->gpios_len)) {
		if (req->gpios_len > QMI_WLFW_MAX_NUM_GPIO_V01) {
			cnss_pr_err("Invalid GPIOs array length %d\n",
				    req->gpios_len);
			ret = -EINVAL;
			goto err;
		}

		if (of_property_read_u32_array(dev->of_node, "gpios",
					       req->gpios, req->gpios_len)) {
			cnss_pr_err("Failed to get gpios from device tree\n");
			ret = -EINVAL;
			goto err;
		}

		req->gpios_valid = 1;
		cnss_pr_info("Sending %d GPIO entries in Host Capabilities\n",
			     req->gpios_len);
	}

	/* update MLO configuration
	 * Note: MLO capabilities needs to be sent only for mission mode
	 * and plat_priv->mlo_support will be disabled for all other modes.
	 * However, coldboot calibration is now handled within CNSS2 and
	 * transparent to driver, so explictily check for cal_in_progress and
	 * don't send MLO capabilitities for coldboot cal mode.
	 */
	if (!plat_priv->cal_in_progress && plat_priv->mlo_support &&
	    plat_priv->mlo_capable) {
		cnss_pr_info("MLO Capabilities added to QMI Host Cap msg\n");
		req->mlo_capable_valid = 1;
		req->mlo_capable = 1;

		req->mlo_chip_id = plat_priv->mlo_chip_info->chip_id;
		req->mlo_chip_id_valid = 1;

		req->mlo_group_id = plat_priv->mlo_group_info->group_id;
		req->mlo_group_id_valid = 1;

		req->max_mlo_peer_valid = 1;
		req->max_mlo_peer = plat_priv->mlo_group_info->max_num_peers;

		req->mlo_num_chips_valid = 1;
		req->mlo_num_chips = plat_priv->mlo_group_info->num_chips;

		req->mlo_chip_info_valid = 0;
		req->mlo_chip_v2_info_valid = 1;
		for (i = 0; i < req->mlo_num_chips; i++) {
			v2 = &req->mlo_chip_v2_info[i];
			cnss_mlo_config_fill_req(plat_priv, v2, i);
		}
	}

	if (num_wlan_clients) {
		req->num_wlan_clients_valid = 1;
		req->num_wlan_clients = num_wlan_clients;
		cnss_pr_info("Sending %d Number of WLAN clients in Host Capabilities\n",
			     req->num_wlan_clients);
	} else if (!of_property_read_u16(dev->of_node, "num_wlan_clients",
					 &req->num_wlan_clients)) {
		req->num_wlan_clients_valid = 1;
		cnss_pr_info("Sending %d Number of WLAN clients in Host Capabilities\n",
			     req->num_wlan_clients);
	}

	if (num_wlan_vaps) {
		req->num_wlan_vaps_valid = 1;
		req->num_wlan_vaps = num_wlan_vaps;
		cnss_pr_info("Sending %d Number of WLAN Vaps in Host Capabilities\n",
			     req->num_wlan_vaps);
	} else if (!of_property_read_u8(dev->of_node, "num_wlan_vaps",
					&req->num_wlan_vaps)) {
		req->num_wlan_vaps_valid = 1;
		cnss_pr_info("Sending %d Number of WLAN vaps in Host Capabilities\n",
			     req->num_wlan_vaps);
	}

	qmi_record(plat_priv->wlfw_service_instance_id,
		  (QMI_TYPE_REQ | QMI_WLFW_HOST_CAP_REQ_V01), ret,
		  resp_error_msg);
	ret = qmi_txn_init(&plat_priv->qmi_wlfw, &txn,
			   wlfw_host_cap_resp_msg_v01_ei, resp);
	if (ret < 0) {
		cnss_pr_err("Failed to initialize txn for host capability request, err: %d\n",
			    ret);
		goto out;
	}

	ret = qmi_send_request(&plat_priv->qmi_wlfw, NULL, &txn,
			       QMI_WLFW_HOST_CAP_REQ_V01,
			       WLFW_HOST_CAP_REQ_MSG_V01_MAX_MSG_LEN,
			       wlfw_host_cap_req_msg_v01_ei, req);
	if (ret < 0) {
		qmi_txn_cancel(&txn);
		cnss_pr_err("Failed to send host capability request, err: %d\n",
			    ret);
		goto out;
	}

	ret = qmi_txn_wait(&txn, QMI_WLFW_TIMEOUT_JF);
	if (ret < 0) {
		resp_error_msg = -QMI_RESULT_FAILURE_V01;
		cnss_pr_err("Failed to wait for response of host capability request, err: %d\n",
			    ret);
		goto out;
	}

	if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
		cnss_pr_err("Host capability request failed, result: %d, err: %d\n",
			    resp->resp.result, resp->resp.error);
		ret = -resp->resp.result;
		resp_error_msg = resp->resp.error;
		goto out;
	}
	qmi_record(plat_priv->wlfw_service_instance_id,
		  (QMI_TYPE_RESP | QMI_WLFW_HOST_CAP_REQ_V01), ret,
		  resp_error_msg);

	kfree(req);
	kfree(resp);
	return 0;

err:
	kfree(req);
	kfree(resp);
out:
	qmi_record(plat_priv->wlfw_service_instance_id,
		  (QMI_WLFW_HOST_CAP_REQ_V01), ret,
		  resp_error_msg);
	CNSS_ASSERT(0);

	return ret;
}

int cnss_wlfw_respond_mem_send_sync(struct cnss_plat_data *plat_priv)
{
	struct wlfw_respond_mem_req_msg_v01 *req;
	struct wlfw_respond_mem_resp_msg_v01 *resp;
	struct qmi_txn txn;
	struct cnss_fw_mem *fw_mem = plat_priv->fw_mem;
	int ret = 0, i;
	int resp_error_msg = 0;

	cnss_pr_dbg("Sending respond memory message, state: 0x%lx\n",
		    plat_priv->driver_state);

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	resp = kzalloc(sizeof(*resp), GFP_KERNEL);
	if (!resp) {
		kfree(req);
		return -ENOMEM;
	}

	req->mem_seg_len = plat_priv->fw_mem_seg_len;
	for (i = 0; i < req->mem_seg_len; i++) {
		if (plat_priv->cold_boot_support &&
		    (!fw_mem[i].pa || !fw_mem[i].size)) {
			if (fw_mem[i].type == 0) {
				cnss_pr_err("Invalid memory for FW type, segment = %d\n",
					    i);
				ret = -EINVAL;
				goto out;
			}
			cnss_pr_err("Memory for FW is not available for type: %u\n",
				    fw_mem[i].type);
			ret = -ENOMEM;
			goto out;
		}

		cnss_pr_dbg("Memory for FW, va: 0x%pK, pa: %pa, size: 0x%zx, type: %u\n",
			    fw_mem[i].va, &fw_mem[i].pa,
			    fw_mem[i].size, fw_mem[i].type);

		req->mem_seg[i].addr = fw_mem[i].pa;
		req->mem_seg[i].size = fw_mem[i].size;
		req->mem_seg[i].type = fw_mem[i].type;
	}

	qmi_record(plat_priv->wlfw_service_instance_id,
		  (QMI_TYPE_REQ | QMI_WLFW_RESPOND_MEM_REQ_V01), ret,
		  resp_error_msg);
	ret = qmi_txn_init(&plat_priv->qmi_wlfw, &txn,
			   wlfw_respond_mem_resp_msg_v01_ei, resp);
	if (ret < 0) {
		cnss_pr_err("Failed to initialize txn for respond memory request, err: %d\n",
			    ret);
		goto out;
	}

	ret = qmi_send_request(&plat_priv->qmi_wlfw, NULL, &txn,
			       QMI_WLFW_RESPOND_MEM_REQ_V01,
			       WLFW_RESPOND_MEM_REQ_MSG_V01_MAX_MSG_LEN,
			       wlfw_respond_mem_req_msg_v01_ei, req);
	if (ret < 0) {
		qmi_txn_cancel(&txn);
		cnss_pr_err("Failed to send respond memory request, err: %d\n",
			    ret);
		goto out;
	}

	ret = qmi_txn_wait(&txn, QMI_WLFW_TIMEOUT_JF);
	if (ret < 0) {
		resp_error_msg = -QMI_RESULT_FAILURE_V01;
		cnss_pr_err("Failed to wait for response of respond memory request, err: %d\n",
			    ret);
		goto out;
	}

	if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
		cnss_pr_err("Respond memory request failed, result: %d, err: %d\n",
			    resp->resp.result, resp->resp.error);
		ret = -resp->resp.result;
		resp_error_msg = resp->resp.error;
		goto out;
	}
	qmi_record(plat_priv->wlfw_service_instance_id,
		  (QMI_TYPE_RESP | QMI_WLFW_RESPOND_MEM_REQ_V01), ret,
		  resp_error_msg);

	kfree(req);
	kfree(resp);
	return 0;
out:
	qmi_record(plat_priv->wlfw_service_instance_id,
		  (QMI_WLFW_RESPOND_MEM_REQ_V01), ret,
		  resp_error_msg);
	CNSS_ASSERT(0);
	kfree(req);
	kfree(resp);
	return ret;
}

int cnss_wlfw_tgt_cap_send_sync(struct cnss_plat_data *plat_priv)
{
	struct wlfw_cap_req_msg_v01 *req;
	struct wlfw_cap_resp_msg_v01 *resp;
	struct qmi_txn txn;
	char *fw_build_timestamp;
	int i, ret = 0;
	int resp_error_msg = 0;

	cnss_pr_dbg("Sending target capability message, state: 0x%lx\n",
		    plat_priv->driver_state);

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	resp = kzalloc(sizeof(*resp), GFP_KERNEL);
	if (!resp) {
		kfree(req);
		return -ENOMEM;
	}

	qmi_record(plat_priv->wlfw_service_instance_id,
		  (QMI_TYPE_REQ | QMI_WLFW_CAP_REQ_V01), ret,
		  resp_error_msg);

	ret = qmi_txn_init(&plat_priv->qmi_wlfw, &txn,
			   wlfw_cap_resp_msg_v01_ei, resp);
	if (ret < 0) {
		cnss_pr_err("Failed to initialize txn for target capability request, err: %d\n",
			    ret);
		goto out;
	}

	ret = qmi_send_request(&plat_priv->qmi_wlfw, NULL, &txn,
			       QMI_WLFW_CAP_REQ_V01,
			       WLFW_CAP_REQ_MSG_V01_MAX_MSG_LEN,
			       wlfw_cap_req_msg_v01_ei, req);
	if (ret < 0) {
		qmi_txn_cancel(&txn);
		cnss_pr_err("Failed to send respond target capability request, err: %d\n",
			    ret);
		goto out;
	}

	ret = qmi_txn_wait(&txn, QMI_WLFW_TIMEOUT_JF);
	if (ret < 0) {
		resp_error_msg = -QMI_RESULT_FAILURE_V01;
		cnss_pr_err("Failed to wait for response of target capability request, err: %d\n",
			    ret);
		goto out;
	}

	if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
		cnss_pr_err("Target capability request failed, result: %d, err: %d\n",
			    resp->resp.result, resp->resp.error);
		ret = -resp->resp.result;
		resp_error_msg = resp->resp.error;
		goto out;
	}

	qmi_record(plat_priv->wlfw_service_instance_id,
		  (QMI_TYPE_RESP | QMI_WLFW_CAP_REQ_V01), ret,
		  resp_error_msg);
	if (resp->chip_info_valid) {
		plat_priv->chip_info.chip_id = resp->chip_info.chip_id;
		plat_priv->chip_info.chip_family = resp->chip_info.chip_family;
	}
	if (resp->board_info_valid)
		plat_priv->board_info.board_id = resp->board_info.board_id;
	else
		plat_priv->board_info.board_id = 0xFF;
	if (resp->soc_info_valid)
		plat_priv->soc_info.soc_id = resp->soc_info.soc_id;
	if (resp->fw_version_info_valid) {
		plat_priv->fw_version_info.fw_version =
			resp->fw_version_info.fw_version;
		fw_build_timestamp = resp->fw_version_info.fw_build_timestamp;
		fw_build_timestamp[QMI_WLFW_MAX_TIMESTAMP_LEN] = '\0';
		strlcpy(plat_priv->fw_version_info.fw_build_timestamp,
			resp->fw_version_info.fw_build_timestamp,
			QMI_WLFW_MAX_TIMESTAMP_LEN + 1);
	}
	if (resp->time_freq_hz_valid) {
		plat_priv->device_freq_hz = resp->time_freq_hz;
		cnss_pr_dbg("Device frequency is %d HZ\n",
			    plat_priv->device_freq_hz);
	}
#ifdef CNSS2_CPR
	if (resp->voltage_mv_valid) {
		plat_priv->cpr_info.voltage = resp->voltage_mv;
		cnss_pr_dbg("Voltage for CPR: %dmV\n",
			    plat_priv->cpr_info.voltage);
		cnss_update_cpr_info(plat_priv);
	}
	if (resp->otp_version_valid)
		plat_priv->otp_version = resp->otp_version;
#endif

	if (resp->eeprom_caldata_read_timeout_valid)
		plat_priv->eeprom_caldata_read_timeout =
			resp->eeprom_caldata_read_timeout;

	if (resp->dev_mem_info_valid) {
		for (i = 0; i < QMI_WLFW_MAX_DEV_MEM_NUM_V01; i++) {
			plat_priv->dev_mem_info[i].start =
				resp->dev_mem_info[i].start;
			plat_priv->dev_mem_info[i].size =
				resp->dev_mem_info[i].size;
			cnss_pr_info("Device memory info[%d]: start = 0x%llx, size = 0x%llx\n",
				     i, plat_priv->dev_mem_info[i].start,
				     plat_priv->dev_mem_info[i].size);
		}
	}

	if (resp->bdf_dnld_method_valid)
		plat_priv->bdf_dnld_method = resp->bdf_dnld_method;
	if (resp->regdb_mandatory_valid)
		plat_priv->regdb_mandatory = !!resp->regdb_mandatory;
	if (resp->rxgainlut_support_valid)
		plat_priv->rxgainlut_support = !!resp->rxgainlut_support;

	cnss_pr_info("Target capability: chip_id: 0x%x, chip_family: 0x%x, board_id: 0x%x, soc_id: 0x%x, fw_version: 0x%x, fw_build_timestamp: %s, otp_version: 0x%x eeprom_caldata_read_timeout %ds bdf_dnld_method %d regdb_mandatory %u rxgainlut_support %u\n",
		     plat_priv->chip_info.chip_id,
		     plat_priv->chip_info.chip_family,
		     plat_priv->board_info.board_id, plat_priv->soc_info.soc_id,
		     plat_priv->fw_version_info.fw_version,
		     plat_priv->fw_version_info.fw_build_timestamp,
		     plat_priv->otp_version,
		     plat_priv->eeprom_caldata_read_timeout,
		     plat_priv->bdf_dnld_method,
		     plat_priv->regdb_mandatory,
		     plat_priv->rxgainlut_support);

	kfree(req);
	kfree(resp);
	return 0;

out:
	qmi_record(plat_priv->wlfw_service_instance_id,
		  (QMI_WLFW_CAP_REQ_V01), ret,
		  resp_error_msg);
	CNSS_ASSERT(0);
	kfree(req);
	kfree(resp);
	return ret;
}

static int cnss_wlfw_load_bdf(struct wlfw_bdf_download_req_msg_v01 *req,
			      struct cnss_plat_data *plat_priv,
			      unsigned int remaining,
			      uint8_t bdf_type)
{
	int ret;
	char filename[30];
	const struct firmware *fw;
	char *bdf_addr;
	unsigned int bdf_addr_pa, *location = NULL;
	int size, bdf_arr_size;
	struct device *dev;

	dev = &plat_priv->plat_dev->dev;

	switch (bdf_type) {
	case BDF_TYPE_GOLDEN:
		if (plat_priv->board_info.board_id_override)
			snprintf(filename, sizeof(filename),
				 "%s" BDF_WIN_FILE_NAME_PREFIX "%.*x",
				 cnss_get_fw_path(plat_priv),
				 (plat_priv->board_info.num_bytes * 2),
				 plat_priv->board_info.board_id_override);
		else if (plat_priv->board_info.board_id == 0xFF)
			snprintf(filename, sizeof(filename),
				 "%s" DEFAULT_BDF_FILE_NAME,
				 cnss_get_fw_path(plat_priv));
		else
			snprintf(filename, sizeof(filename),
				 "%s" BDF_WIN_FILE_NAME_PREFIX "%.*x",
				 cnss_get_fw_path(plat_priv),
				 (plat_priv->board_info.num_bytes * 2),
				 plat_priv->board_info.board_id);
		break;
	case BDF_TYPE_CALDATA:
		if (plat_priv->device_id == QCN6122_DEVICE_ID ||
		    plat_priv->device_id == QCN9160_DEVICE_ID) {
			snprintf(filename, sizeof(filename),
				 "%s" DEFAULT_CAL_FILE_PREFIX
				 "%d" DEFAULT_CAL_FILE_SUFFIX,
				 cnss_get_fw_path(plat_priv),
				 plat_priv->userpd_id);
		} else if (plat_priv->device_id == QCN6432_DEVICE_ID) {
			snprintf(filename, sizeof(filename),
				"%s" DEFAULT_CAL_FILE_PREFIX
				"%d.b%.*x", cnss_get_fw_path(plat_priv),
				(plat_priv->userpd_id),
				(plat_priv->board_info.num_bytes * 2),
				plat_priv->board_info.board_id);
		} else {
			snprintf(filename, sizeof(filename),
				 "%s" DEFAULT_CAL_FILE_NAME,
				 cnss_get_fw_path(plat_priv));
		}
		break;
	case BDF_TYPE_HDS:
		snprintf(filename, sizeof(filename),
			 "%s" HDS_FILE_NAME, cnss_get_fw_path(plat_priv));
		break;
	case BDF_TYPE_REGDB:
		snprintf(filename, sizeof(filename),
			 "%s" DEFAULT_REGDB_FILE_NAME,
			 cnss_get_fw_path(plat_priv));
		break;
	default:
		return -EINVAL;
	}

	ret = request_firmware_direct(&fw, filename, &plat_priv->plat_dev->dev);
	if (ret) {
		cnss_pr_err("Failed to get BDF file %s (%d)", filename, ret);
		return ret;
	}
	size = fw->size;

	bdf_arr_size = of_property_count_elems_of_size(dev->of_node,
						"qcom,bdf-addr",
						sizeof(u32));
	location = kcalloc(bdf_arr_size, sizeof(unsigned int), GFP_KERNEL);
	if (!location) {
		cnss_pr_err("Error: Cannot allocate location arr memory\n");
		return -ENOMEM;
	}

	if (of_property_read_u32_array(dev->of_node, "qcom,bdf-addr", location,
				       bdf_arr_size)) {
		pr_err("Error: No bdf_addr in device_tree\n");
		kfree(location);
		CNSS_ASSERT(0);
		goto out;
	}
	CNSS_ASSERT(plat_priv->tgt_mem_cfg_mode < bdf_arr_size);
	bdf_addr_pa = *(location + plat_priv->tgt_mem_cfg_mode);
	bdf_addr = ioremap(bdf_addr_pa, BDF_MAX_SIZE);
	if (!bdf_addr) {
		cnss_pr_err("ERROR. not able to ioremap BDF location\n");
		ret = -EIO;
		CNSS_ASSERT(0);
	}
	if (size != 0 && size <= BDF_MAX_SIZE) {
		if (bdf_type == BDF_TYPE_GOLDEN ||
		    bdf_type == BDF_TYPE_HDS ||
		    bdf_type == BDF_TYPE_REGDB) {
			cnss_pr_info("BDF location : 0x%x\n", bdf_addr_pa);
			cnss_pr_info("BDF %s size %d\n",
				     filename, (unsigned int)fw->size);
			memcpy(bdf_addr, fw->data, fw->size);
		}
		if (bdf_type == BDF_TYPE_CALDATA) {
			cnss_pr_info("per device BDF location : 0x%x\n",
				     CALDATA_OFFSET(bdf_addr_pa));
			memcpy(CALDATA_OFFSET(bdf_addr), fw->data, fw->size);
			cnss_pr_info("CALDATA %s size %d offset 0x%x\n",
				     filename, (unsigned int)fw->size,
				     CALDATA_OFFSET(0));
		}
		req->total_size_valid = 1;
		req->total_size = size;
		req->data_valid = 0;
		req->end_valid = 1;
		req->end = 1;
		req->data_len = remaining;
		req->bdf_type = bdf_type;
		req->bdf_type_valid = 1;
	} else {
		cnss_pr_info("bdf size %d > segsz %d\n", size, BDF_MAX_SIZE);
		req->data_len = remaining;
		req->end = 1;
	}
	iounmap(bdf_addr);
out:
	if (fw)
		release_firmware(fw);
	if (location)
		kfree(location);
	return ret;
}

int cnss_wlfw_bdf_dnld_send_sync(struct cnss_plat_data *plat_priv,
				 u32 bdf_type)
{
	struct qmi_txn txn;
	char filename[MAX_BDF_FILE_NAME];
	const struct firmware *fw_entry = NULL;
	const u8 *temp;
	unsigned int remaining;
	struct wlfw_bdf_download_req_msg_v01 *req;
	struct wlfw_bdf_download_resp_msg_v01 *resp;
	int ret = 0;
	int resp_error_msg = 0;
	u8 fw_bdf_type = BDF_TYPE_GOLDEN;
	uint32_t board_id = 0;

	cnss_pr_dbg("Sending BDF download message, state: 0x%lx, type: %d\n",
		    plat_priv->driver_state, bdf_type);

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	resp = kzalloc(sizeof(*resp), GFP_KERNEL);
	if (!resp) {
		kfree(req);
		return -ENOMEM;
	}
	switch (bdf_type) {
	case CNSS_BDF_WIN:
		if (plat_priv->board_info.board_id_override)
			snprintf(filename, sizeof(filename),
				 "%s" BDF_WIN_FILE_NAME_PREFIX "%.*x",
				 cnss_get_fw_path(plat_priv),
				 (plat_priv->board_info.num_bytes * 2),
				 plat_priv->board_info.board_id_override);
		else if (plat_priv->board_info.board_id == 0xFF)
			snprintf(filename, sizeof(filename),
				 "%s" DEFAULT_BDF_FILE_NAME,
				 cnss_get_fw_path(plat_priv));
		else
			snprintf(filename, sizeof(filename),
				 "%s" BDF_WIN_FILE_NAME_PREFIX "%.*x",
				 cnss_get_fw_path(plat_priv),
				 (plat_priv->board_info.num_bytes * 2),
				 plat_priv->board_info.board_id);

		/* Temporary check to be compatible with older FW which
		 * still has 1 byte board_id based BDF files
		 */
		if (plat_priv->device_id == QCN9224_DEVICE_ID &&
		    request_firmware_direct(&fw_entry, filename,
					    &plat_priv->plat_dev->dev))
			snprintf(filename, sizeof(filename),
				 "%s" BDF_WIN_FILE_NAME_PREFIX "%02x",
				 cnss_get_fw_path(plat_priv),
				 (plat_priv->board_info.board_id_override &
				  ~CNSS_FW_TYPE_MASK));
		if (fw_entry)
			release_firmware(fw_entry);

		if (plat_priv->bdf_dnld_method == WLFW_DIRECT_BDF_COPY_V01) {
			cnss_pr_dbg("BDF download through direct copy\n");
			temp = filename;
			remaining = MAX_BDF_FILE_NAME;
			goto bypass_bdf;
		}
		break;
	case CNSS_CALDATA_WIN:
		fw_bdf_type = BDF_TYPE_CALDATA;
		/* If the ftm.conf is not found,
		 * download caldata_x.bin which is the default file.
		 */
		if (plat_priv->board_info.board_id_override)
			board_id = plat_priv->board_info.board_id_override;
		else
			board_id = plat_priv->board_info.board_id;

		if (plat_priv->bus_type == CNSS_BUS_PCI) {
			snprintf(filename, sizeof(filename), "%s",
				 FTM_CONF_FILE_PATH);
			if (cnss_check_path_exists(FTM_CONF_FILE_PATH)) {
				snprintf(filename, sizeof(filename),
					 "%s" DEFAULT_CAL_FILE_PREFIX
				"%d.b%.*x", cnss_get_fw_path(plat_priv),
				(plat_priv->pci_slot_id + 1),
				(plat_priv->board_info.num_bytes * 2),
				board_id);
			} else {
				snprintf(filename, sizeof(filename),
					 "%s" DEFAULT_CAL_FILE_PREFIX
					 "%d" DEFAULT_CAL_FILE_SUFFIX,
					 cnss_get_fw_path(plat_priv),
					 (plat_priv->pci_slot_id + 1));
			}
		} else if (plat_priv->device_id == QCN6122_DEVICE_ID ||
			 plat_priv->device_id == QCN9160_DEVICE_ID) {
			snprintf(filename, sizeof(filename),
				 "%s" DEFAULT_CAL_FILE_PREFIX
				 "%d" DEFAULT_CAL_FILE_SUFFIX,
				 cnss_get_fw_path(plat_priv),
				 plat_priv->userpd_id);
		} else if (plat_priv->device_id == QCN6432_DEVICE_ID) {
			snprintf(filename, sizeof(filename), "%s",
				FTM_CONF_FILE_PATH);
			if (cnss_check_path_exists(FTM_CONF_FILE_PATH)) {
				snprintf(filename, sizeof(filename),
					"%s" DEFAULT_CAL_FILE_PREFIX
				"%d.b%.*x", cnss_get_fw_path(plat_priv),
				(plat_priv->userpd_id),
				(plat_priv->board_info.num_bytes * 2),
				board_id);
			} else {
				snprintf(filename, sizeof(filename),
					"%s" DEFAULT_CAL_FILE_PREFIX
					"%d" DEFAULT_CAL_FILE_SUFFIX,
					cnss_get_fw_path(plat_priv),
					plat_priv->userpd_id);
			}
		} else {
			snprintf(filename, sizeof(filename),
				 "%s" DEFAULT_CAL_FILE_NAME,
				 cnss_get_fw_path(plat_priv));
		}

		if (plat_priv->bdf_dnld_method == WLFW_DIRECT_BDF_COPY_V01) {
			cnss_pr_dbg("Caldata download through direct copy\n");
			temp = filename;
			remaining = MAX_BDF_FILE_NAME;
			goto bypass_bdf;
		}

		if (plat_priv->eeprom_caldata_read_timeout &&
		    (plat_priv->device_id == QCN9000_DEVICE_ID ||
		     plat_priv->device_id == QCN9224_DEVICE_ID)) {
			fw_bdf_type = BDF_TYPE_EEPROM;
			temp = filename;
			remaining = MAX_BDF_FILE_NAME;
			goto bypass_bdf;
		}
		break;
	case CNSS_BDF_REGDB:
		fw_bdf_type = BDF_TYPE_REGDB;
		if (plat_priv->board_info.board_id_override) {
			snprintf(filename, sizeof(filename),
				 "%s" REGDB_WIN_FILE_NAME_PREFIX "%.*x",
				 cnss_get_fw_path(plat_priv),
				 (plat_priv->board_info.num_bytes * 2),
				 plat_priv->board_info.board_id_override);
		} else {
			/* If plat_priv->board_info.board_id is 0xFF,
			 * regdb.bff will not be found and eventually,
			 * cnss2 would consider regdb.bin in this case
			 * as well, hence there is no need for a separate
			 * check for plat_priv->board_info.board_id as 0xFF.
			 */
			snprintf(filename, sizeof(filename),
				 "%s" REGDB_WIN_FILE_NAME_PREFIX "%.*x",
				 cnss_get_fw_path(plat_priv),
				 (plat_priv->board_info.num_bytes * 2),
				 plat_priv->board_info.board_id);
		}
		/* If the regdb corresponding to board ID is not found,
		 * download regdb.bin which is the default file.
		 */
		ret = request_firmware_direct(&fw_entry, filename,
					      &plat_priv->plat_dev->dev);
		if (ret) {
			snprintf(filename, sizeof(filename),
				 "%s" DEFAULT_REGDB_FILE_NAME,
				 cnss_get_fw_path(plat_priv));
		}

		if (fw_entry)
			release_firmware(fw_entry);
		break;
	case CNSS_BDF_HDS:
		fw_bdf_type = BDF_TYPE_HDS;
		snprintf(filename, sizeof(filename),
			 "%s" HDS_FILE_NAME, cnss_get_fw_path(plat_priv));
		break;
	case CNSS_BDF_RXGAINLUT:
		fw_bdf_type = BDF_TYPE_RXGAINLUT;
		if (plat_priv->board_info.board_id_override) {
			snprintf(filename, sizeof(filename),
				 "%s" RXGAINLUT_WIN_FILE_NAME_PREFIX "%.*x",
				 cnss_get_fw_path(plat_priv),
				 (plat_priv->board_info.num_bytes * 2),
				 plat_priv->board_info.board_id_override);
		} else {
			/* If plat_priv->board_info.board_id is 0xFF,
			 * rxgainlut.bff will not be found and eventually,
			 * cnss2 would consider rxgainlut.bin in this case
			 * as well, hence there is no need for a separate
			 * check for plat_priv->board_info.board_id as 0xFF.
			 */
			snprintf(filename, sizeof(filename),
				 "%s" RXGAINLUT_WIN_FILE_NAME_PREFIX "%.*x",
				 cnss_get_fw_path(plat_priv),
				 (plat_priv->board_info.num_bytes * 2),
				 plat_priv->board_info.board_id);
		}
		/* If the rxgainlut corresponding to board ID is not found,
		 * download rxgainlut.bin which is the default file.
		 */
		ret = request_firmware_direct(&fw_entry, filename,
					      &plat_priv->plat_dev->dev);
		if (ret) {
			snprintf(filename, sizeof(filename),
				 "%s" DEFAULT_RXGAINLUT_FILE_NAME,
				 cnss_get_fw_path(plat_priv));
		}
		/* Temporary check to be compatible with older FW which
		 * still has 1 byte board_id based BDF files
		 */
		if (plat_priv->device_id == QCN9224_DEVICE_ID &&
		    request_firmware_direct(&fw_entry, filename,
					    &plat_priv->plat_dev->dev))
			snprintf(filename, sizeof(filename),
				 "%s" RXGAINLUT_WIN_FILE_NAME_PREFIX "%02x",
				 cnss_get_fw_path(plat_priv),
				 (plat_priv->board_info.board_id_override &
				  ~CNSS_FW_TYPE_MASK));
		if (fw_entry)
			release_firmware(fw_entry);
		break;
	default:
		cnss_pr_err("Invalid BDF type: %d\n",
			    plat_priv->ctrl_params.bdf_type);
		ret = -EINVAL;
		goto out;
	}

	ret = request_firmware_direct(&fw_entry, filename,
				      &plat_priv->plat_dev->dev);
	if (ret) {
		if (bdf_type == CNSS_CALDATA_WIN) {
			cnss_pr_warn("Caldata not present. Skipping caldata download: %s\n",
				     filename);
			ret = 0;
			resp_error_msg = -ENOENT;
			goto err_req_fw;
		} else if (bdf_type == CNSS_BDF_HDS) {
			/* HDS bin download is not mandatory */
			ret = 0;
			goto out;
		} else if (bdf_type == CNSS_BDF_REGDB) {
			/* If REGDB bin file is not present, but
			 * regdb_mandatory is true, assert. If it is false,
			 * just print the message and skip it.
			 */
			cnss_pr_info("Failed to load RegDB %s\n", filename);
			if (!plat_priv->regdb_mandatory) {
				cnss_pr_info("Skipping regdb download for %s since it is not mandatory as indicated by the target caps\n",
					     plat_priv->device_name);
				ret = 0;
			}
			goto out;
		} else if (bdf_type == CNSS_BDF_RXGAINLUT) {
			/* If RXGAINLUT bin file download is not mandatory */
			cnss_pr_dbg("Failed to load RXGAINLUT. %s is not a mandatory file\n",
					     filename);
			ret = 0;
			goto out;
		} else {
			/* BDF download is mandatory for all targets */
			cnss_pr_err("Failed to load BDF: %s\n", filename);
			goto out;
		}
	}

	temp = fw_entry->data;
	remaining = fw_entry->size;

	cnss_pr_info("Downloading BDF: %s, size: %u\n", filename, remaining);
bypass_bdf:

	qmi_record(plat_priv->wlfw_service_instance_id,
		  (QMI_TYPE_REQ | QMI_WLFW_BDF_DOWNLOAD_REQ_V01), ret,
		  resp_error_msg);
	while (remaining) {
		req->valid = 1;
		req->file_id_valid = 1;
		req->file_id = plat_priv->board_info.board_id;
		req->total_size_valid = 1;
		req->total_size = remaining;
		req->seg_id_valid = 1;
		req->data_valid = 1;
		req->end_valid = 1;
		req->bdf_type_valid = 1;
		req->bdf_type = fw_bdf_type;

		if (remaining > QMI_WLFW_MAX_DATA_SIZE_V01) {
			req->data_len = QMI_WLFW_MAX_DATA_SIZE_V01;
		} else {
			req->data_len = remaining;
			req->end = 1;
		}

		if (plat_priv->bdf_dnld_method == WLFW_DIRECT_BDF_COPY_V01) {
			cnss_pr_dbg("%s: Bus type %d BDF download method %d\n",
				    __func__, plat_priv->bus_type,
				    plat_priv->bdf_dnld_method);
			ret = cnss_wlfw_load_bdf(req, plat_priv,
						 MAX_BDF_FILE_NAME,
						 fw_bdf_type);
			if (ret) {
				if (bdf_type == CNSS_CALDATA_WIN) {
					cnss_pr_warn("Caldata not present. Skipping caldata download: %s\n",
						     filename);
					ret = 0;
					resp_error_msg = -ENOENT;
					goto err_req_fw;
				} else if (bdf_type == CNSS_BDF_HDS ||
					   bdf_type == CNSS_BDF_REGDB) {
					/* HDS is not mandatory and
					 * REGDB is mandatory only for
					 * QCN9224
					 */
					ret = 0;
					goto err_req_fw;
				} else {
					/* BDF download is mandatory for
					 * all targets.
					 */
					cnss_pr_err("Failed to load BDF: %s\n",
						    filename);
					goto err_req_fw;
				}
			}
		}

		memcpy(req->data, temp, req->data_len);

		ret = qmi_txn_init(&plat_priv->qmi_wlfw, &txn,
				   wlfw_bdf_download_resp_msg_v01_ei, resp);
		if (ret < 0) {
			cnss_pr_err("Failed to initialize txn for BDF download request, err: %d\n",
				    ret);
			goto err_send;
		}

		ret = qmi_send_request
			(&plat_priv->qmi_wlfw, NULL, &txn,
			 QMI_WLFW_BDF_DOWNLOAD_REQ_V01,
			 WLFW_BDF_DOWNLOAD_REQ_MSG_V01_MAX_MSG_LEN,
			 wlfw_bdf_download_req_msg_v01_ei, req);
		if (ret < 0) {
			qmi_txn_cancel(&txn);
			cnss_pr_err("Failed to send respond BDF download request, err: %d\n",
				    ret);
			goto err_send;
		}
		if (fw_bdf_type == BDF_TYPE_EEPROM) {
			cnss_pr_info("EEPROM READ WAIT STARTED: %d seconds",
				     plat_priv->eeprom_caldata_read_timeout);
			ret = qmi_txn_wait(&txn,
					   msecs_to_jiffies(
					   plat_priv->
					   eeprom_caldata_read_timeout * 1000));
		} else {
			ret = qmi_txn_wait(&txn, QMI_WLFW_TIMEOUT_JF);
		}

		if (ret < 0) {
			resp_error_msg = -QMI_RESULT_FAILURE_V01;
			cnss_pr_err("Failed to wait for response of BDF download request, err: %d\n",
				    ret);
			goto err_send;
		}

		if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
			cnss_pr_err("BDF download request failed, result: %d, err: %d\n",
				    resp->resp.result, resp->resp.error);
			ret = -resp->resp.result;
			resp_error_msg = resp->resp.error;
			goto err_send;
		}
		remaining -= req->data_len;
		temp += req->data_len;
		req->seg_id++;
	}

	if (fw_entry)
		release_firmware(fw_entry);

	qmi_record(plat_priv->wlfw_service_instance_id,
		  (QMI_TYPE_RESP | QMI_WLFW_BDF_DOWNLOAD_REQ_V01), ret,
		  resp_error_msg);
	kfree(req);
	kfree(resp);
	return 0;

err_send:
	if (plat_priv->ctrl_params.bdf_type != CNSS_BDF_DUMMY)
		release_firmware(fw_entry);
err_req_fw:
	qmi_record(plat_priv->wlfw_service_instance_id,
		  (QMI_WLFW_BDF_DOWNLOAD_REQ_V01), ret,
		  resp_error_msg);
out:
	kfree(req);
	kfree(resp);

	if (ret)
		CNSS_ASSERT(0);
	else
		ret = 0;
	return ret;
}

int cnss_wlfw_m3_dnld_send_sync(struct cnss_plat_data *plat_priv)
{
	struct wlfw_m3_info_req_msg_v01 *req;
	struct wlfw_m3_info_resp_msg_v01 *resp;
	struct qmi_txn txn;
	struct cnss_fw_mem *m3_mem = &plat_priv->m3_mem;
	int ret = 0;
	int resp_error_msg = 0;

	cnss_pr_dbg("Sending M3 information message, state: 0x%lx\n",
		    plat_priv->driver_state);

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	resp = kzalloc(sizeof(*resp), GFP_KERNEL);
	if (!resp) {
		kfree(req);
		return -ENOMEM;
	}
	if ((plat_priv->device_id == QCN9000_DEVICE_ID ||
	     plat_priv->device_id == QCN9224_DEVICE_ID) &&
	    (!m3_mem->pa || !m3_mem->size)) {
		cnss_pr_err("Memory for M3 is not available\n");
		ret = -ENOMEM;
		goto out;
	}

	cnss_pr_dbg("M3 memory, va: 0x%pK, pa: %pa, size: 0x%zx\n",
		    m3_mem->va, &m3_mem->pa, m3_mem->size);

	req->addr = plat_priv->m3_mem.pa;
	req->size = plat_priv->m3_mem.size;

	qmi_record(plat_priv->wlfw_service_instance_id,
		  (QMI_TYPE_REQ | QMI_WLFW_M3_INFO_REQ_V01), ret,
		  resp_error_msg);
	ret = qmi_txn_init(&plat_priv->qmi_wlfw, &txn,
			   wlfw_m3_info_resp_msg_v01_ei, resp);
	if (ret < 0) {
		cnss_pr_err("Failed to initialize txn for M3 information request, err: %d\n",
			    ret);
		goto out;
	}

	ret = qmi_send_request(&plat_priv->qmi_wlfw, NULL, &txn,
			       QMI_WLFW_M3_INFO_REQ_V01,
			       WLFW_M3_INFO_REQ_MSG_V01_MAX_MSG_LEN,
			       wlfw_m3_info_req_msg_v01_ei, req);
	if (ret < 0) {
		qmi_txn_cancel(&txn);
		cnss_pr_err("Failed to send M3 information request, err: %d\n",
			    ret);
		goto out;
	}

	ret = qmi_txn_wait(&txn, QMI_WLFW_TIMEOUT_JF);
	if (ret < 0) {
		resp_error_msg = -QMI_RESULT_FAILURE_V01;
		cnss_pr_err("Failed to wait for response of M3 information request, err: %d\n",
			    ret);
		goto out;
	}

	if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
		cnss_pr_err("M3 information request failed, result: %d, err: %d\n",
			    resp->resp.result, resp->resp.error);
		ret = -resp->resp.result;
		resp_error_msg = resp->resp.error;
		goto out;
	}

	qmi_record(plat_priv->wlfw_service_instance_id,
		  (QMI_TYPE_RESP | QMI_WLFW_M3_INFO_REQ_V01), ret,
		  resp_error_msg);
	kfree(req);
	kfree(resp);
	return 0;

out:
	qmi_record(plat_priv->wlfw_service_instance_id,
		  (QMI_WLFW_M3_INFO_REQ_V01), ret,
		  resp_error_msg);
	CNSS_ASSERT(0);
	kfree(req);
	kfree(resp);
	return ret;
}

int cnss_wlfw_qdss_data_send_sync(struct cnss_plat_data *plat_priv,
				  char *file_name, u32 total_size)
{
	int ret = 0;
	int resp_error_msg = 0;
	struct wlfw_qdss_trace_data_req_msg_v01 *req;
	struct wlfw_qdss_trace_data_resp_msg_v01 *resp;
	unsigned char *p_qdss_trace_data_temp, *p_qdss_trace_data = NULL;
	u32 remaining;
	struct qmi_txn txn;

	cnss_pr_dbg("%s: %s\n", __func__, file_name);

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	resp = kzalloc(sizeof(*resp), GFP_KERNEL);
	if (!resp) {
		kfree(req);
		return -ENOMEM;
	}

	p_qdss_trace_data = kzalloc(total_size, GFP_KERNEL);
	if (!p_qdss_trace_data) {
		ret = -ENOMEM;
		goto end;
	}

	remaining = total_size;
	p_qdss_trace_data_temp = p_qdss_trace_data;
	qmi_record(plat_priv->wlfw_service_instance_id,
		  (QMI_TYPE_REQ | QMI_WLFW_QDSS_TRACE_DATA_REQ_V01), ret,
		  resp_error_msg);
	while (remaining && resp->end == 0) {
		ret = qmi_txn_init(&plat_priv->qmi_wlfw, &txn,
				   wlfw_qdss_trace_data_resp_msg_v01_ei, resp);

		if (ret < 0) {
			cnss_pr_err("Fail to init txn for QDSS trace resp %d\n",
				    ret);
			goto fail;
		}

		ret = qmi_send_request
		      (&plat_priv->qmi_wlfw, NULL, &txn,
		       QMI_WLFW_QDSS_TRACE_DATA_REQ_V01,
		       WLFW_QDSS_TRACE_DATA_REQ_MSG_V01_MAX_MSG_LEN,
		       wlfw_qdss_trace_data_req_msg_v01_ei, req);

		if (ret < 0) {
			qmi_txn_cancel(&txn);
			cnss_pr_err("Fail to send QDSS trace data req %d\n",
				    ret);
			goto fail;
		}

		ret = qmi_txn_wait(&txn, plat_priv->ctrl_params.qmi_timeout);

		if (ret < 0) {
			resp_error_msg = -QMI_RESULT_FAILURE_V01;
			cnss_pr_err("QDSS trace resp wait failed with rc %d\n",
				    ret);
			goto fail;
		} else if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
			cnss_pr_err("QMI QDSS trace request rejected, result:%d error:%d\n",
				    resp->resp.result, resp->resp.error);
			ret = -resp->resp.result;
			resp_error_msg = resp->resp.error;
			goto fail;
		} else {
			ret = 0;
		}

		cnss_pr_dbg("%s: response total size  %d data len %d",
			    __func__, resp->total_size, resp->data_len);

		if ((resp->total_size_valid == 1 &&
		     resp->total_size == total_size) &&
		    (resp->seg_id_valid == 1 && resp->seg_id == req->seg_id) &&
		    (resp->data_valid == 1 &&
		     resp->data_len <= QMI_WLFW_MAX_DATA_SIZE_V01) &&
			resp->data_len <= remaining) {
			memcpy(p_qdss_trace_data_temp,
			       resp->data, resp->data_len);
		} else {
			cnss_pr_err("%s: Unmatched qdss trace data, Expect total_size %u, seg_id %u, Recv total_size_valid %u, total_size %u, seg_id_valid %u, seg_id %u, data_len_valid %u, data_len %u",
				    __func__,
				    total_size, req->seg_id,
				    resp->total_size_valid,
				    resp->total_size,
				    resp->seg_id_valid,
				    resp->seg_id,
				    resp->data_valid,
				    resp->data_len);
			ret = -EINVAL;
			goto fail;
		}

		remaining -= resp->data_len;
		p_qdss_trace_data_temp += resp->data_len;
		req->seg_id++;
	}

	qmi_record(plat_priv->wlfw_service_instance_id,
		  (QMI_TYPE_RESP | QMI_WLFW_QDSS_TRACE_DATA_REQ_V01), ret,
		  resp_error_msg);

	if (remaining == 0 && (resp->end_valid && resp->end)) {
		ret = cnss_genl_send_msg(p_qdss_trace_data,
					 CNSS_GENL_MSG_TYPE_QDSS, file_name,
					 total_size);
		if (ret < 0) {
			cnss_pr_err("Fail to save QDSS trace data: %d\n",
				    ret);
		ret = -EINVAL;
		goto fail;
		}
	} else {
		cnss_pr_err("%s: QDSS trace file corrupted: remaining %u, end_valid %u, end %u",
			    __func__,
			    remaining, resp->end_valid, resp->end);
		ret = -EINVAL;
		goto fail;
	}

fail:
	qmi_record(plat_priv->wlfw_service_instance_id,
		  (QMI_WLFW_QDSS_TRACE_DATA_REQ_V01), ret,
		  resp_error_msg);
	kfree(p_qdss_trace_data);

end:
	kfree(req);
	kfree(resp);
	return ret;
}

int cnss_wlfw_wlan_mode_send_sync(struct cnss_plat_data *plat_priv,
				  enum cnss_driver_mode mode)
{
	struct wlfw_wlan_mode_req_msg_v01 *req;
	struct wlfw_wlan_mode_resp_msg_v01 *resp;
	struct qmi_txn txn;
	int ret = 0;
	int resp_error_msg = 0;

	if (!plat_priv)
		return -ENODEV;

	cnss_pr_info("Sending mode message, mode: %s(%d), state: 0x%lx\n",
		     cnss_qmi_mode_to_str(mode), mode, plat_priv->driver_state);

	if (mode == CNSS_OFF &&
	    test_bit(CNSS_DRIVER_RECOVERY, &plat_priv->driver_state)) {
		cnss_pr_dbg("Recovery is in progress, ignore mode off request\n");
		return 0;
	}

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	resp = kzalloc(sizeof(*resp), GFP_KERNEL);
	if (!resp) {
		kfree(req);
		return -ENOMEM;
	}

	req->mode = (enum wlfw_driver_mode_enum_v01)mode;
	req->hw_debug_valid = 1;
	req->hw_debug = 0;
	qmi_record(plat_priv->wlfw_service_instance_id,
		  (QMI_TYPE_REQ | QMI_WLFW_WLAN_MODE_REQ_V01), ret,
		  resp_error_msg);
	ret = qmi_txn_init(&plat_priv->qmi_wlfw, &txn,
			   wlfw_wlan_mode_resp_msg_v01_ei, resp);
	if (ret < 0) {
		cnss_pr_err("Failed to initialize txn for mode request, mode: %s(%d), err: %d\n",
			    cnss_qmi_mode_to_str(mode), mode, ret);
		goto out;
	}

	ret = qmi_send_request(&plat_priv->qmi_wlfw, NULL, &txn,
			       QMI_WLFW_WLAN_MODE_REQ_V01,
			       WLFW_WLAN_MODE_REQ_MSG_V01_MAX_MSG_LEN,
			       wlfw_wlan_mode_req_msg_v01_ei, req);
	if (ret < 0) {
		qmi_txn_cancel(&txn);
		cnss_pr_err("Failed to send mode request, mode: %s(%d), err: %d\n",
			    cnss_qmi_mode_to_str(mode), mode, ret);
		goto out;
	}

	if (mode == CNSS_MISSION)
		cnss_unmount_firmware(plat_priv);

	ret = qmi_txn_wait(&txn, QMI_WLFW_TIMEOUT_JF);
	if (ret < 0) {
		resp_error_msg = -QMI_RESULT_FAILURE_V01;
		cnss_pr_err("Failed to wait for response of mode request, mode: %s(%d), err: %d\n",
			    cnss_qmi_mode_to_str(mode), mode, ret);
		goto out;
	}

	if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
		cnss_pr_err("Mode request failed, mode: %s(%d), result: %d, err: %d\n",
			    cnss_qmi_mode_to_str(mode), mode, resp->resp.result,
			    resp->resp.error);
		ret = -resp->resp.result;
		resp_error_msg = resp->resp.error;
		goto out;
	}
	qmi_record(plat_priv->wlfw_service_instance_id,
		  (QMI_TYPE_RESP | QMI_WLFW_WLAN_MODE_REQ_V01), ret,
		  resp_error_msg);

	kfree(req);
	kfree(resp);
	return 0;

out:
	qmi_record(plat_priv->wlfw_service_instance_id,
		  (QMI_WLFW_WLAN_MODE_REQ_V01), ret,
		  resp_error_msg);
	if (mode == CNSS_OFF) {
		cnss_pr_dbg("WLFW service is disconnected while sending mode off request\n");
		ret = 0;
	} else {
		CNSS_ASSERT(0);
	}
	kfree(req);
	kfree(resp);
	return ret;
}

int cnss_wlfw_wlan_cfg_send_sync(struct cnss_plat_data *plat_priv,
				 struct cnss_wlan_enable_cfg *config,
				 const char *host_version)
{
	struct wlfw_wlan_cfg_req_msg_v01 *req;
	struct wlfw_wlan_cfg_resp_msg_v01 *resp;
	struct qmi_txn txn;
	u32 i;
	int ret = 0;
	int resp_error_msg = 0;

	cnss_pr_dbg("Sending WLAN config message, state: 0x%lx\n",
		    plat_priv->driver_state);

	if (!plat_priv)
		return -ENODEV;

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	resp = kzalloc(sizeof(*resp), GFP_KERNEL);
	if (!resp) {
		kfree(req);
		return -ENOMEM;
	}

	req->host_version_valid = 1;
	strlcpy(req->host_version, host_version,
		QMI_WLFW_MAX_STR_LEN_V01 + 1);

	req->tgt_cfg_valid = 1;
	if (config->num_ce_tgt_cfg > QMI_WLFW_MAX_NUM_CE_V01)
		req->tgt_cfg_len = QMI_WLFW_MAX_NUM_CE_V01;
	else
		req->tgt_cfg_len = config->num_ce_tgt_cfg;
	for (i = 0; i < req->tgt_cfg_len; i++) {
		req->tgt_cfg[i].pipe_num = config->ce_tgt_cfg[i].pipe_num;
		req->tgt_cfg[i].pipe_dir = config->ce_tgt_cfg[i].pipe_dir;
		req->tgt_cfg[i].nentries = config->ce_tgt_cfg[i].nentries;
		req->tgt_cfg[i].nbytes_max = config->ce_tgt_cfg[i].nbytes_max;
		req->tgt_cfg[i].flags = config->ce_tgt_cfg[i].flags;
	}

	req->svc_cfg_valid = 1;
	if (config->num_ce_svc_pipe_cfg > QMI_WLFW_MAX_NUM_SVC_V01)
		req->svc_cfg_len = QMI_WLFW_MAX_NUM_SVC_V01;
	else
		req->svc_cfg_len = config->num_ce_svc_pipe_cfg;
	for (i = 0; i < req->svc_cfg_len; i++) {
		req->svc_cfg[i].service_id = config->ce_svc_cfg[i].service_id;
		req->svc_cfg[i].pipe_dir = config->ce_svc_cfg[i].pipe_dir;
		req->svc_cfg[i].pipe_num = config->ce_svc_cfg[i].pipe_num;
	}

	req->shadow_reg_v2_valid = 1;
	if (config->num_shadow_reg_v2_cfg >
	    QMI_WLFW_MAX_NUM_SHADOW_REG_V2_V01)
		req->shadow_reg_v2_len = QMI_WLFW_MAX_NUM_SHADOW_REG_V2_V01;
	else
		req->shadow_reg_v2_len = config->num_shadow_reg_v2_cfg;

	memcpy(req->shadow_reg_v2, config->shadow_reg_v2_cfg,
	       sizeof(struct wlfw_shadow_reg_v2_cfg_s_v01)
	       * req->shadow_reg_v2_len);

	qmi_record(plat_priv->wlfw_service_instance_id,
		  (QMI_TYPE_REQ | QMI_WLFW_WLAN_CFG_REQ_V01), ret,
		  resp_error_msg);

	ret = qmi_txn_init(&plat_priv->qmi_wlfw, &txn,
			   wlfw_wlan_cfg_resp_msg_v01_ei, resp);
	if (ret < 0) {
		cnss_pr_err("Failed to initialize txn for WLAN config request, err: %d\n",
			    ret);
		goto out;
	}

	ret = qmi_send_request(&plat_priv->qmi_wlfw, NULL, &txn,
			       QMI_WLFW_WLAN_CFG_REQ_V01,
			       WLFW_WLAN_CFG_REQ_MSG_V01_MAX_MSG_LEN,
			       wlfw_wlan_cfg_req_msg_v01_ei, req);
	if (ret < 0) {
		qmi_txn_cancel(&txn);
		cnss_pr_err("Failed to send WLAN config request, err: %d\n",
			    ret);
		goto out;
	}

	ret = qmi_txn_wait(&txn, QMI_WLFW_TIMEOUT_JF);
	if (ret < 0) {
		resp_error_msg = -QMI_RESULT_FAILURE_V01;
		cnss_pr_err("Failed to wait for response of WLAN config request, err: %d\n",
			    ret);
		goto out;
	}

	if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
		cnss_pr_err("WLAN config request failed, result: %d, err: %d\n",
			    resp->resp.result, resp->resp.error);
		ret = -resp->resp.result;
		resp_error_msg = resp->resp.error;
		goto out;
	}
	qmi_record(plat_priv->wlfw_service_instance_id,
		  (QMI_TYPE_RESP | QMI_WLFW_WLAN_CFG_REQ_V01), ret,
		  resp_error_msg);

	kfree(req);
	kfree(resp);
	return 0;

out:
	qmi_record(plat_priv->wlfw_service_instance_id,
		  (QMI_WLFW_WLAN_CFG_REQ_V01), ret,
		  resp_error_msg);
	CNSS_ASSERT(0);
	kfree(req);
	kfree(resp);
	return ret;
}

int cnss_wlfw_athdiag_read_send_sync(struct cnss_plat_data *plat_priv,
				     u32 offset, u32 mem_type,
				     u32 data_len, u8 *data)
{
	struct wlfw_athdiag_read_req_msg_v01 *req;
	struct wlfw_athdiag_read_resp_msg_v01 *resp;
	struct qmi_txn txn;
	int ret = 0;

	if (!plat_priv)
		return -ENODEV;

	if (!data || data_len == 0 || data_len > QMI_WLFW_MAX_DATA_SIZE_V01) {
		cnss_pr_err("Invalid parameters for athdiag read: data %p, data_len %u\n",
			    data, data_len);
		return -EINVAL;
	}

	cnss_pr_dbg("athdiag read: state 0x%lx, offset %x, mem_type %x, data_len %u\n",
		    plat_priv->driver_state, offset, mem_type, data_len);

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	resp = kzalloc(sizeof(*resp), GFP_KERNEL);
	if (!resp) {
		kfree(req);
		return -ENOMEM;
	}

	req->offset = offset;
	req->mem_type = mem_type;
	req->data_len = data_len;

	ret = qmi_txn_init(&plat_priv->qmi_wlfw, &txn,
			   wlfw_athdiag_read_resp_msg_v01_ei, resp);
	if (ret < 0) {
		cnss_pr_err("Failed to initialize txn for athdiag read request, err: %d\n",
			    ret);
		goto out;
	}

	ret = qmi_send_request(&plat_priv->qmi_wlfw, NULL, &txn,
			       QMI_WLFW_ATHDIAG_READ_REQ_V01,
			       WLFW_ATHDIAG_READ_REQ_MSG_V01_MAX_MSG_LEN,
			       wlfw_athdiag_read_req_msg_v01_ei, req);
	if (ret < 0) {
		qmi_txn_cancel(&txn);
		cnss_pr_err("Failed to send athdiag read request, err: %d\n",
			    ret);
		goto out;
	}

	ret = qmi_txn_wait(&txn, QMI_WLFW_TIMEOUT_JF);
	if (ret < 0) {
		cnss_pr_err("Failed to wait for response of athdiag read request, err: %d\n",
			    ret);
		goto out;
	}

	if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
		cnss_pr_err("Athdiag read request failed, result: %d, err: %d\n",
			    resp->resp.result, resp->resp.error);
		ret = -resp->resp.result;
		goto out;
	}

	if (!resp->data_valid || resp->data_len != data_len) {
		cnss_pr_err("athdiag read data is invalid, data_valid = %u, data_len = %u\n",
			    resp->data_valid, resp->data_len);
		ret = -EINVAL;
		goto out;
	}

	memcpy(data, resp->data, resp->data_len);

	kfree(req);
	kfree(resp);
	return 0;

out:
	kfree(req);
	kfree(resp);
	return ret;
}

int cnss_wlfw_athdiag_write_send_sync(struct cnss_plat_data *plat_priv,
				      u32 offset, u32 mem_type,
				      u32 data_len, u8 *data)
{
	struct wlfw_athdiag_write_req_msg_v01 *req;
	struct wlfw_athdiag_write_resp_msg_v01 *resp;
	struct qmi_txn txn;
	int ret = 0;

	if (!plat_priv)
		return -ENODEV;

	if (!data || data_len == 0 || data_len > QMI_WLFW_MAX_DATA_SIZE_V01) {
		cnss_pr_err("Invalid parameters for athdiag write: data %p, data_len %u\n",
			    data, data_len);
		return -EINVAL;
	}

	cnss_pr_dbg("athdiag write: state 0x%lx, offset %x, mem_type %x, data_len %u, data %p\n",
		    plat_priv->driver_state, offset, mem_type, data_len, data);

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	resp = kzalloc(sizeof(*resp), GFP_KERNEL);
	if (!resp) {
		kfree(req);
		return -ENOMEM;
	}

	req->offset = offset;
	req->mem_type = mem_type;
	req->data_len = data_len;
	memcpy(req->data, data, data_len);

	ret = qmi_txn_init(&plat_priv->qmi_wlfw, &txn,
			   wlfw_athdiag_write_resp_msg_v01_ei, resp);
	if (ret < 0) {
		cnss_pr_err("Failed to initialize txn for athdiag write request, err: %d\n",
			    ret);
		goto out;
	}

	ret = qmi_send_request(&plat_priv->qmi_wlfw, NULL, &txn,
			       QMI_WLFW_ATHDIAG_WRITE_REQ_V01,
			       WLFW_ATHDIAG_WRITE_REQ_MSG_V01_MAX_MSG_LEN,
			       wlfw_athdiag_write_req_msg_v01_ei, req);
	if (ret < 0) {
		qmi_txn_cancel(&txn);
		cnss_pr_err("Failed to send athdiag write request, err: %d\n",
			    ret);
		goto out;
	}

	ret = qmi_txn_wait(&txn, QMI_WLFW_TIMEOUT_JF);
	if (ret < 0) {
		cnss_pr_err("Failed to wait for response of athdiag write request, err: %d\n",
			    ret);
		goto out;
	}

	if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
		cnss_pr_err("Athdiag write request failed, result: %d, err: %d\n",
			    resp->resp.result, resp->resp.error);
		ret = -resp->resp.result;
		goto out;
	}

	kfree(req);
	kfree(resp);
	return 0;

out:
	kfree(req);
	kfree(resp);
	return ret;
}

int cnss_wlfw_ini_send_sync(struct cnss_plat_data *plat_priv,
			    u8 fw_log_mode)
{
	struct wlfw_ini_req_msg_v01 *req;
	struct wlfw_ini_resp_msg_v01 *resp;
	struct qmi_txn txn;
	int ret = 0;
	int resp_error_msg = 0;

	if (!plat_priv)
		return -ENODEV;

	cnss_pr_dbg("Sending ini sync request, state: 0x%lx, fw_log_mode: %d\n",
		    plat_priv->driver_state, fw_log_mode);

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	resp = kzalloc(sizeof(*resp), GFP_KERNEL);
	if (!resp) {
		kfree(req);
		return -ENOMEM;
	}

	req->enablefwlog_valid = 1;
	req->enablefwlog = fw_log_mode;

	qmi_record(plat_priv->wlfw_service_instance_id,
		  (QMI_TYPE_REQ | QMI_WLFW_INI_REQ_V01), ret,
		  resp_error_msg);

	ret = qmi_txn_init(&plat_priv->qmi_wlfw, &txn,
			   wlfw_ini_resp_msg_v01_ei, resp);
	if (ret < 0) {
		cnss_pr_err("Failed to initialize txn for ini request, fw_log_mode: %d, err: %d\n",
			    fw_log_mode, ret);
		goto out;
	}

	ret = qmi_send_request(&plat_priv->qmi_wlfw, NULL, &txn,
			       QMI_WLFW_INI_REQ_V01,
			       WLFW_INI_REQ_MSG_V01_MAX_MSG_LEN,
			       wlfw_ini_req_msg_v01_ei, req);
	if (ret < 0) {
		qmi_txn_cancel(&txn);
		cnss_pr_err("Failed to send ini request, fw_log_mode: %d, err: %d\n",
			    fw_log_mode, ret);
		goto out;
	}

	ret = qmi_txn_wait(&txn, QMI_WLFW_TIMEOUT_JF);
	if (ret < 0) {
		resp_error_msg = -QMI_RESULT_FAILURE_V01;
		cnss_pr_err("Failed to wait for response of ini request, fw_log_mode: %d, err: %d\n",
			    fw_log_mode, ret);
		goto out;
	}

	if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
		cnss_pr_err("Ini request failed, fw_log_mode: %d, result: %d, err: %d\n",
			    fw_log_mode, resp->resp.result, resp->resp.error);
		ret = -resp->resp.result;
		resp_error_msg = resp->resp.error;
		goto out;
	}

	qmi_record(plat_priv->wlfw_service_instance_id,
		  (QMI_TYPE_RESP | QMI_WLFW_INI_REQ_V01), ret,
		  resp_error_msg);

	kfree(req);
	kfree(resp);
	return 0;

out:
	qmi_record(plat_priv->wlfw_service_instance_id,
		  (QMI_WLFW_INI_REQ_V01), ret,
		  resp_error_msg);

	kfree(req);
	kfree(resp);
	return ret;
}

int cnss_wlfw_antenna_switch_send_sync(struct cnss_plat_data *plat_priv)
{
	struct wlfw_antenna_switch_req_msg_v01 *req;
	struct wlfw_antenna_switch_resp_msg_v01 *resp;
	struct qmi_txn txn;
	int ret = 0;
	int resp_error_msg = 0;

	if (!plat_priv)
		return -ENODEV;

	cnss_pr_dbg("Sending antenna switch sync request, state: 0x%lx\n",
		    plat_priv->driver_state);

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	resp = kzalloc(sizeof(*resp), GFP_KERNEL);
	if (!resp) {
		kfree(req);
		return -ENOMEM;
	}

	qmi_record(plat_priv->wlfw_service_instance_id,
		  (QMI_TYPE_REQ | QMI_WLFW_ANTENNA_SWITCH_REQ_V01), ret,
		  resp_error_msg);
	ret = qmi_txn_init(&plat_priv->qmi_wlfw, &txn,
			   wlfw_antenna_switch_resp_msg_v01_ei, resp);
	if (ret < 0) {
		cnss_pr_err("Failed to initialize txn for antenna switch request, err: %d\n",
			    ret);
		goto out;
	}

	ret = qmi_send_request(&plat_priv->qmi_wlfw, NULL, &txn,
			       QMI_WLFW_ANTENNA_SWITCH_REQ_V01,
			       WLFW_ANTENNA_SWITCH_REQ_MSG_V01_MAX_MSG_LEN,
			       wlfw_antenna_switch_req_msg_v01_ei, req);
	if (ret < 0) {
		qmi_txn_cancel(&txn);
		cnss_pr_err("Failed to send antenna switch request, err: %d\n",
			    ret);
		goto out;
	}

	ret = qmi_txn_wait(&txn, QMI_WLFW_TIMEOUT_JF);
	if (ret < 0) {
		resp_error_msg = -QMI_RESULT_FAILURE_V01;
		cnss_pr_err("Failed to wait for response of antenna switch request, err: %d\n",
			    ret);
		goto out;
	}

	if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
		cnss_pr_err("Antenna switch request failed, result: %d, err: %d\n",
			    resp->resp.result, resp->resp.error);
		ret = -resp->resp.result;
		resp_error_msg = resp->resp.error;
		goto out;
	}

	if (resp->antenna_valid)
		plat_priv->antenna = resp->antenna;

	cnss_pr_dbg("Antenna valid: %u, antenna 0x%llx\n",
		    resp->antenna_valid, resp->antenna);

	qmi_record(plat_priv->wlfw_service_instance_id,
		  (QMI_TYPE_RESP | QMI_WLFW_ANTENNA_SWITCH_REQ_V01), ret,
		  resp_error_msg);
	kfree(req);
	kfree(resp);
	return 0;

out:
	qmi_record(plat_priv->wlfw_service_instance_id,
		  (QMI_WLFW_ANTENNA_SWITCH_REQ_V01), ret,
		  resp_error_msg);
	kfree(req);
	kfree(resp);
	return ret;
}

int cnss_wlfw_antenna_grant_send_sync(struct cnss_plat_data *plat_priv)
{
	struct wlfw_antenna_grant_req_msg_v01 *req;
	struct wlfw_antenna_grant_resp_msg_v01 *resp;
	struct qmi_txn txn;
	int ret = 0;

	if (!plat_priv)
		return -ENODEV;

	cnss_pr_dbg("Sending antenna grant sync request, state: 0x%lx, grant 0x%llx\n",
		    plat_priv->driver_state, plat_priv->grant);

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	resp = kzalloc(sizeof(*resp), GFP_KERNEL);
	if (!resp) {
		kfree(req);
		return -ENOMEM;
	}

	req->grant_valid = 1;
	req->grant = plat_priv->grant;

	ret = qmi_txn_init(&plat_priv->qmi_wlfw, &txn,
			   wlfw_antenna_grant_resp_msg_v01_ei, resp);
	if (ret < 0) {
		cnss_pr_err("Failed to initialize txn for antenna grant request, err: %d\n",
			    ret);
		goto out;
	}

	ret = qmi_send_request(&plat_priv->qmi_wlfw, NULL, &txn,
			       QMI_WLFW_ANTENNA_GRANT_REQ_V01,
			       WLFW_ANTENNA_GRANT_REQ_MSG_V01_MAX_MSG_LEN,
			       wlfw_antenna_grant_req_msg_v01_ei, req);
	if (ret < 0) {
		qmi_txn_cancel(&txn);
		cnss_pr_err("Failed to send antenna grant request, err: %d\n",
			    ret);
		goto out;
	}

	ret = qmi_txn_wait(&txn, QMI_WLFW_TIMEOUT_JF);
	if (ret < 0) {
		cnss_pr_err("Failed to wait for response of antenna grant request, err: %d\n",
			    ret);
		goto out;
	}

	if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
		cnss_pr_err("Antenna grant request failed, result: %d, err: %d\n",
			    resp->resp.result, resp->resp.error);
		ret = -resp->resp.result;
		goto out;
	}

	kfree(req);
	kfree(resp);
	return 0;

out:
	kfree(req);
	kfree(resp);
	return ret;
}

int cnss_wlfw_qdss_trace_mem_info_send_sync(struct cnss_plat_data *plat_priv)
{
	struct wlfw_qdss_trace_mem_info_req_msg_v01 *req;
	struct wlfw_qdss_trace_mem_info_resp_msg_v01 *resp;
	struct qmi_txn txn;
	struct cnss_fw_mem *qdss_mem = plat_priv->qdss_mem;
	struct qdss_stream_data *qdss_stream = &plat_priv->qdss_stream;
	int i;
	int total_ents = 1;
	int ret = 0;
	int resp_error_msg = 0;
	uint32_t pte_n = 0;
	uint32_t *virt_st_tbl, *virt_pte;
	phys_addr_t phys_pte;

	virt_st_tbl = qdss_stream->qdss_vaddr;
	cnss_pr_dbg("Sending QDSS trace mem info, state: 0x%lx\n",
		    plat_priv->driver_state);

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	resp = kzalloc(sizeof(*resp), GFP_KERNEL);
	if (!resp) {
		kfree(req);
		return -ENOMEM;
	}

	req->mem_seg_len = plat_priv->qdss_mem_seg_len;
	if (plat_priv->qdss_etr_sg_mode)
		total_ents = DIV_ROUND_UP(qdss_mem[0].size, PAGE_SIZE);

	while (pte_n < total_ents) {
		if (plat_priv->qdss_etr_sg_mode) {
			req->mem_seg_len = ((total_ents - pte_n) >
				QMI_WLFW_MAX_NUM_MEM_SEG_V01) ?
				QMI_WLFW_MAX_NUM_MEM_SEG_V01 :
				(total_ents - pte_n);
			req->end_valid = 1;
			if (req->mem_seg_len != QMI_WLFW_MAX_NUM_MEM_SEG_V01)
				req->end = 1;
			for (i = 0; i < req->mem_seg_len; i++) {
				virt_pte = virt_st_tbl + pte_n;
				phys_pte = CNSS_ETR_SG_ENT_TO_BLK(*virt_pte);
				req->mem_seg[i].addr = phys_pte;
				req->mem_seg[i].size = PAGE_SIZE;
				req->mem_seg[i].type = qdss_mem[0].type;
				pte_n++;
			}
		} else {
			for (i = 0; i < req->mem_seg_len; i++) {
				cnss_pr_dbg("Memory for FW, pa: 0x%x, size: 0x%x, type: %u\n",
						(unsigned int)qdss_mem[i].pa,
						(unsigned int)qdss_mem[i].size,
						qdss_mem[i].type);
				req->mem_seg[i].addr = qdss_mem[i].pa;
				req->mem_seg[i].size = qdss_mem[i].size;
				req->mem_seg[i].type = qdss_mem[i].type;
				pte_n++;
			}
		}

		qmi_record(plat_priv->wlfw_service_instance_id,
			  (QMI_TYPE_REQ | QMI_WLFW_QDSS_TRACE_MEM_INFO_REQ_V01),
			  ret, resp_error_msg);
		ret = qmi_txn_init(&plat_priv->qmi_wlfw, &txn,
				wlfw_qdss_trace_mem_info_resp_msg_v01_ei, resp);
		if (ret < 0) {
			cnss_pr_err("Fail to initialize txn for QDSS trace mem request: err %d\n",
					ret);
			goto out;
		}

		ret = qmi_send_request(&plat_priv->qmi_wlfw, NULL, &txn,
			QMI_WLFW_QDSS_TRACE_MEM_INFO_REQ_V01,
			WLFW_QDSS_TRACE_MEM_INFO_REQ_MSG_V01_MAX_MSG_LEN,
			wlfw_qdss_trace_mem_info_req_msg_v01_ei, req);
		if (ret < 0) {
			qmi_txn_cancel(&txn);
			cnss_pr_err("Fail to send QDSS trace mem info request: err %d\n",
					ret);
			goto out;
		}

		ret = qmi_txn_wait(&txn, QMI_WLFW_TIMEOUT_JF);
		if (ret < 0) {
			resp_error_msg = -QMI_RESULT_FAILURE_V01;
			cnss_pr_err("Fail to wait for response of QDSS trace mem info request, err %d\n",
					ret);
			goto out;
		}

		if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
			cnss_pr_err("QDSS trace mem info request failed, result: %d, err: %d\n",
					resp->resp.result, resp->resp.error);
			ret = -resp->resp.result;
			resp_error_msg = resp->resp.error;
			goto out;
		}
	}

	qmi_record(plat_priv->wlfw_service_instance_id,
		  (QMI_TYPE_RESP | QMI_WLFW_QDSS_TRACE_MEM_INFO_REQ_V01), ret,
		  resp_error_msg);
	kfree(req);
	kfree(resp);
	return 0;

out:
	qmi_record(plat_priv->wlfw_service_instance_id,
		  (QMI_WLFW_QDSS_TRACE_MEM_INFO_REQ_V01), ret,
		  resp_error_msg);
	kfree(req);
	kfree(resp);
	return ret;
}

int cnss_wlfw_qdss_dnld_send_sync(struct cnss_plat_data *plat_priv)
{
	struct wlfw_qdss_trace_config_download_req_msg_v01 *req;
	struct wlfw_qdss_trace_config_download_resp_msg_v01 *resp;
	struct qmi_txn txn;
	const struct firmware *fw_entry = NULL;
	const u8 *temp;
	char default_cfg_file_name[MAX_QDSS_CONFIG_FILE_NAME];
	char custom_cfg_filename[MAX_QDSS_CONFIG_FILE_NAME];
	unsigned int remaining;
	int ret = 0, resp_error_msg = 0;

	if (!(test_bit(CNSS_FW_READY, &plat_priv->driver_state) &&
	      (test_bit(CNSS_QMI_WLFW_CONNECTED, &plat_priv->driver_state)))) {
		cnss_pr_err("Invalid state to download QDSS config: 0x%lx\n",
			    plat_priv->driver_state);
		return -EINVAL;
	}

	if (test_bit(CNSS_QDSS_STARTED, &plat_priv->driver_state)) {
		cnss_pr_info("QDSS is already started: 0x%lx\n",
			     plat_priv->driver_state);
		return -EINVAL;
	}

	cnss_pr_info("Sending QDSS config download message, state: 0x%lx\n",
		     plat_priv->driver_state);

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	resp = kzalloc(sizeof(*resp), GFP_KERNEL);
	if (!resp) {
		kfree(req);
		return -ENOMEM;
	}

	/* Per device custom QDSS config file name format is
	 * qdss_trace_config_QCNXXXX_PCIX.bin in /lib/firmware/
	 */
	snprintf(custom_cfg_filename, sizeof(custom_cfg_filename), "%s_%s%s",
		 QDSS_CONFIG_FILE_PREFIX, plat_priv->device_name,
		 QDSS_CONFIG_FILE_SUFFIX);

	/* Default QDSS config file present in
	 * /lib/firmware/<fw_path>/qdss_trace_config.bin
	 */
	snprintf(default_cfg_file_name, sizeof(default_cfg_file_name), "%s%s%s",
		 cnss_get_fw_path(plat_priv),
		 QDSS_CONFIG_FILE_PREFIX, QDSS_CONFIG_FILE_SUFFIX);

	/* Falling back to sysfs helper would cause delay, use direct */
	ret = request_firmware_direct(&fw_entry, custom_cfg_filename,
				      &plat_priv->plat_dev->dev);
	if (ret) {
		cnss_pr_info("No Custom QDSS config found, loading default file %s\n",
			     default_cfg_file_name);

		ret = request_firmware_direct(&fw_entry, default_cfg_file_name,
					      &plat_priv->plat_dev->dev);
		if (ret) {
			cnss_pr_err("Failed to load QDSS Config: %s ret:%d\n",
				     default_cfg_file_name, ret);
			goto err_req_fw;
		}
	}

	temp = fw_entry->data;
	remaining = fw_entry->size;

	cnss_pr_dbg("Downloading QDSS Config file of size: %u\n", remaining);
	qmi_record(plat_priv->wlfw_service_instance_id,
		  (QMI_TYPE_REQ | QMI_WLFW_QDSS_TRACE_CONFIG_DOWNLOAD_REQ_V01),
		  ret, resp_error_msg);

	while (remaining) {
		req->total_size_valid = 1;
		req->total_size = remaining;
		req->seg_id_valid = 1;
		req->data_valid = 1;
		req->end_valid = 1;

		if (remaining > QMI_WLFW_MAX_DATA_SIZE_V01) {
			req->data_len = QMI_WLFW_MAX_DATA_SIZE_V01;
		} else {
			req->data_len = remaining;
			req->end = 1;
		}

		memcpy(req->data, temp, req->data_len);

		ret = qmi_txn_init(&plat_priv->qmi_wlfw, &txn,
				wlfw_qdss_trace_config_download_resp_msg_v01_ei,
				resp);
		if (ret < 0) {
			cnss_pr_err("Failed to initialize txn for QDSS download request, err: %d\n",
				    ret);
			goto err_send;
		}

		ret = qmi_send_request(&plat_priv->qmi_wlfw, NULL, &txn,
			QMI_WLFW_QDSS_TRACE_CONFIG_DOWNLOAD_REQ_V01,
			WLFW_QDSS_TRACE_CONFIG_DOWNLOAD_REQ_MSG_V01_MAX_MSG_LEN,
			wlfw_qdss_trace_config_download_req_msg_v01_ei,
			req);
		if (ret < 0) {
			qmi_txn_cancel(&txn);
			cnss_pr_err("Failed to send respond QDSS download request, err: %d\n",
				    ret);
			goto err_send;
		}

		ret = qmi_txn_wait(&txn, QMI_WLFW_TIMEOUT_JF);
		if (ret < 0) {
			resp_error_msg = -QMI_RESULT_FAILURE_V01;
			cnss_pr_err("Failed to wait for response of QDSS download request, err: %d\n",
				    ret);
			goto err_send;
		}

		if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
			cnss_pr_err("QDSS download request failed, result: %d, err: %d\n",
				    resp->resp.result, resp->resp.error);
			ret = -resp->resp.result;
			resp_error_msg = resp->resp.error;
			goto err_send;
		}

		remaining -= req->data_len;
		temp += req->data_len;
		req->seg_id++;
	}

	qmi_record(plat_priv->wlfw_service_instance_id,
		  (QMI_TYPE_RESP | QMI_WLFW_QDSS_TRACE_CONFIG_DOWNLOAD_REQ_V01),
		   ret, resp_error_msg);
	release_firmware(fw_entry);

	kfree(req);
	kfree(resp);
	return ret;

err_send:
	qmi_record(plat_priv->wlfw_service_instance_id,
		  (QMI_WLFW_QDSS_TRACE_CONFIG_DOWNLOAD_REQ_V01),
		   ret, resp_error_msg);

	release_firmware(fw_entry);

err_req_fw:
	kfree(req);
	kfree(resp);
	return ret;
}

int cnss_wlfw_send_qdss_trace_mode_req(struct cnss_plat_data *plat_priv,
				       enum wlfw_qdss_trace_mode_enum_v01 mode,
				       u64 option)
{
	struct wlfw_qdss_trace_mode_req_msg_v01 *req;
	struct wlfw_qdss_trace_mode_resp_msg_v01 *resp;
	struct qmi_txn txn;
	int ret = 0, resp_error_msg = 0;

	if (!plat_priv)
		return -ENODEV;

	if (!(test_bit(CNSS_FW_READY, &plat_priv->driver_state) &&
	      (test_bit(CNSS_QMI_WLFW_CONNECTED, &plat_priv->driver_state)))) {
		cnss_pr_err("Invalid state for QDSS Mode Message: 0x%lx\n",
			    plat_priv->driver_state);
		return -EINVAL;
	}

	if (!test_bit(CNSS_QDSS_STARTED, &plat_priv->driver_state) &&
	    mode == QMI_WLFW_QDSS_TRACE_OFF_V01) {
		cnss_pr_info("QDSS not started, ignoring stop command. 0x%lx\n",
			     plat_priv->driver_state);
		return -EINVAL;
	}

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	resp = kzalloc(sizeof(*resp), GFP_KERNEL);
	if (!resp) {
		kfree(req);
		return -ENOMEM;
	}

	req->mode_valid = 1;
	req->mode = mode;
	req->option_valid = 1;
	if (plat_priv->qdss_etr_sg_mode)
		req->option = plat_priv->qdss_etr_sg_mode;
	else
		req->option = option;
	req->hw_trc_disable_override_valid = 0;

	cnss_pr_info("Sending QDSS Mode %u, option %llu", mode, option);

	qmi_record(plat_priv->wlfw_service_instance_id,
		  (QMI_TYPE_REQ | QMI_WLFW_QDSS_TRACE_MODE_REQ_V01), ret,
		  resp_error_msg);
	ret = qmi_txn_init(&plat_priv->qmi_wlfw, &txn,
			   wlfw_qdss_trace_mode_resp_msg_v01_ei, resp);
	if (ret < 0) {
		cnss_pr_err("Fail to init txn for QDSS Mode resp %d\n",
			    ret);
		goto err_send;
	}

	ret = qmi_send_request(&plat_priv->qmi_wlfw, NULL, &txn,
			       QMI_WLFW_QDSS_TRACE_MODE_REQ_V01,
			       WLFW_QDSS_TRACE_MODE_REQ_MSG_V01_MAX_MSG_LEN,
			       wlfw_qdss_trace_mode_req_msg_v01_ei, req);
	if (ret < 0) {
		qmi_txn_cancel(&txn);
		cnss_pr_err("Fail to send QDSS Mode req %d\n", ret);
		goto err_send;
	}

	ret = qmi_txn_wait(&txn, QMI_WLFW_TIMEOUT_JF);
	if (ret < 0) {
		resp_error_msg = -QMI_RESULT_FAILURE_V01;
		cnss_pr_err("QDSS Mode resp wait failed with rc %d\n",
			    ret);
		goto err_send;
	}

	if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
		cnss_pr_err("QMI QDSS Mode request rejected, result:%d error:%d\n",
			    resp->resp.result, resp->resp.error);
		ret = -resp->resp.result;
		resp_error_msg = resp->resp.error;
		goto err_send;
	}
	qmi_record(plat_priv->wlfw_service_instance_id,
		  (QMI_TYPE_RESP | QMI_WLFW_QDSS_TRACE_MODE_REQ_V01), ret,
		  resp_error_msg);
	goto out;

err_send:
	qmi_record(plat_priv->wlfw_service_instance_id,
		  (QMI_WLFW_QDSS_TRACE_MODE_REQ_V01), ret,
		  resp_error_msg);

out:
	kfree(resp);
	kfree(req);

	/* If cnsscli command is issued to start QDSS when wifi down is in
	 * progress, there is a chance QMI message might fail and ECONNRESET
	 * would be returned. Also, if cnsscli command is issued before VAPs
	 * are created for AHB targets, FW would return INCOMPATIBLE state
	 * error and sometimes leads to resp wait timeout error.
	 * Avoiding assert for all these cases.
	 */

	if (ret < 0 && ret != -ECONNRESET && ret != -ETIMEDOUT &&
	    resp_error_msg != QMI_ERR_INCOMPATIBLE_STATE_V01)
		CNSS_ASSERT(0);

	if (mode == QMI_WLFW_QDSS_TRACE_ON_V01)
		set_bit(CNSS_QDSS_STARTED, &plat_priv->driver_state);
	else if (mode == QMI_WLFW_QDSS_TRACE_OFF_V01)
		clear_bit(CNSS_QDSS_STARTED, &plat_priv->driver_state);

	return ret;
}

#ifdef CNSS2_IMS
static int cnss_wlfw_wfc_call_status_send_sync(struct cnss_plat_data *plat_priv,
					       u32 data_len, const void *data)
{
	struct wlfw_wfc_call_status_req_msg_v01 *req;
	struct wlfw_wfc_call_status_resp_msg_v01 *resp;
	struct qmi_txn txn;
	int ret = 0;

	cnss_pr_dbg("Sending WFC call status: state: 0x%lx\n",
		    plat_priv->driver_state);

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	resp = kzalloc(sizeof(*resp), GFP_KERNEL);
	if (!resp) {
		kfree(req);
		return -ENOMEM;
	}

	req->wfc_call_status_len = data_len;
	memcpy(req->wfc_call_status, data, req->wfc_call_status_len);

	ret = qmi_txn_init(&plat_priv->qmi_wlfw, &txn,
			   wlfw_wfc_call_status_resp_msg_v01_ei, resp);
	if (ret < 0) {
		cnss_pr_err("Fail to initialize txn for WFC call status request: err %d\n",
			    ret);
		goto out;
	}

	ret = qmi_send_request(&plat_priv->qmi_wlfw, NULL, &txn,
			       QMI_WLFW_WFC_CALL_STATUS_REQ_V01,
			       WLFW_WFC_CALL_STATUS_REQ_MSG_V01_MAX_MSG_LEN,
			       wlfw_wfc_call_status_req_msg_v01_ei, req);
	if (ret < 0) {
		qmi_txn_cancel(&txn);
		cnss_pr_err("Fail to send WFC call status request: err %d\n",
			    ret);
		goto out;
	}

	ret = qmi_txn_wait(&txn, QMI_WLFW_TIMEOUT_JF);
	if (ret < 0) {
		cnss_pr_err("Fail to wait for response of WFC call status request, err %d\n",
			    ret);
		goto out;
	}

	if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
		cnss_pr_err("WFC call status request failed, result: %d, err: %d\n",
			    resp->resp.result, resp->resp.error);
		ret = -resp->resp.result;
		goto out;
	}

	kfree(req);
	kfree(resp);
	return 0;

out:
	kfree(req);
	kfree(resp);
	return ret;
}
#endif

int cnss_wlfw_dynamic_feature_mask_send_sync(struct cnss_plat_data *plat_priv)
{
	struct wlfw_dynamic_feature_mask_req_msg_v01 *req;
	struct wlfw_dynamic_feature_mask_resp_msg_v01 *resp;
	struct qmi_txn txn;
	int ret = 0;

	cnss_pr_dbg("Sending dynamic feature mask 0x%llx, state: 0x%lx\n",
		    plat_priv->dynamic_feature,
		    plat_priv->driver_state);

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	resp = kzalloc(sizeof(*resp), GFP_KERNEL);
	if (!resp) {
		kfree(req);
		return -ENOMEM;
	}

	req->mask_valid = 1;
	req->mask = plat_priv->dynamic_feature;

	ret = qmi_txn_init(&plat_priv->qmi_wlfw, &txn,
			   wlfw_dynamic_feature_mask_resp_msg_v01_ei, resp);
	if (ret < 0) {
		cnss_pr_err("Fail to initialize txn for dynamic feature mask request: err %d\n",
			    ret);
		goto out;
	}

	ret = qmi_send_request
		(&plat_priv->qmi_wlfw, NULL, &txn,
		 QMI_WLFW_DYNAMIC_FEATURE_MASK_REQ_V01,
		 WLFW_DYNAMIC_FEATURE_MASK_REQ_MSG_V01_MAX_MSG_LEN,
		 wlfw_dynamic_feature_mask_req_msg_v01_ei, req);
	if (ret < 0) {
		qmi_txn_cancel(&txn);
		cnss_pr_err("Fail to send dynamic feature mask request: err %d\n",
			    ret);
		goto out;
	}

	ret = qmi_txn_wait(&txn, QMI_WLFW_TIMEOUT_JF);
	if (ret < 0) {
		cnss_pr_err("Fail to wait for response of dynamic feature mask request, err %d\n",
			    ret);
		goto out;
	}

	if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
		cnss_pr_err("Dynamic feature mask request failed, result: %d, err: %d\n",
			    resp->resp.result, resp->resp.error);
		ret = -resp->resp.result;
		goto out;
	}

out:
	kfree(req);
	kfree(resp);
	return ret;
}

int cnss_wlfw_m3_dump_upload_done_send_sync(struct cnss_plat_data *plat_priv,
					    u32 pdev_id, int status)
{
	struct wlfw_m3_dump_upload_done_req_msg_v01 *req;
	struct wlfw_m3_dump_upload_done_resp_msg_v01 *resp;
	struct qmi_txn txn;
	int ret = 0;
	int resp_error_msg = 0;

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	resp = kzalloc(sizeof(*resp), GFP_KERNEL);
	if (!resp) {
		kfree(req);
		return -ENOMEM;
	}

	cnss_pr_dbg("Sending M3 Upload done req, pdev %d, status %d\n",
		    pdev_id, status);

	req->pdev_id = pdev_id;
	req->status = status;

	qmi_record(plat_priv->wlfw_service_instance_id,
		  (QMI_TYPE_REQ | QMI_WLFW_M3_DUMP_UPLOAD_DONE_REQ_V01), ret,
		  resp_error_msg);
	ret = qmi_txn_init(&plat_priv->qmi_wlfw, &txn,
			   wlfw_m3_dump_upload_done_resp_msg_v01_ei, resp);
	if (ret < 0) {
		cnss_pr_err("Fail to initialize txn for M3 dump upload done req: err %d\n",
			    ret);
		goto out;
	}

	ret = qmi_send_request(&plat_priv->qmi_wlfw, NULL, &txn,
			       QMI_WLFW_M3_DUMP_UPLOAD_DONE_REQ_V01,
			       WLFW_M3_DUMP_UPLOAD_DONE_REQ_MSG_V01_MAX_MSG_LEN,
			       wlfw_m3_dump_upload_done_req_msg_v01_ei, req);
	if (ret < 0) {
		qmi_txn_cancel(&txn);
		cnss_pr_err("Fail to send M3 dump upload done request: err %d\n",
			    ret);
		goto out;
	}

	ret = qmi_txn_wait(&txn, QMI_WLFW_TIMEOUT_JF);
	if (ret < 0) {
		resp_error_msg = -QMI_RESULT_FAILURE_V01;
		cnss_pr_err("Fail to wait for response of M3 dump upload done request, err %d\n",
			    ret);
		goto out;
	}

	if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
		cnss_pr_err("M3 Dump Upload Done Req failed, result: %d, err: 0x%X\n",
			    resp->resp.result, resp->resp.error);
		ret = -resp->resp.result;
		resp_error_msg = resp->resp.error;
		goto out;
	}

	qmi_record(plat_priv->wlfw_service_instance_id,
		  (QMI_TYPE_RESP | QMI_WLFW_M3_DUMP_UPLOAD_DONE_REQ_V01), ret,
		  resp_error_msg);
	kfree(req);
	kfree(resp);
	return ret;

out:
	qmi_record(plat_priv->wlfw_service_instance_id,
		  (QMI_WLFW_M3_DUMP_UPLOAD_DONE_REQ_V01), ret,
		  resp_error_msg);
	kfree(req);
	kfree(resp);
	return ret;
}

int cnss_wlfw_device_info_send_sync(struct cnss_plat_data *plat_priv)
{
	struct wlfw_device_info_req_msg_v01 *req;
	struct wlfw_device_info_resp_msg_v01 *resp;
	struct qmi_txn txn;
	int ret = 0;
	int resp_error_msg = 0;

	cnss_pr_dbg("Sending Device Info request message, state: 0x%lx\n",
		    plat_priv->driver_state);

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	resp = kzalloc(sizeof(*resp), GFP_KERNEL);
	if (!resp) {
		kfree(req);
		return -ENOMEM;
	}

	qmi_record(plat_priv->wlfw_service_instance_id,
		  (QMI_TYPE_REQ | QMI_WLFW_DEVICE_INFO_REQ_V01), ret,
		  resp_error_msg);

	ret = qmi_txn_init(&plat_priv->qmi_wlfw, &txn,
			   wlfw_device_info_resp_msg_v01_ei, resp);
	if (ret < 0) {
		cnss_pr_err("Fail to init txn for Device Info req, err: %d\n",
			    ret);
		goto out;
	}

	ret = qmi_send_request(&plat_priv->qmi_wlfw, NULL, &txn,
			       QMI_WLFW_DEVICE_INFO_REQ_V01,
			       WLFW_DEVICE_INFO_REQ_MSG_V01_MAX_MSG_LEN,
			       wlfw_device_info_req_msg_v01_ei, req);
	if (ret < 0) {
		qmi_txn_cancel(&txn);
		cnss_pr_err("Fail to send device info req %d\n", ret);
		goto out;
	}

	ret = qmi_txn_wait(&txn, QMI_WLFW_TIMEOUT_JF);
	if (ret < 0) {
		resp_error_msg = -QMI_RESULT_FAILURE_V01;
		cnss_pr_err("Failed to wait for device info response err: %d\n",
			    ret);
		goto out;
	}

	if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
		cnss_pr_err("Device info request failed, result: %d error: %d\n",
			    resp->resp.result, resp->resp.error);
		ret = -resp->resp.result;
		resp_error_msg = resp->resp.error;
		goto out;
	}

	if (!resp->bar_addr_valid || !resp->bar_size_valid) {
		resp_error_msg = -QMI_RESULT_FAILURE_V01;
		cnss_pr_err("bar addr(%d) or bar size(%d) not received\n",
			    resp->bar_addr_valid, resp->bar_size_valid);
		ret = -EINVAL;
		goto out;
	}

	if (!resp->bar_addr ||
	    (resp->bar_size != QCN6122_DEVICE_BAR_SIZE)) {
		resp_error_msg = -QMI_RESULT_FAILURE_V01;
		cnss_pr_err("Invalid bar addr(0x%llx) or bar size (0x%x)\n",
			    resp->bar_addr, resp->bar_size);
		ret = -EINVAL;
		goto out;
	}

	plat_priv->tgt_data.bar_addr_pa = resp->bar_addr;
	plat_priv->tgt_data.bar_size = resp->bar_size;

#if (KERNEL_VERSION(5, 6, 0) < LINUX_VERSION_CODE)
	plat_priv->tgt_data.bar_addr_va =
		ioremap(plat_priv->tgt_data.bar_addr_pa,
			plat_priv->tgt_data.bar_size);
#else
	plat_priv->tgt_data.bar_addr_va =
		ioremap_nocache(plat_priv->tgt_data.bar_addr_pa,
				plat_priv->tgt_data.bar_size);
#endif

	if (!plat_priv->tgt_data.bar_addr_va) {
		cnss_pr_err("Ioremap failed for bar address\n");
		plat_priv->tgt_data.bar_addr_pa = 0;
		plat_priv->tgt_data.bar_size = 0;
		ret = -EIO;
		goto out;
	}

	cnss_pr_info("Device BAR Info pa: 0x%llx, va: 0x%p, size: 0x%x\n",
			plat_priv->tgt_data.bar_addr_pa,
			plat_priv->tgt_data.bar_addr_va,
			plat_priv->tgt_data.bar_size);

	qmi_record(plat_priv->wlfw_service_instance_id,
		  (QMI_TYPE_RESP | QMI_WLFW_DEVICE_INFO_REQ_V01), ret,
		  resp_error_msg);

	kfree(resp);
	kfree(req);
	return 0;

out:
	qmi_record(plat_priv->wlfw_service_instance_id,
		  (QMI_WLFW_DEVICE_INFO_REQ_V01), ret,
		  resp_error_msg);

	kfree(resp);
	kfree(req);
	if (ret < 0)
		CNSS_ASSERT(0);

	return ret;
}

unsigned int cnss_get_qmi_timeout(struct cnss_plat_data *plat_priv)
{
	cnss_pr_dbg("QMI timeout is %u ms\n", QMI_WLFW_TIMEOUT_MS);

	return QMI_WLFW_TIMEOUT_MS;
}
EXPORT_SYMBOL(cnss_get_qmi_timeout);

static void cnss_wlfw_request_mem_ind_cb(struct qmi_handle *qmi_wlfw,
					 struct sockaddr_qrtr *sq,
					 struct qmi_txn *txn, const void *data)
{
	struct cnss_plat_data *plat_priv =
		container_of(qmi_wlfw, struct cnss_plat_data, qmi_wlfw);
	const struct wlfw_request_mem_ind_msg_v01 *ind_msg = data;
	int i;

	cnss_pr_dbg("Received QMI WLFW request memory indication\n");

	qmi_record(plat_priv->wlfw_service_instance_id,
		   QMI_WLFW_REQUEST_MEM_IND_V01, 0, 0);
	if (!txn) {
		cnss_pr_err("Spurious indication\n");
		return;
	}
	if (ind_msg->mem_seg_len == 0 ||
		ind_msg->mem_seg_len > QMI_WLFW_MAX_NUM_MEM_SEG_V01)
		cnss_pr_err("Invalid memory segment length: %u\n",
			    ind_msg->mem_seg_len);

	cnss_pr_dbg("FW memory segment count is %u\n", ind_msg->mem_seg_len);
	plat_priv->fw_mem_seg_len = ind_msg->mem_seg_len;

	for (i = 0; i < plat_priv->fw_mem_seg_len; i++) {
		cnss_pr_dbg("FW requests for memory, size: 0x%x, type: %u\n",
			    ind_msg->mem_seg[i].size, ind_msg->mem_seg[i].type);
		plat_priv->fw_mem[i].type = ind_msg->mem_seg[i].type;
		plat_priv->fw_mem[i].size = ind_msg->mem_seg[i].size;
		if (plat_priv->fw_mem[i].type == CNSS_MEM_CAL_V01) {
			plat_priv->cal_mem = &plat_priv->fw_mem[i];
		}
	}

	cnss_driver_event_post(plat_priv, CNSS_DRIVER_EVENT_REQUEST_MEM,
			       0, NULL);
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0))
static void cnss_cal_report_download(struct cnss_plat_data *plat_priv)
{
}
#else
static void cnss_cal_report_download(struct cnss_plat_data *plat_priv)
{
	u32 cal_file_size = 0;

	if (is_ipc_qmi_client_connected(CNSS_PLAT_IPC_DAEMON_QMI_CLIENT_V01,
					0)) {
		if (plat_priv->cold_boot_support &&
		    plat_priv->cal_in_progress) {
			if (plat_priv->cal_mem && plat_priv->cal_mem->va) {
				cnss_cal_file_download_to_mem(plat_priv,
							      &cal_file_size);
				plat_priv->cal_file_size = cal_file_size;
				cnss_pr_dbg("%s: Cold boot support enabled. CALDB downloaded, file size %u\n",
					    __func__,
					    plat_priv->cal_file_size);
			} else
				cnss_pr_err("FW CALDB memory invalid, Unable to copy cal data to mem.");
		}
	}
}
#endif

static void cnss_wlfw_fw_mem_ready_ind_cb(struct qmi_handle *qmi_wlfw,
					  struct sockaddr_qrtr *sq,
					  struct qmi_txn *txn, const void *data)
{
	struct cnss_plat_data *plat_priv =
		container_of(qmi_wlfw, struct cnss_plat_data, qmi_wlfw);

	cnss_pr_dbg("Received QMI WLFW FW memory ready indication\n");

	if (!txn) {
		cnss_pr_err("Spurious indication\n");
		return;
	}

	cnss_cal_report_download(plat_priv);

	qmi_record(plat_priv->wlfw_service_instance_id,
		   QMI_WLFW_FW_MEM_READY_IND_V01, 0, 0);
	/* WAR Conditional check of driver state to hinder processing */
	/* FW_mem_ready msg i.e. received twice from QMI stack occasionally */
	if (!test_bit(CNSS_FW_MEM_READY, &plat_priv->driver_state))
		cnss_driver_event_post(plat_priv,
			CNSS_DRIVER_EVENT_FW_MEM_READY, 0, NULL);
	else
		cnss_pr_err("FW_Mem_Ready_Ind received twice\n");

}

static void cnss_wlfw_fw_ready_ind_cb(struct qmi_handle *qmi_wlfw,
				      struct sockaddr_qrtr *sq,
				      struct qmi_txn *txn, const void *data)
{
	struct cnss_plat_data *plat_priv =
		container_of(qmi_wlfw, struct cnss_plat_data, qmi_wlfw);
	struct cnss_cal_info *cal_info;

	cnss_pr_dbg("Received QMI WLFW FW ready indication\n");

	if (!txn) {
		cnss_pr_err("Spurious indication\n");
		return;
	}

	/* Return here as FW sends a different cold boot cal done indication
	 * in case of single QMI client.
	 */
	if (is_ipc_qmi_client_connected(CNSS_PLAT_IPC_DAEMON_QMI_CLIENT_V01, 0))
		return;

	cal_info = kzalloc(sizeof(*cal_info), GFP_KERNEL);
	if (!cal_info)
		return;

	qmi_record(plat_priv->wlfw_service_instance_id,
		   QMI_WLFW_FW_READY_IND_V01, 0, 0);
	cal_info->cal_status = CNSS_CAL_DONE;
	cnss_driver_event_post(plat_priv, CNSS_DRIVER_EVENT_COLD_BOOT_CAL_DONE,
			       0, cal_info);
}

static void cnss_wlfw_fw_init_done_ind_cb(struct qmi_handle *qmi_wlfw,
					  struct sockaddr_qrtr *sq,
					  struct qmi_txn *txn, const void *data)
{
	struct cnss_plat_data *plat_priv =
		container_of(qmi_wlfw, struct cnss_plat_data, qmi_wlfw);

	cnss_pr_dbg("Received QMI WLFW FW initialization done indication\n");

	if (!txn) {
		cnss_pr_err("Spurious indication\n");
		return;
	}
	qmi_record(plat_priv->wlfw_service_instance_id,
		   QMI_WLFW_FW_INIT_DONE_IND_V01, 0, 0);
	cnss_driver_event_post(plat_priv,
				CNSS_DRIVER_EVENT_FW_READY,
				0, NULL);
}

static void cnss_wlfw_pin_result_ind_cb(struct qmi_handle *qmi_wlfw,
					struct sockaddr_qrtr *sq,
					struct qmi_txn *txn, const void *data)
{
	struct cnss_plat_data *plat_priv =
		container_of(qmi_wlfw, struct cnss_plat_data, qmi_wlfw);
	const struct wlfw_pin_connect_result_ind_msg_v01 *ind_msg = data;

	cnss_pr_dbg("Received QMI WLFW pin connect result indication\n");

	qmi_record(plat_priv->wlfw_service_instance_id,
		   QMI_WLFW_PIN_CONNECT_RESULT_IND_V01, 0, 0);
	if (!txn) {
		cnss_pr_err("Spurious indication\n");
		return;
	}

	if (ind_msg->pwr_pin_result_valid)
		plat_priv->pin_result.fw_pwr_pin_result =
		    ind_msg->pwr_pin_result;
	if (ind_msg->phy_io_pin_result_valid)
		plat_priv->pin_result.fw_phy_io_pin_result =
		    ind_msg->phy_io_pin_result;
	if (ind_msg->rf_pin_result_valid)
		plat_priv->pin_result.fw_rf_pin_result = ind_msg->rf_pin_result;

	cnss_pr_dbg("Pin connect Result: pwr_pin: 0x%x phy_io_pin: 0x%x rf_io_pin: 0x%x\n",
		    ind_msg->pwr_pin_result, ind_msg->phy_io_pin_result,
		    ind_msg->rf_pin_result);
}

int cnss_wlfw_cal_report_req_send_sync(struct cnss_plat_data *plat_priv,
				       u32 cal_file_download_size)
{
	struct wlfw_cal_report_req_msg_v01 *req;
	struct wlfw_cal_report_resp_msg_v01 *resp;
	struct qmi_txn txn;
	int ret = 0;
	int resp_error_msg = 0;

	cnss_pr_dbg("Sending cal file report request. File size: %d, state: 0x%lx\n",
		    cal_file_download_size, plat_priv->driver_state);

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	resp = kzalloc(sizeof(*resp), GFP_KERNEL);
	if (!resp) {
		kfree(req);
		return -ENOMEM;
	}

	req->cal_file_download_size_valid = 1;
	req->cal_file_download_size = cal_file_download_size;

	qmi_record(plat_priv->wlfw_service_instance_id,
		  (QMI_TYPE_REQ | QMI_WLFW_CAL_REPORT_REQ_V01), ret,
		  resp_error_msg);

	ret = qmi_txn_init(&plat_priv->qmi_wlfw, &txn,
			   wlfw_cal_report_resp_msg_v01_ei, resp);
	if (ret < 0) {
		cnss_pr_err("Failed to initialize txn for Cal Report request, err: %d\n",
			    ret);
		goto out;
	}
	ret = qmi_send_request(&plat_priv->qmi_wlfw, NULL, &txn,
			       QMI_WLFW_CAL_REPORT_REQ_V01,
			       WLFW_CAL_REPORT_REQ_MSG_V01_MAX_MSG_LEN,
			       wlfw_cal_report_req_msg_v01_ei, req);
	if (ret < 0) {
		qmi_txn_cancel(&txn);
		cnss_pr_err("Failed to send Cal Report request, err: %d\n",
			    ret);
		goto out;
	}
	ret = qmi_txn_wait(&txn, QMI_WLFW_TIMEOUT_JF);
	if (ret < 0) {
		resp_error_msg = -QMI_RESULT_FAILURE_V01;
		cnss_pr_err("Failed to wait for response of Cal Report request, err: %d\n",
			    ret);
		goto out;
	}
	if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
		cnss_pr_err("Cal Report request failed, result: %d, err: %d\n",
			    resp->resp.result, resp->resp.error);
		resp_error_msg = resp->resp.error;
		ret = -resp->resp.result;
		goto out;
	}

	qmi_record(plat_priv->wlfw_service_instance_id,
		  (QMI_TYPE_RESP | QMI_WLFW_CAL_REPORT_REQ_V01), ret,
		  resp_error_msg);
	kfree(req);
	kfree(resp);
	return ret;
out:
	qmi_record(plat_priv->wlfw_service_instance_id,
		  (QMI_WLFW_CAL_REPORT_REQ_V01), ret,
		  resp_error_msg);
	kfree(req);
	kfree(resp);
	return ret;
}

static void cnss_wlfw_cal_done_ind_cb(struct qmi_handle *qmi_wlfw,
				      struct sockaddr_qrtr *sq,
				      struct qmi_txn *txn, const void *data)
{
	struct cnss_plat_data *plat_priv =
		container_of(qmi_wlfw, struct cnss_plat_data, qmi_wlfw);
	struct cnss_cal_info *cal_info;
	const struct wlfw_cal_done_ind_msg_v01 *ind = data;

	cnss_pr_dbg("Received Cal done indication. File size: %lld\n",
		    ind->cal_file_upload_size);
	cnss_pr_info("Calibration took %d ms\n",
		     jiffies_to_msecs(jiffies - plat_priv->cal_time));

	if (!txn) {
		cnss_pr_err("Spurious indication\n");
		return;
	}

	if (ind->cal_file_upload_size_valid)
		plat_priv->cal_file_size = ind->cal_file_upload_size;

	cal_info = kzalloc(sizeof(*cal_info), GFP_KERNEL);
	if (!cal_info)
		return;

	qmi_record(plat_priv->wlfw_service_instance_id,
		   QMI_WLFW_CAL_DONE_IND_V01, 0, 0);

	cal_info->cal_status = CNSS_CAL_DONE;
	cnss_driver_event_post(plat_priv, CNSS_DRIVER_EVENT_COLD_BOOT_CAL_DONE,
			       0, cal_info);
}

static void cnss_wlfw_qdss_trace_req_mem_ind_cb(struct qmi_handle *qmi_wlfw,
						struct sockaddr_qrtr *sq,
						struct qmi_txn *txn,
						const void *data)
{
	struct cnss_plat_data *plat_priv =
		container_of(qmi_wlfw, struct cnss_plat_data, qmi_wlfw);
	const struct wlfw_qdss_trace_req_mem_ind_msg_v01 *ind_msg = data;
	int i;

	cnss_pr_dbg("Received QMI WLFW QDSS trace request mem indication\n");
	qmi_record(plat_priv->wlfw_service_instance_id,
		   QMI_WLFW_QDSS_TRACE_REQ_MEM_IND_V01, 0, 0);

	if (!txn) {
		cnss_pr_err("Spurious indication\n");
		return;
	}

	if (plat_priv->qdss_mem_seg_len) {
		cnss_pr_err("Ignore double allocation for QDSS trace, "
			    "current len %u\n",
			    plat_priv->qdss_mem_seg_len);
	} else {
		plat_priv->qdss_mem_seg_len = ind_msg->mem_seg_len;
		if (ind_msg->mem_seg_len > 1) {
			cnss_pr_dbg("%s: FW requests %d segments, "
				    "overwriting it with 1",
				    __func__, ind_msg->mem_seg_len);
			plat_priv->qdss_mem_seg_len = 1;
		}

		for (i = 0; i < plat_priv->qdss_mem_seg_len; i++) {
			cnss_pr_dbg("QDSS requests for memory, size: 0x%x, "
				    "type: %u\n", ind_msg->mem_seg[i].size,
				    ind_msg->mem_seg[i].type);
			plat_priv->qdss_mem[i].type = ind_msg->mem_seg[i].type;
			plat_priv->qdss_mem[i].size = ind_msg->mem_seg[i].size;
		}
	}

	cnss_driver_event_post(plat_priv, CNSS_DRIVER_EVENT_QDSS_TRACE_REQ_MEM,
			       0, NULL);
}

static void cnss_wlfw_qdss_trace_save_ind_cb(struct qmi_handle *qmi_wlfw,
					     struct sockaddr_qrtr *sq,
					     struct qmi_txn *txn,
					     const void *data)
{
	struct cnss_plat_data *plat_priv =
		container_of(qmi_wlfw, struct cnss_plat_data, qmi_wlfw);
	const struct wlfw_qdss_trace_save_ind_msg_v01 *ind_msg = data;
	struct cnss_qmi_event_qdss_trace_save_data *event_data;
	int i = 0;

	qmi_record(plat_priv->wlfw_service_instance_id,
		   QMI_WLFW_QDSS_TRACE_SAVE_IND_V01, 0, 0);
	cnss_pr_info("Received QMI WLFW QDSS trace save indication. Source: %d\n",
		     ind_msg->source);

	if (!txn) {
		cnss_pr_err("Spurious indication\n");
		return;
	}

	/* QDSS Save indication is supported only PCI devices,
	 * drop this indication for other targets
	 */
	switch (plat_priv->device_id) {
	case QCN9000_DEVICE_ID:
	case QCN6122_DEVICE_ID:
	case QCN9160_DEVICE_ID:
	case QCN9224_DEVICE_ID:
	case QCA5332_DEVICE_ID:
	case QCN6432_DEVICE_ID:
	case QCA5424_DEVICE_ID:
		break;
	case QCA8074_DEVICE_ID:
	case QCA8074V2_DEVICE_ID:
	case QCA6018_DEVICE_ID:
	case QCA5018_DEVICE_ID:
	case QCA9574_DEVICE_ID:
		/* Source 0 is for ETR and not supported for AHB targets */
		if (ind_msg->source == 1)
			break;
		/* fall through */
	default:
		cnss_pr_info("QDSS Trace save not supported for %s, source %d\n",
			     plat_priv->device_name, ind_msg->source);
		return;
	}

	cnss_pr_dbg("QDSS_trace_save info: source %u, total_size %u, file_name_valid %u, file_name %s\n",
		    ind_msg->source, ind_msg->total_size,
		    ind_msg->file_name_valid, ind_msg->file_name);

	event_data = kzalloc(sizeof(*event_data), GFP_KERNEL);
	if (!event_data)
		return;

	event_data->total_size = ind_msg->total_size;

	if (ind_msg->file_name_valid)
		strlcpy(event_data->file_name, ind_msg->file_name,
			QDSS_TRACE_FILE_NAME_MAX + 1);
	else
		strlcpy(event_data->file_name, "qdss_trace",
			QDSS_TRACE_FILE_NAME_MAX + 1);

	if (ind_msg->source == 1) {
		cnss_driver_event_post(plat_priv,
				       CNSS_DRIVER_EVENT_QDSS_TRACE_REQ_DATA,
				       0, event_data);
		return;
        }

	/* Source 0 */
	if (ind_msg->mem_seg_valid) {
		if (ind_msg->mem_seg_len > QDSS_TRACE_SEG_LEN_MAX) {
			cnss_pr_err("Invalid seg len %u\n",
				    ind_msg->mem_seg_len);
			goto free_event_data;
		}
		cnss_pr_dbg("QDSS_trace_save seg len %u\n",
			    ind_msg->mem_seg_len);
		event_data->mem_seg_len = ind_msg->mem_seg_len;
		for (i = 0; i < ind_msg->mem_seg_len; i++) {
			event_data->mem_seg[i].addr = ind_msg->mem_seg[i].addr;
			event_data->mem_seg[i].size = ind_msg->mem_seg[i].size;
			cnss_pr_dbg("seg-%d: addr 0x%llx size 0x%x\n",
				    i, ind_msg->mem_seg[i].addr,
				    ind_msg->mem_seg[i].size);
		}
	}

	cnss_driver_event_post(plat_priv,
			       CNSS_DRIVER_EVENT_QDSS_TRACE_SAVE,
			       0, event_data);

	return;

free_event_data:
	kfree(event_data);
}

static void cnss_wlfw_qdss_trace_free_ind_cb(struct qmi_handle *qmi_wlfw,
					     struct sockaddr_qrtr *sq,
					     struct qmi_txn *txn,
					     const void *data)
{
	struct cnss_plat_data *plat_priv =
		container_of(qmi_wlfw, struct cnss_plat_data, qmi_wlfw);

	cnss_pr_dbg("Received QMI WLFW QDSS memory free indication\n");
	qmi_record(plat_priv->wlfw_service_instance_id,
		   QMI_WLFW_QDSS_TRACE_FREE_IND_V01, 0, 0);
	cnss_driver_event_post(plat_priv, CNSS_DRIVER_EVENT_QDSS_TRACE_FREE,
			       0, NULL);
}

static void cnss_qdss_mem_ready_ind_cb(struct qmi_handle *qmi_wlfw,
				       struct sockaddr_qrtr *sq,
				       struct qmi_txn *txn,
				       const void *data)
{
	struct cnss_plat_data *plat_priv =
		container_of(qmi_wlfw, struct cnss_plat_data, qmi_wlfw);

	cnss_pr_dbg("Received QMI WLFW QDSS memory ready indication\n");
	qmi_record(plat_priv->wlfw_service_instance_id,
		   QMI_WLFW_QDSS_MEM_READY_IND_V01, 0, 0);
	cnss_driver_event_post(plat_priv, CNSS_DRIVER_EVENT_QDSS_MEM_READY,
			       0, NULL);
}

static void cnss_wlfw_m3_dump_upload_req_ind_cb(struct qmi_handle *qmi_wlfw,
						struct sockaddr_qrtr *sq,
						struct qmi_txn *txn,
						const void *data)
{
	struct cnss_plat_data *plat_priv =
		container_of(qmi_wlfw, struct cnss_plat_data, qmi_wlfw);
	const struct wlfw_m3_dump_upload_req_ind_msg_v01 *ind_msg = data;
	struct cnss_qmi_event_m3_dump_upload_req_data *event_data;

	qmi_record(plat_priv->wlfw_service_instance_id,
		   QMI_WLFW_M3_DUMP_UPLOAD_REQ_IND_V01, 0, 0);
	cnss_pr_dbg("Received QMI WLFW M3 Dump Upload indication\n");

	if (!txn) {
		cnss_pr_err("Spurious indication\n");
		return;
	}

	cnss_pr_dbg("M3 Dump upload info: pdev_id %d addr: 0x%llx size 0x%llx\n",
		    ind_msg->pdev_id, ind_msg->addr, ind_msg->size);

	event_data = kzalloc(sizeof(*event_data), GFP_KERNEL);
	if (!event_data)
		return;

	event_data->pdev_id = ind_msg->pdev_id;
	event_data->addr = ind_msg->addr;
	event_data->size = ind_msg->size;

	cnss_driver_event_post(plat_priv, CNSS_DRIVER_EVENT_M3_DUMP_UPLOAD_REQ,
			       0, event_data);
}

static struct qmi_msg_handler qmi_wlfw_msg_handlers[] = {
	{
		.type = QMI_INDICATION,
		.msg_id = QMI_WLFW_REQUEST_MEM_IND_V01,
		.ei = wlfw_request_mem_ind_msg_v01_ei,
		.decoded_size = sizeof(struct wlfw_request_mem_ind_msg_v01),
		.fn = cnss_wlfw_request_mem_ind_cb,
	},
	{
		.type = QMI_INDICATION,
		.msg_id = QMI_WLFW_FW_MEM_READY_IND_V01,
		.ei = wlfw_fw_mem_ready_ind_msg_v01_ei,
		.decoded_size = sizeof(struct wlfw_fw_mem_ready_ind_msg_v01),
		.fn = cnss_wlfw_fw_mem_ready_ind_cb,
	},
	{
		.type = QMI_INDICATION,
		.msg_id = QMI_WLFW_FW_READY_IND_V01,
		.ei = wlfw_fw_ready_ind_msg_v01_ei,
		.decoded_size = sizeof(struct wlfw_fw_ready_ind_msg_v01),
		.fn = cnss_wlfw_fw_ready_ind_cb,
	},
	{
		.type = QMI_INDICATION,
		.msg_id = QMI_WLFW_FW_INIT_DONE_IND_V01,
		.ei = wlfw_fw_init_done_ind_msg_v01_ei,
		.decoded_size = sizeof(struct wlfw_fw_init_done_ind_msg_v01),
		.fn = cnss_wlfw_fw_init_done_ind_cb,
	},
	{
		.type = QMI_INDICATION,
		.msg_id = QMI_WLFW_PIN_CONNECT_RESULT_IND_V01,
		.ei = wlfw_pin_connect_result_ind_msg_v01_ei,
		.decoded_size =
			sizeof(struct wlfw_pin_connect_result_ind_msg_v01),
		.fn = cnss_wlfw_pin_result_ind_cb,
	},
	{
		.type = QMI_INDICATION,
		.msg_id = QMI_WLFW_CAL_DONE_IND_V01,
		.ei = wlfw_cal_done_ind_msg_v01_ei,
		.decoded_size = sizeof(struct wlfw_cal_done_ind_msg_v01),
		.fn = cnss_wlfw_cal_done_ind_cb
	},
	{
		.type = QMI_INDICATION,
		.msg_id = QMI_WLFW_QDSS_TRACE_REQ_MEM_IND_V01,
		.ei = wlfw_qdss_trace_req_mem_ind_msg_v01_ei,
		.decoded_size =
			sizeof(struct wlfw_qdss_trace_req_mem_ind_msg_v01),
		.fn = cnss_wlfw_qdss_trace_req_mem_ind_cb,
	},
	{
		.type = QMI_INDICATION,
		.msg_id = QMI_WLFW_QDSS_TRACE_SAVE_IND_V01,
		.ei = wlfw_qdss_trace_save_ind_msg_v01_ei,
		.decoded_size =
			sizeof(struct wlfw_qdss_trace_save_ind_msg_v01),
		.fn = cnss_wlfw_qdss_trace_save_ind_cb,
	},
	{
		.type = QMI_INDICATION,
		.msg_id = QMI_WLFW_QDSS_TRACE_FREE_IND_V01,
		.ei = wlfw_qdss_trace_free_ind_msg_v01_ei,
		.decoded_size =
			sizeof(struct wlfw_qdss_trace_free_ind_msg_v01),
		.fn = cnss_wlfw_qdss_trace_free_ind_cb,
	},
	{
		.type = QMI_INDICATION,
		.msg_id = QMI_WLFW_QDSS_MEM_READY_IND_V01,
		.ei = wlfw_qdss_mem_ready_ind_msg_v01_ei,
		.decoded_size =
			sizeof(struct wlfw_qdss_mem_ready_ind_msg_v01),
		.fn = cnss_qdss_mem_ready_ind_cb,
	},
	{
		.type = QMI_INDICATION,
		.msg_id = QMI_WLFW_M3_DUMP_UPLOAD_REQ_IND_V01,
		.ei = wlfw_m3_dump_upload_req_ind_msg_v01_ei,
		.decoded_size =
			sizeof(struct wlfw_m3_dump_upload_req_ind_msg_v01),
		.fn = cnss_wlfw_m3_dump_upload_req_ind_cb,
	},
	{}
};

static int cnss_wlfw_connect_to_server(struct cnss_plat_data *plat_priv,
				       void *data)
{
	struct cnss_qmi_event_server_arrive_data *event_data = data;
	struct qmi_handle *qmi_wlfw;
	struct sockaddr_qrtr sq = { 0 };
	int ret = 0;

	if (!event_data)
		return -EINVAL;

	if (!plat_priv)
		return -ENODEV;

	qmi_wlfw = &plat_priv->qmi_wlfw;
	sq.sq_family = AF_QIPCRTR;
	sq.sq_node = event_data->node;
	sq.sq_port = event_data->port;

	ret = kernel_connect(qmi_wlfw->sock, (struct sockaddr *)&sq,
			     sizeof(sq), 0);
	if (ret < 0) {
		cnss_pr_err("Failed to connect to QMI WLFW remote service port\n");
		goto out;
	}

	set_bit(CNSS_QMI_WLFW_CONNECTED, &plat_priv->driver_state);

	cnss_pr_info("QMI WLFW service connected, state: 0x%lx\n",
		     plat_priv->driver_state);

	kfree(data);
	return 0;

out:
	CNSS_ASSERT(0);
	kfree(data);
	return ret;
}

int cnss_wlfw_server_arrive(struct cnss_plat_data *plat_priv, void *data)
{
	int ret = 0;

	if (!plat_priv)
		return -ENODEV;
	clear_bit(CNSS_FW_READY, &plat_priv->driver_state);
	clear_bit(CNSS_FW_MEM_READY, &plat_priv->driver_state);

	ret = cnss_wlfw_connect_to_server(plat_priv, data);
	if (ret < 0)
		goto out;

	ret = cnss_wlfw_ind_register_send_sync(plat_priv);
	if (ret < 0) {
		if (ret == -EALREADY)
			ret = 0;
		goto out;
	}

	ret = cnss_wlfw_host_cap_send_sync(plat_priv);
	if (ret < 0)
		goto out;

	/* Send FW INI CFG QMI message only if the file is present */
	if (plat_priv->fw_ini_cfg_support) {
		ret = cnss_wlfw_ini_file_send_sync(plat_priv,
						   WLFW_INI_CFG_FILE_V01);
		if (ret < 0)
			goto out;
	}
	return 0;

out:
	return ret;
}

int cnss_wlfw_server_exit(struct cnss_plat_data *plat_priv)
{
	if (!plat_priv)
		return -ENODEV;

	clear_bit(CNSS_QMI_WLFW_CONNECTED, &plat_priv->driver_state);

	cnss_pr_info("QMI WLFW service disconnected, state: 0x%lx\n",
		     plat_priv->driver_state);
	return 0;
}

static int wlfw_new_server(struct qmi_handle *qmi_wlfw,
			   struct qmi_service *service)
{
	struct cnss_plat_data *plat_priv =
		container_of(qmi_wlfw, struct cnss_plat_data, qmi_wlfw);
	struct cnss_qmi_event_server_arrive_data *event_data;

	cnss_pr_dbg("WLFW server arriving: node %u port %u\n",
		    service->node, service->port);

	event_data = kzalloc(sizeof(*event_data), GFP_KERNEL);
	if (!event_data)
		return -ENOMEM;

	event_data->node = service->node;
	event_data->port = service->port;

	cnss_driver_event_post(plat_priv, CNSS_DRIVER_EVENT_SERVER_ARRIVE,
			       0, event_data);

	return 0;
}

static void wlfw_del_server(struct qmi_handle *qmi_wlfw,
			    struct qmi_service *service)
{
	struct cnss_plat_data *plat_priv =
		container_of(qmi_wlfw, struct cnss_plat_data, qmi_wlfw);

	cnss_pr_dbg("WLFW server exiting\n");

	cnss_driver_event_post(plat_priv, CNSS_DRIVER_EVENT_SERVER_EXIT,
			       0, NULL);
}

static struct qmi_ops qmi_wlfw_ops = {
	.new_server = wlfw_new_server,
	.del_server = wlfw_del_server,
};

struct qmi_handle *whandle;

int cnss_qmi_init(struct cnss_plat_data *plat_priv)
{
	int ret = 0;
	struct device *dev;

	dev = &plat_priv->plat_dev->dev;

	if (plat_priv->bus_type == CNSS_BUS_AHB) {
		if (qca8074_fw_mem_mode != 0xFF) {
			plat_priv->tgt_mem_cfg_mode = qca8074_fw_mem_mode;
			pr_info("Using qca8074_fw_mem_mode 0x%x\n",
				qca8074_fw_mem_mode);
		} else if (of_property_read_u32(dev->of_node,
						"qcom,tgt-mem-mode",
						&plat_priv->tgt_mem_cfg_mode)) {
			pr_info("No qca8074_tgt_mem_mode entry in dev-tree.\n");
			plat_priv->tgt_mem_cfg_mode = 0;
		}
	} else if (plat_priv->bus_type == CNSS_BUS_PCI) {
		if (pci_fw_mem_mode != 0xFF) {
			plat_priv->tgt_mem_cfg_mode = pci_fw_mem_mode;
			pr_info("Using pci_fw_mem_mode 0x%x\n",
				pci_fw_mem_mode);
		} else if (of_property_read_u32(dev->of_node,
					 "tgt-mem-mode",
					 &plat_priv->tgt_mem_cfg_mode)) {
			pr_info("No tgt-mem-mode entry in dev-tree.\n");
			plat_priv->tgt_mem_cfg_mode = 0;
		}
	}

	ret = qmi_handle_init(&plat_priv->qmi_wlfw,
			      QMI_WLFW_MAX_RECV_BUF_SIZE,
			      &qmi_wlfw_ops, qmi_wlfw_msg_handlers);
	if (ret < 0) {
		cnss_pr_err("Failed to initialize QMI handle, err: %d\n", ret);
		goto out;
	}

	ret = qmi_add_lookup(&plat_priv->qmi_wlfw, plat_priv->service_id,
				WLFW_SERVICE_VERS_V01,
				plat_priv->wlfw_service_instance_id);
	if (ret < 0) {
		cnss_pr_err("Failed to add QMI lookup, err: %d\n", ret);
		return ret;
	}

	whandle = &plat_priv->qmi_wlfw;

out:
	return ret;
}

void cnss_qmi_deinit(struct cnss_plat_data *plat_priv)
{
	qmi_handle_release(&plat_priv->qmi_wlfw);
}

#ifdef CNSS_COEX
int coex_antenna_switch_to_wlan_send_sync_msg(struct cnss_plat_data *plat_priv)
{
	int ret;
	struct coex_antenna_switch_to_wlan_req_msg_v01 *req;
	struct coex_antenna_switch_to_wlan_resp_msg_v01 *resp;
	struct qmi_txn txn;

	if (!plat_priv)
		return -ENODEV;

	cnss_pr_dbg("Sending coex antenna switch_to_wlan\n");

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	resp = kzalloc(sizeof(*resp), GFP_KERNEL);
	if (!resp) {
		kfree(req);
		return -ENOMEM;
	}

	req->antenna = plat_priv->antenna;

	ret = qmi_txn_init(&plat_priv->coex_qmi, &txn,
			   coex_antenna_switch_to_wlan_resp_msg_v01_ei, resp);
	if (ret < 0) {
		cnss_pr_err("Fail to init txn for coex antenna switch_to_wlan resp %d\n",
			    ret);
		goto out;
	}

	ret = qmi_send_request
		(&plat_priv->coex_qmi, NULL, &txn,
		 QMI_COEX_SWITCH_ANTENNA_TO_WLAN_REQ_V01,
		 COEX_ANTENNA_SWITCH_TO_WLAN_REQ_MSG_V01_MAX_MSG_LEN,
		 coex_antenna_switch_to_wlan_req_msg_v01_ei, req);
	if (ret < 0) {
		qmi_txn_cancel(&txn);
		cnss_pr_err("Fail to send coex antenna switch_to_wlan req %d\n",
			    ret);
		goto out;
	}

	ret = qmi_txn_wait(&txn, COEX_TIMEOUT);
	if (ret < 0) {
		cnss_pr_err("Coex antenna switch_to_wlan resp wait failed with ret %d\n",
			    ret);
		goto out;
	} else if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
		cnss_pr_err("Coex antenna switch_to_wlan request rejected, result:%d error:%d\n",
			    resp->resp.result, resp->resp.error);
		ret = -resp->resp.result;
		goto out;
	}

	if (resp->grant_valid)
		plat_priv->grant = resp->grant;

	cnss_pr_dbg("Coex antenna grant: 0x%llx\n", resp->grant);

	kfree(resp);
	kfree(req);
	return 0;

out:
	kfree(resp);
	kfree(req);
	return ret;
}

int coex_antenna_switch_to_mdm_send_sync_msg(struct cnss_plat_data *plat_priv)
{
	int ret;
	struct coex_antenna_switch_to_mdm_req_msg_v01 *req;
	struct coex_antenna_switch_to_mdm_resp_msg_v01 *resp;
	struct qmi_txn txn;

	if (!plat_priv)
		return -ENODEV;

	cnss_pr_dbg("Sending coex antenna switch_to_mdm\n");

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	resp = kzalloc(sizeof(*resp), GFP_KERNEL);
	if (!resp) {
		kfree(req);
		return -ENOMEM;
	}

	req->antenna = plat_priv->antenna;

	ret = qmi_txn_init(&plat_priv->coex_qmi, &txn,
			   coex_antenna_switch_to_mdm_resp_msg_v01_ei, resp);
	if (ret < 0) {
		cnss_pr_err("Fail to init txn for coex antenna switch_to_mdm resp %d\n",
			    ret);
		goto out;
	}

	ret = qmi_send_request
		(&plat_priv->coex_qmi, NULL, &txn,
		 QMI_COEX_SWITCH_ANTENNA_TO_MDM_REQ_V01,
		 COEX_ANTENNA_SWITCH_TO_MDM_REQ_MSG_V01_MAX_MSG_LEN,
		 coex_antenna_switch_to_mdm_req_msg_v01_ei, req);
	if (ret < 0) {
		qmi_txn_cancel(&txn);
		cnss_pr_err("Fail to send coex antenna switch_to_mdm req %d\n",
			    ret);
		goto out;
	}

	ret = qmi_txn_wait(&txn, COEX_TIMEOUT);
	if (ret < 0) {
		cnss_pr_err("Coex antenna switch_to_mdm resp wait failed with ret %d\n",
			    ret);
		goto out;
	} else if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
		cnss_pr_err("Coex antenna switch_to_mdm request rejected, result:%d error:%d\n",
			    resp->resp.result, resp->resp.error);
		ret = -resp->resp.result;
		goto out;
	}

	kfree(resp);
	kfree(req);
	return 0;

out:
	kfree(resp);
	kfree(req);
	return ret;
}

static int coex_new_server(struct qmi_handle *qmi,
			   struct qmi_service *service)
{
	struct cnss_plat_data *plat_priv =
		container_of(qmi, struct cnss_plat_data, coex_qmi);
	struct sockaddr_qrtr sq = { 0 };
	int ret = 0;

	cnss_pr_dbg("COEX server arrive: node %u port %u\n",
		    service->node, service->port);

	sq.sq_family = AF_QIPCRTR;
	sq.sq_node = service->node;
	sq.sq_port = service->port;
	ret = kernel_connect(qmi->sock, (struct sockaddr *)&sq, sizeof(sq), 0);
	if (ret < 0) {
		cnss_pr_err("Fail to connect to remote service port\n");
		return ret;
	}

	set_bit(CNSS_COEX_CONNECTED, &plat_priv->driver_state);
	cnss_pr_dbg("COEX Server Connected: 0x%lx\n",
		    plat_priv->driver_state);
	return 0;
}

static void coex_del_server(struct qmi_handle *qmi,
			    struct qmi_service *service)
{
	struct cnss_plat_data *plat_priv =
		container_of(qmi, struct cnss_plat_data, coex_qmi);

	cnss_pr_dbg("COEX server exit\n");

	clear_bit(CNSS_COEX_CONNECTED, &plat_priv->driver_state);
}

static struct qmi_ops coex_qmi_ops = {
	.new_server = coex_new_server,
	.del_server = coex_del_server,
};

int cnss_register_coex_service(struct cnss_plat_data *plat_priv)
{	int ret;

	ret = qmi_handle_init(&plat_priv->coex_qmi,
			      COEX_SERVICE_MAX_MSG_LEN,
			      &coex_qmi_ops, NULL);
	if (ret < 0)
		return ret;

	ret = qmi_add_lookup(&plat_priv->coex_qmi, COEX_SERVICE_ID_V01,
			     COEX_SERVICE_VERS_V01, 0);
	return ret;
}

void cnss_unregister_coex_service(struct cnss_plat_data *plat_priv)
{
	qmi_handle_release(&plat_priv->coex_qmi);
}
#endif

#ifdef CNSS2_IMS
/* IMS Service */
int ims_subscribe_for_indication_send_async(struct cnss_plat_data *plat_priv)
{
	int ret;
	struct ims_private_service_subscribe_for_indications_req_msg_v01 *req;
	struct qmi_txn *txn;

	if (!plat_priv)
		return -ENODEV;

	cnss_pr_dbg("Sending ASYNC ims subscribe for indication\n");

	req = kzalloc(sizeof(*req), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	req->wfc_call_status_valid = 1;
	req->wfc_call_status = 1;

	txn = &plat_priv->txn;
	ret = qmi_txn_init(&plat_priv->ims_qmi, txn, NULL, NULL);
	if (ret < 0) {
		cnss_pr_err("Fail to init txn for ims subscribe for indication resp %d\n",
			    ret);
		goto out;
	}

	ret = qmi_send_request
	(&plat_priv->ims_qmi, NULL, txn,
	QMI_IMS_PRIVATE_SERVICE_SUBSCRIBE_FOR_INDICATIONS_REQ_V01,
	IMS_PRIVATE_SERVICE_SUBSCRIBE_FOR_INDICATIONS_REQ_MSG_V01_MAX_MSG_LEN,
	ims_private_service_subscribe_for_indications_req_msg_v01_ei, req);
	if (ret < 0) {
		qmi_txn_cancel(txn);
		cnss_pr_err("Fail to send ims subscribe for indication req %d\n",
			    ret);
		goto out;
	}

	kfree(req);
	return 0;

out:
	kfree(req);
	return ret;
}

static void ims_subscribe_for_indication_resp_cb(struct qmi_handle *qmi,
						 struct sockaddr_qrtr *sq,
						 struct qmi_txn *txn,
						 const void *data)
{
	const
	struct ims_private_service_subscribe_for_indications_rsp_msg_v01 *resp =
		data;

	cnss_pr_dbg("Received IMS subscribe indication response\n");

	if (!txn) {
		cnss_pr_err("spurious response\n");
		return;
	}

	if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
		cnss_pr_err("IMS subscribe for indication request rejected, result:%d error:%d\n",
			    resp->resp.result, resp->resp.error);
		txn->result = -resp->resp.result;
	}
}

static void ims_wfc_call_status_ind_cb(struct qmi_handle *ims_qmi,
				       struct sockaddr_qrtr *sq,
				       struct qmi_txn *txn, const void *data)
{
	struct cnss_plat_data *plat_priv =
		container_of(ims_qmi, struct cnss_plat_data, ims_qmi);
	const
	struct ims_private_service_wfc_call_status_ind_msg_v01 *ind_msg = data;
	u32 data_len = 0;

	cnss_pr_dbg("Received IMS wfc call status indication\n");

	if (!txn) {
		cnss_pr_err("Spurious indication\n");
		return;
	}

	if (!ind_msg) {
		cnss_pr_err("Invalid indication\n");
		return;
	}

	data_len = sizeof(*ind_msg);
	if (data_len > QMI_WLFW_MAX_WFC_CALL_STATUS_DATA_SIZE_V01) {
		cnss_pr_err("Exceed maxinum data len:%u\n", data_len);
		return;
	}

	cnss_wlfw_wfc_call_status_send_sync(plat_priv, data_len, ind_msg);
}

static struct qmi_msg_handler qmi_ims_msg_handlers[] = {
	{
		.type = QMI_RESPONSE,
		.msg_id =
		QMI_IMS_PRIVATE_SERVICE_SUBSCRIBE_FOR_INDICATIONS_REQ_V01,
		.ei =
		ims_private_service_subscribe_for_indications_rsp_msg_v01_ei,
		.decoded_size = sizeof(struct
		ims_private_service_subscribe_for_indications_rsp_msg_v01),
		.fn = ims_subscribe_for_indication_resp_cb
	},
	{
		.type = QMI_INDICATION,
		.msg_id = QMI_IMS_PRIVATE_SERVICE_WFC_CALL_STATUS_IND_V01,
		.ei = ims_private_service_wfc_call_status_ind_msg_v01_ei,
		.decoded_size =
		sizeof(struct ims_private_service_wfc_call_status_ind_msg_v01),
		.fn = ims_wfc_call_status_ind_cb
	},
	{}
};

static int ims_new_server(struct qmi_handle *qmi,
			  struct qmi_service *service)
{
	struct cnss_plat_data *plat_priv =
		container_of(qmi, struct cnss_plat_data, ims_qmi);
	struct sockaddr_qrtr sq = { 0 };
	int ret = 0;

	cnss_pr_dbg("IMS server arrive: node %u port %u\n",
		    service->node, service->port);

	sq.sq_family = AF_QIPCRTR;
	sq.sq_node = service->node;
	sq.sq_port = service->port;
	ret = kernel_connect(qmi->sock, (struct sockaddr *)&sq, sizeof(sq), 0);
	if (ret < 0) {
		cnss_pr_err("Fail to connect to remote service port\n");
		return ret;
	}

	set_bit(CNSS_IMS_CONNECTED, &plat_priv->driver_state);
	cnss_pr_dbg("IMS Server Connected: 0x%lx\n",
		    plat_priv->driver_state);

	ret = ims_subscribe_for_indication_send_async(plat_priv);
	return ret;
}

static void ims_del_server(struct qmi_handle *qmi,
			   struct qmi_service *service)
{
	struct cnss_plat_data *plat_priv =
		container_of(qmi, struct cnss_plat_data, ims_qmi);

	cnss_pr_dbg("IMS server exit\n");

	clear_bit(CNSS_IMS_CONNECTED, &plat_priv->driver_state);
}

static struct qmi_ops ims_qmi_ops = {
	.new_server = ims_new_server,
	.del_server = ims_del_server,
};

int cnss_register_ims_service(struct cnss_plat_data *plat_priv)
{	int ret;

	ret = qmi_handle_init(&plat_priv->ims_qmi,
			      IMSPRIVATE_SERVICE_MAX_MSG_LEN,
			      &ims_qmi_ops, qmi_ims_msg_handlers);
	if (ret < 0)
		return ret;

	ret = qmi_add_lookup(&plat_priv->ims_qmi, IMSPRIVATE_SERVICE_ID_V01,
			     IMSPRIVATE_SERVICE_VERS_V01, 0);
	return ret;
}

void cnss_unregister_ims_service(struct cnss_plat_data *plat_priv)
{
	qmi_handle_release(&plat_priv->ims_qmi);
}
#endif
