/*
 * Copyright (c) 2017-2020 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC: This file has DFS_RCSA Functions.
 *
 */

#include <dfs.h>
#include <dfs_process_radar_found_ind.h>
#include <wlan_dfs_mlme_api.h>
#include <wlan_dfs_utils_api.h>
#include <wlan_reg_services_api.h>
#include <dfs_zero_cac.h>

#if defined(QCA_DFS_RCSA_SUPPORT)
/* dfs_prepare_nol_ie_bitmap: Create a Bitmap from the radar found subchannels
 * to be sent along with RCSA.
 * @dfs: Pointer to wlan_dfs.
 * @radar_found: Pointer to radar_found_info.
 * @in_sub_channels: Pointer to Sub-channels.
 * @n_in_sub_channels: Number of sub-channels.
 */
#ifdef CONFIG_CHAN_FREQ_API
static void
dfs_prepare_nol_ie_bitmap_for_freq(struct wlan_dfs *dfs,
				   struct radar_found_info *radar_found,
				   uint16_t *in_sub_channels,
				   uint8_t n_in_sub_channels)
{
	uint8_t i;
	uint8_t bits;

	dfs->dfs_nol_ie_bandwidth = MIN_DFS_SUBCHAN_BW;
	dfs->dfs_nol_ie_startfreq = in_sub_channels[0];

	/* Number of radar infected channels cannot more than 3 */
	if (n_in_sub_channels > 3)
	    return;

	/* The maximum number of radar infected channels at an instant
	 * can be 3 (if it is a chirp) as per the current radar detection
	 * logic. Also, these radar infected channels will be contiguous in
	 * frequency spectrum. Hence marking the start_freq of NOL IE as
	 * the first subchannel in in_sub_channels. And then setting
	 * "n_in_sub_channels" contiguous bit to indicate the other radar
	 * channels.
	 *
	 * In the receiver side, NOL channels are computed based on the
	 * start_freq specified. Hence no changes needed in the parsing logic.
	 */
	bits = 0x01;
	for (i = 0; i < n_in_sub_channels; i++) {
		dfs->dfs_nol_ie_bitmap |= bits;
		bits <<= 1;
	}
}
#endif

void dfs_fetch_nol_ie_info(struct wlan_dfs *dfs,
			   uint8_t *nol_ie_bandwidth,
			   uint16_t *nol_ie_startfreq,
			   uint8_t *nol_ie_bitmap)
{
	if (nol_ie_bandwidth)
		*nol_ie_bandwidth = dfs->dfs_nol_ie_bandwidth;
	if (nol_ie_startfreq)
		*nol_ie_startfreq = dfs->dfs_nol_ie_startfreq;
	if (nol_ie_bitmap)
		*nol_ie_bitmap = dfs->dfs_nol_ie_bitmap;
}

void dfs_get_rcsa_flags(struct wlan_dfs *dfs, bool *is_rcsa_ie_sent,
			bool *is_nol_ie_sent)
{
	if (is_rcsa_ie_sent)
		*is_rcsa_ie_sent = dfs->dfs_is_rcsa_ie_sent;
	if (is_nol_ie_sent)
		*is_nol_ie_sent = dfs->dfs_is_nol_ie_sent;
}

void dfs_set_rcsa_flags(struct wlan_dfs *dfs, bool is_rcsa_ie_sent,
			bool is_nol_ie_sent)
{
	dfs->dfs_is_rcsa_ie_sent = is_rcsa_ie_sent;
	dfs->dfs_is_nol_ie_sent = is_nol_ie_sent;
}

static void dfs_reset_nol_ie_bitmap(struct wlan_dfs *dfs)
{
	dfs->dfs_nol_ie_bitmap = 0;
}

/**
 * dfs_get_radar_subchans_from_nol_ie(): Populate the radar frequencies
 * from the NOL IE received in RCSA.
 * @dfs: Pointer to struct wlan_dfs
 * @radar_subchans: Output array of radar sub-channels to be filled.
 * @nol_ie_start_freq: NOL IE start frequency
 * @nol_ie_bitmap: NOL IE bitmap
 *
 * Return: Number of channels radar infected.
 */
static uint8_t
dfs_get_radar_subchans_from_nol_ie(struct wlan_dfs *dfs,
				   uint16_t *radar_subchans,
				   qdf_freq_t nol_ie_start_freq,
				   uint8_t nol_ie_bitmap)
{
	uint8_t num_subchans = 0, i;
	uint32_t frequency = (uint32_t)nol_ie_start_freq;
	uint8_t bits = 0x01;

	for (i = 0; i < MAX_20MHZ_SUBCHANS; i++) {
		if (nol_ie_bitmap & bits) {
			radar_subchans[num_subchans] = frequency;
			dfs_debug(dfs, WLAN_DEBUG_DFS_PUNCTURING, "radar_suchan[%u] = %u\n",
				  num_subchans, radar_subchans[num_subchans]);
			num_subchans++;
		}
		bits <<= 1;
		frequency += dfs->dfs_nol_ie_bandwidth;
	}
	return num_subchans;
}

#if defined(WLAN_FEATURE_11BE) && defined(QCA_DFS_BW_PUNCTURE) && \
	defined(QCA_DFS_RCSA_SUPPORT)
/**
 * dfs_is_freq_radar_infected() - Given a chan_freq, return true if the
 * freq is part of radar_subchans, else return false.
 * @chan_freq: Channel frequency in MHz
 * @radar_subchans: An array of 20 MHz sub-channels that are radar infected
 *
 * Return: True if chan_freq is radar infected, false otherwise.
 */
static bool
dfs_is_freq_radar_infected(uint16_t chan_freq, uint16_t *radar_subchans)
{
	uint8_t i;
	bool is_freq_radar = false;

	for (i = 0; i < MAX_20MHZ_SUBCHANS; i++)
		if (chan_freq == radar_subchans[i]) {
			is_freq_radar = true;
			break;
		}

	return is_freq_radar;
}

/**
 * dfs_is_radar_subchans_empty() - Return true if the radar_subchan array
 * is empty (0 frequencies), false otherwise.
 * @radar_subchans: An array of radar subchannels.
 *
 * Return: True if array is empty, false otherwise.
 */
static bool
dfs_is_radar_subchans_empty(uint16_t *radar_subchans)
{
	uint8_t i;

	for (i = 0; i < MAX_20MHZ_SUBCHANS; i++)
		if (radar_subchans[i])
			return false;

	return true;
}

/**
 * dfs_get_radar_bitmap_from_nolie() - Translate the NOL IE bitmap of RCSA
 * to puncture pattern.
 * @dfs: Pointer to struct wlan_dfs
 * @phymode: Phymode of type wlan_phymode
 * @nol_ie_start_freq: NOL IE start freq
 * @nol_ie_bitmap: NOL bitmap
 *
 * Return: Puncture bitmap
 */
uint16_t
dfs_get_radar_bitmap_from_nolie(struct wlan_dfs *dfs,
				qdf_freq_t nol_ie_start_freq,
				uint8_t nol_ie_bitmap,
				bool *is_ignore_radar_puncture)
{
	uint16_t radar_subchans[MAX_20MHZ_SUBCHANS] = {};
	uint16_t radar_punc_bitmap = NO_SCHANS_PUNC;
	uint8_t num_schans;

	*is_ignore_radar_puncture = false;
	num_schans = dfs_get_radar_subchans_from_nol_ie(dfs, radar_subchans,
							nol_ie_start_freq,
							nol_ie_bitmap);

	dfs_handle_radar_puncturing(dfs, &radar_punc_bitmap,
				    radar_subchans, num_schans,
				    is_ignore_radar_puncture);

	return radar_punc_bitmap;
}

static bool
dfs_check_and_ignore_nol_ie_on_punctured_chans(struct wlan_dfs *dfs,
					       uint16_t *radar_subchans,
					       uint8_t num_subchans)
{
	uint16_t dfs_radar_bitmap;

	if (!dfs->dfs_use_puncture)
		return false;

	dfs_radar_bitmap = dfs_generate_radar_bitmap(dfs, radar_subchans,
						     num_subchans);
	if (dfs_radar_bitmap &&
	    dfs_is_ignore_radar_for_punctured_chans(dfs, dfs_radar_bitmap)) {
		dfs_debug(dfs, WLAN_DEBUG_DFS_PUNCTURING,
			  "Ignore NOL IE on punc channel - NOL Bitmap %x"
			  " Cur chan Bitmap %x", dfs_radar_bitmap,
			  dfs->dfs_curchan->dfs_ch_punc_pattern);
		return true;
	}

	return false;
}
#else
static inline bool
dfs_check_and_ignore_nol_ie_on_punctured_chans(struct wlan_dfs *dfs,
					       uint16_t *radar_subchans,
					       uint8_t num_subchans)
{
	return false;
}
#endif

#ifdef CONFIG_CHAN_FREQ_API
bool dfs_process_nol_ie_bitmap(struct wlan_dfs *dfs, uint8_t nol_ie_bandwidth,
			       uint16_t nol_ie_startfreq, uint8_t nol_ie_bitmap)
{
	uint16_t radar_subchans[MAX_20MHZ_SUBCHANS] = {};
	uint16_t nol_freq_list[MAX_20MHZ_SUBCHANS];
	bool should_nol_ie_be_sent = true;
	uint8_t num_subchans;

	qdf_mem_zero(radar_subchans, sizeof(radar_subchans));
	if (!dfs->dfs_use_nol_subchannel_marking) {
		/* Since subchannel marking is disabled, disregard
		 * NOL IE and set NOL IE flag as false, so it
		 * can't be sent to uplink.
		 */
		num_subchans =
		    dfs_get_bonding_channels_for_freq(dfs,
						      dfs->dfs_curchan,
						      SEG_ID_PRIMARY,
						      DETECTOR_ID_0,
						      radar_subchans);
		should_nol_ie_be_sent = false;
	} else {
		/* Add the NOL IE information in DFS structure so that RCSA
		 * and NOL IE can be sent to uplink if uplink exists.
		 */

		dfs->dfs_nol_ie_bandwidth = nol_ie_bandwidth;
		dfs->dfs_nol_ie_startfreq = nol_ie_startfreq;
		dfs->dfs_nol_ie_bitmap = nol_ie_bitmap;
		num_subchans =
			dfs_get_radar_subchans_from_nol_ie(dfs, radar_subchans,
							   nol_ie_startfreq,
							   nol_ie_bitmap);
	}

	if (dfs_check_and_ignore_nol_ie_on_punctured_chans(dfs, radar_subchans,
							   num_subchans)) {
		return should_nol_ie_be_sent;
	}

	dfs_radar_add_channel_list_to_nol_for_freq(dfs, radar_subchans,
						   nol_freq_list,
						   &num_subchans);
	return should_nol_ie_be_sent;
}
#endif
#endif /* QCA_DFS_RCSA_SUPPORT */

#if defined(QCA_DFS_RCSA_SUPPORT)
/**
 * dfs_send_nol_ie_and_rcsa()- Send NOL IE and RCSA action frames.
 * @dfs: Pointer to wlan_dfs structure.
 * @radar_found: Pointer to radar found structure.
 * @nol_freq_list: List of 20MHz frequencies on which radar has been detected.
 * @num_channels: number of radar affected channels.
 * @wait_for_csa: indicates if the repeater AP should take DFS action or wait
 * for CSA
 *
 * Return: void.
 */
void dfs_send_nol_ie_and_rcsa(struct wlan_dfs *dfs,
			      struct radar_found_info *radar_found,
			      uint16_t *nol_freq_list,
			      uint8_t num_channels,
			      bool *wait_for_csa)
{
	dfs->dfs_is_nol_ie_sent = false;
	(dfs->is_radar_during_precac ||
	 radar_found->detector_id == dfs_get_agile_detector_id(dfs)) ?
		(dfs->dfs_is_rcsa_ie_sent = false) :
		(dfs->dfs_is_rcsa_ie_sent = true);
	if (dfs->dfs_use_nol_subchannel_marking) {
		dfs_reset_nol_ie_bitmap(dfs);
		dfs_prepare_nol_ie_bitmap_for_freq(dfs, radar_found,
						   nol_freq_list,
						   num_channels);
		dfs->dfs_is_nol_ie_sent = true;
	}

	/*
	 * This calls into the umac DFS code, which sets the umac
	 * related radar flags and begins the channel change
	 * machinery.

	 * Even during precac, this API is called, but with a flag
	 * saying not to send RCSA, but only the radar affected subchannel
	 * information.
	 */
	dfs_mlme_start_rcsa(dfs->dfs_pdev_obj, wait_for_csa);
}
#endif /* QCA_DFS_RCSA_SUPPORT */

