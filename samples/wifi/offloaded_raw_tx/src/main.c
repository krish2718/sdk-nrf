#include <zephyr/kernel.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <zephyr/drivers/wifi/nrfwifi/off_raw_tx/off_raw_tx_api.h>

uint8_t bcn_extra_fields[] = {
    0X03, 0X01, 0X06, 0X05, 0X04, 0X00, 0X01, 0X00, 0X00, 0X07, 0X06, 0X49, 0X4E, 0X04, 0X01, 0X0B,
    0X24, 0X20, 0X01, 0X00, 0X2A, 0X01, 0X04, 0X30, 0X14, 0X01, 0X00, 0X00, 0X0F, 0XAC, 0X04, 0X01,
    0X00, 0X00, 0X0F, 0XAC, 0X04, 0X01, 0X00, 0X00, 0X0F, 0XAC, 0X02, 0X28, 0X00, 0X0B, 0X05, 0X02,
    0X00, 0X67, 0X8D, 0X5B, 0X46, 0X05, 0X33, 0X00, 0X00, 0X00, 0X00, 0X2D, 0X1A, 0X21, 0X09, 0X17,
    0XFF, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X3D, 0X16, 0X06, 0X08, 0X04, 0X00, 0X00, 0X00, 0X00, 0X00,
    0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X7F,
    0X04, 0X04, 0X00, 0X08, 0X04, 0XFF, 0X1A, 0X23, 0X01, 0X00, 0X08, 0X12, 0X00, 0X10, 0X20, 0X20,
    0X02, 0XC0, 0X0F, 0X40, 0X85, 0X18, 0X00, 0X0C, 0X00, 0XFE, 0XFF, 0XFE, 0XFF, 0X38, 0X1C, 0XC7,
    0X01, 0XFF, 0X07, 0X24, 0X04, 0X00, 0X00, 0X81, 0XFC, 0XFF, 0XFF, 0X0E, 0X26, 0X08, 0X00, 0XA4,
    0X08, 0X20, 0XA4, 0X08, 0X40, 0X43, 0X08, 0X60, 0X32, 0X08, 0XDD, 0X05, 0X00, 0X40, 0X96, 0X03,
    0X05, 0XDD, 0X05, 0X00, 0X40, 0X96, 0X14, 0X00, 0XDD, 0X05, 0X00, 0X40, 0X96, 0X0B, 0X89, 0XDD,
    0X05, 0X00, 0X40, 0X96, 0X2C, 0X0E, 0XDD, 0X18, 0X00, 0X50, 0XF2, 0X02, 0X01, 0X01, 0X88, 0X00,
    0X03, 0XA4, 0X00, 0X00, 0X27, 0XA4, 0X00, 0X00, 0X42, 0X43, 0X5E, 0X00, 0X62, 0X32, 0X2F, 0X00
};

struct bcn_frame {
	void frame[NRF_WIFI_OFF_RAW_TX_FRAME_SIZE_MAX];
	uint16_t len;
} bcn;

unsigned char bssid[6];

unsigned char bcn_supp_rates[] = {0x02, 0x04, 0x0b, 0x16};


/* Example function to build a beacon frame */
static int build_wifi_beacon(unsigned short beacon_interval,
			     unsigned short cap_info,
			     const char *ssid,
			     unsigned char *extra_fields,
			     unsigned int extra_fields_len)
{
	unsigned int ssid_len = strlen(ssid);
	unsigned char pos = 0;
	unsigned char *bcn_frm = bcn.frame;
	unsigned int len = 0;

	len = sizeof(beacon_interval) + sizeof(cap_info) + ssid_len + extra_fields_len;

	if (len < NRF_WIFI_OFF_RAW_TX_FRAME_SIZE_MIN) {
		printf("Beacon frame exceeds maximum size\n");
		return -1;
	}

	if (len > NRF_WIFI_OFF_RAW_TX_FRAME_SIZE_MAX) {
		printf("Beacon frame exceeds maximum size\n");
		return 0;
	}

	/* Frame Control */
	bcn_frm[pos++] = 0x80; /* Beacon frame */
	bcn_frm[pos++] = 0x00;

	/* Duration */
	bcn_frm[pos++] = 0x00;
	bcn_frm[pos++] = 0x00;

	/* Destination Address (Broadcast) */
	memset(&bcn_frm[pos], 0xff, 6);
	pos += 6;

	if(nrf_wifi_off_raw_tx_get_mac_addr(bssid)) {
		printf("Failed to get MAC address\n");
		return -1;
	}

	/* Source Address */
	memcpy(&bcn_frm[pos], bssid, 6);
	pos += 6;

	/* BSSID */
	memcpy(&bcn_frm[pos], bssid, 6);
	pos += 6;

	/* Sequence Control */
	bcn_frm[pos++] = 0x00;
	bcn_frm[pos++] = 0x00;

	/* Timestamp */
	memset(&bcn_frm[pos], 0x00, 8);
	pos += 8;

	/* Beacon Interval */
	bcn_frm[pos++] = beacon_interval & 0xff;
	bcn_frm[pos++] = (beacon_interval >> 8) & 0xff;

	/* Capability Information */
	bcn_frm[pos++] = cap_info & 0xff;
	bcn_frm[pos++] = (cap_info >> 8) & 0xff;

	/* SSID Parameter Set */
	bcn_frm[pos++] = 0x00; // SSID Element ID
	bcn_frm[pos++] = ssid_len; // SSID Length
	memcpy(&bcn_frm[pos], ssid, ssid_len);
	pos += ssid_len;

	/* Extra fields */
	memcpy(&bcn_frm[pos], extra_fields, extra_fields_len);
	pos += extra_fields_len;

	return pos;
}


int main(void)
{
	struct nrf_wifi_off_raw_tx_conf conf;
	int len = -1;

	printf("----- Initializing nRF70 -----\n");
	nrf_wifi_off_raw_tx_init(NULL);

	/* Build a beacon frame */
	len = build_wifi_beacon(100,
				0x431,
				"nRF70_off_raw_tx_1",
				bcn_extra_fields,
				sizeof(bcn_extra_fields));

	memset(&conf, 0, sizeof(conf));
	conf.period_us = 100000;
	conf.tx_pwr = 15;
	conf.chan = 1;
	conf.short_preamble = false;
	conf.num_retries = 10;
	conf.tput_mode = 0;
	conf.rate = 54;
	conf.he_gi = 1;
	conf.he_ltf = 1;

	printf("----- Starting to transmit beacons with the following configuration -----\n");
	printf("\tSSID: nRF70_off_raw_tx_1\n");
	printf("\tPeriod: %d\n", conf.period_us);
	printf("\tTX Power: %d\n", conf.tx_pwr);
	printf("\tChannel: %d\n", conf.chan);
	printf("\tShort Preamble: %d\n", conf.short_preamble);
	printf("\tNumber of Retries: %d\n", conf.num_retries);
	printf("\tThroughput Mode: %d\n", conf.tput_mode);
	printf("\tRate: %d\n", conf.rate);
	printf("\tHE GI: %d\n", conf.he_gi);
	printf("\tHE LTF: %d\n", conf.he_ltf);
	nrf70_off_raw_tx_start(&conf);

	k_sleep(K_SECONDS(10));

	/* Build a beacon frame */
	len = build_wifi_beacon(100,
				0x431,
				"nRF70_off_raw_tx_2",
				bcn_extra_fields,
				sizeof(bcn_extra_fields));
	conf.period_us = 50000;
	conf.tx_pwr = 11;
	conf.chan = 36;
	conf.short_preamble = false;
	conf.num_retries = 10;
	conf.rate = 12;

	printf("----- Updating configuration to -----\n");
	printf("\tSSID: nRF70_off_raw_tx_2\n");
	printf("\tPeriod: %d\n", conf.period_us);
	printf("\tTX Power: %d\n", conf.tx_pwr);
	printf("\tChannel: %d\n", conf.chan);
	printf("\tShort Preamble: %d\n", conf.short_preamble);
	printf("\tNumber of Retries: %d\n", conf.num_retries);
	printf("\tThroughput Mode: %d\n", conf.tput_mode);
	printf("\tRate: %d\n", conf.rate);
	printf("\tHE GI: %d\n", conf.he_gi);
	printf("\tHE LTF: %d\n", conf.he_ltf);
	nrf70_off_raw_tx_conf(&conf);

	k_sleep(K_SECONDS(10));

	printf("----- Stopping transmission -----\n");
	nrf70_off_raw_tx_stop();

	printf("----- Deinitializing nRF70 -----\n");
	nrf_wifi_off_raw_tx_deinit();

	return 0;
}
