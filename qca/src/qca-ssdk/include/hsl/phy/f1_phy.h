/*
 * Copyright (c) 2012, 2015, 2017-2018, The Linux Foundation. All rights reserved.
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

#ifndef _F1_PHY_H_
#define _F1_PHY_H_

#ifdef __cplusplus
extern "C" {
#endif                          /* __cplusplus */

    /* PHY Registers */
#define F1_PHY_CONTROL                      0
#define F1_PHY_STATUS                       1
#define F1_PHY_ID1                          2
#define F1_PHY_ID2                          3
#define F1_AUTONEG_ADVERT                   4
#define F1_LINK_PARTNER_ABILITY             5
#define F1_AUTONEG_EXPANSION                6
#define F1_NEXT_PAGE_TRANSMIT               7
#define F1_LINK_PARTNER_NEXT_PAGE           8
#define F1_1000BASET_CONTROL                9
#define F1_1000BASET_STATUS                 10
#define F1_MMD_CTRL_REG                     13
#define F1_MMD_DATA_REG                     14
#define F1_EXTENDED_STATUS                  15
#define F1_PHY_SPEC_CONTROL                 16
#define F1_PHY_SPEC_STATUS                  17
#define F1_PHY_INTR_MASK                    18
#define F1_PHY_INTR_STATUS                  19
#define F1_PHY_CDT_CONTROL                  22
#define F1_PHY_CDT_STATUS                   28
#define F1_DEBUG_PORT_ADDRESS               29
#define F1_DEBUG_PORT_DATA                  30
#define F1_PHY_8023AZ_EEE_CTRL	0x3c
#define F1_PHY_MMD7_NUM	7
#define F1_PHY_AZ_ENABLE	0x6

    /*debug port*/
#define F1_DEBUG_PORT_RGMII_MODE            18
#define F1_DEBUG_PORT_RGMII_MODE_EN         0x0008

#define F1_DEBUG_PORT_RX_DELAY            0
#define F1_DEBUG_PORT_RX_DELAY_EN         0x8000

#define F1_DEBUG_PORT_TX_DELAY            5
#define F1_DEBUG_PORT_TX_DELAY_EN         0x0100

    /* PHY Registers Field*/

    /* Control Register fields  offset:0*/
    /* bits 6,13: 10=1000, 01=100, 00=10 */
#define F1_CTRL_SPEED_MSB                   0x0040

    /* Collision test enable */
#define F1_CTRL_COLL_TEST_ENABLE            0x0080

    /* FDX =1, half duplex =0 */
#define F1_CTRL_FULL_DUPLEX                 0x0100

    /* Restart auto negotiation */
#define F1_CTRL_RESTART_AUTONEGOTIATION     0x0200

    /* Isolate PHY from MII */
#define F1_CTRL_ISOLATE                     0x0400

    /* Power down */
#define F1_CTRL_POWER_DOWN                  0x0800

    /* Auto Neg Enable */
#define F1_CTRL_AUTONEGOTIATION_ENABLE      0x1000

    /* bits 6,13: 10=1000, 01=100, 00=10 */
#define F1_CTRL_SPEED_LSB                   0x2000

    /* 0 = normal, 1 = loopback */
#define F1_LOCAL_LOOPBACK_ENABLE            0x4000
#define F1_COMMON_CTRL                      0x1040
#define F1_10M_LOOPBACK                     0x4100
#define F1_100M_LOOPBACK                    0x6100
#define F1_1000M_LOOPBACK                   0x4140

#define F1_PHY_MMD3_NUM  3
#define F1_PHY_MMD3_ADDR_REMOTE_LOOPBACK_CTRL       0x805a
#define F1_PHY_REMOTE_LOOPBACK_ENABLE       0x0001
#define F1_CTRL_SOFTWARE_RESET              0x8000

#define F1_CTRL_SPEED_MASK                  0x2040
#define F1_CTRL_SPEED_1000                  0x0040
#define F1_CTRL_SPEED_100                   0x2000
#define F1_CTRL_SPEED_10                    0x0000

#define F1_RESET_DONE(phy_control)                   \
    (((phy_control) & (F1_CTRL_SOFTWARE_RESET)) == 0)

    /* Status Register fields offset:1*/
    /* Extended register capabilities */
#define F1_STATUS_EXTENDED_CAPS             0x0001

    /* Jabber Detected */
#define F1_STATUS_JABBER_DETECT             0x0002

    /* Link Status 1 = link */
#define F1_STATUS_LINK_STATUS_UP            0x0004

    /* Auto Neg Capable */
#define F1_STATUS_AUTONEG_CAPS              0x0008

    /* Remote Fault Detect */
#define F1_STATUS_REMOTE_FAULT              0x0010

    /* Auto Neg Complete */
#define F1_STATUS_AUTO_NEG_DONE             0x0020

    /* Preamble may be suppressed */
#define F1_STATUS_PREAMBLE_SUPPRESS         0x0040

    /* Ext. status info in Reg 0x0F */
#define F1_STATUS_EXTENDED_STATUS           0x0100

    /* 100T2 Half Duplex Capable */
#define F1_STATUS_100T2_HD_CAPS             0x0200

    /* 100T2 Full Duplex Capable */
#define F1_STATUS_100T2_FD_CAPS             0x0400

    /* 10T   Half Duplex Capable */
#define F1_STATUS_10T_HD_CAPS               0x0800

    /* 10T   Full Duplex Capable */
#define F1_STATUS_10T_FD_CAPS               0x1000

    /* 100X  Half Duplex Capable */
#define F1_STATUS_100X_HD_CAPS              0x2000

    /* 100X  Full Duplex Capable */
#define F1_STATUS_100X_FD_CAPS              0x4000

    /* 100T4 Capable */
#define F1_STATUS_100T4_CAPS                0x8000

    /* extended status register capabilities */

#define F1_STATUS_1000T_HD_CAPS             0x1000

#define F1_STATUS_1000T_FD_CAPS             0x2000

#define F1_STATUS_1000X_HD_CAPS             0x4000

#define F1_STATUS_1000X_FD_CAPS             0x8000

#define F1_AUTONEG_DONE(ip_phy_status) \
    (((ip_phy_status) & (F1_STATUS_AUTO_NEG_DONE)) ==  \
        (F1_STATUS_AUTO_NEG_DONE))

    /* PHY identifier1  offset:2*/
//Organizationally Unique Identifier bits 3:18

    /* PHY identifier2  offset:3*/
//Organizationally Unique Identifier bits 19:24

    /* Auto-Negotiation Advertisement register. offset:4*/
    /* indicates IEEE 802.3 CSMA/CD */
#define F1_ADVERTISE_SELECTOR_FIELD         0x0001

    /* 10T   Half Duplex Capable */
#define F1_ADVERTISE_10HALF                 0x0020

    /* 10T   Full Duplex Capable */
#define F1_ADVERTISE_10FULL                 0x0040

    /* 100TX Half Duplex Capable */
#define F1_ADVERTISE_100HALF                0x0080

    /* 100TX Full Duplex Capable */
#define F1_ADVERTISE_100FULL                0x0100

    /* 100T4 Capable */
#define F1_ADVERTISE_100T4                  0x0200

    /* Pause operation desired */
#define F1_ADVERTISE_PAUSE                  0x0400

    /* Asymmetric Pause Direction bit */
#define F1_ADVERTISE_ASYM_PAUSE             0x0800

    /* Remote Fault detected */
#define F1_ADVERTISE_REMOTE_FAULT           0x2000

    /* Next Page ability supported */
#define F1_ADVERTISE_NEXT_PAGE              0x8000

    /* 100TX Half Duplex Capable */
#define F1_ADVERTISE_1000HALF                0x0100

    /* 100TX Full Duplex Capable */
#define F1_ADVERTISE_1000FULL                0x0200

#define F1_ADVERTISE_ALL \
    (F1_ADVERTISE_10HALF | F1_ADVERTISE_10FULL | \
     F1_ADVERTISE_100HALF | F1_ADVERTISE_100FULL | \
     F1_ADVERTISE_1000FULL)

#define F1_ADVERTISE_MEGA_ALL \
    (F1_ADVERTISE_10HALF | F1_ADVERTISE_10FULL | \
     F1_ADVERTISE_100HALF | F1_ADVERTISE_100FULL)

    /* Link Partner ability offset:5*/
    /* Same as advertise selector  */
#define F1_LINK_SLCT                        0x001f

    /* Can do 10mbps half-duplex   */
#define F1_LINK_10BASETX_HALF_DUPLEX        0x0020

    /* Can do 10mbps full-duplex   */
#define F1_LINK_10BASETX_FULL_DUPLEX        0x0040

    /* Can do 100mbps half-duplex  */
#define F1_LINK_100BASETX_HALF_DUPLEX       0x0080

    /* Can do 100mbps full-duplex  */
#define F1_LINK_100BASETX_FULL_DUPLEX       0x0100

    /* Can do 1000mbps full-duplex  */
#define F1_LINK_1000BASETX_FULL_DUPLEX       0x0800

    /* Can do 1000mbps half-duplex  */
#define F1_LINK_1000BASETX_HALF_DUPLEX       0x0400

    /* 100BASE-T4  */
#define F1_LINK_100BASE4                    0x0200

    /* PAUSE */
#define F1_LINK_PAUSE                       0x0400

    /* Asymmetrical PAUSE */
#define F1_LINK_ASYPAUSE                    0x0800

    /* Link partner faulted  */
#define F1_LINK_RFAULT                      0x2000

    /* Link partner acked us */
#define F1_LINK_LPACK                       0x4000

    /* Next page bit  */
#define F1_LINK_NPAGE                       0x8000

    /* Auto-Negotiation Expansion Register offset:6 */

    /* Next Page Transmit Register offset:7 */

    /* Link partner Next Page Register offset:8*/

    /* 1000BASE-T Control Register offset:9*/
    /* Advertise 1000T HD capability */
#define F1_CTL_1000T_HD_CAPS                0x0100

    /* Advertise 1000T FD capability  */
#define F1_CTL_1000T_FD_CAPS                0x0200

    /* 1=Repeater/switch device port 0=DTE device*/
#define F1_CTL_1000T_REPEATER_DTE           0x0400

    /* 1=Configure PHY as Master  0=Configure PHY as Slave */
#define F1_CTL_1000T_MS_VALUE               0x0800

    /* 1=Master/Slave manual config value  0=Automatic Master/Slave config */
#define F1_CTL_1000T_MS_ENABLE              0x1000

    /* Normal Operation */
#define F1_CTL_1000T_TEST_MODE_NORMAL       0x0000

    /* Transmit Waveform test */
#define F1_CTL_1000T_TEST_MODE_1            0x2000

    /* Master Transmit Jitter test */
#define F1_CTL_1000T_TEST_MODE_2            0x4000

    /* Slave Transmit Jitter test */
#define F1_CTL_1000T_TEST_MODE_3            0x6000

    /* Transmitter Distortion test */
#define F1_CTL_1000T_TEST_MODE_4            0x8000
#define F1_CTL_1000T_SPEED_MASK             0x0300
#define F1_CTL_1000T_DEFAULT_CAP_MASK       0x0300

    /* 1000BASE-T Status Register offset:10 */
    /* LP is 1000T HD capable */
#define F1_STATUS_1000T_LP_HD_CAPS          0x0400

    /* LP is 1000T FD capable */
#define F1_STATUS_1000T_LP_FD_CAPS          0x0800

    /* Remote receiver OK */
#define F1_STATUS_1000T_REMOTE_RX_STATUS    0x1000

    /* Local receiver OK */
#define F1_STATUS_1000T_LOCAL_RX_STATUS     0x2000

    /* 1=Local TX is Master, 0=Slave */
#define F1_STATUS_1000T_MS_CONFIG_RES       0x4000

#define F1_STATUS_1000T_MS_CONFIG_FAULT     0x8000

    /* Master/Slave config fault */
#define F1_STATUS_1000T_REMOTE_RX_STATUS_SHIFT   12
#define F1_STATUS_1000T_LOCAL_RX_STATUS_SHIFT    13

    /* Phy Specific Control Register offset:16*/
    /* 1=Jabber Function disabled */
#define F1_CTL_JABBER_DISABLE               0x0001

    /* 1=Polarity Reversal enabled */
#define F1_CTL_POLARITY_REVERSAL            0x0002

    /* 1=SQE Test enabled */
#define F1_CTL_SQE_TEST                     0x0004
#define F1_CTL_MAC_POWERDOWN                0x0008

    /* 1=CLK125 low, 0=CLK125 toggling
    #define F1_CTL_CLK125_DISABLE               0x0010
     */
    /* MDI Crossover Mode bits 6:5 */
    /* Manual MDI configuration */
#define F1_CTL_MDI_MANUAL_MODE              0x0000

    /* Manual MDIX configuration */
#define F1_CTL_MDIX_MANUAL_MODE             0x0020

    /* 1000BASE-T: Auto crossover, 100BASE-TX/10BASE-T: MDI Mode */
#define F1_CTL_AUTO_X_1000T                 0x0040

    /* Auto crossover enabled all speeds */
#define F1_CTL_AUTO_X_MODE                  0x0060

    /* 1=Enable Extended 10BASE-T distance
      * (Lower 10BASE-T RX Threshold)
     * 0=Normal 10BASE-T RX Threshold */
#define F1_CTL_10BT_EXT_DIST_ENABLE         0x0080

    /* 1=5-Bit interface in 100BASE-TX
      * 0=MII interface in 100BASE-TX */
#define F1_CTL_MII_5BIT_ENABLE              0x0100

    /* 1=Scrambler disable */
#define F1_CTL_SCRAMBLER_DISABLE            0x0200

    /* 1=Force link good */
#define F1_CTL_FORCE_LINK_GOOD              0x0400

    /* 1=Assert CRS on Transmit */
#define F1_CTL_ASSERT_CRS_ON_TX             0x0800

#define F1_CTL_POLARITY_REVERSAL_SHIFT      1
#define F1_CTL_AUTO_X_MODE_SHIFT            5
#define F1_CTL_10BT_EXT_DIST_ENABLE_SHIFT   7

    /* Phy Specific status fields offset:17*/
    /* 1=Speed & Duplex resolved */
#define F1_STATUS_LINK_PASS                 0x0400
#define F1_STATUS_RESOVLED                  0x0800

    /* 1=Duplex 0=Half Duplex */
#define F1_STATUS_FULL_DUPLEX               0x2000

    /* Speed, bits 14:15 */
#define F1_STATUS_SPEED                    0xC000
#define F1_STATUS_SPEED_MASK               0xC000

    /* 00=10Mbs */
#define F1_STATUS_SPEED_10MBS              0x0000

    /* 01=100Mbs */
#define F1_STATUS_SPEED_100MBS             0x4000

    /* 10=1000Mbs */
#define F1_STATUS_SPEED_1000MBS            0x8000
#define F1_SPEED_DUPLEX_RESOVLED(phy_status)                   \
    (((phy_status) &                                  \
        (F1_STATUS_RESOVLED)) ==                    \
        (F1_STATUS_RESOVLED))

    /*phy debug port1 register offset:29*/
    /*phy debug port2 register offset:30*/

    /*F1 interrupt flag */
#define F1_INTR_SPEED_CHANGE              0x4000
#define F1_INTR_DUPLEX_CHANGE             0x2000
#define F1_INTR_STATUS_UP_CHANGE          0x0400
#define F1_INTR_STATUS_DOWN_CHANGE        0x0800

#define F1_PHY_MDIX                       0x0020
#define F1_PHY_MDIX_AUTO                  0x0060
#define F1_PHY_MDIX_STATUS                0x0040

    sw_error_t
    f1_phy_set_powersave(a_uint32_t dev_id, a_uint32_t phy_id, a_bool_t enable);

    sw_error_t
    f1_phy_get_powersave(a_uint32_t dev_id, a_uint32_t phy_id, a_bool_t *enable);

    sw_error_t
    f1_phy_set_hibernate(a_uint32_t dev_id, a_uint32_t phy_id, a_bool_t enable);

    sw_error_t
    f1_phy_get_hibernate(a_uint32_t dev_id, a_uint32_t phy_id, a_bool_t *enable);

    sw_error_t
    f1_phy_cdt(a_uint32_t dev_id, a_uint32_t phy_id, a_uint32_t mdi_pair,
               fal_cable_status_t *cable_status, a_uint32_t *cable_len) ;

    sw_error_t
    f1_phy_set_duplex(a_uint32_t dev_id, a_uint32_t phy_id,
                      fal_port_duplex_t duplex);

    sw_error_t
    f1_phy_get_duplex(a_uint32_t dev_id, a_uint32_t phy_id,
                      fal_port_duplex_t * duplex);

    sw_error_t
    f1_phy_set_speed(a_uint32_t dev_id, a_uint32_t phy_id,
                     fal_port_speed_t speed);

    sw_error_t
    f1_phy_get_speed(a_uint32_t dev_id, a_uint32_t phy_id,
                     fal_port_speed_t * speed);

    sw_error_t
    f1_phy_intr_mask_set(a_uint32_t dev_id, a_uint32_t phy_id,
                         a_uint32_t intr_mask_flag);

    sw_error_t
    f1_phy_intr_mask_get(a_uint32_t dev_id, a_uint32_t phy_id,
                         a_uint32_t * intr_mask_flag);

    sw_error_t
    f1_phy_intr_status_get(a_uint32_t dev_id, a_uint32_t phy_id,
                           a_uint32_t * intr_status_flag);

    a_bool_t
    f1_phy_reset_done(a_uint32_t dev_id, a_uint32_t phy_addr);

    int
    f1_phy_init(a_uint32_t dev_id, a_uint32_t port_bmp);

#ifdef __cplusplus
}
#endif                          /* __cplusplus */
#endif                          /* _F1_PHY_H_ */
