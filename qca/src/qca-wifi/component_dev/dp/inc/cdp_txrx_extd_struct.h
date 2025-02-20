/*
 * Copyright (c) 2016-2019 The Linux Foundation. All rights reserved.
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
#ifndef _CDP_TXRX_EXTD_STRUCT_H_
#define _CDP_TXRX_EXTD_STRUCT_H_

/* Maximum number of receive chains */
#define CDP_MAX_RX_CHAINS 8

#ifdef WLAN_RX_PKT_CAPTURE_ENH

#define RX_ENH_CB_BUF_SIZE 0
#define RX_ENH_CB_BUF_RESERVATION 256
#define RX_ENH_CB_BUF_ALIGNMENT 4

#define RX_ENH_CAPTURE_TRAILER_LEN 8
#define CDP_RX_ENH_CAPTURE_MODE_MASK 0x0F
#define RX_ENH_CAPTURE_TRAILER_ENABLE_MASK 0x10
#define CDP_RX_ENH_CAPTURE_PEER_MASK 0xFFFFFFF0
#define CDP_RX_ENH_CAPTURE_PEER_LSB  4

/**
 * struct cdp_rx_indication_mpdu_info - Rx MPDU info
 * @ppdu_id: PPDU Id
 * @duration: PPDU duration
 * @bw: Bandwidth
 *       <enum 0 bw_20_MHz>
 *       <enum 1 bw_40_MHz>
 *       <enum 2 bw_80_MHz>
 *       <enum 3 bw_160_MHz>
 * @mu_ul_info_valid: RU info valid
 * @ofdma_ru_start_index: RU index number(0-73)
 * @ofdma_ru_width: size of RU in units of 1(26tone)RU
 * @nss: NSS 1,2, ...8
 * @mcs: MCS index
 * @preamble: preamble
 * @gi: <enum 0     0_8_us_sgi > Legacy normal GI
 *       <enum 1     0_4_us_sgi > Legacy short GI
 *       <enum 2     1_6_us_sgi > HE related GI
 *       <enum 3     3_2_us_sgi > HE
 * @ldpc: ldpc
 * @fcs_err: FCS error
 * @ppdu_type: SU/MU_MIMO/MU_OFDMA/MU_MIMO_OFDMA/UL_TRIG/BURST_BCN/UL_BSR_RESP/
 * UL_BSR_TRIG/UNKNOWN
 * @rate: legacy packet rate
 * @rssi_comb: Combined RSSI value (units = dB above noise floor)
 * @nf: noise floor
 * @timestamp: TSF at the reception of PPDU
 * @length: PPDU length
 * @per_chain_rssi: RSSI per chain
 * @channel: Channel informartion
 */
struct cdp_rx_indication_mpdu_info {
	uint32_t ppdu_id;
	uint16_t duration;
	uint64_t bw:4,
		 mu_ul_info_valid:1,
		 ofdma_ru_start_index:7,
		 ofdma_ru_width:7,
		 nss:4,
		 mcs:4,
		 preamble:4,
		 gi:4,
		 ldpc:1,
		 fcs_err:1,
		 ppdu_type:5,
		 rate:8;
	uint32_t rssi_comb;
	uint32_t nf;
	uint64_t timestamp;
	uint32_t length;
	uint8_t per_chain_rssi[CDP_MAX_RX_CHAINS];
	uint8_t channel;
	qdf_freq_t chan_freq;
};

#ifdef __KERNEL__
/**
 * struct cdp_rx_indication_mpdu- Rx MPDU plus MPDU info
 * @mpdu_info: defined in cdp_rx_indication_mpdu_info
 * @nbuf: nbuf of mpdu control block
 */
struct cdp_rx_indication_mpdu {
	struct cdp_rx_indication_mpdu_info mpdu_info;
	qdf_nbuf_t nbuf;
};
#endif
#endif /* WLAN_RX_PKT_CAPTURE_ENH */
struct ol_ath_dbg_rx_rssi {
	uint8_t     rx_rssi_pri20;
	uint8_t     rx_rssi_sec20;
	uint8_t     rx_rssi_sec40;
	uint8_t     rx_rssi_sec80;
};

struct ol_ath_radiostats {
	uint64_t    tx_beacon;
	uint32_t    tx_buf_count;
	int32_t     tx_mgmt;
	int32_t     rx_mgmt;
	uint32_t    rx_num_mgmt;
	uint32_t    rx_num_ctl;
	uint32_t    tx_rssi;
	uint32_t    rx_rssi_comb;
	struct      ol_ath_dbg_rx_rssi rx_rssi_chain0;
	struct      ol_ath_dbg_rx_rssi rx_rssi_chain1;
	struct      ol_ath_dbg_rx_rssi rx_rssi_chain2;
	struct      ol_ath_dbg_rx_rssi rx_rssi_chain3;
	uint32_t    rx_overrun;
	uint32_t    rx_phyerr;
	uint32_t    ackrcvbad;
	uint32_t    rtsbad;
	uint32_t    rtsgood;
	uint32_t    fcsbad;
	uint32_t    nobeacons;
	uint32_t    mib_int_count;
	uint32_t    rx_looplimit_start;
	uint32_t    rx_looplimit_end;
	uint8_t     ap_stats_tx_cal_enable;
	uint8_t     self_bss_util;
	uint8_t     obss_util;
	uint8_t     ap_rx_util;
	uint8_t     free_medium;
	uint8_t     ap_tx_util;
	uint8_t     obss_rx_util;
	uint8_t     non_wifi_util;
	uint32_t    tgt_asserts;
	int16_t     chan_nf;
	int16_t     chan_nf_sec80;
	uint64_t    wmi_tx_mgmt;
	uint64_t    wmi_tx_mgmt_completions;
	uint32_t    wmi_tx_mgmt_completion_err;
	uint32_t    rx_mgmt_rssi_drop;
	uint32_t    tx_frame_count;
	uint32_t    rx_frame_count;
	uint32_t    rx_clear_count;
	uint32_t    cycle_count;
	uint32_t    phy_err_count;
	uint32_t    chan_tx_pwr;
	uint32_t    be_nobuf;
	uint32_t    tx_packets;
	uint32_t    rx_packets;
	uint32_t    tx_num_data;
	uint32_t    rx_num_data;
	uint32_t    tx_mcs[10];
	uint32_t    rx_mcs[10];
	uint64_t    rx_bytes;
	uint64_t    tx_bytes;
	uint32_t    tx_compaggr;
	uint32_t    rx_aggr;
	uint32_t    tx_bawadv;
	uint32_t    tx_compunaggr;
	uint32_t    rx_badcrypt;
	uint32_t    rx_badmic;
	uint32_t    rx_crcerr;
	uint32_t    rx_last_msdu_unset_cnt;
	uint32_t    rx_data_bytes;
	uint32_t    tx_retries;
	uint32_t    created_vap;
	uint32_t    active_vap;
	uint32_t    rnr_count;
	uint32_t    soc_status_6ghz;
};

/* Enumeration of PDEV Configuration parameter */
enum _ol_hal_param_t {
	OL_HAL_CONFIG_DMA_BEACON_RESPONSE_TIME = 0
};

#ifdef CONFIG_SAWF
#define DP_SAWF_MAX_TIDS	8
#define DP_SAWF_MAX_QUEUES	2
#define DP_SAWF_NUM_AVG_WINDOWS	5

/**
 * struct sawf_delay_stats- sawf Tx-delay stats
 * @delay_hist: histogram for various delay-buckets
 * @nwdelay_avg: moving average for nwdelay
 * @swdelay_avg: moving average for swdelay
 * @hwdelay_avg: moving average for hwdelay
 * @num_pkt: count of pkts for which delay is calculated
 * @nwdelay_win_total: total nwdelay for a window
 * @swdelay_win_total: total swdelay for a window
 * @hwdelay_win_total: total hwdelay for a window
 * @success: count of pkts that met delay-bound
 * @failure: count of pkts that did not meet delay-bound
 * @tid: tid no
 * @msduq: msdu-queue used for Tx
 */
struct sawf_delay_stats {
	struct cdp_hist_stats delay_hist;

	uint32_t nwdelay_avg;
	uint32_t swdelay_avg;
	uint32_t hwdelay_avg;
	uint32_t num_pkt;
	uint64_t nwdelay_win_total;
	uint64_t swdelay_win_total;
	uint64_t hwdelay_win_total;
	uint64_t success;
	uint64_t failure;

	uint8_t tid;
	uint8_t msduq;
};

/**
 * struct sawf_fw_mpdu_stats- per-mpdu Tx success/failure snapshot
 * @success_cnt: count of pkts successfully transmitted
 * @failure_cnt: count of pkts failed to transmit
 */
struct sawf_fw_mpdu_stats {
	uint64_t success_cnt;
	uint64_t failure_cnt;
};

/**
 * struct sawf_tx_stats- Tx stats
 * @tx_ucast: unicast transmit success stats
 * @tx_ingress: enqueue success stats
 * @dropped: detailed information for for tx-drops
 * @svc_intval_stats: success/failure stats per service-interval
 * @burst_size_stats: success/failure stats per burst-size
 * @tx_failed: tx failure count
 * @queue_depth: transmit queue-depth
 * @throughput: throughput
 * @ingress_rate: ingress-rate
 * @tid: tid used for transmit
 * @msduq: msdu-queue used for transmit
 * @reinject_pkt: reinject packet
 */
struct sawf_tx_stats {
	struct cdp_pkt_info tx_success;
	struct cdp_pkt_info tx_ingress;
	struct {
		struct cdp_pkt_info fw_rem;
		uint32_t fw_rem_notx;
		uint32_t fw_rem_tx;
		uint32_t age_out;
		uint32_t fw_reason1;
		uint32_t fw_reason2;
		uint32_t fw_reason3;
		uint32_t fw_rem_queue_disable;
		uint32_t fw_rem_no_match;
		uint32_t drop_threshold;
		uint32_t drop_link_desc_na;
		uint32_t invalid_drop;
		uint32_t mcast_vdev_drop;
		uint32_t invalid_rr;
	} dropped;
	struct sawf_fw_mpdu_stats svc_intval_stats;
	struct sawf_fw_mpdu_stats burst_size_stats;
	uint32_t tx_failed;
	uint32_t queue_depth;
	uint32_t throughput;
	uint32_t ingress_rate;
	uint32_t retry_count;
	uint32_t multiple_retry_count;
	uint32_t failed_retry_count;
	uint8_t tid;
	uint8_t msduq;
	uint16_t reinject_pkt;
};

/*
 * struct sawf_msduq_svc_params - MSDUQ Params
 * @is_used: flag to indicate if there is active flow on this queue
 * @svc_id: service class id
 * @svc_type: service class type
 * @svc_tid: service class tid
 * @svc_ac: service class access category
 * @priority: service class priority
 * @service_interval: service interval (in milliseconds)
 * @burst_size: burst size (in bytes)
 * @min_throughput: minimum throughput (in Kbps)
 * @delay_bound: max latency (in milliseconds)
 * @mark_metadata: mark metadata
 */
struct sawf_msduq_svc_params {
	bool is_used;
	uint8_t svc_id;
	uint8_t svc_type;
	uint8_t svc_tid;
	uint8_t svc_ac;
	uint8_t priority;
	uint32_t service_interval;
	uint32_t burst_size;
	uint32_t min_throughput;
	uint32_t delay_bound;
	uint32_t mark_metadata;
};

/*
 * struct sawf_admctrl_msduq_stats - SAWF AdmCtrl MSDUQ Stats
 * @tx_success_pkt: MSDUQ Tx success pkt count
 */
struct sawf_admctrl_msduq_stats {
	uint64_t tx_success_pkt;
};

/*
 * struct sawf_admctrl_peer_stats - SAWF AdmCtrl Peer Stats
 * @tx_success_pkt: Tx success pkt count
 * @avg_tx_rate: Average Tx rate
 * @tx_airtime_consumption: Tx airtime consumption on per AC basis
 * @msduq_stats: SAWF AdmCtrl MSDUQ Stats
 */
struct sawf_admctrl_peer_stats {
	uint64_t tx_success_pkt;
	uint64_t avg_tx_rate;
	uint16_t tx_airtime_consumption[WME_AC_MAX];
	struct sawf_admctrl_msduq_stats msduq_stats[DP_SAWF_MAX_TIDS * DP_SAWF_MAX_QUEUES];
};
#endif /* CONFIG_SAWF */
#endif /* _CDP_TXRX_EXTD_STRUCT_H_ */
