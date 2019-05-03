#ifndef __SVREGS_H__
#define __SVREGS_H__

#include <stdint.h>

#define C_SupV 0x53757056L

#define SV_VERSION (*(volatile uint32_t *) 0x8001007C)

#define LONGREG(addr) (*(volatile uint32_t *) addr)
#define LONGPNT(addr) ((volatile uint32_t *) addr)

#define DDR_BASE (0xA0000000UL + 0x01000000UL)
#define DDR_SIZE (0x08000000UL - 0x01000000UL)

/*************************************
  Ethernet MAC controller registers
**************************************/
//Define the number of packet buffers each for TX and RX
#define ETH_PKT_BUFFS		64UL

/*
Struct used for both TX and RX BD. Note that you can only access
these registers as whole longwords, so there's no point in defining
the bit fields within.
*/
typedef volatile struct {
	volatile uint32_t	len_ctrl;
	volatile uint32_t	data_pnt;
} ETH_BD_TYPE;

#define ETH_MODER			LONGREG(0x80012000UL)
#define ETH_INT_SOURCE		LONGREG(0x80012004UL)
#define ETH_INT_MASK		LONGREG(0x80012008UL)
#define ETH_IPGT			LONGREG(0x8001200CUL)
#define ETH_IPGR1			LONGREG(0x80012010UL)
#define ETH_IPGR2			LONGREG(0x80012014UL)
#define ETH_PACKETLEN		LONGREG(0x80012018UL)
#define ETH_COLLCONF		LONGREG(0x8001201CUL)
#define ETH_TX_BD_NUM		LONGREG(0x80012020UL)
#define ETH_CTRLMODER		LONGREG(0x80012024UL)
#define ETH_MIIMODER		LONGREG(0x80012028UL)
#define ETH_MIICOMMAND		LONGREG(0x8001202CUL)
#define ETH_MIIADDRESS		LONGREG(0x80012030UL)
#define ETH_MIITX_DATA		LONGREG(0x80012034UL)
#define ETH_MIIRX_DATA		LONGREG(0x80012038UL)
#define ETH_MIISTATUS		LONGREG(0x8001203CUL)
#define ETH_MAC_ADDR0		LONGREG(0x80012040UL)
#define ETH_MAC_ADDR1		LONGREG(0x80012044UL)
#define ETH_ETH_HASH0_ADR	LONGREG(0x80012048UL)
#define ETH_ETH_HASH1_ADR	LONGREG(0x8001204CUL)
#define ETH_ETH_TXCTRL		LONGREG(0x80012050UL)

/* Eth MAC BD base */
#define ETH_BD_BASE_PNT		0x80012400UL

/*Define TX BD and put at offset 0 in the BD space*/
/*ETH_BD_TYPE * const eth_tx_bd = (ETH_BD_TYPE*)ETH_BD_BASE_PNT;*/
#define eth_tx_bd  ((ETH_BD_TYPE*)ETH_BD_BASE_PNT)

/*Define RX BD and put two slots away in the BD space*/
/*ETH_BD_TYPE * const eth_rx_bd = (ETH_BD_TYPE*)(ETH_BD_BASE_PNT + 2UL * sizeof(ETH_BD_TYPE));*/
#define eth_rx_bd  ((ETH_BD_TYPE*)(ETH_BD_BASE_PNT + ETH_PKT_BUFFS * sizeof(ETH_BD_TYPE)))

/* Data buffer pointer */
#define ETH_DATA_BASE_ADDR	0x80012800UL
#define ETH_DATA_BASE_PNT	LONGPNT(ETH_DATA_BASE_ADDR)

/* Eth TX BD MASKS */
#define ETH_TX_BD_READY		0x00008000UL
#define ETH_TX_BD_IRQ		0x00004000UL
#define ETH_TX_BD_WRAP		0x00002000UL
#define ETH_TX_BD_PAD		0x00001000UL
#define ETH_TX_BD_CRC		0x00000800UL
#define ETH_TX_BD_UNDERRUN	0x00000100UL
#define ETH_TX_BD_RETRY		0x000000F0UL
#define ETH_TX_BD_RETLIM	0x00000008UL
#define ETH_TX_BD_LATECOL	0x00000004UL
#define ETH_TX_BD_DEFER		0x00000002UL
#define ETH_TX_BD_CARRIER	0x00000001UL

/* Eth RX BD MASKS */
#define ETH_RX_BD_EMPTY		0x00008000UL
#define ETH_RX_BD_IRQ		0x00004000UL
#define ETH_RX_BD_WRAP		0x00002000UL
#define ETH_RX_BD_MISS		0x00000080UL
#define ETH_RX_BD_OVERRUN	0x00000040UL
#define ETH_RX_BD_INVSIMB	0x00000020UL
#define ETH_RX_BD_DRIBBLE	0x00000010UL
#define ETH_RX_BD_TOOLONG	0x00000008UL
#define ETH_RX_BD_SHORT		0x00000004UL
#define ETH_RX_BD_CRCERR	0x00000002UL
#define ETH_RX_BD_LATECOL	0x00000001UL

#define ETH_MODER_RXEN		0x00000001UL
#define ETH_MODER_TXEN		0x00000002UL
#define ETH_MODER_NOPRE		0x00000004UL
#define ETH_MODER_BRO		0x00000008UL
#define ETH_MODER_IAM		0x00000010UL
#define ETH_MODER_PRO		0x00000020UL
#define ETH_MODER_IFG		0x00000040UL
#define ETH_MODER_LOOPBCK	0x00000080UL
#define ETH_MODER_NOBCKOF	0x00000100UL
#define ETH_MODER_EXDFREN	0x00000200UL
#define ETH_MODER_FULLD		0x00000400UL
#define ETH_MODER_RST		0x00000800UL
#define ETH_MODER_DLYCRCEN	0x00001000UL
#define ETH_MODER_CRCEN		0x00002000UL
#define ETH_MODER_HUGEN		0x00004000UL
#define ETH_MODER_PAD		0x00008000UL
#define ETH_MODER_RECSMALL	0x00010000UL

#define ETH_INT_TXB			0x00000001UL
#define ETH_INT_TXE			0x00000002UL
#define ETH_INT_RXB			0x00000004UL
#define ETH_INT_RXE			0x00000008UL
#define ETH_INT_BUSY		0x00000010UL
#define ETH_INT_TXC			0x00000020UL
#define ETH_INT_RXC			0x00000040UL

#define ETH_INT_MASK_TXB	0x00000001UL
#define ETH_INT_MASK_TXE	0x00000002UL
#define ETH_INT_MASK_RXF	0x00000004UL
#define ETH_INT_MASK_RXE	0x00000008UL
#define ETH_INT_MASK_BUSY	0x00000010UL
#define ETH_INT_MASK_TXC	0x00000020UL
#define ETH_INT_MASK_RXC	0x00000040UL

#define ETH_CTRLMODER_PASSALL	0x00000001UL
#define ETH_CTRLMODER_RXFLOW	0x00000002UL
#define ETH_CTRLMODER_TXFLOW	0x00000004UL

#define ETH_MIIMODER_CLKDIV			0x000000FFUL
#define ETH_MIIMODER_NOPRE			0x00000100UL
#define ETH_MIIMODER_RST			0x00000200UL

#define ETH_MIICOMMAND_SCANSTAT		0x00000001UL
#define ETH_MIICOMMAND_RSTAT		0x00000002UL
#define ETH_MIICOMMAND_WCTRLDATA	0x00000004UL

#define ETH_MIIADDRESS_FIAD_MASK	0x0000001FUL
#define ETH_MIIADDRESS_FIAD(x)		((uint32_t)((x << 0) & ETH_MIIADDRESS_FIAD_MASK))
#define ETH_MIIADDRESS_RGAD_MASK	0x00001F00UL
#define ETH_MIIADDRESS_RGAD(x)		((uint32_t)((x << 8) & ETH_MIIADDRESS_RGAD_MASK))


#define ETH_MIISTATUS_LINKFAIL		0x00000001UL
#define ETH_MIISTATUS_BUSY			0x00000002UL
#define ETH_MIISTATUS_NVALID		0x00000004UL

#define ETH_TX_CTRL_TXPAUSERQ		0x00010000UL

#endif