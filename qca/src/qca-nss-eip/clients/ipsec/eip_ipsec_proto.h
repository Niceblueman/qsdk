/*
 * Copyright (c) 2022-2024, Qualcomm Innovation Center, Inc. All rights reserved.
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

#ifndef __EIP_IPSEC_PROTO_H
#define __EIP_IPSEC_PROTO_H

#define EIP_IPSEC_PROTO_NON_ESP_MARKER 0x0000

/*
 * SFE hook for the receive processing.
 */
extern int (*athrs_fast_nat_recv)(struct sk_buff *skb);

void eip_ipsec_proto_udp_sock_restore(void);
bool eip_ipsec_proto_udp_sock_override(struct eip_ipsec_tuple *sa_tuple);
bool eip_ipsec_proto_esp_init(void);
void eip_ipsec_proto_esp_deinit(void);
void eip_ipsec_proto_dec_done(void *app_data, eip_req_t req);
void eip_ipsec_proto_dec_err(void *app_data, eip_req_t req, int err);
bool eip_ipsec_proto_vp_rx(struct ppe_vp_cb_info *info, void *cb_data);
#endif /* !__EIP_IPSEC_PROTO_H */
