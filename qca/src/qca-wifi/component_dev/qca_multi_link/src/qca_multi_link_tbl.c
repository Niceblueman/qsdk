/*
 * Copyright (c) 2020-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "qca_multi_link_tbl.h"
#include "qca_multi_link.h"
#include "qdf_module.h"
#include "qdf_trace.h"
#include "qal_vbus_dev.h"
#include "qal_bridge.h"
#include <br_private.h>

int qca_multi_link_tbl_get_eth_entries(struct net_device *net_dev,
					void *fill_buff, int buff_size)
{
	struct net_bridge_fdb_entry *search_fdb = NULL;
	int i;
	struct net_bridge_port *p = br_port_get_rcu(net_dev);
	struct net_device *ndev = NULL;
	struct wireless_dev *wdev = NULL;
	int num_of_entries = 0;
	qca_multi_link_tbl_entry_t *qfdb = (qca_multi_link_tbl_entry_t *)fill_buff;
	int fdb_entry_size = sizeof(qca_multi_link_tbl_entry_t);

	if (!p) {
		qdf_err("bridge port is NULL or disabled for dev %p \n", net_dev);
		return 0;
	}

	if (buff_size < fdb_entry_size) {
		qdf_err("Buffer size cannot be less than size of entry:%d dev:%p\n", fdb_entry_size, net_dev);
		return 0;
	}

	/*
	 * Traverse the bridge hah to get all ethernet interface entries.
	 */
	qal_vbus_rcu_read_lock();
	for (i = 0; i < BR_HASH_SIZE ; i++) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 24)
		hlist_for_each_entry_rcu(search_fdb, &p->br->hash[i], hlist) {
#else
		hlist_for_each_entry_rcu(search_fdb, &p->br->fdb_list,
					 fdb_node) {
#endif
			ndev = search_fdb->dst ? search_fdb->dst->dev : NULL;
			wdev = ndev ? ndev->ieee80211_ptr : NULL;
			if (!wdev && ndev) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 24)
				memcpy(qfdb->qal_mac_addr,
				       search_fdb->addr.addr, 6);
#else
				memcpy(qfdb->qal_mac_addr,
				       search_fdb->key.addr.addr, 6);
#endif
				qfdb->qal_fdb_dev = ndev;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
				qfdb->qal_fdb_is_local =
				    test_bit(BR_FDB_LOCAL, &search_fdb->flags);
#else
				qfdb->qal_fdb_is_local =  search_fdb->is_local;
#endif
				num_of_entries++;
				qfdb += 1;
				buff_size -= fdb_entry_size;
				if (buff_size < fdb_entry_size) {
					qal_vbus_rcu_read_unlock();
					return num_of_entries;
				}
			}
		}
	}
	qal_vbus_rcu_read_unlock();

	return num_of_entries;
}

qdf_export_symbol(qca_multi_link_tbl_get_eth_entries);

struct net_device *qca_multi_link_tbl_find_sta_or_ap(struct net_device *net_dev,
					uint8_t dev_type)
{
	struct net_bridge_fdb_entry *search_fdb = NULL;
	struct net_device *search_dev = NULL;
	struct wireless_dev	*ieee80211_ptr = NULL;
	int i;
	enum nl80211_iftype search_if_type;
	struct net_bridge_port *p = br_port_get_rcu(net_dev);

	if (!p || (p && p->state == BR_STATE_DISABLED))
		return NULL;

	if (!dev_type)
		search_if_type = NL80211_IFTYPE_AP;
	else
		search_if_type = NL80211_IFTYPE_STATION;

	qal_vbus_rcu_read_lock();
	for (i = 0; i < BR_HASH_SIZE; i++) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 24)
		hlist_for_each_entry_rcu(search_fdb, &p->br->hash[i], hlist) {
#else
		hlist_for_each_entry_rcu(search_fdb, &p->br->fdb_list,
					 fdb_node) {
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
			if (!test_bit(BR_FDB_LOCAL, &search_fdb->flags))
#else
			if (!search_fdb->is_local)
#endif
				continue;

			if ((!search_fdb->dst) || (!search_fdb->dst->dev)) {
				qal_vbus_rcu_read_unlock();
				return NULL;
			}

			search_dev = search_fdb->dst->dev;
			ieee80211_ptr = search_dev->ieee80211_ptr;
			if (ieee80211_ptr
		&& (ieee80211_ptr->iftype == search_if_type)
		&& (ieee80211_ptr->wiphy == net_dev->ieee80211_ptr->wiphy)) {
				qal_vbus_rcu_read_unlock();
				return search_dev;
			}
		}
	}

	qal_vbus_rcu_read_unlock();
	return NULL;
}

qdf_export_symbol(qca_multi_link_tbl_find_sta_or_ap);

QDF_STATUS qca_multi_link_tbl_add_or_refresh_entry(struct net_device *net_dev, uint8_t *addr,
							qca_multi_link_entry_type_t entry_type)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	uint16_t state = NUD_NONE;

	if (entry_type == QCA_MULTI_LINK_ENTRY_USER_ADDED) {
		state = NUD_REACHABLE;
	} else if (entry_type == QCA_MULTI_LINK_ENTRY_LOCAL) {
		state = NUD_PERMANENT;
	} else if (entry_type == QCA_MULTI_LINK_ENTRY_STATIC) {
		state = NUD_NOARP;
	}

	status = qal_bridge_fdb_add_or_refresh_by_netdev(net_dev, addr, 0, state);
	return status;
}
qdf_export_symbol(qca_multi_link_tbl_add_or_refresh_entry);

QDF_STATUS qca_multi_link_tbl_delete_entry(struct net_device *net_dev, uint8_t *addr)
{
	QDF_STATUS status;
	struct net_bridge_fdb_entry *fdb_entry = NULL;

	fdb_entry = qal_bridge_fdb_has_entry(net_dev, addr, 0);
	if (!fdb_entry) {
		return QDF_STATUS_E_FAILURE;
	}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
	if (test_bit(BR_FDB_LOCAL, &fdb_entry->flags)) {
#else
	if (fdb_entry->is_local) {
#endif
		return QDF_STATUS_SUCCESS;
	}

	if (!qca_multi_link_is_dbdc_processing_reqd(net_dev)) {
		return QDF_STATUS_SUCCESS;
	}

	status = qal_bridge_fdb_delete_by_netdev(net_dev, addr, 0);
	if (status != QDF_STATUS_SUCCESS) {
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

qdf_export_symbol(qca_multi_link_tbl_delete_entry);

QDF_STATUS qca_multi_link_tbl_has_entry(struct net_device *net_dev,
				const char *addr, uint16_t vlan_id,
				qca_multi_link_tbl_entry_t *qca_ml_entry)
{
	struct net_bridge_fdb_entry *fdb_entry = NULL;
	struct net_bridge_port *fdb_port = NULL;
	struct net_device *fdb_dev = NULL;

	if (!qca_ml_entry) {
		qdf_err("qca_ml_entry is null for ifname:%s with mac:%pM\n", net_dev->name, addr);
		return QDF_STATUS_E_FAILURE;
	}

	fdb_entry = qal_bridge_fdb_has_entry(net_dev, addr, vlan_id);
	if (!fdb_entry) {
		return QDF_STATUS_E_FAILURE;
	}

	fdb_port = fdb_entry->dst;
	if (!fdb_port) {
		qdf_err("bridge port is NULL for mac-addr %pM\n", addr);
		return QDF_STATUS_E_FAILURE;
	}

	fdb_dev = fdb_port->dev;
	if (!fdb_dev) {
		qdf_err("bridge netdev is NULL for mac-addr %pM\n", addr);
		return QDF_STATUS_E_FAILURE;
	}

	qca_ml_entry->qal_fdb_ieee80211_ptr = fdb_dev->ieee80211_ptr;
	qca_ml_entry->qal_fdb_dev = fdb_dev;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
	qca_ml_entry->qal_fdb_is_local =
		test_bit(BR_FDB_LOCAL, &fdb_entry->flags);
#else
	qca_ml_entry->qal_fdb_is_local = fdb_entry->is_local;
#endif
	return QDF_STATUS_SUCCESS;
}

qdf_export_symbol(qca_multi_link_tbl_has_entry);

QDF_STATUS qca_multi_link_tbl_register_update_notifier(void *nb)
{
	struct notifier_block *notifier = (struct notifier_block *)nb;

	if (!notifier) {
		qdf_err("Bridge Notifier is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	qal_bridge_fdb_update_register_notify(notifier);

	return QDF_STATUS_SUCCESS;
}

qdf_export_symbol(qca_multi_link_tbl_register_update_notifier);

QDF_STATUS qca_multi_link_tbl_unregister_update_notifier(void *nb)
{
	struct notifier_block *notifier = (struct notifier_block *)nb;

	if (!notifier) {
		qdf_err("Bridge Notifier is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	qal_bridge_fdb_update_unregister_notify(notifier);

	return QDF_STATUS_SUCCESS;
}

qdf_export_symbol(qca_multi_link_tbl_unregister_update_notifier);

QDF_STATUS qca_multi_link_tbl_register_notifier(void *nb)
{
	struct notifier_block *notifier = (struct notifier_block *)nb;

	if (!notifier) {
		qdf_err("Bridge Notifier is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	qal_bridge_fdb_register_notify(notifier);

	return QDF_STATUS_SUCCESS;
}

qdf_export_symbol(qca_multi_link_tbl_register_notifier);

QDF_STATUS qca_multi_link_tbl_unregister_notifier(void *nb)
{
	struct notifier_block *notifier = (struct notifier_block *)nb;

	if (!notifier) {
		qdf_err("Bridge Notifier is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	qal_bridge_fdb_unregister_notify(notifier);

	return QDF_STATUS_SUCCESS;
}

qdf_export_symbol(qca_multi_link_tbl_unregister_notifier);

struct net_device *qca_multi_link_tbl_get_bridge_dev(struct net_device *port_dev)
{
	struct net_bridge_port *fdb_port = NULL;
	struct net_bridge *br = NULL;

	if (!port_dev) {
		qdf_err("Port dev is NULL");
		return NULL;
	}

	fdb_port = br_port_get_rcu(port_dev);
	if (!fdb_port) {
		qdf_err("fdb port is NULL");
		return NULL;
	}

	br = fdb_port->br;
	if (!br) {
		qdf_err("bridge dev is NULL");
		return NULL;
	}

	return br->dev;
}

qdf_export_symbol(qca_multi_link_tbl_get_bridge_dev);
