/*
 * Copyright (c) 2001
 *	Fortress Technologies, Inc.  All rights reserved.
 *      Charlie Lenahan (clenahan@fortresstech.com)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code distributions
 * retain the above copyright notice and this paragraph in its entirety, (2)
 * distributions including binary code include the above copyright notice and
 * this paragraph in its entirety in the documentation or other materials
 * provided with the distribution, and (3) all advertising materials mentioning
 * features or use of this software display the following acknowledgement:
 * ``This product includes software developed by the University of California,
 * Lawrence Berkeley Laboratory and its contributors.'' Neither the name of
 * the University nor the names of its contributors may be used to endorse
 * or promote products derived from this software without specific prior
 * written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

/* \summary: IEEE 802.11 printer */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "netdissect-stdinc.h"

#include <string.h>

#include "netdissect.h"
#include "addrtoname.h"

#include "extract.h"

#include "cpack.h"


/* Lengths of 802.11 header components. */
#define	IEEE802_11_FC_LEN		2
#define	IEEE802_11_DUR_LEN		2
#define	IEEE802_11_DA_LEN		6
#define	IEEE802_11_SA_LEN		6
#define	IEEE802_11_BSSID_LEN		6
#define	IEEE802_11_RA_LEN		6
#define	IEEE802_11_TA_LEN		6
#define	IEEE802_11_ADDR1_LEN		6
#define	IEEE802_11_SEQ_LEN		2
#define	IEEE802_11_CTL_LEN		2
#define	IEEE802_11_CARRIED_FC_LEN	2
#define	IEEE802_11_HT_CONTROL_LEN	4
#define	IEEE802_11_IV_LEN		3
#define	IEEE802_11_KID_LEN		1

/* Frame check sequence length. */
#define	IEEE802_11_FCS_LEN		4

/* Lengths of beacon components. */
#define	IEEE802_11_TSTAMP_LEN		8
#define	IEEE802_11_BCNINT_LEN		2
#define	IEEE802_11_CAPINFO_LEN		2
#define	IEEE802_11_LISTENINT_LEN	2

#define	IEEE802_11_AID_LEN		2
#define	IEEE802_11_STATUS_LEN		2
#define	IEEE802_11_REASON_LEN		2

/* Length of previous AP in reassocation frame */
#define	IEEE802_11_AP_LEN		6

#define	T_MGMT 0x0  /* management */
#define	T_CTRL 0x1  /* control */
#define	T_DATA 0x2 /* data */
#define	T_RESV 0x3  /* reserved */

#define	ST_ASSOC_REQUEST	0x0
#define	ST_ASSOC_RESPONSE	0x1
#define	ST_REASSOC_REQUEST	0x2
#define	ST_REASSOC_RESPONSE	0x3
#define	ST_PROBE_REQUEST	0x4
#define	ST_PROBE_RESPONSE	0x5
/* RESERVED			0x6  */
#define ST_RESERVED		0x7
#define	ST_BEACON		0x8
#define	ST_ATIM			0x9
#define	ST_DISASSOC		0xA
#define	ST_AUTH			0xB
#define	ST_DEAUTH		0xC
#define	ST_ACTION		0xD
/* RESERVED			0xE  */
/* RESERVED			0xF  */

static const struct tok st_str[] = {
	{ ST_ASSOC_REQUEST,    "Assoc Request"    },
	{ ST_ASSOC_RESPONSE,   "Assoc Response"   },
	{ ST_REASSOC_REQUEST,  "ReAssoc Request"  },
	{ ST_REASSOC_RESPONSE, "ReAssoc Response" },
	{ ST_PROBE_REQUEST,    "Probe Request"    },
	{ ST_PROBE_RESPONSE,   "Probe Response"   },
	{ ST_BEACON,           "Beacon"           },
	{ ST_ATIM,             "ATIM"             },
	{ ST_DISASSOC,         "Disassociation"   },
	{ ST_AUTH,             "Authentication"   },
	{ ST_DEAUTH,           "DeAuthentication" },
	{ ST_ACTION,           "Action"           },
	{ ST_RESERVED,         "Reserved"         },
	{ 0, NULL }
};

#define CTRL_CONTROL_WRAPPER	0x7
#define	CTRL_BAR	0x8
#define	CTRL_BA		0x9
#define	CTRL_PS_POLL	0xA
#define	CTRL_RTS	0xB
#define	CTRL_CTS	0xC
#define	CTRL_ACK	0xD
#define	CTRL_CF_END	0xE
#define	CTRL_END_ACK	0xF

static const struct tok ctrl_str[] = {
	{ CTRL_CONTROL_WRAPPER, "Control Wrapper" },
	{ CTRL_BAR,             "BAR"             },
	{ CTRL_BA,              "BA"              },
	{ CTRL_PS_POLL,         "Power Save-Poll" },
	{ CTRL_RTS,             "Request-To-Send" },
	{ CTRL_CTS,             "Clear-To-Send"   },
	{ CTRL_ACK,             "Acknowledgment"  },
	{ CTRL_CF_END,          "CF-End"          },
	{ CTRL_END_ACK,         "CF-End+CF-Ack"   },
	{ 0, NULL }
};

#define	DATA_DATA			0x0
#define	DATA_DATA_CF_ACK		0x1
#define	DATA_DATA_CF_POLL		0x2
#define	DATA_DATA_CF_ACK_POLL		0x3
#define	DATA_NODATA			0x4
#define	DATA_NODATA_CF_ACK		0x5
#define	DATA_NODATA_CF_POLL		0x6
#define	DATA_NODATA_CF_ACK_POLL		0x7

#define DATA_QOS_DATA			0x8
#define DATA_QOS_DATA_CF_ACK		0x9
#define DATA_QOS_DATA_CF_POLL		0xA
#define DATA_QOS_DATA_CF_ACK_POLL	0xB
#define DATA_QOS_NODATA			0xC
#define DATA_QOS_CF_POLL_NODATA		0xE
#define DATA_QOS_CF_ACK_POLL_NODATA	0xF

/*
 * The subtype field of a data frame is, in effect, composed of 4 flag
 * bits - CF-Ack, CF-Poll, Null (means the frame doesn't actually have
 * any data), and QoS.
 */
#define DATA_FRAME_IS_CF_ACK(x)		((x) & 0x01)
#define DATA_FRAME_IS_CF_POLL(x)	((x) & 0x02)
#define DATA_FRAME_IS_NULL(x)		((x) & 0x04)
#define DATA_FRAME_IS_QOS(x)		((x) & 0x08)

/*
 * Bits in the frame control field.
 */
#define	FC_VERSION(fc)		((fc) & 0x3)
#define	FC_TYPE(fc)		(((fc) >> 2) & 0x3)
#define	FC_SUBTYPE(fc)		(((fc) >> 4) & 0xF)
#define	FC_TO_DS(fc)		((fc) & 0x0100)
#define	FC_FROM_DS(fc)		((fc) & 0x0200)
#define	FC_MORE_FLAG(fc)	((fc) & 0x0400)
#define	FC_RETRY(fc)		((fc) & 0x0800)
#define	FC_POWER_MGMT(fc)	((fc) & 0x1000)
#define	FC_MORE_DATA(fc)	((fc) & 0x2000)
#define	FC_PROTECTED(fc)	((fc) & 0x4000)
#define	FC_ORDER(fc)		((fc) & 0x8000)

struct mgmt_header_t {
	nd_uint16_t	fc;
	nd_uint16_t	duration;
	nd_mac48	da;
	nd_mac48	sa;
	nd_mac48	bssid;
	nd_uint16_t	seq_ctrl;
};

#define	MGMT_HDRLEN	(IEEE802_11_FC_LEN+IEEE802_11_DUR_LEN+\
			 IEEE802_11_DA_LEN+IEEE802_11_SA_LEN+\
			 IEEE802_11_BSSID_LEN+IEEE802_11_SEQ_LEN)

#define	CAPABILITY_ESS(cap)	((cap) & 0x0001)
#define	CAPABILITY_IBSS(cap)	((cap) & 0x0002)
#define	CAPABILITY_CFP(cap)	((cap) & 0x0004)
#define	CAPABILITY_CFP_REQ(cap)	((cap) & 0x0008)
#define	CAPABILITY_PRIVACY(cap)	((cap) & 0x0010)

struct ssid_t {
	u_int		length;
	u_char		ssid[33];  /* 32 + 1 for null */
};

struct rates_t {
	u_int		length;
	uint8_t		rate[16];
};

struct challenge_t {
	u_int		length;
	uint8_t		text[254]; /* 1-253 + 1 for null */
};

struct fh_t {
	u_int		length;
	uint16_t	dwell_time;
	uint8_t		hop_set;
	uint8_t	hop_pattern;
	uint8_t		hop_index;
};

struct ds_t {
	u_int		length;
	uint8_t		channel;
};

struct cf_t {
	u_int		length;
	uint8_t		count;
	uint8_t		period;
	uint16_t	max_duration;
	uint16_t	dur_remaining;
};

struct tim_t {
	u_int		length;
	uint8_t		count;
	uint8_t		period;
	uint8_t		bitmap_control;
	uint8_t		bitmap[251];
};

struct meshid_t {
	u_int		length;
	u_char		meshid[33];  /* 32 + 1 for null */
};

#define	E_SSID		0
#define	E_RATES	1
#define	E_FH		2
#define	E_DS		3
#define	E_CF		4
#define	E_TIM		5
#define	E_IBSS		6
/* reserved		7 */
/* reserved		8 */
/* reserved		9 */
/* reserved		10 */
/* reserved		11 */
/* reserved		12 */
/* reserved		13 */
/* reserved		14 */
/* reserved		15 */
/* reserved		16 */

#define	E_CHALLENGE	16
/* reserved		17 */
/* reserved		18 */
/* reserved		19 */
/* reserved		16 */
/* reserved		16 */
#define E_MESHID	114


struct mgmt_body_t {
	uint8_t		timestamp[IEEE802_11_TSTAMP_LEN];
	uint16_t	beacon_interval;
	uint16_t	listen_interval;
	uint16_t	status_code;
	uint16_t	aid;
	u_char		ap[IEEE802_11_AP_LEN];
	uint16_t	reason_code;
	uint16_t	auth_alg;
	uint16_t	auth_trans_seq_num;
	int		challenge_present;
	struct challenge_t  challenge;
	uint16_t	capability_info;
	int		ssid_present;
	struct ssid_t	ssid;
	int		rates_present;
	struct rates_t	rates;
	int		ds_present;
	struct ds_t	ds;
	int		cf_present;
	struct cf_t	cf;
	int		fh_present;
	struct fh_t	fh;
	int		tim_present;
	struct tim_t	tim;
	int		meshid_present;
	struct meshid_t	meshid;
};

struct ctrl_control_wrapper_hdr_t {
	nd_uint16_t	fc;
	nd_uint16_t	duration;
	nd_mac48	addr1;
	nd_uint16_t	carried_fc[IEEE802_11_CARRIED_FC_LEN];
	nd_uint16_t	ht_control[IEEE802_11_HT_CONTROL_LEN];
};

#define	CTRL_CONTROL_WRAPPER_HDRLEN	(IEEE802_11_FC_LEN+IEEE802_11_DUR_LEN+\
					 IEEE802_11_ADDR1_LEN+\
					 IEEE802_11_CARRIED_FC_LEN+\
					 IEEE802_11_HT_CONTROL_LEN)

struct ctrl_rts_hdr_t {
	nd_uint16_t	fc;
	nd_uint16_t	duration;
	nd_mac48	ra;
	nd_mac48	ta;
};

#define	CTRL_RTS_HDRLEN	(IEEE802_11_FC_LEN+IEEE802_11_DUR_LEN+\
			 IEEE802_11_RA_LEN+IEEE802_11_TA_LEN)

struct ctrl_cts_hdr_t {
	nd_uint16_t	fc;
	nd_uint16_t	duration;
	nd_mac48	ra;
};

#define	CTRL_CTS_HDRLEN	(IEEE802_11_FC_LEN+IEEE802_11_DUR_LEN+IEEE802_11_RA_LEN)

struct ctrl_ack_hdr_t {
	nd_uint16_t	fc;
	nd_uint16_t	duration;
	nd_mac48	ra;
};

#define	CTRL_ACK_HDRLEN	(IEEE802_11_FC_LEN+IEEE802_11_DUR_LEN+IEEE802_11_RA_LEN)

struct ctrl_ps_poll_hdr_t {
	nd_uint16_t	fc;
	nd_uint16_t	aid;
	nd_mac48	bssid;
	nd_mac48	ta;
};

#define	CTRL_PS_POLL_HDRLEN	(IEEE802_11_FC_LEN+IEEE802_11_AID_LEN+\
				 IEEE802_11_BSSID_LEN+IEEE802_11_TA_LEN)

struct ctrl_end_hdr_t {
	nd_uint16_t	fc;
	nd_uint16_t	duration;
	nd_mac48	ra;
	nd_mac48	bssid;
};

#define	CTRL_END_HDRLEN	(IEEE802_11_FC_LEN+IEEE802_11_DUR_LEN+\
			 IEEE802_11_RA_LEN+IEEE802_11_BSSID_LEN)

struct ctrl_end_ack_hdr_t {
	nd_uint16_t	fc;
	nd_uint16_t	duration;
	nd_mac48	ra;
	nd_mac48	bssid;
};

#define	CTRL_END_ACK_HDRLEN	(IEEE802_11_FC_LEN+IEEE802_11_DUR_LEN+\
				 IEEE802_11_RA_LEN+IEEE802_11_BSSID_LEN)

struct ctrl_ba_hdr_t {
	nd_uint16_t	fc;
	nd_uint16_t	duration;
	nd_mac48	ra;
	nd_mac48	ta;
};

#define	CTRL_BA_HDRLEN	(IEEE802_11_FC_LEN+IEEE802_11_DUR_LEN+\
			 IEEE802_11_RA_LEN+IEEE802_11_TA_LEN)

struct ctrl_bar_hdr_t {
	nd_uint16_t	fc;
	nd_uint16_t	dur;
	nd_mac48	ra;
	nd_mac48	ta;
	nd_uint16_t	ctl;
	nd_uint16_t	seq;
};

#define	CTRL_BAR_HDRLEN		(IEEE802_11_FC_LEN+IEEE802_11_DUR_LEN+\
				 IEEE802_11_RA_LEN+IEEE802_11_TA_LEN+\
				 IEEE802_11_CTL_LEN+IEEE802_11_SEQ_LEN)

struct meshcntl_t {
	nd_uint8_t	flags;
	nd_uint8_t	ttl;
	nd_uint32_t	seq;
	nd_mac48	addr4;
	nd_mac48	addr5;
	nd_mac48	addr6;
};

#define	IV_IV(iv)	((iv) & 0xFFFFFF)
#define	IV_PAD(iv)	(((iv) >> 24) & 0x3F)
#define	IV_KEYID(iv)	(((iv) >> 30) & 0x03)

#define PRINT_SSID(p) \
	if (p.ssid_present) { \
		ND_PRINT(" ("); \
		fn_print_str(ndo, p.ssid.ssid); \
		ND_PRINT(")"); \
	}

#define PRINT_RATE(_sep, _r, _suf) \
	ND_PRINT("%s%2.1f%s", _sep, (.5 * ((_r) & 0x7f)), _suf)
#define PRINT_RATES(p) \
	if (p.rates_present) { \
		const char *sep = " ["; \
		if (p.rates.length != 0) { \
			for (u_int z = 0; z < p.rates.length ; z++) { \
				PRINT_RATE(sep, p.rates.rate[z], \
					(p.rates.rate[z] & 0x80 ? "*" : "")); \
				sep = " "; \
			} \
			ND_PRINT(" Mbit]"); \
		} \
	}

#define PRINT_DS_CHANNEL(p) \
	if (p.ds_present) \
		ND_PRINT(" CH: %u", p.ds.channel); \
	ND_PRINT("%s", \
	    CAPABILITY_PRIVACY(p.capability_info) ? ", PRIVACY" : "");

#define PRINT_MESHID(p) \
	if (p.meshid_present) { \
		ND_PRINT(" (MESHID: "); \
		fn_print_str(ndo, p.meshid.meshid); \
		ND_PRINT(")"); \
	}

#define MAX_MCS_INDEX	76

/*
 * Indices are:
 *
 *	the MCS index (0-76);
 *
 *	0 for 20 MHz, 1 for 40 MHz;
 *
 *	0 for a long guard interval, 1 for a short guard interval.
 */
static const float ieee80211_float_htrates[MAX_MCS_INDEX+1][2][2] = {
	/* MCS  0  */
	{	/* 20 Mhz */ {    6.5f,		/* SGI */    7.2f, },
		/* 40 Mhz */ {   13.5f,		/* SGI */   15.0f, },
	},

	/* MCS  1  */
	{	/* 20 Mhz */ {   13.0f,		/* SGI */   14.4f, },
		/* 40 Mhz */ {   27.0f,		/* SGI */   30.0f, },
	},

	/* MCS  2  */
	{	/* 20 Mhz */ {   19.5f,		/* SGI */   21.7f, },
		/* 40 Mhz */ {   40.5f,		/* SGI */   45.0f, },
	},

	/* MCS  3  */
	{	/* 20 Mhz */ {   26.0f,		/* SGI */   28.9f, },
		/* 40 Mhz */ {   54.0f,		/* SGI */   60.0f, },
	},

	/* MCS  4  */
	{	/* 20 Mhz */ {   39.0f,		/* SGI */   43.3f, },
		/* 40 Mhz */ {   81.0f,		/* SGI */   90.0f, },
	},

	/* MCS  5  */
	{	/* 20 Mhz */ {   52.0f,		/* SGI */   57.8f, },
		/* 40 Mhz */ {  108.0f,		/* SGI */  120.0f, },
	},

	/* MCS  6  */
	{	/* 20 Mhz */ {   58.5f,		/* SGI */   65.0f, },
		/* 40 Mhz */ {  121.5f,		/* SGI */  135.0f, },
	},

	/* MCS  7  */
	{	/* 20 Mhz */ {   65.0f,		/* SGI */   72.2f, },
		/* 40 Mhz */ {   135.0f,	/* SGI */  150.0f, },
	},

	/* MCS  8  */
	{	/* 20 Mhz */ {   13.0f,		/* SGI */   14.4f, },
		/* 40 Mhz */ {   27.0f,		/* SGI */   30.0f, },
	},

	/* MCS  9  */
	{	/* 20 Mhz */ {   26.0f,		/* SGI */   28.9f, },
		/* 40 Mhz */ {   54.0f,		/* SGI */   60.0f, },
	},

	/* MCS 10  */
	{	/* 20 Mhz */ {   39.0f,		/* SGI */   43.3f, },
		/* 40 Mhz */ {   81.0f,		/* SGI */   90.0f, },
	},

	/* MCS 11  */
	{	/* 20 Mhz */ {   52.0f,		/* SGI */   57.8f, },
		/* 40 Mhz */ {  108.0f,		/* SGI */  120.0f, },
	},

	/* MCS 12  */
	{	/* 20 Mhz */ {   78.0f,		/* SGI */   86.7f, },
		/* 40 Mhz */ {  162.0f,		/* SGI */  180.0f, },
	},

	/* MCS 13  */
	{	/* 20 Mhz */ {  104.0f,		/* SGI */  115.6f, },
		/* 40 Mhz */ {  216.0f,		/* SGI */  240.0f, },
	},

	/* MCS 14  */
	{	/* 20 Mhz */ {  117.0f,		/* SGI */  130.0f, },
		/* 40 Mhz */ {  243.0f,		/* SGI */  270.0f, },
	},

	/* MCS 15  */
	{	/* 20 Mhz */ {  130.0f,		/* SGI */  144.4f, },
		/* 40 Mhz */ {  270.0f,		/* SGI */  300.0f, },
	},

	/* MCS 16  */
	{	/* 20 Mhz */ {   19.5f,		/* SGI */   21.7f, },
		/* 40 Mhz */ {   40.5f,		/* SGI */   45.0f, },
	},

	/* MCS 17  */
	{	/* 20 Mhz */ {   39.0f,		/* SGI */   43.3f, },
		/* 40 Mhz */ {   81.0f,		/* SGI */   90.0f, },
	},

	/* MCS 18  */
	{	/* 20 Mhz */ {   58.5f,		/* SGI */   65.0f, },
		/* 40 Mhz */ {  121.5f,		/* SGI */  135.0f, },
	},

	/* MCS 19  */
	{	/* 20 Mhz */ {   78.0f,		/* SGI */   86.7f, },
		/* 40 Mhz */ {  162.0f,		/* SGI */  180.0f, },
	},

	/* MCS 20  */
	{	/* 20 Mhz */ {  117.0f,		/* SGI */  130.0f, },
		/* 40 Mhz */ {  243.0f,		/* SGI */  270.0f, },
	},

	/* MCS 21  */
	{	/* 20 Mhz */ {  156.0f,		/* SGI */  173.3f, },
		/* 40 Mhz */ {  324.0f,		/* SGI */  360.0f, },
	},

	/* MCS 22  */
	{	/* 20 Mhz */ {  175.5f,		/* SGI */  195.0f, },
		/* 40 Mhz */ {  364.5f,		/* SGI */  405.0f, },
	},

	/* MCS 23  */
	{	/* 20 Mhz */ {  195.0f,		/* SGI */  216.7f, },
		/* 40 Mhz */ {  405.0f,		/* SGI */  450.0f, },
	},

	/* MCS 24  */
	{	/* 20 Mhz */ {   26.0f,		/* SGI */   28.9f, },
		/* 40 Mhz */ {   54.0f,		/* SGI */   60.0f, },
	},

	/* MCS 25  */
	{	/* 20 Mhz */ {   52.0f,		/* SGI */   57.8f, },
		/* 40 Mhz */ {  108.0f,		/* SGI */  120.0f, },
	},

	/* MCS 26  */
	{	/* 20 Mhz */ {   78.0f,		/* SGI */   86.7f, },
		/* 40 Mhz */ {  162.0f,		/* SGI */  180.0f, },
	},

	/* MCS 27  */
	{	/* 20 Mhz */ {  104.0f,		/* SGI */  115.6f, },
		/* 40 Mhz */ {  216.0f,		/* SGI */  240.0f, },
	},

	/* MCS 28  */
	{	/* 20 Mhz */ {  156.0f,		/* SGI */  173.3f, },
		/* 40 Mhz */ {  324.0f,		/* SGI */  360.0f, },
	},

	/* MCS 29  */
	{	/* 20 Mhz */ {  208.0f,		/* SGI */  231.1f, },
		/* 40 Mhz */ {  432.0f,		/* SGI */  480.0f, },
	},

	/* MCS 30  */
	{	/* 20 Mhz */ {  234.0f,		/* SGI */  260.0f, },
		/* 40 Mhz */ {  486.0f,		/* SGI */  540.0f, },
	},

	/* MCS 31  */
	{	/* 20 Mhz */ {  260.0f,		/* SGI */  288.9f, },
		/* 40 Mhz */ {  540.0f,		/* SGI */  600.0f, },
	},

	/* MCS 32  */
	{	/* 20 Mhz */ {    0.0f,		/* SGI */    0.0f, }, /* not valid */
		/* 40 Mhz */ {    6.0f,		/* SGI */    6.7f, },
	},

	/* MCS 33  */
	{	/* 20 Mhz */ {   39.0f,		/* SGI */   43.3f, },
		/* 40 Mhz */ {   81.0f,		/* SGI */   90.0f, },
	},

	/* MCS 34  */
	{	/* 20 Mhz */ {   52.0f,		/* SGI */   57.8f, },
		/* 40 Mhz */ {  108.0f,		/* SGI */  120.0f, },
	},

	/* MCS 35  */
	{	/* 20 Mhz */ {   65.0f,		/* SGI */   72.2f, },
		/* 40 Mhz */ {  135.0f,		/* SGI */  150.0f, },
	},

	/* MCS 36  */
	{	/* 20 Mhz */ {   58.5f,		/* SGI */   65.0f, },
		/* 40 Mhz */ {  121.5f,		/* SGI */  135.0f, },
	},

	/* MCS 37  */
	{	/* 20 Mhz */ {   78.0f,		/* SGI */   86.7f, },
		/* 40 Mhz */ {  162.0f,		/* SGI */  180.0f, },
	},

	/* MCS 38  */
	{	/* 20 Mhz */ {   97.5f,		/* SGI */  108.3f, },
		/* 40 Mhz */ {  202.5f,		/* SGI */  225.0f, },
	},

	/* MCS 39  */
	{	/* 20 Mhz */ {   52.0f,		/* SGI */   57.8f, },
		/* 40 Mhz */ {  108.0f,		/* SGI */  120.0f, },
	},

	/* MCS 40  */
	{	/* 20 Mhz */ {   65.0f,		/* SGI */   72.2f, },
		/* 40 Mhz */ {  135.0f,		/* SGI */  150.0f, },
	},

	/* MCS 41  */
	{	/* 20 Mhz */ {   65.0f,		/* SGI */   72.2f, },
		/* 40 Mhz */ {  135.0f,		/* SGI */  150.0f, },
	},

	/* MCS 42  */
	{	/* 20 Mhz */ {   78.0f,		/* SGI */   86.7f, },
		/* 40 Mhz */ {  162.0f,		/* SGI */  180.0f, },
	},

	/* MCS 43  */
	{	/* 20 Mhz */ {   91.0f,		/* SGI */  101.1f, },
		/* 40 Mhz */ {  189.0f,		/* SGI */  210.0f, },
	},

	/* MCS 44  */
	{	/* 20 Mhz */ {   91.0f,		/* SGI */  101.1f, },
		/* 40 Mhz */ {  189.0f,		/* SGI */  210.0f, },
	},

	/* MCS 45  */
	{	/* 20 Mhz */ {  104.0f,		/* SGI */  115.6f, },
		/* 40 Mhz */ {  216.0f,		/* SGI */  240.0f, },
	},

	/* MCS 46  */
	{	/* 20 Mhz */ {   78.0f,		/* SGI */   86.7f, },
		/* 40 Mhz */ {  162.0f,		/* SGI */  180.0f, },
	},

	/* MCS 47  */
	{	/* 20 Mhz */ {   97.5f,		/* SGI */  108.3f, },
		/* 40 Mhz */ {  202.5f,		/* SGI */  225.0f, },
	},

	/* MCS 48  */
	{	/* 20 Mhz */ {   97.5f,		/* SGI */  108.3f, },
		/* 40 Mhz */ {  202.5f,		/* SGI */  225.0f, },
	},

	/* MCS 49  */
	{	/* 20 Mhz */ {  117.0f,		/* SGI */  130.0f, },
		/* 40 Mhz */ {  243.0f,		/* SGI */  270.0f, },
	},

	/* MCS 50  */
	{	/* 20 Mhz */ {  136.5f,		/* SGI */  151.7f, },
		/* 40 Mhz */ {  283.5f,		/* SGI */  315.0f, },
	},

	/* MCS 51  */
	{	/* 20 Mhz */ {  136.5f,		/* SGI */  151.7f, },
		/* 40 Mhz */ {  283.5f,		/* SGI */  315.0f, },
	},

	/* MCS 52  */
	{	/* 20 Mhz */ {  156.0f,		/* SGI */  173.3f, },
		/* 40 Mhz */ {  324.0f,		/* SGI */  360.0f, },
	},

	/* MCS 53  */
	{	/* 20 Mhz */ {   65.0f,		/* SGI */   72.2f, },
		/* 40 Mhz */ {  135.0f,		/* SGI */  150.0f, },
	},

	/* MCS 54  */
	{	/* 20 Mhz */ {   78.0f,		/* SGI */   86.7f, },
		/* 40 Mhz */ {  162.0f,		/* SGI */  180.0f, },
	},

	/* MCS 55  */
	{	/* 20 Mhz */ {   91.0f,		/* SGI */  101.1f, },
		/* 40 Mhz */ {  189.0f,		/* SGI */  210.0f, },
	},

	/* MCS 56  */
	{	/* 20 Mhz */ {   78.0f,		/* SGI */   86.7f, },
		/* 40 Mhz */ {  162.0f,		/* SGI */  180.0f, },
	},

	/* MCS 57  */
	{	/* 20 Mhz */ {   91.0f,		/* SGI */  101.1f, },
		/* 40 Mhz */ {  189.0f,		/* SGI */  210.0f, },
	},

	/* MCS 58  */
	{	/* 20 Mhz */ {  104.0f,		/* SGI */  115.6f, },
		/* 40 Mhz */ {  216.0f,		/* SGI */  240.0f, },
	},

	/* MCS 59  */
	{	/* 20 Mhz */ {  117.0f,		/* SGI */  130.0f, },
		/* 40 Mhz */ {  243.0f,		/* SGI */  270.0f, },
	},

	/* MCS 60  */
	{	/* 20 Mhz */ {  104.0f,		/* SGI */  115.6f, },
		/* 40 Mhz */ {  216.0f,		/* SGI */  240.0f, },
	},

	/* MCS 61  */
	{	/* 20 Mhz */ {  117.0f,		/* SGI */  130.0f, },
		/* 40 Mhz */ {  243.0f,		/* SGI */  270.0f, },
	},

	/* MCS 62  */
	{	/* 20 Mhz */ {  130.0f,		/* SGI */  144.4f, },
		/* 40 Mhz */ {  270.0f,		/* SGI */  300.0f, },
	},

	/* MCS 63  */
	{	/* 20 Mhz */ {  130.0f,		/* SGI */  144.4f, },
		/* 40 Mhz */ {  270.0f,		/* SGI */  300.0f, },
	},

	/* MCS 64  */
	{	/* 20 Mhz */ {  143.0f,		/* SGI */  158.9f, },
		/* 40 Mhz */ {  297.0f,		/* SGI */  330.0f, },
	},

	/* MCS 65  */
	{	/* 20 Mhz */ {   97.5f,		/* SGI */  108.3f, },
		/* 40 Mhz */ {  202.5f,		/* SGI */  225.0f, },
	},

	/* MCS 66  */
	{	/* 20 Mhz */ {  117.0f,		/* SGI */  130.0f, },
		/* 40 Mhz */ {  243.0f,		/* SGI */  270.0f, },
	},

	/* MCS 67  */
	{	/* 20 Mhz */ {  136.5f,		/* SGI */  151.7f, },
		/* 40 Mhz */ {  283.5f,		/* SGI */  315.0f, },
	},

	/* MCS 68  */
	{	/* 20 Mhz */ {  117.0f,		/* SGI */  130.0f, },
		/* 40 Mhz */ {  243.0f,		/* SGI */  270.0f, },
	},

	/* MCS 69  */
	{	/* 20 Mhz */ {  136.5f,		/* SGI */  151.7f, },
		/* 40 Mhz */ {  283.5f,		/* SGI */  315.0f, },
	},

	/* MCS 70  */
	{	/* 20 Mhz */ {  156.0f,		/* SGI */  173.3f, },
		/* 40 Mhz */ {  324.0f,		/* SGI */  360.0f, },
	},

	/* MCS 71  */
	{	/* 20 Mhz */ {  175.5f,		/* SGI */  195.0f, },
		/* 40 Mhz */ {  364.5f,		/* SGI */  405.0f, },
	},

	/* MCS 72  */
	{	/* 20 Mhz */ {  156.0f,		/* SGI */  173.3f, },
		/* 40 Mhz */ {  324.0f,		/* SGI */  360.0f, },
	},

	/* MCS 73  */
	{	/* 20 Mhz */ {  175.5f,		/* SGI */  195.0f, },
		/* 40 Mhz */ {  364.5f,		/* SGI */  405.0f, },
	},

	/* MCS 74  */
	{	/* 20 Mhz */ {  195.0f,		/* SGI */  216.7f, },
		/* 40 Mhz */ {  405.0f,		/* SGI */  450.0f, },
	},

	/* MCS 75  */
	{	/* 20 Mhz */ {  195.0f,		/* SGI */  216.7f, },
		/* 40 Mhz */ {  405.0f,		/* SGI */  450.0f, },
	},

	/* MCS 76  */
	{	/* 20 Mhz */ {  214.5f,		/* SGI */  238.3f, },
		/* 40 Mhz */ {  445.5f,		/* SGI */  495.0f, },
	},
};

/* For IEEE80211_RADIOTAP_HE Data 1 */
#define IEEE80211_RADIOTAP_HE_PPDU_FORMAT_MASK			0x0003
#define IEEE80211_RADIOTAP_HE_PPDU_FORMAT_HE_SU			0
#define IEEE80211_RADIOTAP_HE_PPDU_FORMAT_HE_EXT_SU		1
#define IEEE80211_RADIOTAP_HE_PPDU_FORMAT_HE_MU			2
#define IEEE80211_RADIOTAP_HE_PPDU_FORMAT_HE_TRIG		3
#define IEEE80211_RADIOTAP_HE_BSS_COLOR_KNOWN			0x0004
#define IEEE80211_RADIOTAP_HE_BEAM_CHANGE_KNOWN			0x0008
#define IEEE80211_RADIOTAP_HE_UL_DL_KNOWN			0x0010
#define IEEE80211_RADIOTAP_HE_DATA_MCS_KNOWN			0x0020
#define IEEE80211_RADIOTAP_HE_DATA_DCM_KNOWN			0x0040
#define IEEE80211_RADIOTAP_HE_CODING_KNOWN			0x0080
#define IEEE80211_RADIOTAP_HE_LDPC_EXTRA_SYMBOL_SEGMENT_KNOWN	0x0100
#define IEEE80211_RADIOTAP_HE_STBC_KNOWN			0x0200
#define IEEE80211_RADIOTAP_HE_SPATIAL_REUSE_KNOWN		0x0400
#define IEEE80211_RADIOTAP_HE_SPATIAL_REUSE_2_KNOWN		0x0800
#define IEEE80211_RADIOTAP_HE_SPATIAL_REUSE_3_KNOWN		0x1000
#define IEEE80211_RADIOTAP_HE_SPATIAL_REUSE_4_KNOWN		0x2000
#define IEEE80211_RADIOTAP_HE_DATA_BW_RU_ALLOCATION_KNOWN	0x4000
#define IEEE80211_RADIOTAP_HE_DOPPLER_KNOWN			0x8000

/* For IEEE80211_RADIOTAP_HE Data 2 */
#define IEEE80211_RADIOTAP_HE_PRI_SEC_80_MHZ_KNOWN		0x0001
#define IEEE80211_RADIOTAP_HE_GI_KNOWN				0x0002
#define IEEE80211_RADIOTAP_HE_NUM_LTF_SYMBOLS_KNOWN		0x0004
#define IEEE80211_RADIOTAP_HE_PRE_FEC_PADDING_FACTOR_KNOWN	0x0008
#define IEEE80211_RADIOTAP_HE_TXBF_KNOWN			0x0010
#define IEEE80211_RADIOTAP_HE_PE_DISAMBIGUITY_KNOWN		0x0020
#define IEEE80211_RADIOTAP_HE_TXOP_KNOWN			0x0040
#define IEEE80211_RADIOTAP_HE_MIDAMBLE_PERIODICITY_KNOWN	0x0080
#define IEEE80211_RADIOTAP_HE_RU_ALLOCATION_OFFSET		0x3F00
#define IEEE80211_RADIOTAP_HE_RU_ALLOCATION_OFFSET_KNOWN	0x4000
#define IEEE80211_RADIOTAP_HE_PRI_SEC_80_MHZ			0x8000

/* For IEEE80211_RADIOTAP_HE Data 3 */
#define IEEE80211_RADIOTAP_HE_BSS_COLOR_MASK			0x003F
#define IEEE80211_RADIOTAP_HE_BEAM_CHANGE			0x0040
#define IEEE80211_RADIOTAP_HE_UL_DL				0x0080
#define IEEE80211_RADIOTAP_HE_DATA_MCS_MASK			0x0F00
#define IEEE80211_RADIOTAP_HE_DATA_DCM				0x1000
#define IEEE80211_RADIOTAP_HE_CODING				0x2000
#define IEEE80211_RADIOTAP_HE_LDPC_EXTRA_SYMBOL_SEGMENT		0x4000
#define IEEE80211_RADIOTAP_HE_STBC				0x8000

/* HE_SU and HE_EXT_SU formap PPDU */
#define IEEE80211_RADIOTAP_HE_SPATIAL_REUSE_MASK		0x000F
#define IEEE80211_RADIOTAP_HE_D4_FFF0				0xFFF0
/* HE_TRIG format PPDU */
#define IEEE80211_RADIOTAP_HE_SPATIAL_REUSE_1_MASK		0X000F
#define IEEE80211_RADIOTAP_HE_SPATIAL_REUSE_2_MASK		0X00F0
#define IEEE80211_RADIOTAP_HE_SPATIAL_REUSE_3_MASK		0X0F00
#define IEEE80211_RADIOTAP_HE_SPATIAL_REUSE_4_MASK		0XF000
/* HE_MU format PPDU-also uses SPATIAL_REUSE_MASK from above */
#define IEEE80211_RADIOTAP_HE_STA_ID_MASK			0x7FF0
#define IEEE80211_RADIOTAP_HE_RESERVED_D4_B15			0x8000

#define IEEE80211_RADIOTAP_HE_DATA_BANDWIDTH_RU_ALLOC_MASK	0x000F
#define IEEE80211_RADIOTAP_HE_GI_MASK				0x0030
#define IEEE80211_RADIOTAP_HE_GI_0_POINT_8_MICRO		0
#define IEEE80211_RADIOTAP_HE_GI_1_POINT_6_MICRO		1
#define IEEE80211_RADIOTAP_HE_GI_3_POINT_2_MICRO		2
#define IEEE80211_RADIOTAP_HE_GI_RESERVED			3
#define IEEE80211_RADIOTAP_HE_LTF_SYMBOL_SIZE			0x00C0
#define IEEE80211_RADIOTAP_HE_NUM_LTF_SYMBOLS_MASK		0x0700
#define IEEE80211_RADIOTAP_HE_RESERVED_D5_B11			0x0800
#define IEEE80211_RADIOTAP_HE_PRE_FEC_PADDING_FACTOR_MASK	0x3000
#define IEEE80211_RADIOTAP_HE_TXBF				0x4000
#define IEEE80211_RADIOTAP_HE_PE_DISAMBIGUITY			0x8000
#define IEEE80211_RADIOTAP_HE_NSTS_MASK				0x000F
#define IEEE80211_RADIOTAP_HE_DOPLER_VALUE			0x0010
#define IEEE80211_RADIOTAP_HE_RESERVED_D6_00E0			0x00E0
#define IEEE80211_RADIOTAP_HE_TXOP_VALUE_MASK			0x7F00
#define IEEE80211_RADIOTAP_HE_MIDAMBLE_PERIODICITY		0x8000

/*
 * HE SU OFDM MCS rate table converted from http://mcsindex.com/
 * indexed by (NSTS,MCS,BW,GI)
 */
#define HE_MAX_NSTS 8
#define HE_MAX_MCS  12
#define HE_SU_MAX_BW   4
#define HE_MAX_GI   3
static float he_ofdm_tab[HE_MAX_NSTS][HE_MAX_MCS][HE_SU_MAX_BW][HE_MAX_GI] = {
  {
      {{     8.6f,     8.1f,    7.3f},{     17.2f,    16.3f,    14.6f},{     36.0f,    34.0f,    30.6f},{     72.1f,    68.1f,    61.3f}},
      {{    17.2f,    16.3f,   14.6f},{     34.4f,    32.5f,    29.3f},{     72.1f,    68.1f,    61.3f},{    144.1f,   136.1f,   122.5f}},
      {{    25.8f,    24.4f,   21.9f},{     51.6f,    48.8f,    43.9f},{    108.1f,   102.1f,    91.9f},{    216.2f,   204.2f,   183.8f}},
      {{    34.4f,    32.5f,   29.3f},{     68.8f,    65.0f,    58.5f},{    144.1f,   136.1f,   122.5f},{    288.2f,   272.2f,   245.0f}},
      {{    51.6f,    48.8f,   43.9f},{    103.2f,    97.5f,    87.8f},{    216.2f,   204.2f,   183.8f},{    432.4f,   408.3f,   367.5f}},
      {{    68.8f,    65.0f,   58.5f},{    137.6f,   130.0f,   117.0f},{    288.2f,   272.2f,   245.0f},{    576.5f,   544.4f,   490.0f}},
      {{    77.4f,    73.1f,   65.8f},{    154.9f,   146.3f,   131.6f},{    324.3f,   306.3f,   275.6f},{    648.5f,   612.5f,   551.3f}},
      {{    86.0f,    81.3f,   73.1f},{    172.1f,   162.5f,   146.3f},{    360.3f,   340.3f,   306.3f},{    720.6f,   680.6f,   612.5f}},
      {{   103.2f,    97.5f,   87.8f},{    206.5f,   195.0f,   175.5f},{    432.4f,   408.3f,   367.5f},{    864.7f,   816.7f,   735.0f}},
      {{   114.7f,   108.3f,   97.5f},{    229.4f,   216.7f,   195.0f},{    480.4f,   453.7f,   408.3f},{    960.8f,   907.4f,   816.7f}},
      {{   129.0f,   121.9f,  109.7f},{    258.1f,   243.8f,   219.4f},{    540.4f,   510.4f,   459.4f},{   1080.9f,  1020.8f,   918.8f}},
      {{   143.4f,   135.4f,  121.9f},{    286.8f,   270.8f,   243.8f},{    600.5f,   567.1f,   510.4f},{   1201.0f,  1134.3f,  1020.8f}}
  },{
      {{    17.2f,    16.3f,   14.6f},{     34.4f,    32.5f,    29.3f},{     72.1f,    68.1f,    61.3f},{    144.1f,   136.1f,   122.5f}},
      {{    34.4f,    32.5f,   29.3f},{     68.8f,    65.0f,    58.5f},{    144.1f,   136.1f,   122.5f},{    288.2f,   272.2f,   245.0f}},
      {{    51.6f,    48.8f,   43.9f},{    103.2f,    97.5f,    87.8f},{    216.2f,   204.2f,   183.8f},{    432.4f,   408.3f,   367.5f}},
      {{    68.8f,    65.0f,   58.5f},{    137.6f,   130.0f,   117.0f},{    288.2f,   272.2f,   245.0f},{    576.5f,   544.4f,   490.0f}},
      {{   103.2f,    97.5f,   87.8f},{    206.5f,   195.0f,   175.5f},{    432.4f,   408.3f,   367.5f},{    864.7f,   816.7f,   735.0f}},
      {{   137.6f,   130.0f,  117.0f},{    275.3f,   260.0f,   234.0f},{    576.5f,   544.4f,   490.0f},{   1152.9f,  1088.9f,   980.0f}},
      {{   154.9f,   146.3f,  131.6f},{    309.7f,   292.5f,   263.3f},{    648.5f,   612.5f,   551.3f},{   1297.1f,  1225.0f,  1102.5f}},
      {{   172.1f,   162.5f,  146.3f},{    344.1f,   325.0f,   292.5f},{    720.6f,   680.6f,   612.5f},{   1441.2f,  1361.1f,  1225.0f}},
      {{   206.5f,   195.0f,  175.5f},{    412.9f,   390.0f,   351.0f},{    864.7f,   816.7f,   735.0f},{   1729.4f,  1633.3f,  1470.0f}},
      {{   229.4f,   216.7f,  195.0f},{    458.8f,   433.3f,   390.0f},{    960.8f,   907.4f,   816.7f},{   1921.6f,  1814.8f,  1633.3f}},
      {{   258.1f,   243.8f,  219.4f},{    516.2f,   487.5f,   438.8f},{   1080.9f,  1020.8f,   918.8f},{   2161.8f,  2041.7f,  1837.5f}},
      {{   286.8f,   270.8f,  243.8f},{    573.5f,   541.7f,   487.5f},{   1201.0f,  1134.3f,  1020.8f},{   2402.0f,  2268.5f,  2041.7f}}
  },{
      {{    25.8f,    24.4f,   21.9f},{     51.6f,    48.8f,    43.9f},{    108.1f,   102.1f,    91.9f},{    216.2f,   204.2f,   183.8f}},
      {{    51.6f,    48.8f,   43.9f},{    103.2f,    97.5f,    87.8f},{    216.2f,   204.2f,   183.8f},{    432.4f,   408.3f,   367.5f}},
      {{    77.4f,    73.1f,   65.8f},{    154.9f,   146.3f,   131.6f},{    324.3f,   306.3f,   275.6f},{    648.5f,   612.5f,   551.3f}},
      {{   103.2f,    97.5f,   87.8f},{    206.5f,   195.0f,   175.5f},{    432.4f,   408.3f,   367.5f},{    864.7f,   816.7f,   735.0f}},
      {{   154.9f,   146.3f,  131.6f},{    309.7f,   292.5f,   263.3f},{    648.5f,   612.5f,   551.3f},{   1297.1f,  1225.0f,  1102.5f}},
      {{   206.5f,   195.0f,  175.5f},{    412.9f,   390.0f,   351.0f},{    864.7f,   816.7f,   735.0f},{   1729.4f,  1633.3f,  1470.0f}},
      {{   232.3f,   219.4f,  197.4f},{    464.6f,   438.8f,   394.9f},{    972.8f,   918.8f,   826.9f},{   1945.6f,  1837.5f,  1653.8f}},
      {{   258.1f,   243.8f,  219.4f},{    516.2f,   487.5f,   438.8f},{   1080.9f,  1020.8f,   918.8f},{   2161.8f,  2041.7f,  1837.5f}},
      {{   309.7f,   292.5f,  263.3f},{    619.4f,   585.0f,   526.5f},{   1297.1f,  1225.0f,  1102.5f},{   2594.1f,  2450.0f,  2205.0f}},
      {{   344.1f,   325.0f,  292.5f},{    688.2f,   650.0f,   585.0f},{   1441.2f,  1361.1f,  1225.0f},{   2882.4f,  2722.2f,  2450.0f}},
      {{   387.1f,   365.6f,  329.1f},{    774.3f,   731.3f,   658.1f},{   1621.3f,  1531.3f,  1378.1f},{   3242.6f,  3062.5f,  2756.3f}},
      {{   430.1f,   406.3f,  365.6f},{    860.3f,   812.5f,   731.3f},{   1801.5f,  1701.4f,  1531.3f},{   3602.9f,  3402.8f,  3062.5f}}
  },{
      {{    34.4f,    32.5f,   29.3f},{     68.8f,    65.0f,    58.5f},{    144.1f,   136.1f,   122.5f},{    288.2f,   272.2f,   245.0f}},
      {{    68.8f,    65.0f,   58.5f},{    137.6f,   130.0f,   117.0f},{    288.2f,   272.2f,   245.0f},{    576.5f,   544.4f,   490.0f}},
      {{   103.2f,    97.5f,   87.8f},{    206.5f,   195.0f,   175.5f},{    432.4f,   408.3f,   367.5f},{    864.7f,   816.7f,   735.0f}},
      {{   137.6f,   130.0f,  117.0f},{    275.3f,   260.0f,   234.0f},{    576.5f,   544.4f,   490.0f},{   1152.9f,  1088.9f,   980.0f}},
      {{   206.5f,   195.0f,  175.5f},{    412.9f,   390.0f,   351.0f},{    864.7f,   816.7f,   735.0f},{   1729.4f,  1633.3f,  1470.0f}},
      {{   275.3f,   260.0f,  234.0f},{    550.6f,   520.0f,   468.0f},{   1152.9f,  1088.9f,   980.0f},{   2305.9f,  2177.8f,  1960.0f}},
      {{   309.7f,   292.5f,  263.3f},{    619.4f,   585.0f,   526.5f},{   1297.1f,  1225.0f,  1102.5f},{   2594.1f,  2450.0f,  2205.0f}},
      {{   344.1f,   325.0f,  292.5f},{    688.2f,   650.0f,   585.0f},{   1441.2f,  1361.1f,  1225.0f},{   2882.4f,  2722.2f,  2450.0f}},
      {{   412.9f,   390.0f,  351.0f},{    825.9f,   780.0f,   702.0f},{   1729.4f,  1633.3f,  1470.0f},{   3458.8f,  3266.7f,  2940.0f}},
      {{   458.8f,   433.3f,  390.0f},{    917.6f,   866.7f,   780.0f},{   1921.6f,  1814.8f,  1633.3f},{   3843.1f,  3629.6f,  3266.7f}},
      {{   516.2f,   487.5f,  438.8f},{   1032.4f,   975.0f,   877.5f},{   2161.8f,  2041.7f,  1837.5f},{   4323.5f,  4083.3f,  3675.0f}},
      {{   573.5f,   541.7f,  487.5f},{   1147.1f,  1083.3f,   975.0f},{   2402.0f,  2268.5f,  2041.7f},{   4803.9f,  4537.0f,  4083.3f}}
  },{
      {{    43.0f,    40.6f,   36.6f},{     86.0f,    81.3f,    73.1f},{    180.1f,   170.1f,   153.1f},{    360.3f,   340.3f,   306.3f}},
      {{    86.0f,    81.3f,   73.1f},{    172.1f,   162.5f,   146.3f},{    360.3f,   340.3f,   306.3f},{    720.6f,   680.6f,   612.5f}},
      {{   129.0f,   121.9f,  109.7f},{    258.1f,   243.8f,   219.4f},{    540.4f,   510.4f,   459.4f},{   1080.9f,  1020.8f,   918.8f}},
      {{   172.1f,   162.5f,  146.3f},{    344.1f,   325.0f,   292.5f},{    720.6f,   680.6f,   612.5f},{   1441.2f,  1361.1f,  1225.0f}},
      {{   258.1f,   243.8f,  219.4f},{    516.2f,   487.5f,   438.8f},{   1080.9f,  1020.8f,   918.8f},{   2161.8f,  2041.7f,  1837.5f}},
      {{   344.1f,   325.0f,  292.5f},{    688.2f,   650.0f,   585.0f},{   1441.2f,  1361.1f,  1225.0f},{   2882.4f,  2722.2f,  2450.0f}},
      {{   387.1f,   365.6f,  329.1f},{    774.3f,   731.3f,   658.1f},{   1621.3f,  1531.3f,  1378.1f},{   3242.6f,  3062.5f,  2756.3f}},
      {{   430.1f,   406.3f,  365.6f},{    860.3f,   812.5f,   731.3f},{   1801.5f,  1701.4f,  1531.3f},{   3602.9f,  3402.8f,  3062.5f}},
      {{   516.2f,   487.5f,  438.8f},{   1032.4f,   975.0f,   877.5f},{   2161.8f,  2041.7f,  1837.5f},{   4323.5f,  4083.3f,  3675.0f}},
      {{   573.5f,   541.7f,  487.5f},{   1147.1f,  1083.3f,   975.0f},{   2402.0f,  2268.5f,  2041.7f},{   4803.9f,  4537.0f,  4083.3f}},
      {{   645.2f,   609.4f,  548.4f},{   1290.4f,  1218.8f,  1096.9f},{   2702.2f,  2552.1f,  2296.9f},{   5404.4f,  5104.2f,  4593.8f}},
      {{   716.9f,   677.1f,  609.4f},{   1433.8f,  1354.2f,  1218.8f},{   3002.5f,  2835.6f,  2552.1f},{   6004.9f,  5671.3f,  5104.2f}}
  },{
      {{    51.6f,    48.8f,   43.9f},{    103.2f,    97.5f,    87.8f},{    216.2f,   204.2f,   183.8f},{    432.4f,   408.3f,   367.5f}},
      {{   103.2f,    97.5f,   87.8f},{    206.5f,   195.0f,   175.5f},{    432.4f,   408.3f,   367.5f},{    864.7f,   816.7f,   735.0f}},
      {{   154.9f,   146.3f,  131.6f},{    309.7f,   292.5f,   263.3f},{    648.5f,   612.5f,   551.3f},{   1297.1f,  1225.0f,  1102.5f}},
      {{   206.5f,   195.0f,  175.5f},{    412.9f,   390.0f,   351.0f},{    864.7f,   816.7f,   735.0f},{   1729.4f,  1633.3f,  1470.0f}},
      {{   309.7f,   292.5f,  263.3f},{    619.4f,   585.0f,   526.5f},{   1297.1f,  1225.0f,  1102.5f},{   2594.1f,  2450.0f,  2205.0f}},
      {{   412.9f,   390.0f,  351.0f},{    825.9f,   780.0f,   702.0f},{   1729.4f,  1633.3f,  1470.0f},{   3458.8f,  3266.7f,  2940.0f}},
      {{   464.6f,   438.8f,  394.9f},{    929.1f,   877.5f,   789.8f},{   1945.6f,  1837.5f,  1653.8f},{   3891.2f,  3675.0f,  3307.5f}},
      {{   516.2f,   487.5f,  438.8f},{   1032.4f,   975.0f,   877.5f},{   2161.8f,  2041.7f,  1837.5f},{   4323.5f,  4083.3f,  3675.0f}},
      {{   619.4f,   585.0f,  526.5f},{   1238.8f,  1170.0f,  1053.0f},{   2594.1f,  2450.0f,  2205.0f},{   5188.2f,  4900.0f,  4410.0f}},
      {{   688.2f,   650.0f,  585.0f},{   1376.5f,  1300.0f,  1170.0f},{   2882.4f,  2722.2f,  2450.0f},{   5764.7f,  5444.4f,  4900.0f}},
      {{   774.3f,   731.3f,  658.1f},{   1548.5f,  1462.5f,  1316.3f},{   3242.6f,  3062.5f,  2756.3f},{   6485.3f,  6125.0f,  5512.5f}},
      {{   860.3f,   812.5f,  731.3f},{   1720.6f,  1625.0f,  1462.5f},{   3602.9f,  3402.8f,  3062.5f},{   7205.9f,  6805.6f,  6125.0f}}
  },{
      {{    60.2f,    56.9f,   51.2f},{    120.4f,   113.8f,   102.4f},{    252.2f,   238.2f,   214.4f},{    504.4f,   476.4f,   428.8f}},
      {{   120.4f,   113.8f,  102.4f},{    240.9f,   227.5f,   204.8f},{    504.4f,   476.4f,   428.8f},{   1008.8f,   952.8f,   857.5f}},
      {{   180.7f,   170.6f,  153.6f},{    361.3f,   341.3f,   307.1f},{    756.6f,   714.6f,   643.1f},{   1513.2f,  1429.2f,  1286.3f}},
      {{   240.9f,   227.5f,  204.8f},{    481.8f,   455.0f,   409.5f},{   1008.8f,   952.8f,   857.5f},{   2017.6f,  1905.6f,  1715.0f}},
      {{   361.3f,   341.3f,  307.1f},{    722.6f,   682.5f,   614.3f},{   1513.2f,  1429.2f,  1286.3f},{   3026.5f,  2858.3f,  2572.5f}},
      {{   481.8f,   455.0f,  409.5f},{    963.5f,   910.0f,   819.0f},{   2017.6f,  1905.6f,  1715.0f},{   4035.3f,  3811.1f,  3430.0f}},
      {{   542.0f,   511.9f,  460.7f},{   1084.0f,  1023.8f,   921.4f},{   2269.9f,  2143.8f,  1929.4f},{   4539.7f,  4287.5f,  3858.8f}},
      {{   602.2f,   568.8f,  511.9f},{   1204.4f,  1137.5f,  1023.8f},{   2522.1f,  2381.9f,  2143.8f},{   5044.1f,  4763.9f,  4287.5f}},
      {{   722.6f,   682.5f,  614.3f},{   1445.3f,  1365.0f,  1228.5f},{   3026.5f,  2858.3f,  2572.5f},{   6052.9f,  5716.7f,  5145.0f}},
      {{   802.9f,   758.3f,  682.5f},{   1605.9f,  1516.7f,  1365.0f},{   3362.7f,  3175.9f,  2858.3f},{   6725.5f,  6351.9f,  5716.7f}},
      {{   903.3f,   853.1f,  767.8f},{   1806.6f,  1706.3f,  1535.6f},{   3783.1f,  3572.9f,  3215.6f},{   7566.2f,  7145.8f,  6431.3f}},
      {{  1003.7f,   947.9f,  853.1f},{   2007.4f,  1895.8f,  1706.3f},{   4203.4f,  3969.9f,  3572.9f},{   8406.9f,  7939.8f,  7145.8f}}
  },{
      {{    68.8f,    65.0f,   58.5f},{    137.6f,   130.0f,   117.0f},{    288.2f,   272.2f,   245.0f},{    576.5f,   544.4f,   490.0f}},
      {{   137.6f,   130.0f,  117.0f},{    275.3f,   260.0f,   234.0f},{    576.5f,   544.4f,   490.0f},{   1152.9f,  1088.9f,   980.0f}},
      {{   206.5f,   195.0f,  175.5f},{    412.9f,   390.0f,   351.0f},{    864.7f,   816.7f,   735.0f},{   1729.4f,  1633.3f,  1470.0f}},
      {{   275.3f,   260.0f,  234.0f},{    550.6f,   520.0f,   468.0f},{   1152.9f,  1088.9f,   980.0f},{   2305.9f,  2177.8f,  1960.0f}},
      {{   412.9f,   390.0f,  351.0f},{    825.9f,   780.0f,   702.0f},{   1729.4f,  1633.3f,  1470.0f},{   3458.8f,  3266.7f,  2940.0f}},
      {{   550.6f,   520.0f,  468.0f},{   1101.2f,  1040.0f,   936.0f},{   2305.9f,  2177.8f,  1960.0f},{   4611.8f,  4355.6f,  3920.0f}},
      {{   619.4f,   585.0f,  526.5f},{   1238.8f,  1170.0f,  1053.0f},{   2594.1f,  2450.0f,  2205.0f},{   5188.2f,  4900.0f,  4410.0f}},
      {{   688.2f,   650.0f,  585.0f},{   1376.5f,  1300.0f,  1170.0f},{   2882.4f,  2722.2f,  2450.0f},{   5764.7f,  5444.4f,  4900.0f}},
      {{   825.9f,   780.0f,  702.0f},{   1651.8f,  1560.0f,  1404.0f},{   3458.8f,  3266.7f,  2940.0f},{   6917.6f,  6533.3f,  5880.0f}},
      {{   917.6f,   866.7f,  780.0f},{   1835.3f,  1733.3f,  1560.0f},{   3843.1f,  3629.6f,  3266.7f},{   7686.3f,  7259.3f,  6533.3f}},
      {{  1032.4f,   975.0f,  877.5f},{   2064.7f,  1950.0f,  1755.0f},{   4323.5f,  4083.3f,  3675.0f},{   8647.1f,  8166.7f,  7350.0f}},
      {{  1147.1f,  1083.3f,  975.0f},{   2294.1f,  2166.7f,  1950.0f},{   4803.9f,  4537.0f,  4083.3f},{   9607.8f,  9074.1f,  8166.7f}}
  }
};

static const char *auth_alg_text[]={"Open System","Shared Key","EAP"};
#define NUM_AUTH_ALGS	(sizeof(auth_alg_text) / sizeof(auth_alg_text[0]))

static const char *status_text[] = {
	"Successful",						/*  0 */
	"Unspecified failure",					/*  1 */
	"TDLS wakeup schedule rejected but alternative schedule "
	  "provided",					/*  2 */
	"TDLS wakeup schedule rejected",/*  3 */
	"Reserved",						/*  4 */
	"Security disabled",			/*  5 */
	"Unacceptable lifetime",		/*  6 */
	"Not in same BSS",				/*  7 */
	"Reserved",						/*  8 */
	"Reserved",						/*  9 */
	"Cannot Support all requested capabilities in the Capability "
	  "Information field",					/* 10 */
	"Reassociation denied due to inability to confirm that association "
	  "exists",						/* 11 */
	"Association denied due to reason outside the scope of this "
	  "standard",						/* 12 */
	"Responding STA does not support the specified authentication "
	  "algorithm",						/* 13 */
	"Received an Authentication frame with authentication transaction "
	  "sequence number out of expected sequence",		/* 14 */
	"Authentication rejected because of challenge failure",	/* 15 */
	"Authentication rejected due to timeout waiting for next frame in "
	  "sequence",						/* 16 */
	"Association denied because AP is unable to handle "
	  "additional associated STAs",				/* 17 */
	"Association denied due to requesting STA not supporting "
	  "all of the data rates in the BSSBasicRateSet parameter, "
	  "the Basic HT-MCS Set field of the HT Operation "
	  "parameter, or the Basic VHT-MCS and NSS Set field in "
	  "the VHT Operation parameter",	/* 18 */
	"Association denied due to requesting STA not supporting "
	  "the short preamble option",				/* 19 */
	"Reserved",					/* 20 */
	"Reserved",					/* 21 */
	"Association request rejected because Spectrum Management "
	  "capability is required",				/* 22 */
	"Association request rejected because the information in the "
	  "Power Capability element is unacceptable",		/* 23 */
	"Association request rejected because the information in the "
	  "Supported Channels element is unacceptable",		/* 24 */
	"Association denied due to requesting STA not supporting "
	  "the Short Slot Time option",				/* 25 */
	"Reserved",				/* 26 */
	"Association denied because the requested STA does not support HT "
	  "features",						/* 27 */
	"R0KH unreachable",					/* 28 */
	"Association denied because the requesting STA does not "
	  "support the phased coexistence operation (PCO) "
	  "transition time required by the AP",		/* 29 */
	"Association request rejected temporarily; try again "
	  "later",							/* 30 */
	"Robust management frame policy violation",	/* 31 */
	"Unspecified, QoS-related failure",			/* 32 */
	"Association denied because QoS AP or PCP has "
	  "insufficient bandwidth to handle another QoS "
	  "STA",									/* 33 */
	"Association denied due to excessive frame loss rates and/or "
	  "poor conditions on current operating channel",	/* 34 */
	"Association (with QoS BSS) denied because the requesting STA "
	  "does not support the QoS facility",			/* 35 */
	"Reserved",									/* 36 */
	"The request has been declined",			/* 37 */
	"The request has not been successful as one or more parameters "
	  "have invalid values",				/* 38 */
	"The allocation or TS has not been created because the request "
	  "cannot be honored; however, a suggested TSPEC/DMG TSPEC is "
	  "provided so that the initiating STA can attempt to set "
	  "another allocation or TS with the suggested changes to the "
	  "TSPEC/DMG TSPEC",					/* 39 */
	"Invalid element, i.e., an element defined in this standard "
	  "for which the content does not meet the specifications in "
	  "Clause 9",								/* 40 */
	"Invalid group cipher",						/* 41 */
	"Invalid pairwise cipher",					/* 42 */
	"Invalid AKMP",								/* 43 */
	"Unsupported RSNE version",					/* 44 */
	"Invalid RSNE capabilities",				/* 45 */
	"Cipher suite rejected because of security policy",		/* 46 */
	"The TS or allocation has not been created; however, the "
	  "HC or PCP might be capable of creating a TS or "
	  "allocation, in response to a request, after the time "
	  "indicated in the TS Delay element",		/* 47 */
	"Direct Link is not allowed in the BSS by policy",	/* 48 */
	"The Destination STA is not present within this BSS",	/* 49 */
	"The Destination STA is not a QoS STA",		/* 50 */

	"Association denied because the listen interval is "
	  "too large",								/* 51 */
	"Invalid FT Action frame count",			/* 52 */
	"Invalid pairwise master key identifier (PMKID)", /* 53 */
	"Invalid MDE",								/* 54 */
	"Invalid FTE",								/* 55 */
	"Requested TCLAS processing is not supported by the AP "
	  "or PCP",									/* 56 */
	"The AP or PCP has insufficient TCLAS processing "
	  "resources to satisfy the request",		/* 57 */
	"The TS has not been created because the request "
	  "cannot be honored; however, the HC or PCP suggests "
	  "that the STA transition to a different BSS to set up "
	  "the TS",									/* 58 */
	"GAS Advertisement Protocol not supported",	/* 59 */
	"No outstanding GAS request",				/* 60 */
	"GAS Response not received from the Advertisement "
	  "Server",									/* 61 */
	"STA timed out waiting for GAS Query Response", /* 62 */
	"LARGE GAS Response is larger than query response "
	  "length limit",							/* 63 */
	"Request refused because home network does not support "
	  "request",								/* 64 */
	"Advertisement Server in the network is not currently "
	  "reachable",								/* 65 */
	"Reserved",									/* 66 */
	"Request refused due to permissions received via SSPN "
	  "interface",								/* 67 */
	"Request refused because the AP or PCP does not "
	  "support unauthenticated access",			/* 68 */
	"Reserved",									/* 69 */
	"Reserved",									/* 70 */
	"Reserved",									/* 71 */
	"Invalid contents of RSNE",				/* 72 */
	"U-APSD coexistence is not supported",		/* 73 */
	"Requested U-APSD coexistence mode is not supported", /* 74 */
	"Requested Interval/Duration value cannot be "
	  "supported with U-APSD coexistence",		/* 75 */
	"Authentication is rejected because an Anti-Clogging "
	  "Token is required",						/* 76 */
	"Authentication is rejected because the offered "
	  "finite cyclic group is not supported",	/* 77 */
	"The TBTT adjustment request has not been successful "
	  "because the STA could not find an alternative TBTT", /* 78 */
	"Transmission failure",						/* 79 */
	"Requested TCLAS Not Supported",			/* 80 */
	"TCLAS Resources Exhausted",				/* 81 */
	"Rejected with Suggested BSS transition",	/* 82 */
	"Reject with recommended schedule",			/* 83 */
	"Reject because no wakeup schedule specified", /* 84 */
	"Success, the destination STA is in power save mode", /* 85 */
	"FST pending, in process of admitting FST session", /* 86 */
	"Performing FST now",						/* 87 */
	"FST pending, gap(s) in block ack window",	/* 88 */
	"Reject because of U-PID setting",			/* 89 */
	"Reserved",									/* 90 */
	"Reserved",									/* 91 */
	"(Re)Association refused for some external reason", /* 92 */
	"(Re)Association refused because of memory limits "
	  "at the AP",								/* 93 */
	"(Re)Association refused because emergency services "
	  "are not supported at the AP",			/* 94 */
	"GAS query response not yet received",		/* 95 */
	"Reject since the request is for transition to a "
	  "frequency band subject to DSE procedures and "
	  "FST Initiator is a dependent STA",		/* 96 */
	"Requested TCLAS processing has been terminated by "
	  "the AP",									/* 97 */
	"The TS schedule conflicts with an existing "
	  "schedule; an alternative schedule is provided", /* 98 */
	"The association has been denied; however, one or "
	  "more Multi-band elements are included that can "
	  "be used by the receiving STA to join the BSS", /* 99 */
	"The request failed due to a reservation conflict", /* 100 */
	"The request failed due to exceeded MAF limit", /* 101 */
	"The request failed due to exceeded MCCA track "
	  "limit",									/* 102 */
	"Association denied because the information in the"
	  "Spectrum Management field is unacceptable", /* 103 */
	"Association denied because the requesting STA "
	  "does not support VHT features",			/* 104 */
	"Enablement denied",						/* 105 */
	"Enablement denied due to restriction from an "
	  "authorized GDB",							/* 106 */
	"Authorization deenabled",					/* 107 */
};
#define NUM_STATUSES	(sizeof(status_text) / sizeof(status_text[0]))

static const char *reason_text[] = {
	"Reserved",						/* 0 */
	"Unspecified reason",					/* 1 */
	"Previous authentication no longer valid",		/* 2 */
	"Deauthenticated because sending STA is leaving (or has left) "
	  "IBSS or ESS",					/* 3 */
	"Disassociated due to inactivity",			/* 4 */
	"Disassociated because AP is unable to handle all currently "
	  " associated STAs",				/* 5 */
	"Class 2 frame received from nonauthenticated STA", /* 6 */
	"Class 3 frame received from nonassociated STA",	/* 7 */
	"Disassociated because sending STA is leaving "
	  "(or has left) BSS",					/* 8 */
	"STA requesting (re)association is not authenticated with "
	  "responding STA",					/* 9 */
	"Disassociated because the information in the Power Capability "
	  "element is unacceptable",				/* 10 */
	"Disassociated because the information in the Supported Channels "
	  "element is unacceptable",				/* 11 */
	"Disassociated due to BSS transition management",	/* 12 */
	"Invalid element, i.e., an element defined in this standard for "
	  "which the content does not meet the specifications "
	  "in Clause 9",						/* 13 */
	"Message integrity code (MIC) failure",	/* 14 */
	"4-Way Handshake timeout",				/* 15 */
	"Group key handshake timeout",			/* 16 */
	"Information element in 4-Way Handshake different from (Re)Association"
	  "Request/Probe Response/Beacon frame",	/* 17 */
	"Invalid group cipher",					/* 18 */
	"Invalid pairwise cipher",				/* 19 */
	"Invalid AKMP",							/* 20 */
	"Unsupported RSNE version",				/* 21 */
	"Invalid RSNE capabilities",				/* 22 */
	"IEEE 802.1X authentication failed",			/* 23 */
	"Cipher suite rejected because of the security policy",		/* 24 */
	"TDLS direct-link teardown due to TDLS peer STA "
	  "unreachable via the TDLS direct link",				/* 25 */
	"TDLS direct-link teardown for unspecified reason",		/* 26 */
	"Disassociated because session terminated by SSP request",/* 27 */
	"Disassociated because of lack of SSP roaming agreement",/* 28 */
	"Requested service rejected because of SSP cipher suite or "
	  "AKM requirement",						/* 29 */
	"Requested service not authorized in this location",	/* 30 */
	"TS deleted because QoS AP lacks sufficient bandwidth for this "
	  "QoS STA due to a change in BSS service characteristics or "
	  "operational mode (e.g. an HT BSS change from 40 MHz channel "
	  "to 20 MHz channel)",					/* 31 */
	"Disassociated for unspecified, QoS-related reason",	/* 32 */
	"Disassociated because QoS AP lacks sufficient bandwidth for this "
	  "QoS STA",						/* 33 */
	"Disassociated because of excessive number of frames that need to be "
	  "acknowledged, but are not acknowledged due to AP transmissions "
	  "and/or poor channel conditions",			/* 34 */
	"Disassociated because STA is transmitting outside the limits "
	  "of its TXOPs",					/* 35 */
	"Requested from peer STA as the STA is leaving the BSS "
	  "(or resetting)",					/* 36 */
	"Requested from peer STA as it does not want to use the "
	  "mechanism",						/* 37 */
	"Requested from peer STA as the STA received frames using the "
	  "mechanism for which a set up is required",		/* 38 */
	"Requested from peer STA due to time out",		/* 39 */
	"Reserved",						/* 40 */
	"Reserved",						/* 41 */
	"Reserved",						/* 42 */
	"Reserved",						/* 43 */
	"Reserved",						/* 44 */
	"Peer STA does not support the requested cipher suite",	/* 45 */
	"In a DLS Teardown frame: The teardown was initiated by the "
	  "DLS peer. In a Disassociation frame: Disassociated because "
	  "authorized access limit reached",					/* 46 */
	"In a DLS Teardown frame: The teardown was initiated by the "
	  "AP. In a Disassociation frame: Disassociated due to external "
	  "service requirements",								/* 47 */
	"Invalid FT Action frame count",						/* 48 */
	"Invalid pairwise master key identifier (PMKID)",		/* 49 */
	"Invalid MDE",											/* 50 */
	"Invalid FTE",											/* 51 */
	"Mesh peering canceled for unknown reasons",			/* 52 */
	"The mesh STA has reached the supported maximum number of "
	  "peer mesh STAs",										/* 53 */
	"The received information violates the Mesh Configuration "
	  "policy configured in the mesh STA profile",			/* 54 */
	"The mesh STA has received a Mesh Peering Close frame "
	  "requesting to close the mesh peering",				/* 55 */
	"The mesh STA has resent dot11MeshMaxRetries Mesh "
	  "Peering Open frames, without receiving a Mesh Peering "
	  "Confirm frame",										/* 56 */
	"The confirmTimer for the mesh peering instance times out",	/* 57 */
	"The mesh STA fails to unwrap the GTK or the values in the "
	  "wrapped contents do not match",						/* 58 */
	"The mesh STA receives inconsistent information about the "
	  "mesh parameters between mesh peering Management frames",	/* 59 */
	"The mesh STA fails the authenticated mesh peering exchange "
	  "because due to failure in selecting either the pairwise "
	  "ciphersuite or group ciphersuite",					/* 60 */
	"The mesh STA does not have proxy information for this "
	  "external destination",								/* 61 */
	"The mesh STA does not have forwarding information for this "
	  "destination",										/* 62 */
	"The mesh STA determines that the link to the next hop of an "
	  "active path in its forwarding information is no longer "
	  "usable",												/* 63 */
	"The Deauthentication frame was sent because the MAC "
	  "address of the STA already exists in the mesh BSS",	/* 64 */
	"The mesh STA performs channel switch to meet regulatory "
	  "requirements",										/* 65 */
	"The mesh STA performs channel switching with unspecified "
	  "reason",												/* 66 */
};
#define NUM_REASONS	(sizeof(reason_text) / sizeof(reason_text[0]))

#define SHOW_ORIGIN_OUTPUT 0

static int
wep_print(netdissect_options *ndo,
	  const u_char *p)
{
	uint32_t iv;

	ND_TCHECK_LEN(p, IEEE802_11_IV_LEN + IEEE802_11_KID_LEN);
	iv = GET_LE_U_4(p);

	ND_PRINT(" IV:%3x Pad %x KeyID %x", IV_IV(iv), IV_PAD(iv),
	    IV_KEYID(iv));

	return 1;
trunc:
	return 0;
}

static int
parse_elements(netdissect_options *ndo,
	       struct mgmt_body_t *pbody, const u_char *p, int offset,
	       u_int length)
{
	u_int elementlen;
	struct ssid_t ssid;
	struct challenge_t challenge;
	struct rates_t rates;
	struct ds_t ds;
	struct cf_t cf;
	struct tim_t tim;
	struct meshid_t meshid;

	/*
	 * We haven't seen any elements yet.
	 */
	pbody->challenge_present = 0;
	pbody->ssid_present = 0;
	pbody->rates_present = 0;
	pbody->ds_present = 0;
	pbody->cf_present = 0;
	pbody->tim_present = 0;
	pbody->meshid_present = 0;

	while (length != 0) {
		/* Make sure we at least have the element ID and length. */
		ND_TCHECK_2(p + offset);
		if (length < 2)
			goto trunc;
		elementlen = GET_U_1(p + offset + 1);

		/* Make sure we have the entire element. */
		ND_TCHECK_LEN(p + offset + 2, elementlen);
		if (length < elementlen + 2)
			goto trunc;

		switch (GET_U_1(p + offset)) {
		case E_SSID:
			ssid.length = elementlen;
			offset += 2;
			length -= 2;
			if (ssid.length != 0) {
				if (ssid.length > sizeof(ssid.ssid) - 1)
					return 0;
				memcpy(&ssid.ssid, p + offset, ssid.length);
				offset += ssid.length;
				length -= ssid.length;
			}
			ssid.ssid[ssid.length] = '\0';
			/*
			 * Present and not truncated.
			 *
			 * If we haven't already seen an SSID IE,
			 * copy this one, otherwise ignore this one,
			 * so we later report the first one we saw.
			 */
			if (!pbody->ssid_present) {
				pbody->ssid = ssid;
				pbody->ssid_present = 1;
			}
			break;
		case E_CHALLENGE:
			challenge.length = elementlen;
			offset += 2;
			length -= 2;
			if (challenge.length != 0) {
				if (challenge.length >
				    sizeof(challenge.text) - 1)
					return 0;
				memcpy(&challenge.text, p + offset,
				    challenge.length);
				offset += challenge.length;
				length -= challenge.length;
			}
			challenge.text[challenge.length] = '\0';
			/*
			 * Present and not truncated.
			 *
			 * If we haven't already seen a challenge IE,
			 * copy this one, otherwise ignore this one,
			 * so we later report the first one we saw.
			 */
			if (!pbody->challenge_present) {
				pbody->challenge = challenge;
				pbody->challenge_present = 1;
			}
			break;
		case E_RATES:
			rates.length = elementlen;
			offset += 2;
			length -= 2;
			if (rates.length != 0) {
				if (rates.length > sizeof(rates.rate))
					return 0;
				memcpy(&rates.rate, p + offset, rates.length);
				offset += rates.length;
				length -= rates.length;
			}
			/*
			 * Present and not truncated.
			 *
			 * If we haven't already seen a rates IE,
			 * copy this one if it's not zero-length,
			 * otherwise ignore this one, so we later
			 * report the first one we saw.
			 *
			 * We ignore zero-length rates IEs as some
			 * devices seem to put a zero-length rates
			 * IE, followed by an SSID IE, followed by
			 * a non-zero-length rates IE into frames,
			 * even though IEEE Std 802.11-2007 doesn't
			 * seem to indicate that a zero-length rates
			 * IE is valid.
			 */
			if (!pbody->rates_present && rates.length != 0) {
				pbody->rates = rates;
				pbody->rates_present = 1;
			}
			break;
		case E_DS:
			ds.length = elementlen;
			offset += 2;
			length -= 2;
			if (ds.length != 1) {
				offset += ds.length;
				length -= ds.length;
				break;
			}
			ds.channel = GET_U_1(p + offset);
			offset += 1;
			length -= 1;
			/*
			 * Present and not truncated.
			 *
			 * If we haven't already seen a DS IE,
			 * copy this one, otherwise ignore this one,
			 * so we later report the first one we saw.
			 */
			if (!pbody->ds_present) {
				pbody->ds = ds;
				pbody->ds_present = 1;
			}
			break;
		case E_CF:
			cf.length = elementlen;
			offset += 2;
			length -= 2;
			if (cf.length != 6) {
				offset += cf.length;
				length -= cf.length;
				break;
			}
			cf.count = GET_U_1(p + offset);
			offset += 1;
			length -= 1;
			cf.period = GET_U_1(p + offset);
			offset += 1;
			length -= 1;
			cf.max_duration = GET_LE_U_2(p + offset);
			offset += 2;
			length -= 2;
			cf.dur_remaining = GET_LE_U_2(p + offset);
			offset += 2;
			length -= 2;
			/*
			 * Present and not truncated.
			 *
			 * If we haven't already seen a CF IE,
			 * copy this one, otherwise ignore this one,
			 * so we later report the first one we saw.
			 */
			if (!pbody->cf_present) {
				pbody->cf = cf;
				pbody->cf_present = 1;
			}
			break;
		case E_TIM:
			tim.length = elementlen;
			offset += 2;
			length -= 2;
			if (tim.length <= 3U) {
				offset += tim.length;
				length -= tim.length;
				break;
			}
			if (tim.length - 3U > sizeof(tim.bitmap))
				return 0;
			tim.count = GET_U_1(p + offset);
			offset += 1;
			length -= 1;
			tim.period = GET_U_1(p + offset);
			offset += 1;
			length -= 1;
			tim.bitmap_control = GET_U_1(p + offset);
			offset += 1;
			length -= 1;
			memcpy(tim.bitmap, p + offset, tim.length - 3);
			offset += tim.length - 3;
			length -= tim.length - 3;
			/*
			 * Present and not truncated.
			 *
			 * If we haven't already seen a TIM IE,
			 * copy this one, otherwise ignore this one,
			 * so we later report the first one we saw.
			 */
			if (!pbody->tim_present) {
				pbody->tim = tim;
				pbody->tim_present = 1;
			}
			break;
		case E_MESHID:
			meshid.length = elementlen;
			offset += 2;
			length -= 2;
			if (meshid.length != 0) {
				if (meshid.length > sizeof(meshid.meshid) - 1)
					return 0;
				memcpy(&meshid.meshid, p + offset, meshid.length);
				offset += meshid.length;
				length -= meshid.length;
			}
			meshid.meshid[meshid.length] = '\0';
			/*
			 * Present and not truncated.
			 *
			 * If we haven't already seen a MESHID IE,
			 * copy this one, otherwise ignore this one,
			 * so we later report the first one we saw.
			 */
			if (!pbody->meshid_present) {
				pbody->meshid = meshid;
				pbody->meshid_present = 1;
			}
			break;
		default:
#if 0
			ND_PRINT("(1) unhandled element_id (%u)  ",
			    GET_U_1(p + offset));
#endif
			offset += 2 + elementlen;
			length -= 2 + elementlen;
			break;
		}
	}

	/* No problems found. */
	return 1;
trunc:
	return 0;
}

/*********************************************************************************
 * Print Handle functions for the management frame types
 *********************************************************************************/

static int
handle_beacon(netdissect_options *ndo,
	      const u_char *p, u_int length)
{
	struct mgmt_body_t pbody;
	int offset = 0;
	int ret;

	memset(&pbody, 0, sizeof(pbody));

	ND_TCHECK_LEN(p, IEEE802_11_TSTAMP_LEN + IEEE802_11_BCNINT_LEN +
		      IEEE802_11_CAPINFO_LEN);
	if (length < IEEE802_11_TSTAMP_LEN + IEEE802_11_BCNINT_LEN +
	    IEEE802_11_CAPINFO_LEN)
		goto trunc;
	memcpy(&pbody.timestamp, p, IEEE802_11_TSTAMP_LEN);
	offset += IEEE802_11_TSTAMP_LEN;
	length -= IEEE802_11_TSTAMP_LEN;
	pbody.beacon_interval = GET_LE_U_2(p + offset);
	offset += IEEE802_11_BCNINT_LEN;
	length -= IEEE802_11_BCNINT_LEN;
	pbody.capability_info = GET_LE_U_2(p + offset);
	offset += IEEE802_11_CAPINFO_LEN;
	length -= IEEE802_11_CAPINFO_LEN;

	ret = parse_elements(ndo, &pbody, p, offset, length);

	PRINT_SSID(pbody);
	PRINT_RATES(pbody);
	ND_PRINT(" %s",
	    CAPABILITY_ESS(pbody.capability_info) ? "ESS" : "IBSS");
	PRINT_DS_CHANNEL(pbody);
	PRINT_MESHID(pbody);

	return ret;
trunc:
	return 0;
}

static int
handle_assoc_request(netdissect_options *ndo,
		     const u_char *p, u_int length)
{
	struct mgmt_body_t pbody;
	int offset = 0;
	int ret;

	memset(&pbody, 0, sizeof(pbody));

	ND_TCHECK_LEN(p, IEEE802_11_CAPINFO_LEN + IEEE802_11_LISTENINT_LEN);
	if (length < IEEE802_11_CAPINFO_LEN + IEEE802_11_LISTENINT_LEN)
		goto trunc;
	pbody.capability_info = GET_LE_U_2(p);
	offset += IEEE802_11_CAPINFO_LEN;
	length -= IEEE802_11_CAPINFO_LEN;
	pbody.listen_interval = GET_LE_U_2(p + offset);
	offset += IEEE802_11_LISTENINT_LEN;
	length -= IEEE802_11_LISTENINT_LEN;

	ret = parse_elements(ndo, &pbody, p, offset, length);

	PRINT_SSID(pbody);
	PRINT_RATES(pbody);
	return ret;
trunc:
	return 0;
}

static int
handle_assoc_response(netdissect_options *ndo,
		      const u_char *p, u_int length)
{
	struct mgmt_body_t pbody;
	int offset = 0;
	int ret;

	memset(&pbody, 0, sizeof(pbody));

	ND_TCHECK_LEN(p, IEEE802_11_CAPINFO_LEN + IEEE802_11_STATUS_LEN +
		      IEEE802_11_AID_LEN);
	if (length < IEEE802_11_CAPINFO_LEN + IEEE802_11_STATUS_LEN +
	    IEEE802_11_AID_LEN)
		goto trunc;
	pbody.capability_info = GET_LE_U_2(p);
	offset += IEEE802_11_CAPINFO_LEN;
	length -= IEEE802_11_CAPINFO_LEN;
	pbody.status_code = GET_LE_U_2(p + offset);
	offset += IEEE802_11_STATUS_LEN;
	length -= IEEE802_11_STATUS_LEN;
	pbody.aid = GET_LE_U_2(p + offset);
	offset += IEEE802_11_AID_LEN;
	length -= IEEE802_11_AID_LEN;

	ret = parse_elements(ndo, &pbody, p, offset, length);

	ND_PRINT(" AID(%x) :%s: %s", ((uint16_t)(pbody.aid << 2 )) >> 2 ,
	    CAPABILITY_PRIVACY(pbody.capability_info) ? " PRIVACY " : "",
	    (pbody.status_code < NUM_STATUSES
		? status_text[pbody.status_code]
		: "n/a"));

	return ret;
trunc:
	return 0;
}

static int
handle_reassoc_request(netdissect_options *ndo,
		       const u_char *p, u_int length)
{
	struct mgmt_body_t pbody;
	int offset = 0;
	int ret;

	memset(&pbody, 0, sizeof(pbody));

	ND_TCHECK_LEN(p, IEEE802_11_CAPINFO_LEN + IEEE802_11_LISTENINT_LEN +
		      IEEE802_11_AP_LEN);
	if (length < IEEE802_11_CAPINFO_LEN + IEEE802_11_LISTENINT_LEN +
	    IEEE802_11_AP_LEN)
		goto trunc;
	pbody.capability_info = GET_LE_U_2(p);
	offset += IEEE802_11_CAPINFO_LEN;
	length -= IEEE802_11_CAPINFO_LEN;
	pbody.listen_interval = GET_LE_U_2(p + offset);
	offset += IEEE802_11_LISTENINT_LEN;
	length -= IEEE802_11_LISTENINT_LEN;
	memcpy(&pbody.ap, p+offset, IEEE802_11_AP_LEN);
	offset += IEEE802_11_AP_LEN;
	length -= IEEE802_11_AP_LEN;

	ret = parse_elements(ndo, &pbody, p, offset, length);

	PRINT_SSID(pbody);
	ND_PRINT(" AP : %s", mac48_string(ndo,  pbody.ap ));

	return ret;
trunc:
	return 0;
}

static int
handle_reassoc_response(netdissect_options *ndo,
			const u_char *p, u_int length)
{
	/* Same as a Association Response */
	return handle_assoc_response(ndo, p, length);
}

static int
handle_probe_request(netdissect_options *ndo,
		     const u_char *p, u_int length)
{
	struct mgmt_body_t  pbody;
	int offset = 0;
	int ret;

	memset(&pbody, 0, sizeof(pbody));

	ret = parse_elements(ndo, &pbody, p, offset, length);

	PRINT_SSID(pbody);
	PRINT_RATES(pbody);

	return ret;
}

static int
handle_probe_response(netdissect_options *ndo,
		      const u_char *p, u_int length)
{
	struct mgmt_body_t  pbody;
	int offset = 0;
	int ret;

	memset(&pbody, 0, sizeof(pbody));

	ND_TCHECK_LEN(p, IEEE802_11_TSTAMP_LEN + IEEE802_11_BCNINT_LEN +
		      IEEE802_11_CAPINFO_LEN);
	if (length < IEEE802_11_TSTAMP_LEN + IEEE802_11_BCNINT_LEN +
	    IEEE802_11_CAPINFO_LEN)
		goto trunc;
	memcpy(&pbody.timestamp, p, IEEE802_11_TSTAMP_LEN);
	offset += IEEE802_11_TSTAMP_LEN;
	length -= IEEE802_11_TSTAMP_LEN;
	pbody.beacon_interval = GET_LE_U_2(p + offset);
	offset += IEEE802_11_BCNINT_LEN;
	length -= IEEE802_11_BCNINT_LEN;
	pbody.capability_info = GET_LE_U_2(p + offset);
	offset += IEEE802_11_CAPINFO_LEN;
	length -= IEEE802_11_CAPINFO_LEN;

	ret = parse_elements(ndo, &pbody, p, offset, length);

	PRINT_SSID(pbody);
	PRINT_RATES(pbody);
	PRINT_DS_CHANNEL(pbody);
	PRINT_MESHID(pbody);

	return ret;
trunc:
	return 0;
}

static int
handle_atim(void)
{
	/* the frame body for ATIM is null. */
	return 1;
}

static int
handle_disassoc(netdissect_options *ndo,
		const u_char *p, u_int length)
{
	struct mgmt_body_t  pbody;

	memset(&pbody, 0, sizeof(pbody));

	ND_TCHECK_LEN(p, IEEE802_11_REASON_LEN);
	if (length < IEEE802_11_REASON_LEN)
		goto trunc;
	pbody.reason_code = GET_LE_U_2(p);

	ND_PRINT(": %s",
	    (pbody.reason_code < NUM_REASONS)
		? reason_text[pbody.reason_code]
		: "Reserved");

	return 1;
trunc:
	return 0;
}

static int
handle_auth(netdissect_options *ndo,
	    const u_char *p, u_int length)
{
	struct mgmt_body_t  pbody;
	int offset = 0;
	int ret;

	memset(&pbody, 0, sizeof(pbody));

	ND_TCHECK_6(p);
	if (length < 6)
		goto trunc;
	pbody.auth_alg = GET_LE_U_2(p);
	offset += 2;
	length -= 2;
	pbody.auth_trans_seq_num = GET_LE_U_2(p + offset);
	offset += 2;
	length -= 2;
	pbody.status_code = GET_LE_U_2(p + offset);
	offset += 2;
	length -= 2;

	ret = parse_elements(ndo, &pbody, p, offset, length);

	if ((pbody.auth_alg == 1) &&
	    ((pbody.auth_trans_seq_num == 2) ||
	     (pbody.auth_trans_seq_num == 3))) {
		ND_PRINT(" (%s)-%x [Challenge Text] %s",
		    (pbody.auth_alg < NUM_AUTH_ALGS)
			? auth_alg_text[pbody.auth_alg]
			: "Reserved",
		    pbody.auth_trans_seq_num,
		    ((pbody.auth_trans_seq_num % 2)
			? ((pbody.status_code < NUM_STATUSES)
			       ? status_text[pbody.status_code]
			       : "n/a") : ""));
		return ret;
	}
	ND_PRINT(" (%s)-%x: %s",
	    (pbody.auth_alg < NUM_AUTH_ALGS)
		? auth_alg_text[pbody.auth_alg]
		: "Reserved",
	    pbody.auth_trans_seq_num,
	    (pbody.auth_trans_seq_num % 2)
		? ((pbody.status_code < NUM_STATUSES)
		    ? status_text[pbody.status_code]
		    : "n/a")
		: "");

	return ret;
trunc:
	return 0;
}

static int
handle_deauth(netdissect_options *ndo,
	      const uint8_t *src, const u_char *p, u_int length)
{
	struct mgmt_body_t  pbody;
	const char *reason = NULL;

	memset(&pbody, 0, sizeof(pbody));

	ND_TCHECK_LEN(p, IEEE802_11_REASON_LEN);
	if (length < IEEE802_11_REASON_LEN)
		goto trunc;
	pbody.reason_code = GET_LE_U_2(p);

	reason = (pbody.reason_code < NUM_REASONS)
			? reason_text[pbody.reason_code]
			: "Reserved";

	if (ndo->ndo_eflag) {
		ND_PRINT(": %s", reason);
	} else {
		ND_PRINT(" (%s): %s", GET_MAC48_STRING(src), reason);
	}
	return 1;
trunc:
	return 0;
}

static const struct tok category_str[] = {
	{ 0,   "Spectrum Management" },
	{ 1,   "QoS"                 },
	{ 2,   "DLS"                 },
	{ 3,   "BA"                  },
	{ 7,   "HT"                  },
	{ 13,  "MeshAction"          },
	{ 14,  "MultiohopAction"     },
	{ 15,  "SelfprotectAction"   },
	{ 127, "Vendor"              },
	{ 0, NULL }
};

static const struct tok act_ba_str[] = {
	{ 0, "ADDBA Request"  },
	{ 1, "ADDBA Response" },
	{ 2, "DELBA"          },
	{ 0, NULL }
};

static const struct tok act_ht_str[] = {
	{ 0, "TxChWidth"   },
	{ 1, "MIMOPwrSave" },
	{ 0, NULL }
};

static const struct tok act_mesh_str[] = {
	{ 0,  "MeshLink"                   },
	{ 1,  "HWMP"                       },
	{ 2,  "Gate Announcement"          },
	{ 3,  "Congestion Control"         },
	{ 4,  "MCCA Setup Request"         },
	{ 5,  "MCCA Setup Reply"           },
	{ 6,  "MCCA Advertisement Request" },
	{ 7,  "MCCA Advertisement"         },
	{ 8,  "MCCA Teardown"              },
	{ 9,  "TBTT Adjustment Request"    },
	{ 10, "TBTT Adjustment Response"   },
	{ 0, NULL }
};

static const struct tok act_mhop_str[] = {
	{ 0, "Proxy Update" },
	{ 1, "Proxy Update Confirmation" },
	{ 0, NULL }
};

static const struct tok act_selfpr_str[] = {
	{ 1, "Peering Open"          },
	{ 2, "Peering Confirm"       },
	{ 3, "Peering Close"         },
	{ 4, "Group Key Inform"      },
	{ 5, "Group Key Acknowledge" },
	{ 0, NULL }
};

static const struct uint_tokary category2tokary[] = {
	{ 3,   act_ba_str     },
	{ 7,   act_ht_str     },
	{ 13,  act_mesh_str   },
	{ 14,  act_mhop_str   },
	{ 15,  act_selfpr_str },
	/* uint2tokary() does not use array termination. */
};

static int
handle_action(netdissect_options *ndo,
	      const uint8_t *src, const u_char *p, u_int length)
{
	uint8_t category, action;
	const struct tok *action_str;

	ND_TCHECK_2(p);
	if (length < 2)
		goto trunc;
	if (ndo->ndo_eflag) {
		ND_PRINT(": ");
	} else {
		ND_PRINT(" (%s): ", GET_MAC48_STRING(src));
	}
	category = GET_U_1(p);
	ND_PRINT("%s ", tok2str(category_str, "Reserved(%u)", category));
	action = GET_U_1(p + 1);
	action_str = uint2tokary(category2tokary, category);
	if (!action_str)
		ND_PRINT("Act#%u", action);
	else
		ND_PRINT("%s", tok2str(action_str, "Act#%u", action));

	return 1;
trunc:
	return 0;
}

struct cs_hdr {
    unsigned char type;
	unsigned char channel;
};

static int
handle_reserved(netdissect_options *ndo, const u_char *p, u_int length)
{
	char buf[40];
	struct cs_hdr* cshdr = (struct cshdr*)(p);
	ND_PRINT(" Get Reserved Frame, CS msg type => %02x", cshdr->type);
	
	int tp = cshdr->type;
	switch (tp)
    {
    case 0:
    /* CS Request */
        sprintf(buf, "cd ~/cs; echo '%s' >> log", "CS Request");
        break;
    case 1:
    /* CS Reply */
        sprintf(buf, "cd ~/cs; echo '%s' >> log", "CS Reply");
        break;
    case 2:
    /* CS Announcement */
        sprintf(buf, "cd ~/cs; echo '%s' >> log", "CS Announcement");
        break;
    case 16:
    /* Scan Result Request */
        sprintf(buf, "cd ~/cs; echo '%s' >> log", "Scan Result Request");
        break;
    case 17:
    /* Scan Result Reply */
        sprintf(buf, "cd ~/cs; echo '%s' >> log", "Scan Result Reply");
        break;
    case 18:
    /* Scan Announcement */
        sprintf(buf, "cd ~/cs; echo '%s' >> log", "Scan Announcement");
        break;
    default:
        sprintf(buf, "cd ~/cs; echo '%s' >> log", "Unknown Action");
        break;
    }
	system(buf);

	if (cshdr->type == 0x02)
	{
		sprintf(buf, "cd ~/cs; ./cs.sh %d", cshdr->channel);
		system(buf);
	}

	return 1;
}

/*********************************************************************************
 * Print Body funcs
 *********************************************************************************/


static int
mgmt_body_print(netdissect_options *ndo,
		uint16_t fc, const uint8_t *src, const u_char *p, u_int length)
{
	ND_PRINT("%s", tok2str(st_str, "Unhandled Management subtype(%x)", FC_SUBTYPE(fc)));

	/* There may be a problem w/ AP not having this bit set */
	if (FC_PROTECTED(fc))
		return wep_print(ndo, p);
	switch (FC_SUBTYPE(fc)) {
	case ST_ASSOC_REQUEST:
		return handle_assoc_request(ndo, p, length);
	case ST_ASSOC_RESPONSE:
		return handle_assoc_response(ndo, p, length);
	case ST_REASSOC_REQUEST:
		return handle_reassoc_request(ndo, p, length);
	case ST_REASSOC_RESPONSE:
		return handle_reassoc_response(ndo, p, length);
	case ST_PROBE_REQUEST:
		return handle_probe_request(ndo, p, length);
	case ST_PROBE_RESPONSE:
		return handle_probe_response(ndo, p, length);
	case ST_RESERVED:
		return handle_reserved(ndo, p, length);
	case ST_BEACON:
		return handle_beacon(ndo, p, length);
	case ST_ATIM:
		return handle_atim();
	case ST_DISASSOC:
		return handle_disassoc(ndo, p, length);
	case ST_AUTH:
		return handle_auth(ndo, p, length);
	case ST_DEAUTH:
		return handle_deauth(ndo, src, p, length);
	case ST_ACTION:
		return handle_action(ndo, src, p, length);
	default:
		return 1;
	}
}


/*********************************************************************************
 * Handles printing all the control frame types
 *********************************************************************************/

static int
ctrl_body_print(netdissect_options *ndo,
		uint16_t fc, const u_char *p)
{
	ND_PRINT("%s", tok2str(ctrl_str, "Unknown Ctrl Subtype", FC_SUBTYPE(fc)));
	switch (FC_SUBTYPE(fc)) {
	case CTRL_CONTROL_WRAPPER:
		/* XXX - requires special handling */
		break;
	case CTRL_BAR:
		ND_TCHECK_LEN(p, CTRL_BAR_HDRLEN);
		if (!ndo->ndo_eflag)
			ND_PRINT(" RA:%s TA:%s CTL(%x) SEQ(%u) ",
			    GET_MAC48_STRING(((const struct ctrl_bar_hdr_t *)p)->ra),
			    GET_MAC48_STRING(((const struct ctrl_bar_hdr_t *)p)->ta),
			    GET_LE_U_2(((const struct ctrl_bar_hdr_t *)p)->ctl),
			    GET_LE_U_2(((const struct ctrl_bar_hdr_t *)p)->seq));
		break;
	case CTRL_BA:
		ND_TCHECK_LEN(p, CTRL_BA_HDRLEN);
		if (!ndo->ndo_eflag)
			ND_PRINT(" RA:%s ",
			    GET_MAC48_STRING(((const struct ctrl_ba_hdr_t *)p)->ra));
		break;
	case CTRL_PS_POLL:
		ND_TCHECK_LEN(p, CTRL_PS_POLL_HDRLEN);
		ND_PRINT(" AID(%x)",
		    GET_LE_U_2(((const struct ctrl_ps_poll_hdr_t *)p)->aid));
		break;
	case CTRL_RTS:
		ND_TCHECK_LEN(p, CTRL_RTS_HDRLEN);
		if (!ndo->ndo_eflag)
			ND_PRINT(" TA:%s ",
			    GET_MAC48_STRING(((const struct ctrl_rts_hdr_t *)p)->ta));
		break;
	case CTRL_CTS:
		ND_TCHECK_LEN(p, CTRL_CTS_HDRLEN);
		if (!ndo->ndo_eflag)
			ND_PRINT(" RA:%s ",
			    GET_MAC48_STRING(((const struct ctrl_cts_hdr_t *)p)->ra));
		break;
	case CTRL_ACK:
		ND_TCHECK_LEN(p, CTRL_ACK_HDRLEN);
		if (!ndo->ndo_eflag)
			ND_PRINT(" RA:%s ",
			    GET_MAC48_STRING(((const struct ctrl_ack_hdr_t *)p)->ra));
		break;
	case CTRL_CF_END:
		ND_TCHECK_LEN(p, CTRL_END_HDRLEN);
		if (!ndo->ndo_eflag)
			ND_PRINT(" RA:%s ",
			    GET_MAC48_STRING(((const struct ctrl_end_hdr_t *)p)->ra));
		break;
	case CTRL_END_ACK:
		ND_TCHECK_LEN(p, CTRL_END_ACK_HDRLEN);
		if (!ndo->ndo_eflag)
			ND_PRINT(" RA:%s ",
			    GET_MAC48_STRING(((const struct ctrl_end_ack_hdr_t *)p)->ra));
		break;
	}
	return 1;
trunc:
	return 0;
}

/*
 *  Data Frame - Address field contents
 *
 *  To Ds  | From DS | Addr 1 | Addr 2 | Addr 3 | Addr 4
 *    0    |  0      |  DA    | SA     | BSSID  | n/a
 *    0    |  1      |  DA    | BSSID  | SA     | n/a
 *    1    |  0      |  BSSID | SA     | DA     | n/a
 *    1    |  1      |  RA    | TA     | DA     | SA
 */

/*
 * Function to get source and destination MAC addresses for a data frame.
 */
static void
get_data_src_dst_mac(uint16_t fc, const u_char *p, const uint8_t **srcp,
		     const uint8_t **dstp)
{
#define ADDR1  (p + 4)
#define ADDR2  (p + 10)
#define ADDR3  (p + 16)
#define ADDR4  (p + 24)

	if (!FC_TO_DS(fc)) {
		if (!FC_FROM_DS(fc)) {
			/* not To DS and not From DS */
			*srcp = ADDR2;
			*dstp = ADDR1;
		} else {
			/* not To DS and From DS */
			*srcp = ADDR3;
			*dstp = ADDR1;
		}
	} else {
		if (!FC_FROM_DS(fc)) {
			/* To DS and not From DS */
			*srcp = ADDR2;
			*dstp = ADDR3;
		} else {
			/* To DS and From DS */
			*srcp = ADDR4;
			*dstp = ADDR3;
		}
	}

#undef ADDR1
#undef ADDR2
#undef ADDR3
#undef ADDR4
}

static void
get_mgmt_src_dst_mac(const u_char *p, const uint8_t **srcp, const uint8_t **dstp)
{
	const struct mgmt_header_t *hp = (const struct mgmt_header_t *) p;

	if (srcp != NULL)
		*srcp = hp->sa;
	if (dstp != NULL)
		*dstp = hp->da;
}

/*
 * Print Header funcs
 */

static void
data_header_print(netdissect_options *ndo, uint16_t fc, const u_char *p)
{
	u_int subtype = FC_SUBTYPE(fc);

	if (DATA_FRAME_IS_CF_ACK(subtype) || DATA_FRAME_IS_CF_POLL(subtype) ||
	    DATA_FRAME_IS_QOS(subtype)) {
		ND_PRINT("CF ");
		if (DATA_FRAME_IS_CF_ACK(subtype)) {
			if (DATA_FRAME_IS_CF_POLL(subtype))
				ND_PRINT("Ack/Poll");
			else
				ND_PRINT("Ack");
		} else {
			if (DATA_FRAME_IS_CF_POLL(subtype))
				ND_PRINT("Poll");
		}
		if (DATA_FRAME_IS_QOS(subtype))
			ND_PRINT("+QoS");
		ND_PRINT(" ");
	}

#define ADDR1  (p + 4)
#define ADDR2  (p + 10)
#define ADDR3  (p + 16)
#define ADDR4  (p + 24)

	if (!FC_TO_DS(fc) && !FC_FROM_DS(fc)) {
		ND_PRINT("DA:%s SA:%s BSSID:%s ",
		    GET_MAC48_STRING(ADDR1), GET_MAC48_STRING(ADDR2),
		    GET_MAC48_STRING(ADDR3));
	} else if (!FC_TO_DS(fc) && FC_FROM_DS(fc)) {
		ND_PRINT("DA:%s BSSID:%s SA:%s ",
		    GET_MAC48_STRING(ADDR1), GET_MAC48_STRING(ADDR2),
		    GET_MAC48_STRING(ADDR3));
	} else if (FC_TO_DS(fc) && !FC_FROM_DS(fc)) {
		ND_PRINT("BSSID:%s SA:%s DA:%s ",
		    GET_MAC48_STRING(ADDR1), GET_MAC48_STRING(ADDR2),
		    GET_MAC48_STRING(ADDR3));
	} else if (FC_TO_DS(fc) && FC_FROM_DS(fc)) {
		ND_PRINT("RA:%s TA:%s DA:%s SA:%s ",
		    GET_MAC48_STRING(ADDR1), GET_MAC48_STRING(ADDR2),
		    GET_MAC48_STRING(ADDR3), GET_MAC48_STRING(ADDR4));
	}

#undef ADDR1
#undef ADDR2
#undef ADDR3
#undef ADDR4
}

static void
mgmt_header_print(netdissect_options *ndo, const u_char *p)
{
	const struct mgmt_header_t *hp = (const struct mgmt_header_t *) p;

	ND_PRINT("BSSID:%s DA:%s SA:%s ",
	    GET_MAC48_STRING((hp)->bssid), GET_MAC48_STRING((hp)->da),
	    GET_MAC48_STRING((hp)->sa));
}

static void
ctrl_header_print(netdissect_options *ndo, uint16_t fc, const u_char *p)
{
	switch (FC_SUBTYPE(fc)) {
	case CTRL_BAR:
		ND_PRINT(" RA:%s TA:%s CTL(%x) SEQ(%u) ",
		    GET_MAC48_STRING(((const struct ctrl_bar_hdr_t *)p)->ra),
		    GET_MAC48_STRING(((const struct ctrl_bar_hdr_t *)p)->ta),
		    GET_LE_U_2(((const struct ctrl_bar_hdr_t *)p)->ctl),
		    GET_LE_U_2(((const struct ctrl_bar_hdr_t *)p)->seq));
		break;
	case CTRL_BA:
		ND_PRINT("RA:%s TA:%s ",
		    GET_MAC48_STRING(((const struct ctrl_ba_hdr_t *)p)->ra),
		    GET_MAC48_STRING(((const struct ctrl_ba_hdr_t *)p)->ta));
		break;
	case CTRL_PS_POLL:
		ND_PRINT("BSSID:%s TA:%s ",
		    GET_MAC48_STRING(((const struct ctrl_ps_poll_hdr_t *)p)->bssid),
		    GET_MAC48_STRING(((const struct ctrl_ps_poll_hdr_t *)p)->ta));
		break;
	case CTRL_RTS:
		ND_PRINT("RA:%s TA:%s ",
		    GET_MAC48_STRING(((const struct ctrl_rts_hdr_t *)p)->ra),
		    GET_MAC48_STRING(((const struct ctrl_rts_hdr_t *)p)->ta));
		break;
	case CTRL_CTS:
		ND_PRINT("RA:%s ",
		    GET_MAC48_STRING(((const struct ctrl_cts_hdr_t *)p)->ra));
		break;
	case CTRL_ACK:
		ND_PRINT("RA:%s ",
		    GET_MAC48_STRING(((const struct ctrl_ack_hdr_t *)p)->ra));
		break;
	case CTRL_CF_END:
		ND_PRINT("RA:%s BSSID:%s ",
		    GET_MAC48_STRING(((const struct ctrl_end_hdr_t *)p)->ra),
		    GET_MAC48_STRING(((const struct ctrl_end_hdr_t *)p)->bssid));
		break;
	case CTRL_END_ACK:
		ND_PRINT("RA:%s BSSID:%s ",
		    GET_MAC48_STRING(((const struct ctrl_end_ack_hdr_t *)p)->ra),
		    GET_MAC48_STRING(((const struct ctrl_end_ack_hdr_t *)p)->bssid));
		break;
	default:
		/* We shouldn't get here - we should already have quit */
		break;
	}
}

static int
extract_header_length(netdissect_options *ndo,
		      uint16_t fc)
{
	int len;

	switch (FC_TYPE(fc)) {
	case T_MGMT:
		return MGMT_HDRLEN;
	case T_CTRL:
		switch (FC_SUBTYPE(fc)) {
		case CTRL_CONTROL_WRAPPER:
			return CTRL_CONTROL_WRAPPER_HDRLEN;
		case CTRL_BAR:
			return CTRL_BAR_HDRLEN;
		case CTRL_BA:
			return CTRL_BA_HDRLEN;
		case CTRL_PS_POLL:
			return CTRL_PS_POLL_HDRLEN;
		case CTRL_RTS:
			return CTRL_RTS_HDRLEN;
		case CTRL_CTS:
			return CTRL_CTS_HDRLEN;
		case CTRL_ACK:
			return CTRL_ACK_HDRLEN;
		case CTRL_CF_END:
			return CTRL_END_HDRLEN;
		case CTRL_END_ACK:
			return CTRL_END_ACK_HDRLEN;
		default:
			if(SHOW_ORIGIN_OUTPUT)
				ND_PRINT("unknown 802.11 ctrl frame subtype (%u)", FC_SUBTYPE(fc));
			return 0;
		}
	case T_DATA:
		len = (FC_TO_DS(fc) && FC_FROM_DS(fc)) ? 30 : 24;
		if (DATA_FRAME_IS_QOS(FC_SUBTYPE(fc)))
			len += 2;
		return len;
	default:
		if(SHOW_ORIGIN_OUTPUT)
			ND_PRINT("unknown 802.11 frame type (%u)", FC_TYPE(fc));
		return 0;
	}
}

static int
extract_mesh_header_length(netdissect_options *ndo, const u_char *p)
{
	return (GET_U_1(p) &~ 3) ? 0 : 6*(1 + (GET_U_1(p) & 3));
}

/*
 * Print the 802.11 MAC header.
 */
static void
ieee_802_11_hdr_print(netdissect_options *ndo,
		      uint16_t fc, const u_char *p, u_int hdrlen,
		      u_int meshdrlen)
{
	if (ndo->ndo_vflag && SHOW_ORIGIN_OUTPUT) {
		if (FC_MORE_DATA(fc))
			ND_PRINT("More Data ");
		if (FC_MORE_FLAG(fc))
			ND_PRINT("More Fragments ");
		if (FC_POWER_MGMT(fc))
			ND_PRINT("Pwr Mgmt ");
		if (FC_RETRY(fc))
			ND_PRINT("Retry ");
		if (FC_ORDER(fc))
			ND_PRINT("Strictly Ordered ");
		if (FC_PROTECTED(fc))
			ND_PRINT("Protected ");
		if (FC_TYPE(fc) != T_CTRL || FC_SUBTYPE(fc) != CTRL_PS_POLL)
			ND_PRINT("%uus ",
			    GET_LE_U_2(((const struct mgmt_header_t *)p)->duration));
	}
	if (meshdrlen != 0 && SHOW_ORIGIN_OUTPUT) {
		const struct meshcntl_t *mc =
		    (const struct meshcntl_t *)(p + hdrlen - meshdrlen);
		u_int ae = GET_U_1(mc->flags) & 3;

		ND_PRINT("MeshData (AE %u TTL %u seq %u", ae,
		    GET_U_1(mc->ttl), GET_LE_U_4(mc->seq));
		if (ae > 0)
			ND_PRINT(" A4:%s", GET_MAC48_STRING(mc->addr4));
		if (ae > 1)
			ND_PRINT(" A5:%s", GET_MAC48_STRING(mc->addr5));
		if (ae > 2)
			ND_PRINT(" A6:%s", GET_MAC48_STRING(mc->addr6));
		ND_PRINT(") ");
	}

	switch (FC_TYPE(fc)) {
	case T_MGMT:
		mgmt_header_print(ndo, p);
		break;
	case T_CTRL:
		ctrl_header_print(ndo, fc, p);
		break;
	case T_DATA:
		data_header_print(ndo, fc, p);
		break;
	default:
		break;
	}
}

/*
 * Version of 80211
 * 1 => 802.11n
 * 2 => 802.11a/g
 * 3 => 802.11b
 */
static u_int wlan_radio_ver;
#define IEEE80211_N		1
#define IEEE80211_AG	2
#define IEEE80211_B		3

static uint8_t is_short_preamble;  /* check if is short preamble for 802.11b */
static uint8_t has_rate;           /* check if we get bit rate */
static float dt_rate;              /* store Data Rate from radiotap */


static void print_radio_duration(netdissect_options *ndo, u_int len)
{
	u_int preamble = 0;
	u_int duration;

	if(has_rate != 1) {
		ND_PRINT("%d ", 0);
		return;
	}

	if(dt_rate <= 0) {
		ND_PRINT("%d ", 0);
		return;
	}

	/*
	 * From ieee80211 version get relative preamble duration time
	 * unit -> micro second (us)
	 * Check wireshark src packet-ieee80211-radio.c line 1194 ~ 1345
	 */
	switch(wlan_radio_ver) {
		case IEEE80211_N:
		{
			preamble = 40;
			duration = (u_int) ceil(preamble + 8 * len / dt_rate);
			break;
		}

		case IEEE80211_AG:
		{
			/* preamble + signal */
			preamble = 16 + 4;

			/* 16 service bits, data and 6 tail bits */
			u_int bits = 16 + 8 * len + 6;
			u_int symbols = (u_int) ceil(bits / (dt_rate * 4));

			duration = preamble + symbols * 4;
			break;
		}

		case IEEE80211_B:
		{
			preamble = (is_short_preamble)? 72 + 24 : 144 + 48;
			duration = (u_int) ceil(preamble + len * 8 / dt_rate);
			break;
		}

		default:
			preamble = 0;
			duration = (u_int) ceil(preamble + len * 8 / dt_rate);
			break;
	}

	/* the time unit of the duration is us(1e-6) */
	ND_PRINT("%d ", duration);

}

static void print_more_info(netdissect_options *ndo, char* tp, u_int origlen)
{
	u_int len = origlen;	/* frame len without radiotap header */

	ND_PRINT(" !");			/* Indicate Symbol */
	ND_PRINT("%s ", tp);	/* frame type */
	ND_PRINT("%d ", len);	/* frame capture length */
	
	print_radio_duration(ndo, len);  /* wlan_radio.duration */

	ND_PRINT("! ");            /* Indicate Symbol */
}

static u_int
ieee802_11_print(netdissect_options *ndo,
		 const u_char *p, u_int length, u_int orig_caplen, int pad,
		 u_int fcslen)
{
	uint16_t fc;
	u_int caplen, hdrlen, meshdrlen;
	struct lladdr_info src, dst;
	int llc_hdrlen;

	ndo->ndo_protocol = "802.11";
	caplen = orig_caplen;
	/* Remove FCS, if present */
	if (length < fcslen) {
		nd_print_trunc(ndo);
		return caplen;
	}
	length -= fcslen;
	if (caplen > length) {
		/* Amount of FCS in actual packet data, if any */
		fcslen = caplen - length;
		caplen -= fcslen;
		ndo->ndo_snapend -= fcslen;
	}

	if (caplen < IEEE802_11_FC_LEN) {
		nd_print_trunc(ndo);
		return orig_caplen;
	}

	fc = GET_LE_U_2(p);
	hdrlen = extract_header_length(ndo, fc);
	if (hdrlen == 0) {
		print_more_info(ndo, "None", orig_caplen);
		/* Unknown frame type or control frame subtype; quit. */
		return (0);
	}
	if (pad)
		hdrlen = roundup2(hdrlen, 4);
	if (ndo->ndo_Hflag && FC_TYPE(fc) == T_DATA &&
	    DATA_FRAME_IS_QOS(FC_SUBTYPE(fc))) {
		if(!ND_TTEST_1(p + hdrlen)) {
			nd_print_trunc(ndo);
			return hdrlen;
		}
		meshdrlen = extract_mesh_header_length(ndo, p + hdrlen);
		hdrlen += meshdrlen;
	} else
		meshdrlen = 0;

	if (caplen < hdrlen) {
		nd_print_trunc(ndo);
		return hdrlen;
	}

	if (ndo->ndo_eflag)
		ieee_802_11_hdr_print(ndo, fc, p, hdrlen, meshdrlen);

	switch (FC_TYPE(fc))
	{
		case T_MGMT:
			print_more_info(ndo, "Mgmt", orig_caplen);
			break;
		case T_CTRL:
			print_more_info(ndo, "Ctrl", orig_caplen);
			break;
		case T_DATA:
			print_more_info(ndo, "Data", orig_caplen);
			break;
		default:
			break;
	}

	if(SHOW_ORIGIN_OUTPUT) {
		/*
		* Go past the 802.11 header.
		*/
		length -= hdrlen;
		caplen -= hdrlen;
		p += hdrlen;

		src.addr_string = mac48_string;
		dst.addr_string = mac48_string;
		switch (FC_TYPE(fc)) {
		case T_MGMT:
			get_mgmt_src_dst_mac(p - hdrlen, &src.addr, &dst.addr);
			if (!mgmt_body_print(ndo, fc, src.addr, p, length)) {
				nd_print_trunc(ndo);
				return hdrlen;
			}
			break;
		case T_CTRL:
			if (!ctrl_body_print(ndo, fc, p - hdrlen)) {
				nd_print_trunc(ndo);
				return hdrlen;
			}
			break;
		case T_DATA:
			if (DATA_FRAME_IS_NULL(FC_SUBTYPE(fc)))
				return hdrlen;	/* no-data frame */
			/* There may be a problem w/ AP not having this bit set */
			if (FC_PROTECTED(fc)) {
				if (!wep_print(ndo, p)) {
					nd_print_trunc(ndo);
					return hdrlen;
				}
			} else {
				get_data_src_dst_mac(fc, p - hdrlen, &src.addr, &dst.addr);
				llc_hdrlen = llc_print(ndo, p, length, caplen, &src, &dst);
				if (llc_hdrlen < 0) {
					/*
					* Some kinds of LLC packet we cannot
					* handle intelligently
					*/
					if (!ndo->ndo_suppress_default_print)
						ND_DEFAULTPRINT(p, caplen);
					llc_hdrlen = -llc_hdrlen;
				}
				hdrlen += llc_hdrlen;
			}
			break;
		default:
			/* We shouldn't get here - we should already have quit */
			break;
		}
	}
	return hdrlen;
}

/*
 * This is the top level routine of the printer.  'p' points
 * to the 802.11 header of the packet, 'h->ts' is the timestamp,
 * 'h->len' is the length of the packet off the wire, and 'h->caplen'
 * is the number of bytes actually captured.
 */
void
ieee802_11_if_print(netdissect_options *ndo,
		    const struct pcap_pkthdr *h, const u_char *p)
{
	ndo->ndo_protocol = "802.11";
	ndo->ndo_ll_hdr_len += ieee802_11_print(ndo, p, h->len, h->caplen, 0, 0);
}


/* $FreeBSD: src/sys/net80211/ieee80211_radiotap.h,v 1.5 2005/01/22 20:12:05 sam Exp $ */
/* NetBSD: ieee802_11_radio.h,v 1.2 2006/02/26 03:04:03 dyoung Exp  */

/*-
 * Copyright (c) 2003, 2004 David Young.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of David Young may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY DAVID YOUNG ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL DAVID
 * YOUNG BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 */

/* A generic radio capture format is desirable. It must be
 * rigidly defined (e.g., units for fields should be given),
 * and easily extensible.
 *
 * The following is an extensible radio capture format. It is
 * based on a bitmap indicating which fields are present.
 *
 * I am trying to describe precisely what the application programmer
 * should expect in the following, and for that reason I tell the
 * units and origin of each measurement (where it applies), or else I
 * use sufficiently weaselly language ("is a monotonically nondecreasing
 * function of...") that I cannot set false expectations for lawyerly
 * readers.
 */

/*
 * The radio capture header precedes the 802.11 header.
 *
 * Note well: all radiotap fields are little-endian.
 */
struct ieee80211_radiotap_header {
	nd_uint8_t	it_version;	/* Version 0. Only increases
					 * for drastic changes,
					 * introduction of compatible
					 * new fields does not count.
					 */
	nd_uint8_t	it_pad;
	nd_uint16_t	it_len;		/* length of the whole
					 * header in bytes, including
					 * it_version, it_pad,
					 * it_len, and data fields.
					 */
	nd_uint32_t	it_present;	/* A bitmap telling which
					 * fields are present. Set bit 31
					 * (0x80000000) to extend the
					 * bitmap by another 32 bits.
					 * Additional extensions are made
					 * by setting bit 31.
					 */
};

/* Name                                 Data type    Units
 * ----                                 ---------    -----
 *
 * IEEE80211_RADIOTAP_TSFT              __le64       microseconds
 *
 *      Value in microseconds of the MAC's 64-bit 802.11 Time
 *      Synchronization Function timer when the first bit of the
 *      MPDU arrived at the MAC. For received frames, only.
 *
 * IEEE80211_RADIOTAP_CHANNEL           2 x guint16   MHz, bitmap
 *
 *      Tx/Rx frequency in MHz, followed by flags (see below).
 *
 * IEEE80211_RADIOTAP_FHSS              guint16       see below
 *
 *      For frequency-hopping radios, the hop set (first byte)
 *      and pattern (second byte).
 *
 * IEEE80211_RADIOTAP_RATE              u8           500kb/s
 *
 *      Tx/Rx data rate
 *
 * IEEE80211_RADIOTAP_DBM_ANTSIGNAL     s8           decibels from
 *                                                   one milliwatt (dBm)
 *
 *      RF signal power at the antenna, decibel difference from
 *      one milliwatt.
 *
 * IEEE80211_RADIOTAP_DBM_ANTNOISE      s8           decibels from
 *                                                   one milliwatt (dBm)
 *
 *      RF noise power at the antenna, decibel difference from one
 *      milliwatt.
 *
 * IEEE80211_RADIOTAP_DB_ANTSIGNAL      u8           decibel (dB)
 *
 *      RF signal power at the antenna, decibel difference from an
 *      arbitrary, fixed reference.
 *
 * IEEE80211_RADIOTAP_DB_ANTNOISE       u8           decibel (dB)
 *
 *      RF noise power at the antenna, decibel difference from an
 *      arbitrary, fixed reference point.
 *
 * IEEE80211_RADIOTAP_LOCK_QUALITY      guint16       unitless
 *
 *      Quality of Barker code lock. Unitless. Monotonically
 *      nondecreasing with "better" lock strength. Called "Signal
 *      Quality" in datasheets.  (Is there a standard way to measure
 *      this?)
 *
 * IEEE80211_RADIOTAP_TX_ATTENUATION    guint16       unitless
 *
 *      Transmit power expressed as unitless distance from max
 *      power set at factory calibration.  0 is max power.
 *      Monotonically nondecreasing with lower power levels.
 *
 * IEEE80211_RADIOTAP_DB_TX_ATTENUATION guint16       decibels (dB)
 *
 *      Transmit power expressed as decibel distance from max power
 *      set at factory calibration.  0 is max power.  Monotonically
 *      nondecreasing with lower power levels.
 *
 * IEEE80211_RADIOTAP_DBM_TX_POWER      s8           decibels from
 *                                                   one milliwatt (dBm)
 *
 *      Transmit power expressed as dBm (decibels from a 1 milliwatt
 *      reference). This is the absolute power level measured at
 *      the antenna port.
 *
 * IEEE80211_RADIOTAP_FLAGS             u8           bitmap
 *
 *      Properties of transmitted and received frames. See flags
 *      defined below.
 *
 * IEEE80211_RADIOTAP_ANTENNA           u8           antenna index
 *
 *      Unitless indication of the Rx/Tx antenna for this packet.
 *      The first antenna is antenna 0.
 *
 * IEEE80211_RADIOTAP_RX_FLAGS          guint16       bitmap
 *
 *     Properties of received frames. See flags defined below.
 *
 * IEEE80211_RADIOTAP_TX_FLAGS          guint16       bitmap
 *
 *     Properties of transmitted frames. See flags defined below.
 *
 * IEEE80211_RADIOTAP_RTS_RETRIES       u8           data
 *
 *     Number of rts retries a transmitted frame used.
 *
 * IEEE80211_RADIOTAP_DATA_RETRIES      u8           data
 *
 *     Number of unicast retries a transmitted frame used.
 *
 * IEEE80211_RADIOTAP_MCS	u8, u8, u8		unitless
 *
 *     Contains a bitmap of known fields/flags, the flags, and
 *     the MCS index.
 *
 * IEEE80211_RADIOTAP_AMPDU_STATUS	u32, u16, u8, u8	unitlesss
 *
 *	Contains the AMPDU information for the subframe.
 *
 * IEEE80211_RADIOTAP_HE		u16, u16, u16, u16, u16, u16 unitless
 *	Contains some information for HE frames.
 *
 * IEEE80211_RADIOTAP_HE_MU		U16, U16, u8[4] unitless
 *
 * IEEE80211_RADIOTAP_HE_MU_USER
 *
 * IEEE80211_RADIOTAP_0_LENGTH_PSDU
 *
 * IEEE80211_RADIOTAP_L_SIG
 *
 * IEEE80211_RADIOTAP_TLVS_PRESENT
 *
 * THERE ARE NO MORE BITS FREE! If you need a new radiotap header you must
 * ask for a TLV value. See www.radiotap.org.
 *
 */
enum ieee80211_radiotap_type {
	IEEE80211_RADIOTAP_TSFT              = 0,
	IEEE80211_RADIOTAP_FLAGS             = 1,
	IEEE80211_RADIOTAP_RATE              = 2,
	IEEE80211_RADIOTAP_CHANNEL           = 3,
	IEEE80211_RADIOTAP_FHSS              = 4,
	IEEE80211_RADIOTAP_DBM_ANTSIGNAL     = 5,
	IEEE80211_RADIOTAP_DBM_ANTNOISE      = 6,
	IEEE80211_RADIOTAP_LOCK_QUALITY      = 7,
	IEEE80211_RADIOTAP_TX_ATTENUATION    = 8,
	IEEE80211_RADIOTAP_DB_TX_ATTENUATION = 9,
	IEEE80211_RADIOTAP_DBM_TX_POWER      = 10,
	IEEE80211_RADIOTAP_ANTENNA           = 11,
	IEEE80211_RADIOTAP_DB_ANTSIGNAL      = 12,
	IEEE80211_RADIOTAP_DB_ANTNOISE       = 13,
	IEEE80211_RADIOTAP_RX_FLAGS          = 14,
	IEEE80211_RADIOTAP_TX_FLAGS          = 15,
	IEEE80211_RADIOTAP_RTS_RETRIES       = 16,
	IEEE80211_RADIOTAP_DATA_RETRIES      = 17,
	IEEE80211_RADIOTAP_XCHANNEL          = 18,   /* Unofficial, used by FreeBSD */
	IEEE80211_RADIOTAP_MCS               = 19,
	IEEE80211_RADIOTAP_AMPDU_STATUS      = 20,
	IEEE80211_RADIOTAP_VHT               = 21,
	IEEE80211_RADIOTAP_TIMESTAMP         = 22,
	IEEE80211_RADIOTAP_HE                = 23,
	IEEE80211_RADIOTAP_HE_MU             = 24,
	IEEE80211_RAFIOTAP_HE_MU_USER        = 25,
	IEEE80211_RADIOTAP_0_LENGTH_PSDU     = 26,
	IEEE80211_RADIOTAP_L_SIG             = 27,
	IEEE80211_RADIOTAP_TLVS              = 28,

	/* valid in every it_present bitmap, even vendor namespaces */
	IEEE80211_RADIOTAP_RADIOTAP_NAMESPACE = 29,
	IEEE80211_RADIOTAP_VENDOR_NAMESPACE   = 30,
	IEEE80211_RADIOTAP_EXT                = 31
};

/* channel attributes */
#define	IEEE80211_CHAN_TURBO	0x00010	/* Turbo channel */
#define	IEEE80211_CHAN_CCK	0x00020	/* CCK channel */
#define	IEEE80211_CHAN_OFDM	0x00040	/* OFDM channel */
#define	IEEE80211_CHAN_2GHZ	0x00080	/* 2 GHz spectrum channel. */
#define	IEEE80211_CHAN_5GHZ	0x00100	/* 5 GHz spectrum channel */
#define	IEEE80211_CHAN_PASSIVE	0x00200	/* Only passive scan allowed */
#define	IEEE80211_CHAN_DYN	0x00400	/* Dynamic CCK-OFDM channel */
#define	IEEE80211_CHAN_GFSK	0x00800	/* GFSK channel (FHSS PHY) */
#define	IEEE80211_CHAN_GSM	0x01000	/* 900 MHz spectrum channel */
#define	IEEE80211_CHAN_STURBO	0x02000	/* 11a static turbo channel only */
#define	IEEE80211_CHAN_HALF	0x04000	/* Half rate channel */
#define	IEEE80211_CHAN_QUARTER	0x08000	/* Quarter rate channel */
#define	IEEE80211_CHAN_HT20	0x10000	/* HT 20 channel */
#define	IEEE80211_CHAN_HT40U	0x20000	/* HT 40 channel w/ ext above */
#define	IEEE80211_CHAN_HT40D	0x40000	/* HT 40 channel w/ ext below */

/* Useful combinations of channel characteristics, borrowed from Ethereal */
#define IEEE80211_CHAN_A \
	(IEEE80211_CHAN_5GHZ | IEEE80211_CHAN_OFDM)
#define IEEE80211_CHAN_B \
	(IEEE80211_CHAN_2GHZ | IEEE80211_CHAN_CCK)
#define IEEE80211_CHAN_G \
	(IEEE80211_CHAN_2GHZ | IEEE80211_CHAN_DYN)
#define IEEE80211_CHAN_TA \
	(IEEE80211_CHAN_5GHZ | IEEE80211_CHAN_OFDM | IEEE80211_CHAN_TURBO)
#define IEEE80211_CHAN_TG \
	(IEEE80211_CHAN_2GHZ | IEEE80211_CHAN_DYN  | IEEE80211_CHAN_TURBO)


/* For IEEE80211_RADIOTAP_FLAGS */
#define	IEEE80211_RADIOTAP_F_CFP	0x01	/* sent/received
						 * during CFP
						 */
#define	IEEE80211_RADIOTAP_F_SHORTPRE	0x02	/* sent/received
						 * with short
						 * preamble
						 */
#define	IEEE80211_RADIOTAP_F_WEP	0x04	/* sent/received
						 * with WEP encryption
						 */
#define	IEEE80211_RADIOTAP_F_FRAG	0x08	/* sent/received
						 * with fragmentation
						 */
#define	IEEE80211_RADIOTAP_F_FCS	0x10	/* frame includes FCS */
#define	IEEE80211_RADIOTAP_F_DATAPAD	0x20	/* frame has padding between
						 * 802.11 header and payload
						 * (to 32-bit boundary)
						 */
#define	IEEE80211_RADIOTAP_F_BADFCS	0x40	/* does not pass FCS check */

/* For IEEE80211_RADIOTAP_RX_FLAGS */
#define IEEE80211_RADIOTAP_F_RX_BADFCS	0x0001	/* frame failed crc check */
#define IEEE80211_RADIOTAP_F_RX_PLCP_CRC	0x0002	/* frame failed PLCP CRC check */

/* For IEEE80211_RADIOTAP_MCS known */
#define IEEE80211_RADIOTAP_MCS_BANDWIDTH_KNOWN		0x01
#define IEEE80211_RADIOTAP_MCS_MCS_INDEX_KNOWN		0x02	/* MCS index field */
#define IEEE80211_RADIOTAP_MCS_GUARD_INTERVAL_KNOWN	0x04
#define IEEE80211_RADIOTAP_MCS_HT_FORMAT_KNOWN		0x08
#define IEEE80211_RADIOTAP_MCS_FEC_TYPE_KNOWN		0x10
#define IEEE80211_RADIOTAP_MCS_STBC_KNOWN		0x20
#define IEEE80211_RADIOTAP_MCS_NESS_KNOWN		0x40
#define IEEE80211_RADIOTAP_MCS_NESS_BIT_1		0x80

/* For IEEE80211_RADIOTAP_MCS flags */
#define IEEE80211_RADIOTAP_MCS_BANDWIDTH_MASK	0x03
#define IEEE80211_RADIOTAP_MCS_BANDWIDTH_20	0
#define IEEE80211_RADIOTAP_MCS_BANDWIDTH_40	1
#define IEEE80211_RADIOTAP_MCS_BANDWIDTH_20L	2
#define IEEE80211_RADIOTAP_MCS_BANDWIDTH_20U	3
#define IEEE80211_RADIOTAP_MCS_SHORT_GI		0x04 /* short guard interval */
#define IEEE80211_RADIOTAP_MCS_HT_GREENFIELD	0x08
#define IEEE80211_RADIOTAP_MCS_FEC_LDPC		0x10
#define IEEE80211_RADIOTAP_MCS_STBC_MASK	0x60
#define		IEEE80211_RADIOTAP_MCS_STBC_1	1
#define		IEEE80211_RADIOTAP_MCS_STBC_2	2
#define		IEEE80211_RADIOTAP_MCS_STBC_3	3
#define IEEE80211_RADIOTAP_MCS_STBC_SHIFT	5
#define IEEE80211_RADIOTAP_MCS_NESS_BIT_0	0x80

/* For IEEE80211_RADIOTAP_AMPDU_STATUS */
#define IEEE80211_RADIOTAP_AMPDU_REPORT_ZEROLEN		0x0001
#define IEEE80211_RADIOTAP_AMPDU_IS_ZEROLEN		0x0002
#define IEEE80211_RADIOTAP_AMPDU_LAST_KNOWN		0x0004
#define IEEE80211_RADIOTAP_AMPDU_IS_LAST		0x0008
#define IEEE80211_RADIOTAP_AMPDU_DELIM_CRC_ERR		0x0010
#define IEEE80211_RADIOTAP_AMPDU_DELIM_CRC_KNOWN	0x0020

/* For IEEE80211_RADIOTAP_VHT known */
#define IEEE80211_RADIOTAP_VHT_STBC_KNOWN			0x0001
#define IEEE80211_RADIOTAP_VHT_TXOP_PS_NA_KNOWN			0x0002
#define IEEE80211_RADIOTAP_VHT_GUARD_INTERVAL_KNOWN		0x0004
#define IEEE80211_RADIOTAP_VHT_SGI_NSYM_DIS_KNOWN		0x0008
#define IEEE80211_RADIOTAP_VHT_LDPC_EXTRA_OFDM_SYM_KNOWN	0x0010
#define IEEE80211_RADIOTAP_VHT_BEAMFORMED_KNOWN			0x0020
#define IEEE80211_RADIOTAP_VHT_BANDWIDTH_KNOWN			0x0040
#define IEEE80211_RADIOTAP_VHT_GROUP_ID_KNOWN			0x0080
#define IEEE80211_RADIOTAP_VHT_PARTIAL_AID_KNOWN		0x0100

/* For IEEE80211_RADIOTAP_VHT flags */
#define IEEE80211_RADIOTAP_VHT_STBC			0x01
#define IEEE80211_RADIOTAP_VHT_TXOP_PS_NA		0x02
#define IEEE80211_RADIOTAP_VHT_SHORT_GI			0x04
#define IEEE80211_RADIOTAP_VHT_SGI_NSYM_M10_9		0x08
#define IEEE80211_RADIOTAP_VHT_LDPC_EXTRA_OFDM_SYM	0x10
#define IEEE80211_RADIOTAP_VHT_BEAMFORMED		0x20

#define IEEE80211_RADIOTAP_VHT_BANDWIDTH_MASK	0x1f

#define IEEE80211_RADIOTAP_VHT_NSS_MASK		0x0f
#define IEEE80211_RADIOTAP_VHT_MCS_MASK		0xf0
#define IEEE80211_RADIOTAP_VHT_MCS_SHIFT	4

#define IEEE80211_RADIOTAP_CODING_LDPC_USERn			0x01

#define	IEEE80211_CHAN_FHSS \
	(IEEE80211_CHAN_2GHZ | IEEE80211_CHAN_GFSK)
#define	IEEE80211_CHAN_A \
	(IEEE80211_CHAN_5GHZ | IEEE80211_CHAN_OFDM)
#define	IEEE80211_CHAN_B \
	(IEEE80211_CHAN_2GHZ | IEEE80211_CHAN_CCK)
#define	IEEE80211_CHAN_PUREG \
	(IEEE80211_CHAN_2GHZ | IEEE80211_CHAN_OFDM)
#define	IEEE80211_CHAN_G \
	(IEEE80211_CHAN_2GHZ | IEEE80211_CHAN_DYN)

#define	IS_CHAN_FHSS(flags) \
	((flags & IEEE80211_CHAN_FHSS) == IEEE80211_CHAN_FHSS)
#define	IS_CHAN_A(flags) \
	((flags & IEEE80211_CHAN_A) == IEEE80211_CHAN_A)
#define	IS_CHAN_B(flags) \
	((flags & IEEE80211_CHAN_B) == IEEE80211_CHAN_B)
#define	IS_CHAN_PUREG(flags) \
	((flags & IEEE80211_CHAN_PUREG) == IEEE80211_CHAN_PUREG)
#define	IS_CHAN_G(flags) \
	((flags & IEEE80211_CHAN_G) == IEEE80211_CHAN_G)
#define	IS_CHAN_ANYG(flags) \
	(IS_CHAN_PUREG(flags) || IS_CHAN_G(flags))

static void
print_chaninfo(netdissect_options *ndo,
	       uint16_t freq, uint32_t flags, uint32_t presentflags)
{
	ND_PRINT("%u MHz", freq);
	if (presentflags & (1 << IEEE80211_RADIOTAP_MCS)) {
		/*
		 * We have the MCS field, so this is 11n, regardless
		 * of what the channel flags say.
		 */
		ND_PRINT(" 11n");
		wlan_radio_ver = 1;
	} else {
		if (IS_CHAN_FHSS(flags))
			ND_PRINT(" FHSS");
		if (IS_CHAN_A(flags)) {
			if (flags & IEEE80211_CHAN_HALF)
				ND_PRINT(" 11a/10Mhz");
			else if (flags & IEEE80211_CHAN_QUARTER)
				ND_PRINT(" 11a/5Mhz");
			else
				ND_PRINT(" 11a");
			wlan_radio_ver = 2;
		}
		if (IS_CHAN_ANYG(flags)) {
			if (flags & IEEE80211_CHAN_HALF)
				ND_PRINT(" 11g/10Mhz");
			else if (flags & IEEE80211_CHAN_QUARTER)
				ND_PRINT(" 11g/5Mhz");
			else
				ND_PRINT(" 11g");
			wlan_radio_ver = 2;
		} else if (IS_CHAN_B(flags)) {
			ND_PRINT(" 11b");
			wlan_radio_ver = 3;
		}
		if (flags & IEEE80211_CHAN_TURBO)
			ND_PRINT(" Turbo");
	}
	/*
	 * These apply to 11n.
	 */
	if(SHOW_ORIGIN_OUTPUT){
		if (flags & IEEE80211_CHAN_HT20)
			ND_PRINT(" ht/20");
		else if (flags & IEEE80211_CHAN_HT40D)
			ND_PRINT(" ht/40-");
		else if (flags & IEEE80211_CHAN_HT40U)
			ND_PRINT(" ht/40+");
	}
	ND_PRINT(" ");
}

static int
print_radiotap_field(netdissect_options *ndo,
		     struct cpack_state *s, uint32_t bit, uint8_t *flagsp,
		     uint32_t presentflags)
{
	u_int i;
	int rc;

	switch (bit) {

	case IEEE80211_RADIOTAP_TSFT: {
		uint64_t tsft;

		rc = nd_cpack_uint64(ndo, s, &tsft);
		if (rc != 0)
			goto trunc;
		
		ND_PRINT("%" PRIu64 "us tsft ", tsft);
		break;
		}

	case IEEE80211_RADIOTAP_FLAGS: {
		uint8_t flagsval;

		rc = nd_cpack_uint8(ndo, s, &flagsval);
		if (rc != 0)
			goto trunc;
		*flagsp = flagsval;
		if(!SHOW_ORIGIN_OUTPUT)
			break;
		if (flagsval & IEEE80211_RADIOTAP_F_CFP)
			ND_PRINT("cfp ");
		if (flagsval & IEEE80211_RADIOTAP_F_SHORTPRE) {
				ND_PRINT("short preamble ");
				is_short_preamble = 1;
			}
		if (flagsval & IEEE80211_RADIOTAP_F_WEP)
			ND_PRINT("wep ");
		if (flagsval & IEEE80211_RADIOTAP_F_FRAG)
			ND_PRINT("fragmented ");
		if (flagsval & IEEE80211_RADIOTAP_F_BADFCS)
			ND_PRINT("bad-fcs ");
		break;
		}

	case IEEE80211_RADIOTAP_RATE: {
		uint8_t rate;

		rc = nd_cpack_uint8(ndo, s, &rate);
		if (rc != 0)
			goto trunc;
		/*
		 * XXX On FreeBSD rate & 0x80 means we have an MCS. On
		 * Linux and AirPcap it does not.  (What about
		 * macOS, NetBSD, OpenBSD, and DragonFly BSD?)
		 *
		 * This is an issue either for proprietary extensions
		 * to 11a or 11g, which do exist, or for 11n
		 * implementations that stuff a rate value into
		 * this field, which also appear to exist.
		 *
		 * We currently handle that by assuming that
		 * if the 0x80 bit is set *and* the remaining
		 * bits have a value between 0 and 15 it's
		 * an MCS value, otherwise it's a rate.  If
		 * there are cases where systems that use
		 * "0x80 + MCS index" for MCS indices > 15,
		 * or stuff a rate value here between 64 and
		 * 71.5 Mb/s in here, we'll need a preference
		 * setting.  Such rates do exist, e.g. 11n
		 * MCS 7 at 20 MHz with a long guard interval.
		 */
		if (rate >= 0x80 && rate <= 0x8f) {
			/*
			 * XXX - we don't know the channel width
			 * or guard interval length, so we can't
			 * convert this to a data rate.
			 *
			 * If you want us to show a data rate,
			 * use the MCS field, not the Rate field;
			 * the MCS field includes not only the
			 * MCS index, it also includes bandwidth
			 * and guard interval information.
			 *
			 * XXX - can we get the channel width
			 * from XChannel and the guard interval
			 * information from Flags, at least on
			 * FreeBSD?
			 */
			if(SHOW_ORIGIN_OUTPUT)
				ND_PRINT("MCS %u ", rate & 0x7f);
		} else
			has_rate = 1;
			dt_rate = .5 * rate;
			if(SHOW_ORIGIN_OUTPUT)
				ND_PRINT("%2.1f Mb/s ", .5 * rate);
		break;
		}

	case IEEE80211_RADIOTAP_CHANNEL: {
		uint16_t frequency;
		uint16_t flags;

		rc = nd_cpack_uint16(ndo, s, &frequency);
		if (rc != 0)
			goto trunc;
		rc = nd_cpack_uint16(ndo, s, &flags);
		if (rc != 0)
			goto trunc;
		/*
		 * If CHANNEL and XCHANNEL are both present, skip
		 * CHANNEL.
		 */
		if (presentflags & (1 << IEEE80211_RADIOTAP_XCHANNEL))
			break;
		print_chaninfo(ndo, frequency, flags, presentflags);
		break;
		}

	case IEEE80211_RADIOTAP_FHSS: {
		uint8_t hopset;
		uint8_t hoppat;

		rc = nd_cpack_uint8(ndo, s, &hopset);
		if (rc != 0)
			goto trunc;
		rc = nd_cpack_uint8(ndo, s, &hoppat);
		if (rc != 0)
			goto trunc;
		if(SHOW_ORIGIN_OUTPUT)
			ND_PRINT("fhset %u fhpat %u ", hopset, hoppat);
		break;
		}

	case IEEE80211_RADIOTAP_DBM_ANTSIGNAL: {
		int8_t dbm_antsignal;

		rc = nd_cpack_int8(ndo, s, &dbm_antsignal);
		if (rc != 0)
			goto trunc;
		ND_PRINT("%ddBm signal ", dbm_antsignal);
		break;
		}

	case IEEE80211_RADIOTAP_DBM_ANTNOISE: {
		int8_t dbm_antnoise;

		rc = nd_cpack_int8(ndo, s, &dbm_antnoise);
		if (rc != 0)
			goto trunc;
		ND_PRINT("%ddBm noise ", dbm_antnoise);
		break;
		}

	case IEEE80211_RADIOTAP_LOCK_QUALITY: {
		uint16_t lock_quality;

		rc = nd_cpack_uint16(ndo, s, &lock_quality);
		if (rc != 0)
			goto trunc;
		if(SHOW_ORIGIN_OUTPUT)
			ND_PRINT("%u sq ", lock_quality);
		break;
		}

	case IEEE80211_RADIOTAP_TX_ATTENUATION: {
		int16_t tx_attenuation;

		rc = nd_cpack_int16(ndo, s, &tx_attenuation);
		if (rc != 0)
			goto trunc;
		if(SHOW_ORIGIN_OUTPUT)			
			ND_PRINT("%d tx power ", -tx_attenuation);
		break;
		}

	case IEEE80211_RADIOTAP_DB_TX_ATTENUATION: {
		int8_t db_tx_attenuation;

		rc = nd_cpack_int8(ndo, s, &db_tx_attenuation);
		if (rc != 0)
			goto trunc;
		if(SHOW_ORIGIN_OUTPUT)
			ND_PRINT("%ddB tx attenuation ", -db_tx_attenuation);
		break;
		}

	case IEEE80211_RADIOTAP_DBM_TX_POWER: {
		int8_t dbm_tx_power;

		rc = nd_cpack_int8(ndo, s, &dbm_tx_power);
		if (rc != 0)
			goto trunc;
		if(SHOW_ORIGIN_OUTPUT)
			ND_PRINT("%ddBm tx power ", dbm_tx_power);
		break;
		}

	case IEEE80211_RADIOTAP_ANTENNA: {
		uint8_t antenna;

		rc = nd_cpack_uint8(ndo, s, &antenna);
		if (rc != 0)
			goto trunc;
		if(SHOW_ORIGIN_OUTPUT)
			ND_PRINT("antenna %u ", antenna);
		break;
		}

	case IEEE80211_RADIOTAP_DB_ANTSIGNAL: {
		uint8_t db_antsignal;

		rc = nd_cpack_uint8(ndo, s, &db_antsignal);
		if (rc != 0)
			goto trunc;
		if(SHOW_ORIGIN_OUTPUT)
			ND_PRINT("%udB signal ", db_antsignal);
		break;
		}

	case IEEE80211_RADIOTAP_DB_ANTNOISE: {
		uint8_t db_antnoise;

		rc = nd_cpack_uint8(ndo, s, &db_antnoise);
		if (rc != 0)
			goto trunc;
		if(SHOW_ORIGIN_OUTPUT)
			ND_PRINT("%udB noise ", db_antnoise);
		break;
		}

	case IEEE80211_RADIOTAP_RX_FLAGS: {
		uint16_t rx_flags;

		rc = nd_cpack_uint16(ndo, s, &rx_flags);
		if (rc != 0)
			goto trunc;
		/* Do nothing for now */
		break;
		}

	case IEEE80211_RADIOTAP_XCHANNEL: {
		uint32_t flags;
		uint16_t frequency;
		uint8_t channel;
		uint8_t maxpower;

		rc = nd_cpack_uint32(ndo, s, &flags);
		if (rc != 0)
			goto trunc;
		rc = nd_cpack_uint16(ndo, s, &frequency);
		if (rc != 0)
			goto trunc;
		rc = nd_cpack_uint8(ndo, s, &channel);
		if (rc != 0)
			goto trunc;
		rc = nd_cpack_uint8(ndo, s, &maxpower);
		if (rc != 0)
			goto trunc;
		print_chaninfo(ndo, frequency, flags, presentflags);
		break;
		}

	case IEEE80211_RADIOTAP_MCS: {
		uint8_t known;
		uint8_t flags;
		uint8_t mcs_index;
		static const char *ht_bandwidth[4] = {
			"20 MHz",
			"40 MHz",
			"20 MHz (L)",
			"20 MHz (U)"
		};
		float htrate;

		rc = nd_cpack_uint8(ndo, s, &known);
		if (rc != 0)
			goto trunc;
		rc = nd_cpack_uint8(ndo, s, &flags);
		if (rc != 0)
			goto trunc;
		rc = nd_cpack_uint8(ndo, s, &mcs_index);
		if (rc != 0)
			goto trunc;
		if (known & IEEE80211_RADIOTAP_MCS_MCS_INDEX_KNOWN) {
			/*
			 * We know the MCS index.
			 */
			if (mcs_index <= MAX_MCS_INDEX) {
				/*
				 * And it's in-range.
				 */
				if (known & (IEEE80211_RADIOTAP_MCS_BANDWIDTH_KNOWN|IEEE80211_RADIOTAP_MCS_GUARD_INTERVAL_KNOWN)) {
					/*
					 * And we know both the bandwidth and
					 * the guard interval, so we can look
					 * up the rate.
					 */
					htrate =
						ieee80211_float_htrates
							[mcs_index]
							[((flags & IEEE80211_RADIOTAP_MCS_BANDWIDTH_MASK) == IEEE80211_RADIOTAP_MCS_BANDWIDTH_40 ? 1 : 0)]
							[((flags & IEEE80211_RADIOTAP_MCS_SHORT_GI) ? 1 : 0)];
				} else {
					/*
					 * We don't know both the bandwidth
					 * and the guard interval, so we can
					 * only report the MCS index.
					 */
					htrate = 0.0;
				}
			} else {
				/*
				 * The MCS value is out of range.
				 */
				htrate = 0.0;
			}
			if (htrate != 0.0) {
				/*
				 * We have the rate.
				 * Print it.
				 */
				has_rate = 1;
				dt_rate = htrate;
				if(SHOW_ORIGIN_OUTPUT)
					ND_PRINT("%.1f Mb/s MCS %u ", htrate, mcs_index);
			} else {
				/*
				 * We at least have the MCS index.
				 * Print it.
				 */
				if(SHOW_ORIGIN_OUTPUT)
					ND_PRINT("MCS %u ", mcs_index);
			}
		}
		if(!SHOW_ORIGIN_OUTPUT)
			break;
		if (known & IEEE80211_RADIOTAP_MCS_BANDWIDTH_KNOWN) {
			ND_PRINT("%s ",
				ht_bandwidth[flags & IEEE80211_RADIOTAP_MCS_BANDWIDTH_MASK]);
		}
		if (known & IEEE80211_RADIOTAP_MCS_GUARD_INTERVAL_KNOWN) {
			ND_PRINT("%s GI ",
				(flags & IEEE80211_RADIOTAP_MCS_SHORT_GI) ?
				"short" : "long");
		}
		if (known & IEEE80211_RADIOTAP_MCS_HT_FORMAT_KNOWN) {
			ND_PRINT("%s ",
				(flags & IEEE80211_RADIOTAP_MCS_HT_GREENFIELD) ?
				"greenfield" : "mixed");
		}
		if (known & IEEE80211_RADIOTAP_MCS_FEC_TYPE_KNOWN) {
			ND_PRINT("%s FEC ",
				(flags & IEEE80211_RADIOTAP_MCS_FEC_LDPC) ?
				"LDPC" : "BCC");
		}
		if (known & IEEE80211_RADIOTAP_MCS_STBC_KNOWN) {
			ND_PRINT("RX-STBC%u ",
				(flags & IEEE80211_RADIOTAP_MCS_STBC_MASK) >> IEEE80211_RADIOTAP_MCS_STBC_SHIFT);
		}
		break;
		}

	case IEEE80211_RADIOTAP_AMPDU_STATUS: {
		uint32_t reference_num;
		uint16_t flags;
		uint8_t delim_crc;
		uint8_t reserved;

		rc = nd_cpack_uint32(ndo, s, &reference_num);
		if (rc != 0)
			goto trunc;
		rc = nd_cpack_uint16(ndo, s, &flags);
		if (rc != 0)
			goto trunc;
		rc = nd_cpack_uint8(ndo, s, &delim_crc);
		if (rc != 0)
			goto trunc;
		rc = nd_cpack_uint8(ndo, s, &reserved);
		if (rc != 0)
			goto trunc;
		/* Do nothing for now */
		break;
		}

	case IEEE80211_RADIOTAP_VHT: {
		uint16_t known;
		uint8_t flags;
		uint8_t bandwidth;
		uint8_t mcs_nss[4];
		uint8_t coding;
		uint8_t group_id;
		uint16_t partial_aid;
		static const char *vht_bandwidth[32] = {
			"20 MHz",
			"40 MHz",
			"20 MHz (L)",
			"20 MHz (U)",
			"80 MHz",
			"80 MHz (L)",
			"80 MHz (U)",
			"80 MHz (LL)",
			"80 MHz (LU)",
			"80 MHz (UL)",
			"80 MHz (UU)",
			"160 MHz",
			"160 MHz (L)",
			"160 MHz (U)",
			"160 MHz (LL)",
			"160 MHz (LU)",
			"160 MHz (UL)",
			"160 MHz (UU)",
			"160 MHz (LLL)",
			"160 MHz (LLU)",
			"160 MHz (LUL)",
			"160 MHz (UUU)",
			"160 MHz (ULL)",
			"160 MHz (ULU)",
			"160 MHz (UUL)",
			"160 MHz (UUU)",
			"unknown (26)",
			"unknown (27)",
			"unknown (28)",
			"unknown (29)",
			"unknown (30)",
			"unknown (31)"
		};

		rc = nd_cpack_uint16(ndo, s, &known);
		if (rc != 0)
			goto trunc;
		rc = nd_cpack_uint8(ndo, s, &flags);
		if (rc != 0)
			goto trunc;
		rc = nd_cpack_uint8(ndo, s, &bandwidth);
		if (rc != 0)
			goto trunc;
		for (i = 0; i < 4; i++) {
			rc = nd_cpack_uint8(ndo, s, &mcs_nss[i]);
			if (rc != 0)
				goto trunc;
		}
		rc = nd_cpack_uint8(ndo, s, &coding);
		if (rc != 0)
			goto trunc;
		rc = nd_cpack_uint8(ndo, s, &group_id);
		if (rc != 0)
			goto trunc;
		rc = nd_cpack_uint16(ndo, s, &partial_aid);
		if (rc != 0)
			goto trunc;

		if(!SHOW_ORIGIN_OUTPUT)
			break;
		for (i = 0; i < 4; i++) {
			u_int nss, mcs;
			nss = mcs_nss[i] & IEEE80211_RADIOTAP_VHT_NSS_MASK;
			mcs = (mcs_nss[i] & IEEE80211_RADIOTAP_VHT_MCS_MASK) >> IEEE80211_RADIOTAP_VHT_MCS_SHIFT;

			if (nss == 0)
				continue;

			ND_PRINT("User %u MCS %u ", i, mcs);
			ND_PRINT("%s FEC ",
				(coding & (IEEE80211_RADIOTAP_CODING_LDPC_USERn << i)) ?
				"LDPC" : "BCC");
		}
		if (known & IEEE80211_RADIOTAP_VHT_BANDWIDTH_KNOWN) {
			ND_PRINT("%s ",
				vht_bandwidth[bandwidth & IEEE80211_RADIOTAP_VHT_BANDWIDTH_MASK]);
		}
		if (known & IEEE80211_RADIOTAP_VHT_GUARD_INTERVAL_KNOWN) {
			ND_PRINT("%s GI ",
				(flags & IEEE80211_RADIOTAP_VHT_SHORT_GI) ?
				"short" : "long");
		}
		break;
		}
	case IEEE80211_RADIOTAP_TIMESTAMP: {
		// ND_PRINT(" Get Radiotap Timestamp!!! ");

		uint64_t tsft;
		uint32_t unit;

		/* Extract timestamp */
		rc = nd_cpack_uint64(ndo, s, &tsft); 
		if (rc != 0)
			goto trunc;

		/* Extract other units */
		rc = nd_cpack_uint32(ndo, s, &unit);
		if (rc != 0)
			goto trunc;

		break;
	}
	case IEEE80211_RADIOTAP_HE: {
		// ND_PRINT(" Get Radiotap HE Information!!! ");

		uint8_t data_mcs_known = 0;
		uint8_t data_bw_known  = 0;
		uint8_t gi_known       = 0;
		uint8_t nsts_known     = 0;

		uint8_t data_mcs = 0;
		uint8_t data_bw  = 0;
		uint8_t gi       = 0;
		uint8_t nsts     = 0;

		uint16_t data1;
		uint16_t data2;
		uint16_t data3;
		uint16_t data4;
		uint16_t data5;
		uint16_t data6;

		/* HE Data 1 */
		rc = nd_cpack_uint16(ndo, s, &data1);
		if (rc != 0)
			goto trunc;

		/* Determine what is known */
		if(data1 & IEEE80211_RADIOTAP_HE_DATA_MCS_KNOWN)
			data_mcs_known = 1;
		if(data1 & IEEE80211_RADIOTAP_HE_DATA_BW_RU_ALLOCATION_KNOWN)
			data_bw_known = 1;

		/* HE Data 2 */
		rc = nd_cpack_uint16(ndo, s, &data2);
		if (rc != 0)
			goto trunc;

		/* Determine what is known */
		if(data2 & IEEE80211_RADIOTAP_HE_GI_KNOWN)
			gi_known = 1;

		/* If any of them is unknown then we cant get the bit rate */
		if(!(data_mcs_known & data_bw_known & gi_known))
			return 0;

		/* 
		 * HE Data 3 
		 * Get MCS from this byte
		 */
		rc = nd_cpack_uint16(ndo, s, &data3);
		if (rc != 0)
			goto trunc;

		data_mcs = (data3 & IEEE80211_RADIOTAP_HE_DATA_MCS_MASK) >> 8;

		/* 
		 * HE Data 4
		 * not handle this byte
		 */
		rc = nd_cpack_uint16(ndo, s, &data4);
		if (rc != 0)
			goto trunc;

		/* 
		 * HE Data 5 
		 * Get data BW and GI from this byte
		 */
		rc = nd_cpack_uint16(ndo, s, &data5);
		if (rc != 0)
			goto trunc;

		data_bw = (data5 & IEEE80211_RADIOTAP_HE_DATA_BANDWIDTH_RU_ALLOC_MASK);
		gi = (data5 & IEEE80211_RADIOTAP_HE_GI_MASK) >> 4;

		/* 
		 * HE Data 6
		 * Get streams from this byte
		 */
		rc = nd_cpack_uint16(ndo, s, &data6);
		if (rc != 0)
			goto trunc;

		nsts = (data6 & IEEE80211_RADIOTAP_HE_NSTS_MASK);

		has_rate = 1;
		dt_rate = he_ofdm_tab[nsts - 1][data_mcs][data_bw][gi];

		// ND_PRINT("MCS Index: %d ", data_mcs);
		// ND_PRINT("BW Index: %d ", data_bw);
		// ND_PRINT("GI Index: %d ", gi);
		// ND_PRINT("NSTS Value: %d ", nsts);
		if(SHOW_ORIGIN_OUTPUT)
			ND_PRINT("Bit Rate: %.1f ", dt_rate);

		break;
	}
	default:
		/* this bit indicates a field whose
		 * size we do not know, so we cannot
		 * proceed.  Just print the bit number.
		 */
		ND_PRINT("[bit %u] ", bit);
		return -1;
	}

	return 0;

trunc:
	nd_print_trunc(ndo);
	return rc;
}


static int
print_in_radiotap_namespace(netdissect_options *ndo,
			    struct cpack_state *s, uint8_t *flags,
			    uint32_t presentflags, int bit0)
{
#define	BITNO_32(x) (((x) >> 16) ? 16 + BITNO_16((x) >> 16) : BITNO_16((x)))
#define	BITNO_16(x) (((x) >> 8) ? 8 + BITNO_8((x) >> 8) : BITNO_8((x)))
#define	BITNO_8(x) (((x) >> 4) ? 4 + BITNO_4((x) >> 4) : BITNO_4((x)))
#define	BITNO_4(x) (((x) >> 2) ? 2 + BITNO_2((x) >> 2) : BITNO_2((x)))
#define	BITNO_2(x) (((x) & 2) ? 1 : 0)
	uint32_t present, next_present;
	int bitno;
	enum ieee80211_radiotap_type bit;
	int rc;

	for (present = presentflags; present; present = next_present) {
		/*
		 * Clear the least significant bit that is set.
		 */
		next_present = present & (present - 1);

		/*
		 * Get the bit number, within this presence word,
		 * of the remaining least significant bit that
		 * is set.
		 */
		bitno = BITNO_32(present ^ next_present);

		/*
		 * Stop if this is one of the "same meaning
		 * in all presence flags" bits.
		 */
		if (bitno >= IEEE80211_RADIOTAP_RADIOTAP_NAMESPACE)
			break;

		/*
		 * Get the radiotap bit number of that bit.
		 */
		bit = (enum ieee80211_radiotap_type)(bit0 + bitno);

		rc = print_radiotap_field(ndo, s, bit, flags, presentflags);
		if (rc != 0)
			return rc;
	}

	return 0;
}

u_int
ieee802_11_radio_print(netdissect_options *ndo,
		       const u_char *p, u_int length, u_int caplen)
{
#define	BIT(n)	(1U << n)
#define	IS_EXTENDED(__p)	\
	    (GET_LE_U_4(__p) & BIT(IEEE80211_RADIOTAP_EXT)) != 0

	struct cpack_state cpacker;
	const struct ieee80211_radiotap_header *hdr;
	uint32_t presentflags;
	const nd_uint32_t *presentp, *last_presentp;
	int vendor_namespace;
	uint8_t vendor_oui[3];
	uint8_t vendor_subnamespace;
	uint16_t skip_length;
	int bit0;
	u_int len;
	uint8_t flags;
	int pad;
	u_int fcslen;

	ndo->ndo_protocol = "802.11_radio";
	if (caplen < sizeof(*hdr)) {
		nd_print_trunc(ndo);
		return caplen;
	}

	hdr = (const struct ieee80211_radiotap_header *)p;

	len = GET_LE_U_2(hdr->it_len);
	if (len < sizeof(*hdr)) {
		/*
		 * The length is the length of the entire header, so
		 * it must be as large as the fixed-length part of
		 * the header.
		 */
		nd_print_trunc(ndo);
		return caplen;
	}

	/*
	 * If we don't have the entire radiotap header, just give up.
	 */
	if (caplen < len) {
		nd_print_trunc(ndo);
		return caplen;
	}
	nd_cpack_init(&cpacker, (const uint8_t *)hdr, len); /* align against header start */
	nd_cpack_advance(&cpacker, sizeof(*hdr)); /* includes the 1st bitmap */
	for (last_presentp = &hdr->it_present;
	     (const u_char*)(last_presentp + 1) <= p + len &&
	     IS_EXTENDED(last_presentp);
	     last_presentp++)
	  nd_cpack_advance(&cpacker, sizeof(hdr->it_present)); /* more bitmaps */

	/* are there more bitmap extensions than bytes in header? */
	if ((const u_char*)(last_presentp + 1) > p + len) {
		nd_print_trunc(ndo);
		return caplen;
	}

	/*
	 * Start out at the beginning of the default radiotap namespace.
	 */
	bit0 = 0;
	vendor_namespace = 0;
	memset(vendor_oui, 0, 3);
	vendor_subnamespace = 0;
	skip_length = 0;
	/* Assume no flags */
	flags = 0;
	/* Assume no Atheros padding between 802.11 header and body */
	pad = 0;
	/* Assume no FCS at end of frame */
	fcslen = 0;

	/*
	 * Global Varaibles Initialize.
	 */
	{
		/*
		 * Check if there aren't any error bit before HE Info
		 * function print_in_radiotap_namespace will be call many times if there's many present flags in radiotap header
		 */
		has_rate = 0;

		/*
		 * Get ieee 80211 version
		 * and check if its short preamble
		 */
		wlan_radio_ver = 0;
		is_short_preamble = 0;

	}

	for (presentp = &hdr->it_present; presentp <= last_presentp;
	    presentp++) {
		presentflags = GET_LE_U_4(presentp);

		/*
		 * If this is a vendor namespace, we don't handle it.
		 */
		if (vendor_namespace) {
			/*
			 * Skip past the stuff we don't understand.
			 * If we add support for any vendor namespaces,
			 * it'd be added here; use vendor_oui and
			 * vendor_subnamespace to interpret the fields.
			 */
			if (nd_cpack_advance(&cpacker, skip_length) != 0) {
				/*
				 * Ran out of space in the packet.
				 */
				break;
			}

			/*
			 * We've skipped it all; nothing more to
			 * skip.
			 */
			skip_length = 0;
		} else {
			if (print_in_radiotap_namespace(ndo, &cpacker,
			    &flags, presentflags, bit0) != 0) {
				/*
				 * Fatal error - can't process anything
				 * more in the radiotap header.
				 */
				break;
			}
		}

		/*
		 * Handle the namespace switch bits; we've already handled
		 * the extension bit in all but the last word above.
		 */
		switch (presentflags &
		    (BIT(IEEE80211_RADIOTAP_RADIOTAP_NAMESPACE)|BIT(IEEE80211_RADIOTAP_VENDOR_NAMESPACE))) {

		case 0:
			/*
			 * We're not changing namespaces.
			 * advance to the next 32 bits in the current
			 * namespace.
			 */
			bit0 += 32;
			break;

		case BIT(IEEE80211_RADIOTAP_RADIOTAP_NAMESPACE):
			/*
			 * We're switching to the radiotap namespace.
			 * Reset the presence-bitmap index to 0, and
			 * reset the namespace to the default radiotap
			 * namespace.
			 */
			bit0 = 0;
			vendor_namespace = 0;
			memset(vendor_oui, 0, 3);
			vendor_subnamespace = 0;
			skip_length = 0;
			break;

		case BIT(IEEE80211_RADIOTAP_VENDOR_NAMESPACE):
			/*
			 * We're switching to a vendor namespace.
			 * Reset the presence-bitmap index to 0,
			 * note that we're in a vendor namespace,
			 * and fetch the fields of the Vendor Namespace
			 * item.
			 */
			bit0 = 0;
			vendor_namespace = 1;
			if ((nd_cpack_align_and_reserve(&cpacker, 2)) == NULL) {
				nd_print_trunc(ndo);
				break;
			}
			if (nd_cpack_uint8(ndo, &cpacker, &vendor_oui[0]) != 0) {
				nd_print_trunc(ndo);
				break;
			}
			if (nd_cpack_uint8(ndo, &cpacker, &vendor_oui[1]) != 0) {
				nd_print_trunc(ndo);
				break;
			}
			if (nd_cpack_uint8(ndo, &cpacker, &vendor_oui[2]) != 0) {
				nd_print_trunc(ndo);
				break;
			}
			if (nd_cpack_uint8(ndo, &cpacker, &vendor_subnamespace) != 0) {
				nd_print_trunc(ndo);
				break;
			}
			if (nd_cpack_uint16(ndo, &cpacker, &skip_length) != 0) {
				nd_print_trunc(ndo);
				break;
			}
			break;

		default:
			/*
			 * Illegal combination.  The behavior in this
			 * case is undefined by the radiotap spec; we
			 * just ignore both bits.
			 */
			break;
		}
	}

	if (flags & IEEE80211_RADIOTAP_F_DATAPAD)
		pad = 1;	/* Atheros padding */
	if (flags & IEEE80211_RADIOTAP_F_FCS)
		fcslen = 4;	/* FCS at end of packet */
	return len + ieee802_11_print(ndo, p + len, length - len, caplen - len, pad,
	    fcslen);
#undef BITNO_32
#undef BITNO_16
#undef BITNO_8
#undef BITNO_4
#undef BITNO_2
#undef BIT
}

static u_int
ieee802_11_radio_avs_print(netdissect_options *ndo,
			   const u_char *p, u_int length, u_int caplen)
{
	uint32_t caphdr_len;

	ndo->ndo_protocol = "802.11_radio_avs";
	if (caplen < 8) {
		nd_print_trunc(ndo);
		return caplen;
	}

	caphdr_len = GET_BE_U_4(p + 4);
	if (caphdr_len < 8) {
		/*
		 * Yow!  The capture header length is claimed not
		 * to be large enough to include even the version
		 * cookie or capture header length!
		 */
		nd_print_trunc(ndo);
		return caplen;
	}

	if (caplen < caphdr_len) {
		nd_print_trunc(ndo);
		return caplen;
	}

	return caphdr_len + ieee802_11_print(ndo, p + caphdr_len,
	    length - caphdr_len, caplen - caphdr_len, 0, 0);
}

#define PRISM_HDR_LEN		144

#define WLANCAP_MAGIC_COOKIE_BASE 0x80211000
#define WLANCAP_MAGIC_COOKIE_V1	0x80211001
#define WLANCAP_MAGIC_COOKIE_V2	0x80211002

/*
 * For DLT_PRISM_HEADER; like DLT_IEEE802_11, but with an extra header,
 * containing information such as radio information, which we
 * currently ignore.
 *
 * If, however, the packet begins with WLANCAP_MAGIC_COOKIE_V1 or
 * WLANCAP_MAGIC_COOKIE_V2, it's really DLT_IEEE802_11_RADIO_AVS
 * (currently, on Linux, there's no ARPHRD_ type for
 * DLT_IEEE802_11_RADIO_AVS, as there is a ARPHRD_IEEE80211_PRISM
 * for DLT_PRISM_HEADER, so ARPHRD_IEEE80211_PRISM is used for
 * the AVS header, and the first 4 bytes of the header are used to
 * indicate whether it's a Prism header or an AVS header).
 */
void
prism_if_print(netdissect_options *ndo,
	       const struct pcap_pkthdr *h, const u_char *p)
{
	u_int caplen = h->caplen;
	u_int length = h->len;
	uint32_t msgcode;

	ndo->ndo_protocol = "prism";
	if (caplen < 4) {
		nd_print_trunc(ndo);
		ndo->ndo_ll_hdr_len += caplen;
		return;
	}

	msgcode = GET_BE_U_4(p);
	if (msgcode == WLANCAP_MAGIC_COOKIE_V1 ||
	    msgcode == WLANCAP_MAGIC_COOKIE_V2) {
		ndo->ndo_ll_hdr_len += ieee802_11_radio_avs_print(ndo, p, length, caplen);
		return;
	}

	if (caplen < PRISM_HDR_LEN) {
		nd_print_trunc(ndo);
		ndo->ndo_ll_hdr_len += caplen;
		return;
	}

	p += PRISM_HDR_LEN;
	length -= PRISM_HDR_LEN;
	caplen -= PRISM_HDR_LEN;
	ndo->ndo_ll_hdr_len += PRISM_HDR_LEN;
	ndo->ndo_ll_hdr_len += ieee802_11_print(ndo, p, length, caplen, 0, 0);
}

/*
 * For DLT_IEEE802_11_RADIO; like DLT_IEEE802_11, but with an extra
 * header, containing information such as radio information.
 */
void
ieee802_11_radio_if_print(netdissect_options *ndo,
			  const struct pcap_pkthdr *h, const u_char *p)
{
	ndo->ndo_protocol = "802.11_radio";
	ndo->ndo_ll_hdr_len += ieee802_11_radio_print(ndo, p, h->len, h->caplen);
}

/*
 * For DLT_IEEE802_11_RADIO_AVS; like DLT_IEEE802_11, but with an
 * extra header, containing information such as radio information,
 * which we currently ignore.
 */
void
ieee802_11_radio_avs_if_print(netdissect_options *ndo,
			      const struct pcap_pkthdr *h, const u_char *p)
{
	ndo->ndo_protocol = "802.11_radio_avs";
	ndo->ndo_ll_hdr_len += ieee802_11_radio_avs_print(ndo, p, h->len, h->caplen);
}
