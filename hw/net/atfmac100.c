/*
 * Andes ATFMAC100 10/100 Ethernet
 *
 * Copyright (c) 2018 Andes Tech. Corp.
 *
 * Based on Faraday FTGMAC100 Gigabit Ethernet.
 *
 * Copyright (C) 2016-2017, IBM Corporation.
 *
 * This code is licensed under the GPL version 2 or later. See the
 * COPYING file in the top-level directory.
 */

#include "qemu/osdep.h"
#include "qemu/log.h"
#include "qapi/error.h"
#include "sysemu/dma.h"
#include "net/net.h"
#include "net/eth.h"
#include "migration/vmstate.h"
#include "hw/irq.h"
#include "hw/net/mii.h"
#include "hw/net/atfmac100.h"
#include <zlib.h> /* for crc32 */

/* #define DEBUG_ATFMAC100 */
#define xLOG(x...)
#define yLOG(x...) qemu_log(x)
#ifdef DEBUG_ATFMAC100
  #define LOG(x...) yLOG(x)
#else
  #define LOG(x...) xLOG(x)
#endif

#define LOGGE(x...) qemu_log_mask(LOG_GUEST_ERROR, x)

#define ATFMAC_REVISION     0x10070109
#define ATFMAC100_MODID_VALUE 0x00020118

/* registers */
enum {
  REG_ISR              = 0x00,
  REG_IER              = 0x04,
  REG_MAC_MADR         = 0x08,
  REG_MAC_LADR         = 0x0c,
  REG_MAHT0            = 0x10,
  REG_MAHT1            = 0x14,
  REG_TXPD             = 0x18,
  REG_RXPD             = 0x1c,
  REG_TXR_BADR         = 0x20,
  REG_RXR_BADR         = 0x24,
  REG_ITC              = 0x28,
  REG_APTC             = 0x2c,
  REG_DBLAC            = 0x30,
  REG_REVR             = 0x34,
  REG_FEAR             = 0x38,
  REG_MACCR            = 0x88,
  REG_MACSR            = 0x8c,
  REG_PHYCR            = 0x90,
  REG_PHYWDATA         = 0x94,
  REG_FCR              = 0x98,
  REG_BPR              = 0x9c,
  REG_WOLCR            = 0xa0,
  REG_WOLSR            = 0xa4,
  REG_WFCRC            = 0xa8,
  REG_WFBM1            = 0xb0,
  REG_WFBM2            = 0xb4,
  REG_WFBM3            = 0xb8,
  REG_WFBM4            = 0xbc,
  REG_TS               = 0xc4,
  REG_DMAFIFOS         = 0xc8,
  REG_TM               = 0xcc,
  REG_TXMSCOL          = 0xd4,
  REG_RPFAEPCNT        = 0xd8,
  REG_XMPGCNT          = 0xdc,
  REG_RUNTTLCCNT       = 0xe0,
  REG_CRFTLCNT         = 0xe4,
  REG_RLCRCCNT         = 0xe8,
  REG_BRPCNT           = 0xec,
  REG_MULCACNT         = 0xf0,
  REG_RPCNT            = 0xf4,
  REG_XPCNT            = 0xf8,
};

/* ISR & IER */
enum {
    INT_RPKT_FINISH     = (1 <<  0),
    INT_NORXBUF         = (1 <<  1),
    INT_XPKT_FINISH     = (1 <<  2),
    INT_NOTXBUF         = (1 <<  3),
    INT_XPKT_OK         = (1 <<  4),
    INT_XPKT_LOST       = (1 <<  5),
    INT_RPKT_SAV        = (1 <<  6),
    INT_RPKT_LOST       = (1 <<  7),
    INT_AHB_ERR         = (1 <<  8),
    INT_PHYSTS_CHG      = (1 <<  9),
};

/* Automatic polling timer control register */
#define APTC_RXPOLL_CNT(x)        (((x) >>  0) & 0xf)
#define APTC_RXPOLL_TIME_SEL(x)   (((x) >>  4) & 0x1)
#define APTC_TXPOLL_CNT(x)        (((x) >>  8) & 0xf)
#define APTC_TXPOLL_TIME_SEL(x)   (((x) >> 12) & 0x1)

/* PHY control register */
#define PHYCR_MIIRD               (1 << 26)
#define PHYCR_MIIWR               (1 << 27)
#define PHYCR_PHYAD(x)            (((x) >> 16) & 0x1f)
#define PHYCR_REGAD(x)            (((x) >> 21) & 0x1f)
#define PHYCR_MIIRDATA(x)         ((x) & 0xffff)

/* PHY data register */
#define PHYWDATA_MIIWDATA(x)       ((x) & 0xffff)

/* MAC control register */
enum {
    MACCR_XDMA_EN          = (1 << 0),
    MACCR_RDMA_EN          = (1 << 1),
    MACCR_SW_RST           = (1 << 2),
    MACCR_LOOP_EN          = (1 << 3),
    MACCR_CRC_DIS          = (1 << 4),
    MACCR_XMT_EN           = (1 << 5),
    MACCR_ENRX_IN_HALFTX   = (1 << 6),
    MACCR_RCV_EN           = (1 << 8),
    MACCR_HT_MULTI_EN      = (1 << 9),
    MACCR_RX_RUNT          = (1 << 10),
    MACCR_RX_FTL           = (1 << 11),
    MACCR_RCV_ALL          = (1 << 12),
    MACCR_CRC_APD          = (1 << 14),
    MACCR_FULLDUP          = (1 << 15),
    MACCR_RX_MULTIPKT      = (1 << 16),
    MACCR_RX_BROADPKT      = (1 << 17),
};

/* Transmit descriptor */
#define TXDES0_TXDMA_OWN     (1 << 31)
#define TXDES0_TXPKT_EXSCOL  (1 <<  1)
#define TXDES0_TXPKT_LATECOL (1 <<  0)

#define TXDES1_EDOTR         (1 << 31)
#define TXDES1_TXIC          (1 << 30)
#define TXDES1_TX2FIC        (1 << 29)
#define TXDES1_FTS           (1 << 28)
#define TXDES1_LTS           (1 << 27)
#define TXDES1_TXBUF_SIZE(x) ((x) & 0x7ff)

#define TXDES2_TXBUF_BADR(x) (x)

/* Receive descriptor */

#define RXDES0_RFL(x)        ((x) & 0x07ff)
#define RXDES0_MULTICAST     (1 << 16)
#define RXDES0_BROADCAST     (1 << 17)
#define RXDES0_RX_ERR        (1 << 18)
#define RXDES0_CRC_ERR       (1 << 19)
#define RXDES0_FTL           (1 << 20)
#define RXDES0_RUNT          (1 << 21)
#define RXDES0_RX_ODD_NB     (1 << 22)
#define RXDES0_LRS           (1 << 28)
#define RXDES0_FRS           (1 << 29)
#define RXDES0_RXDMA_OWN     (1 << 31)

#define RXDES1_EDORR         (1 << 31)
#define RXDES1_RXBUF_SIZE(x) ((x) & 0x7ff)

#define RXDES2_RXBUF_BADR(x) (x)

/*
 * Specific RTL8211E MII Registers
 */
#define RTL8211E_MII_PHYCR        16 /* PHY Specific Control */
#define RTL8211E_MII_PHYSR        17 /* PHY Specific Status */
#define RTL8211E_MII_INER         18 /* Interrupt Enable */
#define RTL8211E_MII_INSR         19 /* Interrupt Status */
#define RTL8211E_MII_RXERC        24 /* Receive Error Counter */
#define RTL8211E_MII_LDPSR        27 /* Link Down Power Saving */
#define RTL8211E_MII_EPAGSR       30 /* Extension Page Select */
#define RTL8211E_MII_PAGSEL       31 /* Page Select */

/*
 * RTL8211E Interrupt Status
 */
#define PHY_INT_AUTONEG_ERROR       (1 << 15)
#define PHY_INT_PAGE_RECV           (1 << 12)
#define PHY_INT_AUTONEG_COMPLETE    (1 << 11)
#define PHY_INT_LINK_STATUS         (1 << 10)
#define PHY_INT_ERROR               (1 << 9)
#define PHY_INT_DOWN                (1 << 8)
#define PHY_INT_JABBER              (1 << 0)

/* RX & TX Descriptor */
typedef struct {
    uint32_t des0;
    uint32_t des1;
    uint32_t des2;
    uint32_t des3;
} ATFMAC100Desc;

static inline int
atfmac100_max_frame_size(void)
{
    return ATFMAC100_FRAME_SIZE_MAX + 4; /* + 4 for VLAN */
}

static void
atfmac100_update_irq(ATFMAC100State *s)
{
    int level = !!(s->isr & s->ier);
    qemu_set_irq(s->irq, level);
}

/*
 * The MII phy could raise a GPIO to the processor which in turn
 * could be handled as an interrupt by the OS.
 * For now we don't handle any GPIO/interrupt line, so the OS will
 * have to poll for the PHY status.
 */
static void
phy_update_irq(ATFMAC100State *s)
{
    atfmac100_update_irq(s);
}

static void
phy_update_link(ATFMAC100State *s)
{
    /* Auto negotiation status mirrors link status.  */
    if (qemu_get_queue(s->nic)->link_down) {
        s->phy_status &= ~(MII_BMSR_LINK_ST | MII_BMSR_AN_COMP);
        s->phy_int |= PHY_INT_DOWN;
    } else {
        s->phy_status |= (MII_BMSR_LINK_ST | MII_BMSR_AN_COMP);
        s->phy_int |= PHY_INT_AUTONEG_COMPLETE;
    }
    phy_update_irq(s);
}

static void
atfmac100_set_link(NetClientState *nc)
{
    phy_update_link(ATFMAC100(qemu_get_nic_opaque(nc)));
}

static void
phy_reset(ATFMAC100State *s)
{
    s->phy_status = (MII_BMSR_100TX_FD | MII_BMSR_100TX_HD | MII_BMSR_10T_FD |
                     MII_BMSR_10T_HD | MII_BMSR_EXTSTAT | MII_BMSR_MFPS |
                     MII_BMSR_AN_COMP | MII_BMSR_AUTONEG | MII_BMSR_LINK_ST |
                     MII_BMSR_EXTCAP);
    s->phy_control = (MII_BMCR_AUTOEN | MII_BMCR_FD | MII_BMCR_SPEED1000);
    s->phy_advertise = (MII_ANAR_PAUSE_ASYM | MII_ANAR_PAUSE | MII_ANAR_TXFD |
                        MII_ANAR_TX | MII_ANAR_10FD | MII_ANAR_10 |
                        MII_ANAR_CSMACD);
    s->phy_int_mask = 0;
    s->phy_int = 0;
}

static uint32_t
do_phy_read(ATFMAC100State *s, int reg)
{
    uint32_t val;

    switch (reg) {
    case MII_BMCR: /* Basic Control */
        val = s->phy_control;
        break;
    case MII_BMSR: /* Basic Status */
        val = s->phy_status;
        break;
    case MII_PHYID1: /* ID1 */
        val = RTL8211E_PHYID1;
        break;
    case MII_PHYID2: /* ID2 */
        val = RTL8211E_PHYID2;
        break;
    case MII_ANAR: /* Auto-neg advertisement */
        val = s->phy_advertise;
        break;
    case MII_ANLPAR: /* Auto-neg Link Partner Ability */
        val = (MII_ANLPAR_ACK | MII_ANLPAR_PAUSE | MII_ANLPAR_TXFD |
               MII_ANLPAR_TX | MII_ANLPAR_10FD | MII_ANLPAR_10 |
               MII_ANLPAR_CSMACD);
        break;
    case MII_ANER: /* Auto-neg Expansion */
        val = MII_ANER_NWAY;
        break;
    case MII_CTRL1000: /* 1000BASE-T control  */
        val = (MII_CTRL1000_HALF | MII_CTRL1000_FULL);
        break;
    case MII_STAT1000: /* 1000BASE-T status  */
        val = MII_STAT1000_FULL;
        break;
    case RTL8211E_MII_INSR:  /* Interrupt status.  */
        val = s->phy_int;
        s->phy_int = 0;
        phy_update_irq(s);
        break;
    case RTL8211E_MII_INER:  /* Interrupt enable */
        val = s->phy_int_mask;
        break;
    case RTL8211E_MII_PHYCR:
    case RTL8211E_MII_PHYSR:
    case RTL8211E_MII_RXERC:
    case RTL8211E_MII_LDPSR:
    case RTL8211E_MII_EPAGSR:
    case RTL8211E_MII_PAGSEL:
        qemu_log_mask(LOG_UNIMP, "%s: reg %d not implemented\n",
                      __func__, reg);
        val = 0;
        break;
    default:
        qemu_log_mask(LOG_GUEST_ERROR, "%s: Bad address at offset %d\n",
                      __func__, reg);
        val = 0;
        break;
    }

    return val;
}

#define MII_BMCR_MASK (MII_BMCR_LOOPBACK | MII_BMCR_SPEED100 | \
                       MII_BMCR_SPEED | MII_BMCR_AUTOEN | MII_BMCR_PDOWN | \
                       MII_BMCR_FD | MII_BMCR_CTST)
#define MII_ANAR_MASK 0x2d7f

static void
do_phy_write(ATFMAC100State *s, int reg, uint32_t val)
{
    switch (reg) {
    case MII_BMCR:     /* Basic Control */
        if (val & MII_BMCR_RESET) {
            phy_reset(s);
        } else {
            s->phy_control = val & MII_BMCR_MASK;
            /* Complete autonegotiation immediately.  */
            if (val & MII_BMCR_AUTOEN) {
                s->phy_status |= MII_BMSR_AN_COMP;
            }
        }
        break;
    case MII_ANAR:     /* Auto-neg advertisement */
        s->phy_advertise = (val & MII_ANAR_MASK) | MII_ANAR_TX;
        break;
    case RTL8211E_MII_INER: /* Interrupt enable */
        s->phy_int_mask = val & 0xff;
        phy_update_irq(s);
        break;
    case RTL8211E_MII_PHYCR:
    case RTL8211E_MII_PHYSR:
    case RTL8211E_MII_RXERC:
    case RTL8211E_MII_LDPSR:
    case RTL8211E_MII_EPAGSR:
    case RTL8211E_MII_PAGSEL:
        qemu_log_mask(LOG_UNIMP, "%s: reg %d not implemented\n",
                      __func__, reg);
        break;
    default:
        qemu_log_mask(LOG_GUEST_ERROR, "%s: Bad address at offset %d\n",
                      __func__, reg);
        break;
    }
}

static int
atfmac100_read_desc(ATFMAC100Desc *desc, dma_addr_t addr)
{
    int i;
    uint32_t *ptr;

    if (dma_memory_read(&address_space_memory, addr, desc,
                        sizeof(*desc), MEMTXATTRS_UNSPECIFIED)) {
        qemu_log_mask(LOG_GUEST_ERROR, "%s: failed to read descriptor @ 0x%"
                      HWADDR_PRIx "\n", __func__, addr);
        return -1;
    }

    LOG("%s:  %08x - %08x %08x %08x %08x\n", __func__,
        (int)addr, desc->des0, desc->des1, desc->des2, desc->des3);

    ptr = (uint32_t *)desc;
    for (i = 0; i < 4; ++i) {
        ptr[i] = le32_to_cpu(ptr[i]);
    }

    return 0;
}

static int
atfmac100_write_desc(ATFMAC100Desc *desc, dma_addr_t addr)
{
    int i;
    uint32_t *ptr;

    LOG("%s: %08x - %08x %08x %08x %08x\n", __func__,
        (int)addr, desc->des0, desc->des1, desc->des2, desc->des3);

    ptr = (uint32_t *)desc;
    for (i = 0; i < 4; ++i) {
        ptr[i] = cpu_to_le32(ptr[i]);
    }

    if (dma_memory_write(&address_space_memory, addr, desc,
                            sizeof(*desc), MEMTXATTRS_UNSPECIFIED)) {
        qemu_log_mask(LOG_GUEST_ERROR, "%s: failed to write descriptor @ 0x%"
                      HWADDR_PRIx "\n", __func__, addr);
        return -1;
    }
    return 0;
}

static void
atfmac100_do_tx(ATFMAC100State *s, uint32_t tx_ring_base, uint32_t tx_desc_addr)
{
    int frame_size = 0;
    uint8_t *ptr = s->frame;
    uint32_t addr = tx_desc_addr;
    uint32_t flags = 0;
    int max_frame_size = atfmac100_max_frame_size();

    while (1) {
        ATFMAC100Desc desc;
        int len;

        if (atfmac100_read_desc(&desc, addr) ||
            !(desc.des0 & TXDES0_TXDMA_OWN)) {
            /* Run out of descriptors to transmit.  */
            s->isr |= INT_NOTXBUF;
            break;
        }

        /* record TX flags as they are valid only on the first segment */
        if (desc.des1 & TXDES1_FTS) {
            flags = desc.des1;
            LOG("%s: FTS\n", __func__);
        }

        len = TXDES1_TXBUF_SIZE(desc.des1);
        if (frame_size + len > max_frame_size) {
            qemu_log_mask(LOG_GUEST_ERROR, "%s: frame too big : %d bytes\n",
                          __func__, len);
            len = max_frame_size - frame_size;
        }

        if (dma_memory_read(&address_space_memory, desc.des2, ptr, len,
                            MEMTXATTRS_UNSPECIFIED)) {
            qemu_log_mask(LOG_GUEST_ERROR, "%s: failed to read packet @ 0x%x\n",
                          __func__, desc.des2);
            s->isr |= INT_NOTXBUF;
            break;
        }

        ptr += len;
        frame_size += len;
        if (desc.des1 & TXDES1_LTS) {
            /* Last buffer in frame.  */
            qemu_send_packet(qemu_get_queue(s->nic), s->frame, frame_size);
            ptr = s->frame;
            frame_size = 0;
            LOG("%s: LTS\n", __func__);
            if (flags & TXDES1_TXIC) {
                s->isr |= INT_XPKT_OK;
            }
        }

        if (flags & TXDES1_TX2FIC) {
            s->isr |= INT_XPKT_FINISH;
        }
        desc.des0 &= ~TXDES0_TXDMA_OWN;

        atfmac100_write_desc(&desc, addr);
        /* Advance to the next descriptor.  */
        if (desc.des1 & TXDES1_EDOTR) {
            addr = tx_ring_base;
        } else {
            addr += sizeof(ATFMAC100Desc);
        }
    }

    s->tx_desc_addr = addr;

    atfmac100_update_irq(s);
}

static bool
atfmac100_can_receive(NetClientState *nc)
{
    ATFMAC100State *s = ATFMAC100(qemu_get_nic_opaque(nc));
    ATFMAC100Desc desc;

    uint32_t mask = MACCR_RDMA_EN | MACCR_RCV_EN;
    if ((s->maccr & mask) != mask) {
        return 0;
    }

    if (atfmac100_read_desc(&desc, s->rx_desc_addr)) {
        return 0;
    }

    LOG("%s: %d\n", __func__, !!(desc.des0 & RXDES0_RXDMA_OWN));
    return desc.des0 & RXDES0_RXDMA_OWN;
}

/*
 * This is purely informative. The HW can poll the RW (and RX) ring
 * buffers for available descriptors but we don't need to trigger a
 * timer for that in qemu.
 */
static uint32_t
atfmac100_rxpoll(ATFMAC100State *s)
{
    /*
     * Polling times :
     *
     * Speed      TIME_SEL=0    TIME_SEL=1
     *
     *    10         51.2 ms      819.2 ms
     *   100         5.12 ms      81.92 ms
     */
    const int div = 20;
    uint32_t cnt = 1024 * APTC_RXPOLL_CNT(s->aptc);
    uint32_t period;

    if (APTC_RXPOLL_TIME_SEL(s->aptc)) {
        cnt <<= 4;
    }

    period = cnt / div;

    return period;
}

static void
atfmac100_reset(DeviceState *dev)
{
    ATFMAC100State *s = ATFMAC100(dev);
    LOG("%s:\n", __func__);

    /* registers */
    s->revr = 0x010407; /* 1.4.n */
    s->fear = 0x0007; /* full-duplex */
    s->isr = 0;
    s->ier = 0;
    s->mac_madr = 0;
    s->mac_ladr = 0;
    s->maht[0] = 0;
    s->maht[1] = 0;
    s->txpd = 0;
    s->rxpd = 0;
    s->txr_badr = 0;
    s->rxr_badr = 0;
    s->itc = 0;
    s->aptc = 0;
    s->dblac = 0;
    s->maccr = 0;
    s->macsr = 0;
    s->phycr = 0;
    s->phywdata = 0;
    s->fcr = 0xa400;
    s->bpr = 0;
    s->wolcr = 0;
    s->wolsr = 0;
    s->wfcrc = 0;
    s->wfbm1 = 0;
    s->wfbm2 = 0;
    s->wfbm3 = 0;
    s->wfbm4 = 0;
    s->ts = 0;
    s->dmafifos = 0;
    s->tm = 0;
    s->txmscol = 0;
    s->rpfaepcnt = 0;
    s->xmpgcnt = 0;
    s->runttlccnt = 0;
    s->crftlcnt = 0;
    s->rlcrccnt = 0;
    s->brpcnt = 0;
    s->mulcacnt = 0;
    s->rpcnt = 0;
    s->xpcnt = 0;

    phy_reset(s);
}

static uint64_t
atfmac100_read(void *opaque, hwaddr addr, unsigned size)
{
    ATFMAC100State *s = ATFMAC100(opaque);
    uint64_t value = 0;

    switch (addr) {
    case REG_ISR: /* RC */
        value = s->isr;
        s->isr = 0;
        atfmac100_update_irq(s);
        break;
    case REG_IER:
        value = s->ier;
        break;
    case REG_MAC_MADR:
        value = s->mac_madr;
        break;
    case REG_MAC_LADR:
        value = s->mac_ladr;
        break;
    case REG_MAHT0:
        value = s->maht[0];
        break;
    case REG_MAHT1:
        value = s->maht[1];
        break;
    case REG_TXPD: /* WO */
        value = 0;
        break;
    case REG_RXPD: /* WO */
        value = 0;
        break;
    case REG_TXR_BADR:
        value = s->txr_badr;
        break;
    case REG_RXR_BADR:
        value = s->rxr_badr;
        break;
    case REG_ITC:
        value = s->itc;
        break;
    case REG_APTC:
        value = s->aptc;
        break;
    case REG_DBLAC:
        value = s->dblac;
        break;
    case REG_REVR: /* RO */
        value = s->revr;
        break;
    case REG_FEAR: /* RO */
        value = s->fear;
        break;
    case REG_MACCR:
        value = s->maccr;
        break;
    case REG_MACSR: /* RC */
        value = s->macsr;
        s->macsr = 0;
        break;
    case REG_PHYCR:
        value = s->phycr;
        break;
    case REG_PHYWDATA:
        value = s->phywdata;
        break;
    case REG_FCR:
        value = s->fcr;
        break;
    case REG_BPR:
        value = s->bpr;
        break;
    case REG_WOLCR:
        value = s->wolcr;
        break;
    case REG_WOLSR: /* W1C */
        value = s->wolsr;
        break;
    case REG_WFCRC:
        value = s->wfcrc;
        break;
    case REG_WFBM1:
        value = s->wfbm1;
        break;
    case REG_WFBM2:
        value = s->wfbm2;
        break;
    case REG_WFBM3:
        value = s->wfbm3;
        break;
    case REG_WFBM4:
        value = s->wfbm4;
        break;
    case REG_TS:
        value = s->ts;
        break;
    case REG_DMAFIFOS: /* RO */
        value = s->dmafifos;
        break;
    case REG_TM:
        value = s->tm;
        break;
    case REG_TXMSCOL: /* RO */
        value = s->txmscol;
        break;
    case REG_RPFAEPCNT: /* RO */
        value = s->rpfaepcnt;
        break;
    case REG_XMPGCNT: /* RO */
        value = s->xmpgcnt;
        break;
    case REG_RUNTTLCCNT: /* RO */
        value = s->runttlccnt;
        break;
    case REG_CRFTLCNT: /* RO */
        value = s->crftlcnt;
        break;
    case REG_RLCRCCNT: /* RO */
        value = s->rlcrccnt;
        break;
    case REG_BRPCNT: /* RO */
        value = s->brpcnt;
        break;
    case REG_MULCACNT: /* RO */
        value = s->mulcacnt;
        break;
    case REG_RPCNT: /* RO */
        value = s->rpcnt;
        break;
    case REG_XPCNT: /* RO */
        value = s->xpcnt;
        break;
    default:
        LOGGE("%s: Bad address at offset 0x%" HWADDR_PRIx "\n",
              __func__, addr);
    }

    LOG("%s:  addr %02x, value %08x\n", __func__, (int)addr, (int)value);
    return value;
}

static void
atfmac100_write(void *opaque, hwaddr addr,
                uint64_t value, unsigned size)
{
    ATFMAC100State *s = ATFMAC100(opaque);
    uint32_t reg, mask;
    LOG("%s: addr %02x, value %08x\n", __func__, (int)addr, (int)value);

    switch (addr) {
    case REG_ISR: /* RC */
        break;
    case REG_IER:
        s->ier = value;
        atfmac100_update_irq(s);
        break;
    case REG_MAC_MADR:
        s->mac_madr = value;
        s->conf.macaddr.a[0] = value >> 8;
        s->conf.macaddr.a[1] = value;
        break;
    case REG_MAC_LADR:
        s->mac_ladr = value;
        s->conf.macaddr.a[2] = value >> 24;
        s->conf.macaddr.a[3] = value >> 16;
        s->conf.macaddr.a[4] = value >> 8;
        s->conf.macaddr.a[5] = value;
        break;
    case REG_MAHT0:
        s->maht[0] = value;
        break;
    case REG_MAHT1:
        s->maht[1] = value;
        break;
    case REG_TXPD: /* WO */
        mask = MACCR_XDMA_EN | MACCR_XMT_EN;
        if ((s->maccr & mask) == mask) {
            atfmac100_do_tx(s, s->txr_badr, s->tx_desc_addr);
        }
        if (atfmac100_can_receive(qemu_get_queue(s->nic))) {
            qemu_flush_queued_packets(qemu_get_queue(s->nic));
        }
        break;
    case REG_RXPD: /* WO */
        if (atfmac100_can_receive(qemu_get_queue(s->nic))) {
            qemu_flush_queued_packets(qemu_get_queue(s->nic));
        }
        break;
    case REG_TXR_BADR:
        s->txr_badr = value;
        s->tx_desc_addr = value;
        break;
    case REG_RXR_BADR:
        s->rxr_badr = value;
        s->rx_desc_addr = value;
        break;
    case REG_ITC:
        s->itc = value;
        /* NO effect in this model */
        break;
    case REG_APTC:
        s->aptc = value;
        if (APTC_RXPOLL_CNT(s->aptc)) {
            atfmac100_rxpoll(s);
        }
        if (APTC_TXPOLL_CNT(s->aptc)) {
            qemu_log_mask(LOG_UNIMP, "%s: no transmit polling\n", __func__);
        }
        break;
    case REG_DBLAC:
        s->dblac = value;
        break;
    case REG_REVR: /* RO */
        break;
    case REG_FEAR: /* RO */
        break;
    case REG_MACCR:
        s->maccr = value;
        if (value & MACCR_SW_RST) {
            atfmac100_reset(DEVICE(s));
        }

        if (atfmac100_can_receive(qemu_get_queue(s->nic))) {
            qemu_flush_queued_packets(qemu_get_queue(s->nic));
        }
        break;
    case REG_MACSR: /* RC */
        break;
    case REG_PHYCR:
        reg = PHYCR_REGAD(value);
        s->phycr = value;
        if (value & PHYCR_MIIWR) {
            do_phy_write(s, reg, s->phywdata);
            s->phycr &= ~PHYCR_MIIWR;
        } else {
            reg = do_phy_read(s, reg);
            s->phycr = (s->phycr & ~0xffff) | (reg & 0xffff);
            s->phycr &= ~PHYCR_MIIRD;
        }
        break;
    case REG_PHYWDATA:
        s->phywdata = value & 0xffff;
        break;
    case REG_FCR:
        s->fcr = value;
        /* TODO: flow control */
        break;
    case REG_BPR:
        s->bpr = value;
        break;
    case REG_WOLCR:
        s->wolcr = value;
        break;
    case REG_WOLSR:
        s->wolsr = value;
        break;
    case REG_WFCRC:
        s->wfcrc = value;
        break;
    case REG_WFBM1:
        s->wfbm1 = value;
        break;
    case REG_WFBM2:
        s->wfbm2 = value;
        break;
    case REG_WFBM3:
        s->wfbm3 = value;
        break;
    case REG_WFBM4:
        s->wfbm4 = value;
        break;
    case REG_TS:
        s->ts = value;
        break;
    case REG_DMAFIFOS:
        s->dmafifos = value;
        break;
    case REG_TM:
        s->tm = value;
        break;
    case REG_TXMSCOL:
        s->txmscol = value;
        break;
    case REG_RPFAEPCNT:
        s->rpfaepcnt = value;
        break;
    case REG_XMPGCNT:
        s->xmpgcnt = value;
        break;
    case REG_RUNTTLCCNT:
        s->runttlccnt = value;
        break;
    case REG_CRFTLCNT:
        s->crftlcnt = value;
        break;
    case REG_RLCRCCNT:
        s->rlcrccnt = value;
        break;
    case REG_BRPCNT:
        s->brpcnt = value;
        break;
    case REG_MULCACNT:
        s->mulcacnt = value;
        break;
    case REG_RPCNT:
        s->rpcnt = value;
        break;
    case REG_XPCNT:
        s->xpcnt = value;
        break;
    default:
        qemu_log_mask(LOG_GUEST_ERROR,
            "%s: Bad addr %lx\n", __func__, (long)addr);
    }
}

static int
atfmac100_filter(ATFMAC100State *s, const uint8_t *buf, size_t len)
{
    unsigned mcast_idx;

    if (s->maccr & MACCR_RCV_ALL) {
        return 1;
    }

    switch (get_eth_packet_type(PKT_GET_ETH_HDR(buf))) {
    case ETH_PKT_BCAST:
        if (!(s->maccr & MACCR_RX_BROADPKT)) {
            return 0;
        }
        break;
    case ETH_PKT_MCAST:
        if (!(s->maccr & MACCR_RX_MULTIPKT)) {
            if (!(s->maccr & MACCR_HT_MULTI_EN)) {
                return 0;
            }

            mcast_idx = net_crc32_le(buf, ETH_ALEN);
            mcast_idx = (~(mcast_idx >> 2)) & 0x3f;
            if (!(s->maht[mcast_idx / 32] & (1 << (mcast_idx % 32)))) {
                return 0;
            }
        }
        break;
    case ETH_PKT_UCAST:
        if (memcmp(s->conf.macaddr.a, buf, 6)) {
            return 0;
        }
        break;
    }

    return 1;
}

/* TODO: refine CRC data handling */
static ssize_t
atfmac100_receive(NetClientState *nc, const uint8_t *buf, size_t len)
{
    ATFMAC100State *s = ATFMAC100(qemu_get_nic_opaque(nc));
    ATFMAC100Desc bd;
    uint32_t flags = 0;
    uint32_t mask;
    uint32_t addr;
    uint32_t crc;
    uint32_t buf_addr;
    uint8_t *crc_ptr;
    uint32_t buf_len;
    size_t size = len;
    uint32_t first = RXDES0_FRS;
    int max_frame_size = atfmac100_max_frame_size();

    LOG("%s: len %zd (%zx)\n", __func__, len, len + 4);
    mask = MACCR_RDMA_EN | MACCR_RCV_EN;
    if ((s->maccr & mask) != mask) {
        return -1;
    }

    /* TODO : Pad to minimum Ethernet frame length */
    /* handle small packets.  */
    if (size < 10) {
        qemu_log_mask(LOG_GUEST_ERROR, "%s: dropped frame of %zd bytes\n",
                      __func__, size);
        return size;
    }

    if (size < 64 && !(s->maccr & MACCR_RX_RUNT)) {
        qemu_log_mask(LOG_GUEST_ERROR, "%s: dropped runt frame of %zd bytes\n",
                      __func__, size);
        return size;
    }

    if (!atfmac100_filter(s, buf, size)) {
        return size;
    }

    /* 4 bytes for the CRC.  */
    size += 4;
    crc = cpu_to_be32(crc32(~0, buf, size));
    crc_ptr = (uint8_t *) &crc;

    /* Huge frames are truncated.  */
    if (size > max_frame_size) {
        size = max_frame_size;
        qemu_log_mask(LOG_GUEST_ERROR, "%s: frame too big : %zd bytes\n",
                      __func__, size);
        flags |= RXDES0_FTL;
    }

    switch (get_eth_packet_type(PKT_GET_ETH_HDR(buf))) {
    case ETH_PKT_BCAST:
        flags |= RXDES0_BROADCAST;
        break;
    case ETH_PKT_MCAST:
        flags |= RXDES0_MULTICAST;
        break;
    case ETH_PKT_UCAST:
        break;
    }

    addr = s->rx_desc_addr;
    while (size > 0) {
        if (!atfmac100_can_receive(nc)) {
            qemu_log_mask(LOG_GUEST_ERROR, "%s: Unexpected packet\n", __func__);
            return -1;
        }

        if (atfmac100_read_desc(&bd, addr) || !(bd.des0 & RXDES0_RXDMA_OWN)) {
            /* No descriptors available.  Bail out.  */
            qemu_log_mask(LOG_GUEST_ERROR, "%s: Lost end of frame\n",
                          __func__);
            s->isr |= INT_NORXBUF;
            break;
        }
        buf_len = bd.des1 & 0x7ff;
        buf_len = (size < buf_len) ? size : buf_len;
        bd.des0 = (bd.des0 & ~0x7ff) | size; /* RFL */
        bd.des1 = (bd.des1 & ~0x7ff) | buf_len; /* RXBUF_SIZE */
        size -= buf_len;

        /* The last 4 bytes are the CRC.  */
        if (size < 4) {
            buf_len += size - 4;
        }
        buf_addr = bd.des2;
        dma_memory_write(&address_space_memory, buf_addr, buf, buf_len,
                            MEMTXATTRS_UNSPECIFIED);
        buf += buf_len;
        if (size < 4) {
            dma_memory_write(&address_space_memory, buf_addr + buf_len,
                             crc_ptr, 4 - size, MEMTXATTRS_UNSPECIFIED);
            crc_ptr += 4 - size;
        }

        bd.des0 |= first;
        bd.des0 &= ~RXDES0_RXDMA_OWN;
        first = 0;
        if (size == 0) {
            /* Last buffer in frame.  */
            bd.des0 |= flags | RXDES0_LRS;
            s->isr |= INT_RPKT_FINISH;
        } else {
            s->isr |= INT_RPKT_SAV;
        }
        atfmac100_write_desc(&bd, addr);
        /* next RX descriptor */
        if (bd.des1 & RXDES1_EDORR) {
            addr = s->rxr_badr;
        } else {
            addr += sizeof(ATFMAC100Desc);
        }
    }
    s->rx_desc_addr = addr;

    atfmac100_update_irq(s);
    return len;
}

static const MemoryRegionOps atfmac100_ops = {
    .read = atfmac100_read,
    .write = atfmac100_write,
    .valid.min_access_size = 4,
    .valid.max_access_size = 4,
    .endianness = DEVICE_LITTLE_ENDIAN,
};

static void
atfmac100_cleanup(NetClientState *nc)
{
    ATFMAC100State *s = ATFMAC100(qemu_get_nic_opaque(nc));
    s->nic = NULL;
}

static NetClientInfo net_atfmac100_info = {
    .type = NET_CLIENT_DRIVER_NIC,
    .size = sizeof(NICState),
    .can_receive = atfmac100_can_receive,
    .receive = atfmac100_receive,
    .cleanup = atfmac100_cleanup,
    .link_status_changed = atfmac100_set_link,
};

static void atfmac100_realize(DeviceState *dev, Error **errp)
{
    ATFMAC100State *s = ATFMAC100(dev);
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);

    memory_region_init_io(&s->mmio, OBJECT(dev), &atfmac100_ops, s,
                          TYPE_ATFMAC100, 0x100);
    sysbus_init_mmio(sbd, &s->mmio);
    sysbus_init_irq(sbd, &s->irq);

    qemu_macaddr_default_if_unset(&s->conf.macaddr);
    s->conf.peers.ncs[0] = nd_table[0].netdev;
    s->nic = qemu_new_nic(&net_atfmac100_info, &s->conf,
                          object_get_typename(OBJECT(dev)), dev->id, s);
    qemu_format_nic_info_str(qemu_get_queue(s->nic), s->conf.macaddr.a);
}

static const VMStateDescription vmstate_atfmac100 = {
    .name = TYPE_ATFMAC100,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]){
        /* TODO */
        VMSTATE_UINT32(isr, ATFMAC100State),
        VMSTATE_END_OF_LIST()
    }
};

static Property atfmac100_properties[] = {
    DEFINE_NIC_PROPERTIES(ATFMAC100State, conf),
    DEFINE_PROP_UINT32("revision", ATFMAC100State, revision,
                       ATFMAC100_MODID_VALUE),
    DEFINE_PROP_END_OF_LIST(),
};

static void atfmac100_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->vmsd = &vmstate_atfmac100;
    dc->reset = atfmac100_reset;
    dc->realize = atfmac100_realize;
    dc->desc = "Andes ATFMAC100 Ethernet emulation";
    set_bit(DEVICE_CATEGORY_NETWORK, dc->categories);
    device_class_set_props(dc, atfmac100_properties);
}

static const TypeInfo atfmac100_info = {
    .name = TYPE_ATFMAC100,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(ATFMAC100State),
    .class_init = atfmac100_class_init,
};

static void atfmac100_register_types(void)
{
    type_register_static(&atfmac100_info);
}

type_init(atfmac100_register_types)

/*
 * Create MAC device.
 */
void
atfmac100_create(ATFMAC100State *atfmac, const char *name,
                 NICInfo *nd, hwaddr addr, qemu_irq irq)
{
    /* realize */
    if (nd->used) {
        qemu_check_nic_model(nd, TYPE_ATFMAC100);
        qdev_set_nic_properties(DEVICE(atfmac), nd);
    }
    object_property_set_int(OBJECT(atfmac), "revision", ATFMAC_REVISION,
                            &error_abort);
    sysbus_realize(SYS_BUS_DEVICE(atfmac), NULL);
    /* TODO: handle Error */
    sysbus_mmio_map(SYS_BUS_DEVICE(atfmac), 0, addr);
    sysbus_connect_irq(SYS_BUS_DEVICE(atfmac), 0, irq);
    return;
}
