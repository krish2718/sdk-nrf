/****************************************************************************
Copyright (c) 2016 Wi-Fi Alliance.  All Rights Reserved

Permission to use, copy, modify, and/or distribute this software for any purpose with or
without fee is hereby granted, provided that the above copyright notice and this permission
notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING
FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH
THE USE OR PERFORMANCE OF THIS SOFTWARE.

******************************************************************************/

/*
 *   File: wfa_cs.c -- configuration and setup
 *   This file contains all implementation for the dut setup and control
 *   functions, such as network interfaces, ip address and wireless specific
 *   setup with its supplicant.
 *
 *   The current implementation is to show how these functions
 *   should be defined in order to support the Agent Control/Test Manager
 *   control commands. To simplify the current work and avoid any GPL licenses,
 *   the functions mostly invoke shell commands by calling linux system call,
 *   system("<commands>").
 *
 *   It depends on the differnt device and platform, vendors can choice their
 *   own ways to interact its systems, supplicants and process these commands
 *   such as using the native APIs.
 *
 *
 */
#include <stdio.h>
#include <math.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <zephyr/posix/sys/socket.h>
#include <icmpv4.h>
#include <arpa/inet.h>
#include <zephyr/types.h>
#include <zephyr/net/socket.h>
#include <poll.h>

#include "wfa_portall.h"
#include "wfa_debug.h"
#include "wfa_ver.h"
#include "wfa_main.h"
#include "wfa_types.h"
#include "wfa_ca.h"
#include "wfa_tlv.h"
#include "wfa_sock.h"
#include "wfa_tg.h"
#include "wfa_cmds.h"
#include "wfa_rsp.h"
#include "wfa_utils.h"
#ifdef WFA_WMM_PS_EXT
#include "wfa_wmmps.h"
#endif

#include <src/utils/common.h>
#include <wpa_supplicant/config.h>
#include <wpa_supplicant/wpa_supplicant_i.h>
#define CERTIFICATES_PATH    "/etc/wpa_supplicant"

/* For net_mgmt commands */
#include <zephyr/net/net_if.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/net_event.h>
#include "icmpv4.h"

#include "zephyr_fmac_main.h"
extern struct wifi_nrf_drv_priv_zep rpu_drv_priv_zep;

/* Some device may only support UDP ECHO, activate this line */
//#define WFA_PING_UDP_ECHO_ONLY 1

#define WFA_ENABLED 1
int stmp;
int count_seq;
extern struct wpa_global *global;
extern unsigned short wfa_defined_debug;
int wfaExecuteCLI(char *CLI);

/* Since the two definitions are used all over the CA function */
char gCmdStr[WFA_CMD_STR_SZ];
dutCmdResponse_t gGenericResp;
//int wfaTGSetPrio(int sockfd, int tgClass);
void create_apts_msg(int msg, unsigned int txbuf[],int id);

int sret = 0;

extern char *chan_buf1;
extern char *chan_buf2;

extern char e2eResults[];

K_SEM_DEFINE(sem_scan, 0, 1);
#define SCAN_RESULTS_TIMEOUT 10
#define MAX_SCAN_RESULT_SIZE 256
char scan_results_buffer[MAX_SCAN_RESULT_SIZE];
size_t scan_result_index = 0;
size_t scan_result_offset = 0;

static enum net_verdict handle_ipv4_echo_reply(struct net_pkt *pkt,
					       struct net_ipv4_hdr *ip_hdr,
					       struct net_icmp_hdr *icmp_hdr);

static struct net_icmpv4_handler ping4_handler = {
	.type = NET_ICMPV4_ECHO_REPLY,
	.code = 0,
	.handler = handle_ipv4_echo_reply,
};

static inline void remove_ipv4_ping_handler(void)
{
	net_icmpv4_unregister_handler(&ping4_handler);
}

static enum net_verdict handle_ipv4_echo_reply(struct net_pkt *pkt,
					       struct net_ipv4_hdr *ip_hdr,
					       struct net_icmp_hdr *icmp_hdr)
{
	NET_PKT_DATA_ACCESS_CONTIGUOUS_DEFINE(icmp_access,
					      struct net_icmpv4_echo_req);
	uint32_t cycles;
	struct net_icmpv4_echo_req *icmp_echo;

	icmp_echo = (struct net_icmpv4_echo_req *)net_pkt_get_data(pkt,
								&icmp_access);
	if (icmp_echo == NULL) {
		return -NET_DROP;
	}

	net_pkt_skip(pkt, sizeof(*icmp_echo));

	if (net_pkt_remaining_data(pkt) >= sizeof(uint32_t)) {
		if (net_pkt_read_be32(pkt, &cycles)) {
			return -NET_DROP;
		}
		cycles = k_cycle_get_32() - cycles;
	}

    count_seq = ntohs(icmp_echo->sequence);

	net_pkt_unref(pkt);
	return NET_OK;
}


FILE *e2efp = NULL;
int chk_ret_status()
{
    char *ret = getenv(WFA_RET_ENV);

    if(*ret == '1')
        return WFA_SUCCESS;
    else
        return WFA_FAILURE;
}

/*
 * agtCmdProcGetVersion(): response "ca_get_version" command to controller
 *  input:  cmd --- not used
 *          valLen -- not used
 *  output: parms -- a buffer to store the version info response.
 */
int agtCmdProcGetVersion(int len, BYTE *parms, int *respLen, BYTE *respBuf)
{
    dutCmdResponse_t *getverResp = &gGenericResp;

    DPRINT_INFO(WFA_OUT, "entering agtCmdProcGetVersion ...\n");

    getverResp->status = STATUS_COMPLETE;
    wSTRNCPY(getverResp->cmdru.version, WFA_SYSTEM_VER, WFA_VERNAM_LEN);

    wfaEncodeTLV(WFA_GET_VERSION_RESP_TLV, sizeof(dutCmdResponse_t), (BYTE *)getverResp, respBuf);

    *respLen = WFA_TLV_HDR_LEN + sizeof(dutCmdResponse_t);

    return WFA_SUCCESS;
}

/*
 * wfaStaAssociate():
 *    The function is to force the station wireless I/F to re/associate
 *    with the AP.
 */
int wfaStaAssociate(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    dutCommand_t *assoc = (dutCommand_t *)caCmdBuf;
    caStaAssociate_t *setassoc = &assoc->cmdsu.assoc;
    char *ifname = assoc->intf;
    dutCmdResponse_t *staAssocResp = &gGenericResp;
	int ret;
    DPRINT_INFO(WFA_OUT, "entering wfaStaAssociate ...\n");
    /*
     * if bssid appears, station should associate with the specific
     * BSSID AP at its initial association.
     * If it is different to the current associating AP, it will be forced to
     * roam the new AP
     */
    if(assoc->cmdsu.assoc.bssid[0] != '\0')
    {
        /* if (the first association) */
        /* just do initial association to the BSSID */


        /* else (station already associate to an AP) */
        /* Do forced roaming */

    }
    else
    {
        /* use 'ifconfig' command to bring down the interface (linux specific) */
     //	sprintf(gCmdStr, "ifconfig %s down", ifname);
      //	sret = shell_execute_cmd(NULL,gCmdStr);

        /* use 'ifconfig' command to bring up the interface (linux specific) */
      	//sprintf(gCmdStr, "ifconfig %s up", ifname);
        //sret = shell_execute_cmd(NULL,gCmdStr);

        /*
         *  use 'wpa_cli' command to force a 802.11 re/associate
         *  (wpa_supplicant specific)
         */
//sprintf(gCmdStr, "wpa_cli -i%s select_network 0", ifname);
  //     sret = shell_execute_cmd(NULL,gCmdStr);
    }
   //sprintf(gCmdStr, "wpa_cli select_network 0", ifname);
//		printf("\n %s \n", gCmdStr);
  // sret = shell_execute_cmd(NULL,gCmdStr);
   //sleep(2);

    /*
     * Then report back to control PC for completion.
     * This does not have failed/error status. The result only tells
     * a completion.
     */
    int k;
    k=strlen(setassoc->bssid);
    printf("\n%d\n",k);
    if(k!='\0')
    {
	sprintf(gCmdStr, "wpa_cli bssid 0 '\"%s\"'", setassoc->bssid);
	ret = shell_execute_cmd(NULL, gCmdStr);
    	printf("\n %s \n", gCmdStr);
    }
    ret = shell_execute_cmd(NULL, "wifi reg_domain US -f");
    ret = shell_execute_cmd(NULL, "wpa_cli select_network 0");
    staAssocResp->status = STATUS_COMPLETE;
    wfaEncodeTLV(WFA_STA_ASSOCIATE_RESP_TLV, 4, (BYTE *)staAssocResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + 4;

    return WFA_SUCCESS;
}

/*
 * wfaStaReAssociate():
 *    The function is to force the station wireless I/F to re/associate
 *    with the AP.
 */
int wfaStaReAssociate(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    dutCommand_t *assoc = (dutCommand_t *)caCmdBuf;
    char *ifname = assoc->intf;
    dutCmdResponse_t *staAssocResp = &gGenericResp;
	 int ret;
    DPRINT_INFO(WFA_OUT, "entering wfaStaAssociate ...\n");
    /*
     * if bssid appears, station should associate with the specific
     * BSSID AP at its initial association.
     * If it is different to the current associating AP, it will be forced to
     * roam the new AP
     */

    if(assoc->cmdsu.assoc.bssid[0] != '\0')
    {
        /* if (the first association) */
        /* just do initial association to the BSSID */


        /* else (station already associate to an AP) */
        /* Do forced roaming */

    }
    else
    {
        /* use 'ifconfig' command to bring down the interface (linux specific) */
        //sprintf(gCmdStr, "ifconfig %s down", ifname);
        //sret = shell_execute_cmd(NULL,gCmdStr);

        /* use 'ifconfig' command to bring up the interface (linux specific) */
        //sprintf(gCmdStr, "ifconfig %s up", ifname);

        /*
         *  use 'wpa_cli' command to force a 802.11 re/associate
         *  (wpa_supplicant specific)
         */
        //sprintf(gCmdStr, "wpa_cli -i%s reassociate", ifname);
        //sret = shell_execute_cmd(NULL,gCmdStr);
    }

	ret = shell_execute_cmd(NULL, "wpa_cli reassociate");
    /*
     * Then report back to control PC for completion.
     * This does not have failed/error status. The result only tells
     * a completion.
     */
    staAssocResp->status = STATUS_COMPLETE;
    wfaEncodeTLV(WFA_STA_ASSOCIATE_RESP_TLV, 4, (BYTE *)staAssocResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + 4;

    return WFA_SUCCESS;
}

/*
 * wfaStaIsConnected():
 *    The function is to check whether the station's wireless I/F has
 *    already connected to an AP.
 */
int wfaStaIsConnected(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    dutCommand_t *connStat = (dutCommand_t *)caCmdBuf;
    dutCmdResponse_t *staConnectResp = &gGenericResp;
    char *ifname = connStat->intf;
    FILE *tmpfile = NULL;
    char result[32];
    struct wpa_supplicant *wpa_s;
	int ret;
    DPRINT_INFO(WFA_OUT, "Entering isConnected ...\n");

#ifdef WFA_NEW_CLI_FORMAT
    sprintf(gCmdStr, "wfa_chkconnect %s\n", ifname);
    sret = shell_execute_cmd(NULL,gCmdStr);

    if(chk_ret_status() == WFA_SUCCESS)
        staConnectResp->cmdru.connected = 1;
    else
        staConnectResp->cmdru.connected = 0;
#else
    /*
     * use 'wpa_cli' command to check the interface status
     * none, scanning or complete (wpa_supplicant specific)
     */
	ret = shell_execute_cmd(NULL, "wpa_cli status");
	wpa_s = wpa_supplicant_get_iface(global, ifname);
	if (!wpa_s) {
		printf("Unable to find the interface: %s, quitting", ifname);
		return -1;
	}
	ret = os_snprintf(result, 9,"%s",wpa_supplicant_state_txt(wpa_s->wpa_state));
    /*
     * the status is saved in a file.  Open the file and check it.
     */
    if(strncmp(result, "COMPLETE", 9) == 0)
        staConnectResp->cmdru.connected = 1;
    else
        staConnectResp->cmdru.connected = 0;
#endif

    /*
     * Report back the status: Complete or Failed.
     */
    staConnectResp->status = STATUS_COMPLETE;

    wfaEncodeTLV(WFA_STA_IS_CONNECTED_RESP_TLV, sizeof(dutCmdResponse_t), (BYTE *)staConnectResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + sizeof(dutCmdResponse_t);

    return WFA_SUCCESS;
}

/*
 * wfaStaGetIpConfig():
 * This function is to retriev the ip info including
 *     1. dhcp enable
 *     2. ip address
 *     3. mask
 *     4. primary-dns
 *     5. secondary-dns
 *
 *     The current implementation is to use a script to find these information
 *     and store them in a file.
 */
int wfaStaGetIpConfig(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    int slen, ret, i = 0,count = 0;
    dutCommand_t *getIpConf = (dutCommand_t *)caCmdBuf;
    dutCmdResponse_t *ipconfigResp = &gGenericResp;
    char *ifname = getIpConf->intf;
    caStaGetIpConfigResp_t *ifinfo = &ipconfigResp->cmdru.getIfconfig;
	char tmp[30];
	char string_ip[30];
	struct wpa_supplicant *wpa_s;

    DPRINT_INFO(WFA_OUT, "Entering GetIpConfig...\n");
		printf("interface %s\n", ifname);

	struct net_if *iface;
	struct net_if_ipv4 *ipv4;

	iface = net_if_get_by_index(0);
	printf("\nInterface %p \n", iface);
	ipv4 = iface->config.ip.ipv4;
	printf("IPv4 unicast addresses (max %d):\n", NET_IF_MAX_IPV4_ADDR);
	/*
        for (i = 0; ipv4 && i < NET_IF_MAX_IPV4_ADDR; i++) {
                unicast = &ipv4->unicast[i];

                if (!unicast->is_used) {
                        continue;
                }

	printf("IPv4 unicast addresses %s:\n", &unicast->address.in_addr);

        }*/
	wpa_s = wpa_supplicant_get_iface(global, ifname);
        if (!wpa_s) {
                printf("Unable to find the interface: %s, quitting", ifname);
                return -1;
        }
	if (wpa_s->l2 && l2_packet_get_ip_addr(wpa_s->l2, tmp, sizeof(tmp)) >= 0) {
              ret = os_snprintf(string_ip,sizeof(string_ip), "%s", tmp);
		printf("IP ADDRESS :%s\n", string_ip);
	}
            if(string_ip != NULL)
            {
                wSTRNCPY(ifinfo->ipaddr, string_ip,15);

                ifinfo->ipaddr[15]='\0';
            }
            else
                wSTRNCPY(ifinfo->ipaddr, "none", 15);
        
    strcpy(ifinfo->dns[0], "0");
    strcpy(ifinfo->dns[1], "0");



#if 0
    FILE *tmpfd;
    char string[256];
    char *str;

    /*
     * check a script file (the current implementation specific)
     */
    ret = access("/usr/local/sbin/getipconfig.sh", F_OK);
    if(ret == -1)
    {
        ipconfigResp->status = STATUS_ERROR;
        wfaEncodeTLV(WFA_STA_GET_IP_CONFIG_RESP_TLV, 4, (BYTE *)ipconfigResp, respBuf);
        *respLen = WFA_TLV_HDR_LEN + 4;

        DPRINT_ERR(WFA_ERR, "file not exist\n");
        return WFA_FAILURE;

    }

    strcpy(ifinfo->dns[0], "0");
    strcpy(ifinfo->dns[1], "0");

    /*
     * Run the script file "getipconfig.sh" to check the ip status
     * (current implementation  specific).
     * note: "getipconfig.sh" is only defined for the current implementation
     */
    sprintf(gCmdStr, "getipconfig.sh /tmp/ipconfig.txt %s\n", ifname);

    sret = shell_execute_cmd(NULL,gCmdStr);

    /* open the output result and scan/retrieve the info */
    tmpfd = fopen("/tmp/ipconfig.txt", "r+");

    if(tmpfd == NULL)
    {
        ipconfigResp->status = STATUS_ERROR;
        wfaEncodeTLV(WFA_STA_GET_IP_CONFIG_RESP_TLV, 4, (BYTE *)ipconfigResp, respBuf);
        *respLen = WFA_TLV_HDR_LEN + 4;

        DPRINT_ERR(WFA_ERR, "file open failed\n");
        return WFA_FAILURE;
    }

    for(;;)
    {
        if(fgets(string, 256, tmpfd) == NULL)
            break;

        /* check dhcp enabled */
        if(strncmp(string, "dhcpcli", 7) ==0)
        {
            str = strtok(string, "=");
            str = strtok(NULL, "=");
            if(str != NULL)
                ifinfo->isDhcp = 1;
            else
                ifinfo->isDhcp = 0;
        }

        /* find out the ip address */
        if(strncmp(string, "ipaddr", 6) == 0)
        {
            str = strtok(string, "=");
            str = strtok(NULL, " ");
            if(str != NULL)
            {
                wSTRNCPY(ifinfo->ipaddr, str, 15);

                ifinfo->ipaddr[15]='\0';
            }
            else
                wSTRNCPY(ifinfo->ipaddr, "none", 15);
        }

        /* check the mask */
        if(strncmp(string, "mask", 4) == 0)
        {
            char ttstr[16];
            char *ttp = ttstr;

            str = strtok_r(string, "=", &ttp);
            if(*ttp != '\0')
            {
                strcpy(ifinfo->mask, ttp);
                slen = strlen(ifinfo->mask);
                ifinfo->mask[slen-1] = '\0';
            }
            else
                strcpy(ifinfo->mask, "none");
        }

        /* find out the dns server ip address */
        if(strncmp(string, "nameserv", 8) == 0)
        {
            char ttstr[16];
            char *ttp = ttstr;

            str = strtok_r(string, " ", &ttp);
            if(str != NULL && i < 2)
            {
                strcpy(ifinfo->dns[i], ttp);
                slen = strlen(ifinfo->dns[i]);
                ifinfo->dns[i][slen-1] = '\0';
            }
            else
                strcpy(ifinfo->dns[i], "none");

            i++;
        }
    }

    /*
     * Report back the results
     */
    ipconfigResp->status = STATUS_COMPLETE;
    wfaEncodeTLV(WFA_STA_GET_IP_CONFIG_RESP_TLV, sizeof(dutCmdResponse_t), (BYTE *)ipconfigResp, respBuf);

    *respLen = WFA_TLV_HDR_LEN + sizeof(dutCmdResponse_t);

#if 0
    DPRINT_INFO(WFA_OUT, "%i %i %s %s %s %s %i\n", ipconfigResp->status,
                ifinfo->isDhcp, ifinfo->ipaddr, ifinfo->mask,
                ifinfo->dns[0], ifinfo->dns[1], *respLen);
#endif

    fclose(tmpfd);
#endif
    ipconfigResp->status = STATUS_COMPLETE;
    wfaEncodeTLV(WFA_STA_GET_IP_CONFIG_RESP_TLV, sizeof(dutCmdResponse_t), (BYTE *)ipconfigResp, respBuf);

    *respLen = WFA_TLV_HDR_LEN + sizeof(dutCmdResponse_t);
    return WFA_SUCCESS;
}

/*
 * wfaStaSetIpConfig():
 *   The function is to set the ip configuration to a wireless I/F.
 *   1. IP address
 *   2. Mac address
 *   3. default gateway
 *   4. dns nameserver (pri and sec).
 */
int wfaStaSetIpConfig(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    dutCommand_t *setIpConf = (dutCommand_t *)caCmdBuf;
    caStaSetIpConfig_t *ipconfig = &setIpConf->cmdsu.ipconfig;
    dutCmdResponse_t *staSetIpResp = &gGenericResp;
    struct in_addr addr;

    
    const struct device *dev = device_get_binding("wlan0");
    struct net_if *wlan_iface = net_if_lookup_by_dev(dev);
    
    DPRINT_INFO(WFA_OUT, "entering wfaStaSetIpConfig ...\n");

    if (!wlan_iface) {
		printk("Cannot find network interface: %s", dev->name);
		goto err;
	}

    if (ipconfig->isDhcp) {
        printk("Runtime DHCP client is not supported");
        goto err;
    }

	if (sizeof(ipconfig->ipaddr) > 1 && sizeof(ipconfig->mask) > 1) {
		if (net_addr_pton(AF_INET, ipconfig->ipaddr, &addr)) {
			printk("Invalid address: %s", ipconfig->ipaddr);
            goto err;
		}
		net_if_ipv4_addr_add(wlan_iface, &addr, NET_ADDR_MANUAL, 0);

		/* If not empty */
		if (net_addr_pton(AF_INET, ipconfig->mask, &addr)) {
			printk("Invalid netmask: %s", ipconfig->mask);
            goto err;
		} else {
			net_if_ipv4_set_netmask(wlan_iface, &addr);
		}
    } else {
        printk("Address or netmask not configured");
        goto err;
    }

    /*
     * Use command 'ifconfig' to configure the interface ip address, mask.
     * (Linux specific).
     *
    *sprintf(gCmdStr, "/sbin/ifconfig %s %s netmask %s > /dev/null 2>&1 ", ipconfig->intf, ipconfig->ipaddr, ipconfig->mask);
     * sret = shell_execute_cmd(NULL,gCmdStr);
     * 
     * use command 'route add' to set set gatewway (linux specific) */
    /* if(ipconfig->defGateway[0] != '\0')
    {
        sprintf(gCmdStr, "/sbin/route add default gw %s > /dev/null 2>&1", ipconfig->defGateway);
        sret = shell_execute_cmd(NULL,gCmdStr);
    }
 */
    /* set dns (linux specific) */
    /* sprintf(gCmdStr, "cp /etc/resolv.conf /tmp/resolv.conf.bk");
    sret = shell_execute_cmd(NULL,gCmdStr);
    sprintf(gCmdStr, "echo nameserv %s > /etc/resolv.conf", ipconfig->pri_dns);
    sret = shell_execute_cmd(NULL,gCmdStr);
    sprintf(gCmdStr, "echo nameserv %s >> /etc/resolv.conf", ipconfig->sec_dns);
     sret = shell_execute_cmd(NULL,gCmdStr);
	*/
    /*
     * report status
     */

    staSetIpResp->status = STATUS_COMPLETE;
    wfaEncodeTLV(WFA_STA_SET_IP_CONFIG_RESP_TLV, 4, (BYTE *)staSetIpResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + 4;
    return WFA_SUCCESS;

err:
    staSetIpResp->status = STATUS_ERROR;
    wfaEncodeTLV(WFA_STA_SET_IP_CONFIG_RESP_TLV, 4, (BYTE *)staSetIpResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + 4;
    return WFA_FAILURE;
}

/*
 * wfaStaVerifyIpConnection():
 * The function is to verify if the station has IP connection with an AP by
 * send ICMP/pings to the AP.
 */
int wfaStaVerifyIpConnection(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    dutCommand_t *verip = (dutCommand_t *)caCmdBuf;
    dutCmdResponse_t *verifyIpResp = &gGenericResp;

#ifndef WFA_PING_UDP_ECHO_ONLY
    char strout[64], *pcnt;
    FILE *tmpfile;

    DPRINT_INFO(WFA_OUT, "Entering wfaStaVerifyIpConnection ...\n");

    /* set timeout value in case not set */
    if(verip->cmdsu.verifyIp.timeout <= 0)
    {
        verip->cmdsu.verifyIp.timeout = 10;
    }

    /* execute the ping command  and pipe the result to a tmp file */
#if 1
    sprintf(gCmdStr, "ping %s -c 3 -W %u | grep loss | cut -f3 -d, 1>& /tmp/pingout.txt", verip->cmdsu.verifyIp.dipaddr, verip->cmdsu.verifyIp.timeout);
    sret = shell_execute_cmd(NULL,gCmdStr);

    /* scan/check the output */
    tmpfile = fopen("/tmp/pingout.txt", "r+");
    if(tmpfile == NULL)
    {
        verifyIpResp->status = STATUS_ERROR;
        wfaEncodeTLV(WFA_STA_VERIFY_IP_CONNECTION_RESP_TLV, 4, (BYTE *)verifyIpResp, respBuf);
        *respLen = WFA_TLV_HDR_LEN + 4;

        DPRINT_ERR(WFA_ERR, "file open failed\n");
        return WFA_FAILURE;
    }
#endif
    verifyIpResp->status = STATUS_COMPLETE;
    if(fscanf(tmpfile, "%s", strout) == EOF)
        verifyIpResp->cmdru.connected = 0;
    else
    {
        pcnt = strtok(strout, "%");

        /* if the loss rate is 100%, not able to connect */
        if(atoi(pcnt) == 100)
            verifyIpResp->cmdru.connected = 0;
        else
            verifyIpResp->cmdru.connected = 1;
    }

    fclose(tmpfile);
#else
    int btSockfd;
    struct pollfd fds[2];
    int timeout = 2000;
    char anyBuf[64];
    struct sockaddr_in toAddr;
    int done = 1, cnt = 0, ret, nbytes;

    verifyIpResp->status = STATUS_COMPLETE;
    verifyIpResp->cmdru.connected = 0;

    btSockfd = wfaCreateUDPSock("127.0.0.1", WFA_UDP_ECHO_PORT);

    if(btSockfd == -1)
    {
        verifyIpResp->status = STATUS_ERROR;
        wfaEncodeTLV(WFA_STA_VERIFY_IP_CONNECTION_RESP_TLV, 4, (BYTE *)verifyIpResp, respBuf);
        *respLen = WFA_TLV_HDR_LEN + 4;
        return WFA_FAILURE;;
    }

    toAddr.sin_family = AF_INET;
    toAddr.sin_addr.s_addr = inet_addr(verip->cmdsu.verifyIp.dipaddr);
    toAddr.sin_port = htons(WFA_UDP_ECHO_PORT);

    while(done)
    {
        wfaTrafficSendTo(btSockfd, (char *)anyBuf, 64, (struct sockaddr *)&toAddr);
        cnt++;

        fds[0].fd = btSockfd;
        fds[0].events = POLLIN | POLLOUT;

        ret = poll(fds, 1, timeout);
        switch(ret)
        {
        case 0:
            /* it is time out, count a packet lost*/
            break;
        case -1:
        /* it is an error */
        default:
        {
            switch(fds[0].revents)
            {
            case POLLIN:
            case POLLPRI:
            case POLLOUT:
		nbytes = wfaTrafficRecv(btSockfd, NULL, (char *)anyBuf, (struct sockaddr *)&toAddr);
                if(nbytes != 0)
                    verifyIpResp->cmdru.connected = 1;
                done = 0;
                break;
            default:
                /* errors but not care */
                ;
            }
        }
        }
        if(cnt == 3)
        {
            done = 0;
        }
    }

#endif

    wfaEncodeTLV(WFA_STA_VERIFY_IP_CONNECTION_RESP_TLV, sizeof(dutCmdResponse_t), (BYTE *)verifyIpResp, respBuf);

    *respLen = WFA_TLV_HDR_LEN + sizeof(dutCmdResponse_t);

    return WFA_SUCCESS;
}

/*
 * wfaStaGetMacAddress()
 *    This function is to retrieve the MAC address of a wireless I/F.
 */
int wfaStaGetMacAddress(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
	
    dutCommand_t *getMac = (dutCommand_t *)caCmdBuf;
    dutCmdResponse_t *getmacResp = &gGenericResp;
    char *ifname = getMac->intf;
	int mac_addr_len ;
	int idx = 1, ret;
	static char mac_buf[sizeof("%02x:%02x:%02x:%02x:%02x:%02x")];
    DPRINT_INFO(WFA_OUT, "Entering wfaStaGetMacAddress ...\n");
                
	struct wpa_supplicant *wpa_s;

	wpa_s = wpa_supplicant_get_iface(global, ifname);
	if (!wpa_s) {
		printf("Unable to find the interface: %s, quitting", ifname);
		return -1;
	}
	ret = os_snprintf(mac_buf,sizeof(mac_buf), "" MACSTR "\n",MAC2STR(wpa_s->own_addr));
		printf("***************MAC BUF SUPP = %s\n",mac_buf);

    		printf("%s:MAC ADDRESS mac buf = %s size = %d\n",__func__,mac_buf,sizeof(mac_buf));
    		printf("%s:MAC ADDRESS = %s\n",__func__,getmacResp->cmdru.mac);


	strcpy(getmacResp->cmdru.mac, mac_buf);
        getmacResp->status = STATUS_COMPLETE;



    wfaEncodeTLV(WFA_STA_GET_MAC_ADDRESS_RESP_TLV, sizeof(dutCmdResponse_t), (BYTE *)getmacResp, respBuf);

    *respLen = WFA_TLV_HDR_LEN + sizeof(dutCmdResponse_t);

    return WFA_SUCCESS;
}

/*
 * wfaStaGetStats():
 * The function is to retrieve the statistics of the I/F's layer 2 txFrames,
 * rxFrames, txMulticast, rxMulticast, fcsErrors/crc, and txRetries.
 * Currently there is not definition how to use these info.
 */
int wfaStaGetStats(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    dutCmdResponse_t *statsResp = &gGenericResp;

    /* this is never used, you can skip this call */

    statsResp->status = STATUS_ERROR;
    wfaEncodeTLV(WFA_STA_GET_STATS_RESP_TLV, sizeof(dutCmdResponse_t), (BYTE *)statsResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + sizeof(dutCmdResponse_t);


    return WFA_SUCCESS;
}

/*
 * wfaSetEncryption():
 *   The function is to set the wireless interface with WEP or none.
 *
 *   Since WEP is optional test, current function is only used for
 *   resetting the Security to NONE/Plaintext (OPEN). To test WEP,
 *   this function should be replaced by the next one (wfaSetEncryption1())
 *
 *   Input parameters:
 *     1. I/F
 *     2. ssid
 *     3. encpType - wep or none
 *     Optional:
 *     4. key1
 *     5. key2
 *     6. key3
 *     7. key4
 *     8. activeKey Index
 */

int wfaSetEncryption1(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    caStaSetEncryption_t *setEncryp = (caStaSetEncryption_t *)caCmdBuf;
    dutCmdResponse_t *setEncrypResp = &gGenericResp;
	int ret;
    /*
     * disable the network first
     */
	ret = shell_execute_cmd(NULL, "wpa_cli add_network 0");
	ret = shell_execute_cmd(NULL, "wpa_cli add disable_network 0");

    /*
     * set SSID
     */
	sprintf(gCmdStr, "wpa_cli set_network 0 ssid '\"%s\"'", setEncryp->ssid);
	ret = shell_execute_cmd(NULL, gCmdStr);

    /*
     * Tell the supplicant for infrastructure mode (1)
     */
	ret = shell_execute_cmd(NULL, "wpa_cli set_network 0 mode 0");

    /*
     * set Key management to NONE (NO WPA) for plaintext or WEP
     */
	ret = shell_execute_cmd(NULL, "wpa_cli set_network 0 key_mgmt NONE");

     //IMG
	ret = shell_execute_cmd(NULL, "wpa_cli sta_autoconnect 1");
	ret = shell_execute_cmd(NULL, "wpa_cli enable_network 0");


    setEncrypResp->status = STATUS_COMPLETE;
    wfaEncodeTLV(WFA_STA_SET_ENCRYPTION_RESP_TLV, 4, (BYTE *)setEncrypResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + 4;

    return WFA_SUCCESS;
}

/*
 *  Since WEP is optional, this function could be used to replace
 *  wfaSetEncryption() if necessary.
 */
int wfaSetEncryption(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    caStaSetEncryption_t *setEncryp = (caStaSetEncryption_t *)caCmdBuf;
    dutCmdResponse_t *setEncrypResp = &gGenericResp;
    int i,ret;
	
    /*
     * disable the network first
     */

	ret = shell_execute_cmd(NULL, "wpa_cli add_network 0");
	ret = shell_execute_cmd(NULL, "wpa_cli disable_network 0");

    /*
     * set SSID
     */
	sprintf(gCmdStr, "wpa_cli set_network 0 ssid '\"%s\"'", setEncryp->ssid);
	ret = shell_execute_cmd(NULL, gCmdStr);


    /*
     * Tell the supplicant for infrastructure mode (1)
     */
	ret = shell_execute_cmd(NULL, "wpa_cli set_network 0 mode 0");

    /*
     * set Key management to NONE (NO WPA) for plaintext or WEP
     */
	ret = shell_execute_cmd(NULL, "wpa_cli set_network 0 key_mgmt NONE");

    /* set keys */
    if(setEncryp->encpType == 1)
    {
        for(i=0; i<4; i++)
        {
            if(setEncryp->keys[i][0] != '\0')
            {
		sprintf(gCmdStr, "wpa_cli set_network 0 ssid '\"%s\"'", setEncryp->ssid);
		ret = shell_execute_cmd(NULL, gCmdStr);
            }
        }

        /* set active key */
        i = setEncryp->activeKeyIdx;
        if(setEncryp->keys[i][0] != '\0')
        {
		sprintf(gCmdStr, "wpa_cli set_network 0 wep_tx_keyid %i",i, setEncryp->activeKeyIdx);
		ret = shell_execute_cmd(NULL, gCmdStr);
        }
    }
    else /* clearly remove the keys -- reported by p.schwann */
    {

        for(i = 0; i < 4; i++)
        {
            	sprintf(gCmdStr, "wpa_cli set_network 0 wep_key %i ", i);
		ret = shell_execute_cmd(NULL, gCmdStr);
        }
    }

     //IMG
	ret = shell_execute_cmd(NULL, "wpa_cli set_network 0 scan_ssid 1 ");
	ret = shell_execute_cmd(NULL, "wpa_cli sta_autoconnect 1");
//	ret = shell_execute_cmd(NULL, "wpa_cli select_network 0");



    setEncrypResp->status = STATUS_COMPLETE;
    wfaEncodeTLV(WFA_STA_SET_ENCRYPTION_RESP_TLV, 4, (BYTE *)setEncrypResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + 4;

    return WFA_SUCCESS;
}

int wfaStaSetSecurity(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
	dutCommand_t *setSecurity = (dutCommand_t *)caCmdBuf;
	caStaSetSecurity_t *setsec = &setSecurity->cmdsu.setsec;
	dutCmdResponse_t infoResp;
	char *ifname = setSecurity->intf;
	if(ifname[0] == '\0')
	{
		ifname = "wlan0";
		
	}
	printf("\n Entry wfaStaSetSecurity...\n ");

	int ret;

ret = shell_execute_cmd(NULL, "wpa_cli remove_network 0");
ret = shell_execute_cmd(NULL, "wpa_cli add_network 0");


	sprintf(gCmdStr, "wpa_cli set_network 0 ssid '\"%s\"'", setsec->ssid);
	ret = shell_execute_cmd(NULL, gCmdStr);

	printf("\n Interface = %s \n",setSecurity->intf );
	printf("\n keyMgmType = %s \n",setsec->keyMgmtType );
	printf("\n certType = %s \n",setsec->certType );
	printf("\n ssid = %s \n",setsec->ssid );
	printf("\n keyMgmtType = %s \n",setsec->keyMgmtType );
	printf("\n encpType = %s \n",setsec->encpType );
	printf("\n ecGroupID = %s \n",setsec->ecGroupID );
	printf("\n type = %d \n",setsec->type );
	printf("\n SEC_TYPE_PSKSAE = %d \n",SEC_TYPE_PSKSAE);
	printf("\n pmf = %d \n",setsec->pmf );
if(setsec->type == SEC_TYPE_PSKSAE)
                {
                        printf("\n IMG DEBUG >>>>>>> IN SEC_TYPE_PSKSAE");
			ret = shell_execute_cmd(NULL, "wpa_cli SAE_PWE 2");
			ret = shell_execute_cmd(NULL, "wpa_cli disable_network 0");
			ret = shell_execute_cmd(NULL, "wpa_cli set_network 0 pairwise CCMP");
			ret = shell_execute_cmd(NULL, "wpa_cli set_network 0 group CCMP");


			ret = shell_execute_cmd(NULL, "wpa_cli set_network 0 key_mgmt WPA-PSK SAE");
			ret = shell_execute_cmd(NULL, "wpa_cli set_network 0 ieee80211w 1");
			sprintf(gCmdStr, "wpa_cli set_network 0 sae_password '\"%s\"'", setsec->secu.passphrase);
			ret = shell_execute_cmd(NULL, gCmdStr);
			sprintf(gCmdStr, "wpa_cli set_network 0 psk '\"%s\"'",  setsec->secu.passphrase);
			ret = shell_execute_cmd(NULL, gCmdStr);
                        sprintf(gCmdStr, "wpa_cli set_network 0 key_mgmt WPA-PSK SAE", ifname);
			ret = shell_execute_cmd(NULL, gCmdStr);
			ret = shell_execute_cmd(NULL, "wpa_cli enable_network 0");
                   }
	
		if(setsec->type == SEC_TYPE_SAE)
		{
			printf("\n IMG DEBUG >>>>>>> IN SEC_TYPE_SAE");
			ret = shell_execute_cmd(NULL, "wpa_cli SAE_PWE 2");
			ret = shell_execute_cmd(NULL, "wpa_cli set_network 0  pairwise CCMP");
			ret = shell_execute_cmd(NULL, "wpa_cli set_network 0 group CCMP");
			ret = shell_execute_cmd(NULL, "wpa_cli set_network 0 key_mgmt SAE");
			ret = shell_execute_cmd(NULL, "wpa_cli set_network 0 ieee80211w 2");
			sprintf(gCmdStr, "wpa_cli set_network 0 sae_password '\"%s\"'", setsec->secu.passphrase);
			ret = shell_execute_cmd(NULL, gCmdStr);
			ret = shell_execute_cmd(NULL, "wpa_cli enable_network 0");

		}
		else if(setsec->type == SEC_TYPE_PSKSAE)
		{
			printf("\n IMG DEBUG >>>>>>> IN SEC_TYPE_PSKSAE");
			
			ret = shell_execute_cmd(NULL, "wpa_cli SAE_PWE 2");
			ret = shell_execute_cmd(NULL, "wpa_cli disable_network 0");
			ret = shell_execute_cmd(NULL, "wpa_cli set_network 0  pairwise CCMP");
			ret = shell_execute_cmd(NULL, "wpa_cli set_network 0 group CCMP");
			ret = shell_execute_cmd(NULL, "wpa_cli set_network 0 key_mgmt WPA-PSK SAE");
			ret = shell_execute_cmd(NULL, "wpa_cli set_network 0 ieee80211w 1");
			sprintf(gCmdStr, "wpa_cli set_network 0 sae_password '\"%s\"'", setsec->secu.passphrase);
			ret = shell_execute_cmd(NULL, gCmdStr);
			sprintf(gCmdStr, "wpa_cli set_network 0 psk '\"%s\"'", setsec->secu.passphrase);
			ret = shell_execute_cmd(NULL, gCmdStr);
			ret = shell_execute_cmd(NULL, "wpa_cli enable_network 0");
	   	}
		else if(setsec->type == SEC_TYPE_PSK)
		{
			printf("\n IMG DEBUG >>>>>>> IN SEC_TYPE_PSK");
			ret = shell_execute_cmd(NULL, "wpa_cli set_network 0 key_mgmt WPA-PSK WPA-PSK-SHA256");
			ret = shell_execute_cmd(NULL, "wpa_cli set_network 0 auth_alg OPEN");
			ret = shell_execute_cmd(NULL, "wpa_cli set_network 0 group CCMP");
			ret = shell_execute_cmd(NULL, "wpa_cli set_network 0 pairwise CCMP");
			sprintf(gCmdStr, "wpa_cli set_network 0 ieee80211w %d", setsec->pmf);
			ret = shell_execute_cmd(NULL, gCmdStr);
			sprintf(gCmdStr, "wpa_cli set pmf %d", setsec->pmf);
			ret = shell_execute_cmd(NULL, gCmdStr);
			ret = shell_execute_cmd(NULL, "wpa_cli sta_autoconnect 1");
			sprintf(gCmdStr, "wpa_cli set_network 0 psk '\"%s\"'", setsec->secu.passphrase);
			ret = shell_execute_cmd(NULL, gCmdStr);

                }
		else if(setsec->type == SEC_TYPE_EAPTLS)
		{
			printf("\n IMG DEBUG >>>>>>> IN SEC_TYPE_EAPTLS");
			ret = shell_execute_cmd(NULL, "wpa_cli set_network 0 key_mgmt WPA-EAP");
			ret = shell_execute_cmd(NULL, "wpa_cli set_network 0 ieee80211w 1");
			ret = shell_execute_cmd(NULL, "wpa_cli set_network 0 eap TLS");
			ret = shell_execute_cmd(NULL, "wpa_cli set_network 0 identity '\"user@example.com\"'");
			sprintf(gCmdStr, "wpa_cli set_network 0 ca_cert '\"/etc/wpa_supplicant/%s\"'", setsec->trustedRootCA);
			ret = shell_execute_cmd(NULL, gCmdStr);
			sprintf(gCmdStr, "wpa_cli set_network 0 client_cert '\"/etc/wpa_supplicant/%s\"'", setsec->clientCertificate);
			ret = shell_execute_cmd(NULL, gCmdStr);
			sprintf(gCmdStr, "wpa_cli set_network 0 private_key '\"/etc/wpa_supplicant/%s\"'", setsec->clientCertificate);
			ret = shell_execute_cmd(NULL, gCmdStr);
		}
		else if(setsec->type == 0)
                {
                        printf("\n IMG DEBUG >>>>>>> IN SEC_TYPE_OPEN");
			ret = shell_execute_cmd(NULL, "wpa_cli set_network 0 key_mgmt NONE");
                }

		if(setsec->ecGroupID[0] != '\0')
		{
			sprintf(gCmdStr, "wpa_cli SET sae_groups %s", setsec->ecGroupID);
			ret = shell_execute_cmd(NULL, gCmdStr);
			printf("\n %s \n", gCmdStr);
		}
	
	if(strcasecmp(setsec->keyMgmtType, "OWE") == 0)	
	{
		printf("\n IMG DEBUG >>>>>>> IN OWE");
		ret = shell_execute_cmd(NULL, "wpa_cli set_network 0 key_mgmt OWE");
		ret = shell_execute_cmd(NULL, "wpa_cli set_network 0 group CCMP");
		ret = shell_execute_cmd(NULL, "wpa_cli set_network 0 pairwise CCMP");
		ret = shell_execute_cmd(NULL, "wpa_cli set_network 0 proto RSN");
		ret = shell_execute_cmd(NULL, "wpa_cli set_network 0 ieee80211w 2");
	}
	else if(strcasecmp(setsec->keyMgmtType, "SuiteB") == 0)	
	{
		printf("\n IMG DEBUG >>>>>>> IN SuiteB");
		ret = shell_execute_cmd(NULL, "wpa_cli set_network 0 key_mgmt WPA-EAP-SUITE-B-192");
		ret = shell_execute_cmd(NULL, "wpa_cli set_network 0 pairwise GCMP-256");
		ret = shell_execute_cmd(NULL, "wpa_cli set_network 0 group GCMP-256");
		ret = shell_execute_cmd(NULL, "wpa_cli set_network 0 eap TLS");
		ret = shell_execute_cmd(NULL, "wpa_cli set_network 0 identity '\"user@example.com\"'");
		sprintf(gCmdStr, "wpa_cli set_network 0 ca_cert '\"/etc/wpa_supplicant/%s\"'", setsec->trustedRootCA);
		ret = shell_execute_cmd(NULL, gCmdStr);
		sprintf(gCmdStr, "wpa_cli set_network 0 client_cert '\"/etc/wpa_supplicant/%s\"'", setsec->clientCertificate);
		ret = shell_execute_cmd(NULL, gCmdStr);
		sprintf(gCmdStr, "wpa_cli set_network 0 private_key '\"/etc/wpa_supplicant/%s\"'", setsec->clientCertificate);
		ret = shell_execute_cmd(NULL, gCmdStr);
		ret = shell_execute_cmd(NULL, "wpa_cli set_network 0 group_mgmt BIP-GMAC-256");
		if(strcasecmp(setsec->certType, "ecc") == 0)
			ret = shell_execute_cmd(NULL, "wpa_cli set_network 0 openssl_ciphers '\"ECDHE-ECDSA-AES256-GCM-SHA384\"'");
		else if(strcasecmp(setsec->certType, "rsa") == 0)
			ret = shell_execute_cmd(NULL, "wpa_cli set_network 0 openssl_ciphers '\"ECDHE-RSA-AES256-GCM-SHA384:DHE-RSA-AES256-GCM-SHA384\"'");

		ret = shell_execute_cmd(NULL, "wpa_cli set_network 0 ieee80211w 2");
	}

	else
	{
		 if(setsec->type == 0)
                {
                        printf("\n IMG DEBUG >>>>>>> IN SEC_TYPE_OPEN");
			ret = shell_execute_cmd(NULL, "wpa_cli set_network 0 ieee80211w 1");
                }
		
	}
                printf("\n IMG DEBUG >>>>>>> IN STA_AUTO_CONNECT");
		ret = shell_execute_cmd(NULL, "wpa_cli sta_autoconnect 1 ");

		ret = shell_execute_cmd(NULL, "wpa_cli set_network 0 scan_ssid 1 ");

		//ret = shell_execute_cmd(NULL, "wpa_cli select_network 0");
	sleep(2);
	infoResp.status = STATUS_COMPLETE;
	wfaEncodeTLV(WFA_STA_SET_SECURITY_RESP_TLV, sizeof(infoResp), (BYTE *)&infoResp, respBuf);
	*respLen = WFA_TLV_HDR_LEN + sizeof(infoResp);

	return WFA_SUCCESS;
}

/*
 * wfaStaSetEapTLS():
 *   This is to set
 *   1. ssid
 *   2. encrypType - tkip or aes-ccmp
 *   3. keyManagementType - wpa or wpa2
 *   4. trustedRootCA
 *   5. clientCertificate
 */
int wfaStaSetEapTLS(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    caStaSetEapTLS_t *setTLS = (caStaSetEapTLS_t *)caCmdBuf;
    char *ifname = setTLS->intf;
    dutCmdResponse_t *setEapTlsResp = &gGenericResp;
    int ret;
    DPRINT_INFO(WFA_OUT, "Entering wfaStaSetEapTLS ...\n");
    DPRINT_INFO(WFA_OUT, " <><><><><><><><><><> IMG DEBUG <><><><><><><><><><>\n");

    /*
     * need to store the trustedROOTCA and clientCertificate into a file first.
     */
#ifdef WFA_NEW_CLI_FORMAT
	sprintf(gCmdStr, "wfa_set_eaptls -i '\"%s\"' '\"%s\"' '\"%s\"' '\"%s\"'",ifname, setTLS->ssid, setTLS->trustedRootCA, setTLS->clientCertificate);
	ret = shell_execute_cmd(NULL, gCmdStr);
#else

	ret = shell_execute_cmd(NULL, "wpa_cli remove_network 0");
	ret = shell_execute_cmd(NULL, "wpa_cli add_network 0");
	/*ret = shell_execute_cmd(NULL, "wpa_cli disable_network 0");*/
    /* ssid */
    sprintf(gCmdStr, "wpa_cli set_network 0 ssid '\"%s\"'", ifname, setTLS->ssid);
    ret = shell_execute_cmd(NULL, gCmdStr);

    /* key management */
    if(strcasecmp(setTLS->keyMgmtType, "wpa2-sha256") == 0)
    {
    }
    else if(strcasecmp(setTLS->keyMgmtType, "wpa2-eap") == 0)
    {
    }
    else if(strcasecmp(setTLS->keyMgmtType, "wpa2-ft") == 0)
    {

    }
    else if(strcasecmp(setTLS->keyMgmtType, "wpa") == 0)
    {
	ret = shell_execute_cmd(NULL, "wpa_cli set_network 0 key_mgmt WPA-EAP");
	ret = shell_execute_cmd(NULL, "wpa_cli set_network 0 proto WPA");
    }
    else if(strcasecmp(setTLS->keyMgmtType, "wpa2") == 0)
    {
	ret = shell_execute_cmd(NULL, "wpa_cli set_network 0 key_mgmt WPA-EAP");
	ret = shell_execute_cmd(NULL, "wpa_cli set_network 0 pairwise CCMP");
	ret = shell_execute_cmd(NULL, "wpa_cli set_network 0 group CCMP");
	ret = shell_execute_cmd(NULL, "wpa_cli set_network 0 proto WPA2");
 
        // to take all and device to pick any one supported.
    }
    else
    {
        // ??

	ret = shell_execute_cmd(NULL, "wpa_cli set_network 0 key_mgmt WPA-EAP WPA-EAP-SHA256");
    }

    /* if PMF enable */
    if(setTLS->pmf == WFA_ENABLED || setTLS->pmf == WFA_OPTIONAL)
    {

	ret = shell_execute_cmd(NULL, "wpa_cli set_network 0 ieee80211w 1");
    }
    else if(setTLS->pmf == WFA_REQUIRED)
    {
 	 ret = shell_execute_cmd(NULL, "wpa_cli set_network 0 ieee80211w 2");
    }
    else if(setTLS->pmf == WFA_F_REQUIRED)
    {

 	 ret = shell_execute_cmd(NULL, "wpa_cli set_network 0 ieee80211w 2");
   } 
    else if(setTLS->pmf == WFA_F_DISABLED)
    {

    }
    else
    {
        /* Disable PMF */

/*    sprintf(gCmdStr, "wpa_cli set_network 0 iee80211w 0", setTLS->intf);
		printf("\n %s \n", gCmdStr);
   sret = shell_execute_cmd(NULL,gCmdStr);
 */    /* protocol WPA */

     } 
    
    //sprintf(gCmdStr, "wpa_cli set_network 0 proto WPA", ifname);
    //sret = shell_execute_cmd(NULL,gCmdStr);

 	 ret = shell_execute_cmd(NULL, "wpa_cli set_network 0 eap TLS");
 	 ret = shell_execute_cmd(NULL, "wpa_cli set_network 0 identity '\"wifiuser\"'");

 	 ret = shell_execute_cmd(NULL, "wpa_cli set_network 0 password '\"test%11\"'");


 	 ret = shell_execute_cmd(NULL, "wpa_cli set_network 0 ca_cert '\"/usr/test/cas.pem\"'");

   /* sprintf(gCmdStr, "wpa_cli set_network 0 private_key '\"/etc/wpa_supplicant/wifiuser.pem\"'", ifname);//IMG EDITED */
 	 ret = shell_execute_cmd(NULL, "wpa_cli set_network 0 private_key '\"/usr/test/wifiuser.pem\"'");

 	 ret = shell_execute_cmd(NULL, "wpa_cli set_network 0 private_key_passwd '\"wifi\"'");

   /* sprintf(gCmdStr, "wpa_cli set_network 0 client_cert '\"/etc/wpa_supplicant/wifiuser.pem\"'", ifname);//IMG EDITED */
 	 ret = shell_execute_cmd(NULL, "wpa_cli set_network 0 client_cert '\"/usr/test/wifiuser.pem\"'");
   
   //IMG
 	 ret = shell_execute_cmd(NULL, "wpa_cli set_network 0 scan_ssid 1 ");

    DPRINT_INFO(WFA_OUT, "Entering sta_autoconnect ...\n");
 	 ret = shell_execute_cmd(NULL, "wpa_cli sta_autoconnect 1");
 	 //ret = shell_execute_cmd(NULL, "wpa_cli select_network 0");

   sleep(2);
#endif

    setEapTlsResp->status = STATUS_COMPLETE;
    wfaEncodeTLV(WFA_STA_SET_EAPTLS_RESP_TLV, 4, (BYTE *)setEapTlsResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + 4;

    return WFA_SUCCESS;
}

/*
 * The function is to set
 *   1. ssid
 *   2. passPhrase
 *   3. keyMangementType - wpa/wpa2
 *   4. encrypType - tkip or aes-ccmp
 */
int wfaStaSetPSK(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    /*Incompleted function*/
    dutCmdResponse_t *setPskResp = &gGenericResp;

	int ret;
#ifndef WFA_PC_CONSOLE
    caStaSetPSK_t *setPSK = (caStaSetPSK_t *)caCmdBuf;
#ifdef WFA_NEW_CLI_FORMAT
    sprintf(gCmdStr, "wfa_set_psk '\"%s\"' '\"%s\"' '\"%s\"'", setPSK->intf, setPSK->ssid, setPSK->passphrase);
    ret = shell_execute_cmd(NULL, gCmdStr);
#else

ret = shell_execute_cmd(NULL, "wpa_cli remove_network 0");
ret = shell_execute_cmd(NULL, "wpa_cli add_network 0");
    


	sprintf(gCmdStr, "wpa_cli set_network 0 ssid '\"%s\"'", setPSK->ssid);
	ret = shell_execute_cmd(NULL, gCmdStr);

  if(strcasecmp(setPSK->keyMgmtType, "wpa2-sha256") == 0)
     {
	ret = shell_execute_cmd(NULL, "wpa_cli set_network 0 key_mgmt WPA-PSK WPA-PSK-SHA256");
     }
    else if(strcasecmp(setPSK->keyMgmtType, "wpa2") == 0)
    {
     // take all and device to pick it supported.
	ret = shell_execute_cmd(NULL, "wpa_cli set_network 0 key_mgmt WPA-PSK WPA-PSK-SHA256");
	ret = shell_execute_cmd(NULL, "wpa_cli set_network 0  proto WPA2");
	ret = shell_execute_cmd(NULL, "wpa_cli set_network 0 pairwise CCMP");
	ret = shell_execute_cmd(NULL, "wpa_cli set_network 0  group CCMP TKIP");
    }
    else if(strcasecmp(setPSK->keyMgmtType, "wpa2-psk") == 0)
    {
	ret = shell_execute_cmd(NULL, "wpa_cli set_network 0 key_mgmt WPA-PSK WPA-PSK-SHA256");
    }
    else if(strcasecmp(setPSK->keyMgmtType, "wpa2-ft") == 0)
    {

    }
    else if (strcasecmp(setPSK->keyMgmtType, "wpa2-wpa-psk") == 0)
    {

    }
    else
	ret = shell_execute_cmd(NULL, "wpa_cli set_network 0 key_mgmt WPA-PSK WPA-PSK-SHA256");

	sprintf(gCmdStr, "wpa_cli set_network 0 psk '\"%s\"'", setPSK->passphrase);
	ret = shell_execute_cmd(NULL, gCmdStr);

	ret = shell_execute_cmd(NULL, "wpa_cli set_network 0 scan_ssid 1");
 
	ret = shell_execute_cmd(NULL, "wpa_cli sta_autoconnect 1 ");
	ret = shell_execute_cmd(NULL, "wpa_cli enable_network 0");


    /* if PMF enable */
    if(setPSK->pmf == WFA_ENABLED || setPSK->pmf == WFA_OPTIONAL)
    {
	ret = shell_execute_cmd(NULL, "wpa_cli set_network 0 ieee80211w 1");
    }
    else if(setPSK->pmf == WFA_REQUIRED)
    {
	ret = shell_execute_cmd(NULL, "wpa_cli set_network 0 ieee80211w 2");
    }
    else if(setPSK->pmf == WFA_F_REQUIRED)
    {

	ret = shell_execute_cmd(NULL, "wpa_cli set_network 0 ieee80211w 2");
   } 
    else if(setPSK->pmf == WFA_F_DISABLED)
    {

    }
    else
    {
        /* Disable PMF */
	ret = shell_execute_cmd(NULL, "wpa_cli set_network 0 ieee80211w 1");
    }
	//ret = shell_execute_cmd(NULL, "wpa_cli select_network 0");
    sleep(2);
#endif

#endif

    setPskResp->status = STATUS_COMPLETE;
    wfaEncodeTLV(WFA_STA_SET_PSK_RESP_TLV, 4, (BYTE *)setPskResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + 4;

    return WFA_SUCCESS;
}

/*
 * wfaStaGetInfo():
 * Get vendor specific information in name/value pair by a wireless I/F.
 */
int wfaStaGetInfo(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    dutCmdResponse_t infoResp;
    dutCommand_t *getInfo = (dutCommand_t *)caCmdBuf;

    /*
     * Normally this is called to retrieve the vendor information
     * from a interface, no implement yet
     */
    sprintf(infoResp.cmdru.info, "interface,%s,vendor,XXX,cardtype,802.11a/b/g", getInfo->intf);

    infoResp.status = STATUS_COMPLETE;
    wfaEncodeTLV(WFA_STA_GET_INFO_RESP_TLV, sizeof(infoResp), (BYTE *)&infoResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + sizeof(infoResp);

    return WFA_SUCCESS;
}

/*
 * wfaStaSetEapTTLS():
 *   This is to set
 *   1. ssid
 *   2. username
 *   3. passwd
 *   4. encrypType - tkip or aes-ccmp
 *   5. keyManagementType - wpa or wpa2
 *   6. trustedRootCA
 */
int wfaStaSetEapTTLS(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    caStaSetEapTTLS_t *setTTLS = (caStaSetEapTTLS_t *)caCmdBuf;
    char *ifname = setTTLS->intf;
    dutCmdResponse_t *setEapTtlsResp = &gGenericResp;

#ifdef WFA_NEW_CLI_FORMAT
    sprintf(gCmdStr, "wfa_set_eapttls %s %s %s %s %s", ifname, setTTLS->ssid, setTTLS->username, setTTLS->passwd, setTTLS->trustedRootCA);
    sret = shell_execute_cmd(NULL,gCmdStr);
#else

   sprintf(gCmdStr, "wpa_cli add_network 0 ", ifname); 
   sret = shell_execute_cmd(NULL,gCmdStr);
    sprintf(gCmdStr, "wpa_cli disable_network 0", ifname);
    sret = shell_execute_cmd(NULL,gCmdStr);

    sprintf(gCmdStr, "wpa_cli set_network 0 ssid '\"%s\"'", ifname, setTTLS->ssid);
    sret = shell_execute_cmd(NULL,gCmdStr);

    sprintf(gCmdStr, "wpa_cli set_network 0 identity '\"%s\"'", ifname, setTTLS->username);
    sret = shell_execute_cmd(NULL,gCmdStr);

    sprintf(gCmdStr, "wpa_cli set_network 0 password '\"%s\"'", ifname, setTTLS->passwd);
    sret = shell_execute_cmd(NULL,gCmdStr);

    sprintf(gCmdStr, "wpa_cli set_network 0 key_mgmt WPA-EAP", ifname);
    sret = shell_execute_cmd(NULL,gCmdStr);

    /* This may not need to set. if it is not set, default to take all */
//   sprintf(cmdStr, "wpa_cli set_network 0 pairwise '\"%s\"", ifname, setTTLS->encrptype);
    if(strcasecmp(setTTLS->keyMgmtType, "wpa2-sha256") == 0)
    {
    }
    else if(strcasecmp(setTTLS->keyMgmtType, "wpa2-eap") == 0)
    {
    }
    else if(strcasecmp(setTTLS->keyMgmtType, "wpa2-ft") == 0)
    {

    }
    else if(strcasecmp(setTTLS->keyMgmtType, "wpa") == 0)
    {

      sprintf(gCmdStr, "wpa_cli set_network 0 key_mgmt WPA-EAP", ifname);//IMG EDITED
      sret = shell_execute_cmd(NULL,gCmdStr);//IMG EDITED
      sprintf(gCmdStr, "wpa_cli set_network 0 proto WPA", ifname);//IMG EDITED
      sret = shell_execute_cmd(NULL,gCmdStr);//IMG EDITED
    }
    else if(strcasecmp(setTTLS->keyMgmtType, "wpa2") == 0)
    {
        // to take all and device to pick one it supported
      sprintf(gCmdStr, "wpa_cli set_network 0 key_mgmt WPA-EAP", ifname);//IMG EDITED
      sret = shell_execute_cmd(NULL,gCmdStr);//IMG EDITED
      sprintf(gCmdStr, "wpa_cli set_network 0 proto WPA2", ifname);//IMG EDITED
      sret = shell_execute_cmd(NULL,gCmdStr);//IMG EDITED
    }
    else
    {
        // ??
    }
    sret = shell_execute_cmd(NULL,gCmdStr);

    sprintf(gCmdStr, "wpa_cli set_network 0 eap TTLS", ifname);
    sret = shell_execute_cmd(NULL,gCmdStr);

   sprintf(gCmdStr, "wpa_cli set_network 0 ca_cert '\"/etc/wpa_supplicant/cas.pem\"'", ifname);
   sret = shell_execute_cmd(NULL,gCmdStr);

    sprintf(gCmdStr, "wpa_cli set_network 0 phase2 '\"auth=MSCHAPV2\"'", ifname);
    sret = shell_execute_cmd(NULL,gCmdStr);

     //IMG
 sprintf(gCmdStr, "wpa_cli sta_autoconnect 0 ", ifname);
    sret = shell_execute_cmd(NULL,gCmdStr);
    sprintf(gCmdStr, "wpa_cli enable_network 0", ifname);
    sret = shell_execute_cmd(NULL,gCmdStr);
#endif

    setEapTtlsResp->status = STATUS_COMPLETE;
    wfaEncodeTLV(WFA_STA_SET_EAPTTLS_RESP_TLV, 4, (BYTE *)setEapTtlsResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + 4;

    return WFA_SUCCESS;
}

/*
 * wfaStaSetEapSIM():
 *   This is to set
 *   1. ssid
 *   2. user name
 *   3. passwd
 *   4. encrypType - tkip or aes-ccmp
 *   5. keyMangementType - wpa or wpa2
 */
int wfaStaSetEapSIM(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    caStaSetEapSIM_t *setSIM = (caStaSetEapSIM_t *)caCmdBuf;
    char *ifname = setSIM->intf;
    dutCmdResponse_t *setEapSimResp = &gGenericResp;

#ifdef WFA_NEW_CLI_FORMAT
    sprintf(gCmdStr, "wfa_set_eapsim %s %s %s %s", ifname, setSIM->ssid, setSIM->username, setSIM->encrptype);
    sret = shell_execute_cmd(NULL,gCmdStr);
#else

   sprintf(gCmdStr, "wpa_cli add_network 0 ", ifname); 
   sret = shell_execute_cmd(NULL,gCmdStr);
    sprintf(gCmdStr, "wpa_cli disable_network 0", ifname);
    sret = shell_execute_cmd(NULL,gCmdStr);

    sprintf(gCmdStr, "wpa_cli set_network 0 ssid '\"%s\"'", ifname, setSIM->ssid);
    sret = shell_execute_cmd(NULL,gCmdStr);


    sprintf(gCmdStr, "wpa_cli set_network 0 identity '\"%s\"'", ifname, setSIM->username);
    sret = shell_execute_cmd(NULL,gCmdStr);

    sprintf(gCmdStr, "wpa_cli set_network 0 pairwise '\"%s\"'", ifname, setSIM->encrptype);
    sret = shell_execute_cmd(NULL,gCmdStr);

    sprintf(gCmdStr, "wpa_cli set_network 0 eap SIM", ifname);
    sret = shell_execute_cmd(NULL,gCmdStr);

    sprintf(gCmdStr, "wpa_cli set_network 0 proto WPA", ifname);
    sret = shell_execute_cmd(NULL,gCmdStr);

     //IMG
    sprintf(gCmdStr, "wpa_cli sta_autoconnect 0 ", ifname);
    sret = shell_execute_cmd(NULL,gCmdStr);

    sprintf(gCmdStr, "wpa_cli enable_network 0", ifname);
    sret = shell_execute_cmd(NULL,gCmdStr);

    if(strcasecmp(setSIM->keyMgmtType, "wpa2-sha256") == 0)
    {
        sprintf(gCmdStr, "wpa_cli set_network 0 key_mgmt WPA-SHA256", ifname);
    }
    else if(strcasecmp(setSIM->keyMgmtType, "wpa2-eap") == 0)
    {
        sprintf(gCmdStr, "wpa_cli set_network 0 key_mgmt WPA-EAP", ifname);
    }
    else if(strcasecmp(setSIM->keyMgmtType, "wpa2-ft") == 0)
    {
        sprintf(gCmdStr, "wpa_cli set_network 0 key_mgmt WPA-FT", ifname);
    }
    else if(strcasecmp(setSIM->keyMgmtType, "wpa") == 0)
    {
        sprintf(gCmdStr, "wpa_cli set_network 0 key_mgmt WPA-EAP", ifname);
    }
    else if(strcasecmp(setSIM->keyMgmtType, "wpa2") == 0)
    {
        // take all and device to pick one which is supported.
    }
    else
    {
        // ??
    }
    sret = shell_execute_cmd(NULL,gCmdStr);

#endif

    setEapSimResp->status = STATUS_COMPLETE;
    wfaEncodeTLV(WFA_STA_SET_EAPSIM_RESP_TLV, 4, (BYTE *)setEapSimResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + 4;

    return WFA_SUCCESS;
}

/*
 * wfaStaSetPEAP()
 *   This is to set
 *   1. ssid
 *   2. user name
 *   3. passwd
 *   4. encryType - tkip or aes-ccmp
 *   5. keyMgmtType - wpa or wpa2
 *   6. trustedRootCA
 *   7. innerEAP
 *   8. peapVersion
 */
int wfaStaSetPEAP(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    caStaSetEapPEAP_t *setPEAP = (caStaSetEapPEAP_t *)caCmdBuf;
    char *ifname = setPEAP->intf;
    dutCmdResponse_t *setPeapResp = &gGenericResp;

#ifdef WFA_NEW_CLI_FORMAT
    sprintf(gCmdStr, "wfa_set_peap %s %s %s %s %s %s %i %s", ifname, setPEAP->ssid, setPEAP->username,
            setPEAP->passwd, setPEAP->trustedRootCA,
            setPEAP->encrptype, setPEAP->peapVersion,
            setPEAP->innerEAP);
    sret = shell_execute_cmd(NULL,gCmdStr);
#else

   sprintf(gCmdStr, "wpa_cli add_network 0 ", ifname); 
   sret = shell_execute_cmd(NULL,gCmdStr);
    sprintf(gCmdStr, "wpa_cli disable_network 0", ifname);
    sret = shell_execute_cmd(NULL,gCmdStr);

    sprintf(gCmdStr, "wpa_cli set_network 0 ssid '\"%s\"'", ifname, setPEAP->ssid);
    sret = shell_execute_cmd(NULL,gCmdStr);

    sprintf(gCmdStr, "wpa_cli set_network 0 eap PEAP", ifname);
    sret = shell_execute_cmd(NULL,gCmdStr);

    sprintf(gCmdStr, "wpa_cli set_network 0 anonymous_identity '\"anonymous\"' ", ifname);
    sret = shell_execute_cmd(NULL,gCmdStr);

    sprintf(gCmdStr, "wpa_cli set_network 0 identity '\"%s\"'", ifname, setPEAP->username);
    sret = shell_execute_cmd(NULL,gCmdStr);

    sprintf(gCmdStr, "wpa_cli set_network 0 password '\"%s\"'", ifname, setPEAP->passwd);
    sret = shell_execute_cmd(NULL,gCmdStr);

   sprintf(gCmdStr, "wpa_cli set_network 0 ca_cert '\"/etc/wpa_supplicant/cas.pem\"'", ifname);
   sret = shell_execute_cmd(NULL,gCmdStr);

   /* if this not set, default to set support all */
   //sprintf(gCmdStr, "wpa_cli set_network 0 pairwise '\"%s\"'", ifname, setPEAP->encrptype);
   //sret = shell_execute_cmd(NULL,gCmdStr);

    if(strcasecmp(setPEAP->keyMgmtType, "wpa2-sha256") == 0)
    {
        sprintf(gCmdStr, "wpa_cli set_network 0 key_mgmt WPA-SHA256", ifname);
    }
    else if(strcasecmp(setPEAP->keyMgmtType, "wpa2-eap") == 0)
    {
        sprintf(gCmdStr, "wpa_cli set_network 0 key_mgmt WPA-EAP", ifname);
    }
    else if(strcasecmp(setPEAP->keyMgmtType, "wpa2-ft") == 0)
    {
        sprintf(gCmdStr, "wpa_cli set_network 0 key_mgmt WPA-FT", ifname);
    }
    else if(strcasecmp(setPEAP->keyMgmtType, "wpa") == 0)
    {
        sprintf(gCmdStr, "wpa_cli set_network 0 key_mgmt WPA-EAP", ifname);
    }
    else if(strcasecmp(setPEAP->keyMgmtType, "wpa2") == 0)
    {
        // take all and device to pick one which is supported.
    }
    else
    {
        // ??
    }
    sret = shell_execute_cmd(NULL,gCmdStr);

    sprintf(gCmdStr, "wpa_cli set_network 0 phase1 '\"peaplabel=%i\"'", ifname, setPEAP->peapVersion);
    sret = shell_execute_cmd(NULL,gCmdStr);

    sprintf(gCmdStr, "wpa_cli set_network 0 phase2 '\"auth=%s\"'", ifname, setPEAP->innerEAP);
    sret = shell_execute_cmd(NULL,gCmdStr);

     //IMG
    sprintf(gCmdStr, "wpa_cli sta_autoconnect 0 ", ifname);
    sret = shell_execute_cmd(NULL,gCmdStr);

    sprintf(gCmdStr, "wpa_cli enable_network 0", ifname);
    sret = shell_execute_cmd(NULL,gCmdStr);
#endif

    setPeapResp->status = STATUS_COMPLETE;
    wfaEncodeTLV(WFA_STA_SET_PEAP_RESP_TLV, 4, (BYTE *)setPeapResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + 4;

    return WFA_SUCCESS;
}

/*
 * wfaStaSetUAPSD()
 *    This is to set
 *    1. acBE
 *    2. acBK
 *    3. acVI
 *    4. acVO
 */
int wfaStaSetUAPSD(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    dutCmdResponse_t *setUAPSDResp = &gGenericResp;
#if 0 /* used for only one specific device, need to update to reflect yours */
    caStaSetUAPSD_t *setUAPSD = (caStaSetUAPSD_t *)caCmdBuf;
    char *ifname = setUAPSD->intf;
    char tmpStr[10];
    char line[100];
    char *pathl="/etc/Wireless/RT61STA";
    BYTE acBE=1;
    BYTE acBK=1;
    BYTE acVO=1;
    BYTE acVI=1;
    BYTE APSDCapable;
    FILE *pipe;

    /*
     * A series of setting need to be done before doing WMM-PS
     * Additional steps of configuration may be needed.
     */

    /*
     * bring down the interface
     */
    sprintf(gCmdStr, "ifconfig %s down",ifname);
    sret = shell_execute_cmd(NULL,gCmdStr);
    /*
     * Unload the Driver
     */
    sprintf(gCmdStr, "rmmod rt61");
    sret = shell_execute_cmd(NULL,gCmdStr);
#ifndef WFA_WMM_AC
    if(setUAPSD->acBE != 1)
        acBE=setUAPSD->acBE = 0;
    if(setUAPSD->acBK != 1)
        acBK=setUAPSD->acBK = 0;
    if(setUAPSD->acVO != 1)
        acVO=setUAPSD->acVO = 0;
    if(setUAPSD->acVI != 1)
        acVI=setUAPSD->acVI = 0;
#else
    acBE=setUAPSD->acBE;
    acBK=setUAPSD->acBK;
    acVO=setUAPSD->acVO;
    acVI=setUAPSD->acVI;
#endif

    APSDCapable = acBE||acBK||acVO||acVI;
    /*
     * set other AC parameters
     */

    sprintf(tmpStr,"%d;%d;%d;%d",setUAPSD->acBE,setUAPSD->acBK,setUAPSD->acVI,setUAPSD->acVO);
    sprintf(gCmdStr, "sed -e \"s/APSDCapable=.*/APSDCapable=%d/g\" -e \"s/APSDAC=.*/APSDAC=%s/g\" %s/rt61sta.dat >/tmp/wfa_tmp",APSDCapable,tmpStr,pathl);
    sret = shell_execute_cmd(NULL,gCmdStr);

    sprintf(gCmdStr, "mv /tmp/wfa_tmp %s/rt61sta.dat",pathl);
    sret = shell_execute_cmd(NULL,gCmdStr);
    pipe = popen("uname -r", "r");
    /* Read into line the output of uname*/
    fscanf(pipe,"%s",line);
    pclose(pipe);

    /*
     * load the Driver
     */
    sprintf(gCmdStr, "insmod /lib/modules/%s/extra/rt61.ko",line);
    sret = shell_execute_cmd(NULL,gCmdStr);

    sprintf(gCmdStr, "ifconfig %s up",ifname);
    sret = shell_execute_cmd(NULL,gCmdStr);
#endif

    setUAPSDResp->status = STATUS_COMPLETE;
    wfaEncodeTLV(WFA_STA_SET_UAPSD_RESP_TLV, 4, (BYTE *)setUAPSDResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + 4;
    return WFA_SUCCESS;
}

int wifi_nrf_get_fw_ver(char *fw_ver, int len)
{
    int ret = 0;
	unsigned int umac_ver, lmac_ver;
    struct wifi_nrf_fmac_dev_ctx *fmac_dev_ctx = rpu_drv_priv_zep.rpu_ctx_zep.rpu_ctx;

    ret = wifi_nrf_fmac_ver_get(fmac_dev_ctx, &umac_ver, &lmac_ver);
    if (ret) {
        DPRINT_ERR(WFA_ERR, "Failed to get firmware version: %d\n", ret);
        return -1;
    }
    ret = snprintf(fw_ver, len,
            "version: %d.%d.%d.%d",
                NRF_WIFI_UMAC_VER(umac_ver),
                NRF_WIFI_UMAC_VER_MAJ(umac_ver),
                NRF_WIFI_UMAC_VER_MIN(umac_ver),
                NRF_WIFI_UMAC_VER_EXTRA(umac_ver));
    if (ret < 0) {
        DPRINT_ERR(WFA_ERR, "Failed to get firmware version: %d\n", ret);
        return -1;
    }
    return 0;
}

int wfaDeviceGetInfo(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    dutCommand_t *dutCmd = (dutCommand_t *)caCmdBuf;
    caDevInfo_t *devInfo = &dutCmd->cmdsu.dev;
    dutCmdResponse_t *infoResp = &gGenericResp;
    /*a vendor can fill in the proper info or anything non-disclosure */
    caDeviceGetInfoResp_t dinfo = {"Nordic Semi", "nRF7002", "v1.0.0.0"};
    int ret = 0;

    DPRINT_INFO(WFA_OUT, "Entering wfaDeviceGetInfo ...%d\n", devInfo->fw);

    if(devInfo->fw == 0) {
        memcpy(&infoResp->cmdru.devInfo, &dinfo, sizeof(caDeviceGetInfoResp_t));
        infoResp->cmdru.devInfo.firmware[0] = '\0';
    }
    else
    {
        ret = wifi_nrf_get_fw_ver(&infoResp->cmdru.devInfo.firmware,
                sizeof(infoResp->cmdru.devInfo.firmware));
        if (ret) {
            DPRINT_ERR(WFA_ERR, "Failed to get firmware version: %d\n", ret);
            infoResp->status = STATUS_ERROR;
            wfaEncodeTLV(WFA_DEVICE_GET_INFO_RESP_TLV, sizeof(dutCmdResponse_t), (BYTE *)infoResp, respBuf);
            *respLen = WFA_TLV_HDR_LEN + sizeof(dutCmdResponse_t);
            return WFA_SUCCESS;
        }
        printf("Firmware version: %s \n", infoResp->cmdru.devInfo.firmware);
    }

    infoResp->status = STATUS_COMPLETE;
    wfaEncodeTLV(WFA_DEVICE_GET_INFO_RESP_TLV, sizeof(dutCmdResponse_t), (BYTE *)infoResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + sizeof(dutCmdResponse_t);

    return WFA_SUCCESS;

}

/*
 * This funciton is to retrieve a list of interfaces and return
 * the list back to Agent control.
 * ********************************************************************
 * Note: We intend to make this WLAN interface name as a hardcode name.
 * Therefore, for a particular device, you should know and change the name
 * for that device while doing porting. The MACRO "WFA_STAUT_IF" is defined in
 * the file "inc/wfa_ca.h". If the device OS is not linux-like, this most
 * likely is hardcoded just for CAPI command responses.
 * *******************************************************************
 *
 */
int wfaDeviceListIF(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    dutCmdResponse_t *infoResp = &gGenericResp;
    dutCommand_t *ifList = (dutCommand_t *)caCmdBuf;
    caDeviceListIFResp_t *ifListResp = &infoResp->cmdru.ifList;

    DPRINT_INFO(WFA_OUT, "!!!!!!!!Entering wfaDeviceListIF ...\n");
    switch(ifList->cmdsu.iftype)
    {
    case IF_80211:
    DPRINT_INFO(WFA_OUT, "Entering Switch IF_80211 ...\n");
        infoResp->status = STATUS_COMPLETE;
        ifListResp->iftype = IF_80211;
        //strcpy(ifListResp->ifs[0], WFA_STAUT_IF);
        strcpy(ifListResp->ifs[0], "wlan0");
        strcpy(ifListResp->ifs[1], "NULL");
        strcpy(ifListResp->ifs[2], "NULL");
        break;
    case IF_ETH:
    DPRINT_INFO(WFA_OUT, "Entering Switch IF_ETH ...\n");
        infoResp->status = STATUS_COMPLETE;
        ifListResp->iftype = IF_ETH;
        strcpy(ifListResp->ifs[0], "eth0");
        strcpy(ifListResp->ifs[1], "NULL");
        strcpy(ifListResp->ifs[2], "NULL");
        break;
    default:
    {
    DPRINT_INFO(WFA_OUT, "Entering Switch DEFAULT...\n");
        infoResp->status = STATUS_ERROR;
        wfaEncodeTLV(WFA_DEVICE_LIST_IF_RESP_TLV, 4, (BYTE *)infoResp, respBuf);
        *respLen = WFA_TLV_HDR_LEN + 4;

        return WFA_SUCCESS;
    }
    }

    wfaEncodeTLV(WFA_DEVICE_LIST_IF_RESP_TLV, sizeof(dutCmdResponse_t), (BYTE *)infoResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + sizeof(dutCmdResponse_t);

    return WFA_SUCCESS;
}

int wfaStaDebugSet(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    dutCmdResponse_t *debugResp = &gGenericResp;
    dutCommand_t *debugSet = (dutCommand_t *)caCmdBuf;

    DPRINT_INFO(WFA_OUT, "Entering wfaStaDebugSet ...\n");

    if(debugSet->cmdsu.dbg.state == 1) /* enable */
        wfa_defined_debug |= debugSet->cmdsu.dbg.level;
    else
        wfa_defined_debug = (~debugSet->cmdsu.dbg.level & wfa_defined_debug);

    debugResp->status = STATUS_COMPLETE;
    wfaEncodeTLV(WFA_STA_GET_INFO_RESP_TLV, sizeof(dutCmdResponse_t), (BYTE *)debugResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + sizeof(dutCmdResponse_t);


    return WFA_SUCCESS;
}


/*
 *   wfaStaGetBSSID():
 *     This function is to retrieve BSSID of a specific wireless I/F.
 */
int wfaStaGetBSSID(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    char string_bssid[100];
    char *str;
    FILE *tmpfd;
    dutCmdResponse_t *bssidResp = &gGenericResp;
    dutCommand_t *connStat = (dutCommand_t *)caCmdBuf;
    char *ifname = connStat->intf;
    struct wpa_supplicant *wpa_s;

    DPRINT_INFO(WFA_OUT, "Entering wfaStaGetBSSID ...\n");
    /* retrieve the BSSID */
	int ret;	
	ret = shell_execute_cmd(NULL, "wpa_cli status");


	wpa_s = wpa_supplicant_get_iface(global, ifname);
	if (!wpa_s) {
		printf("Unable to find the interface: %s, quitting", ifname);
		return -1;
	}
		ret = os_snprintf(string_bssid,64, "" MACSTR "\n",MAC2STR(wpa_s->bssid));
		//ret = os_snprintf(string_bssid,64,"%s",MAC2STR(wpa_s->bssid));
		//os_memcpy(string_bssid, wpa_s->bssid, ETH_ALEN);
		printf("...string BSSID = %s",string_bssid);

                strcpy(bssidResp->cmdru.bssid, string_bssid);
                bssidResp->status = STATUS_COMPLETE;
		printf("string BSSID = %s bssidresp=  %s",string_bssid,bssidResp->cmdru.bssid);

    wfaEncodeTLV(WFA_STA_GET_BSSID_RESP_TLV, sizeof(dutCmdResponse_t), (BYTE *)bssidResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + sizeof(dutCmdResponse_t);

    return WFA_SUCCESS;
}

/*
 * wfaStaSetIBSS()
 *    This is to set
 *    1. ssid
 *    2. channel
 *    3. encrypType - none or wep
 *    optional
 *    4. key1
 *    5. key2
 *    6. key3
 *    7. key4
 *    8. activeIndex - 1, 2, 3, or 4
 */
int wfaStaSetIBSS(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    caStaSetIBSS_t *setIBSS = (caStaSetIBSS_t *)caCmdBuf;
    dutCmdResponse_t *setIbssResp = &gGenericResp;
    int i;

    /*
     * disable the network first
     */
    sprintf(gCmdStr, "wpa_cli disable_network 0", setIBSS->intf);
    sret = shell_execute_cmd(NULL,gCmdStr);

    /*
     * set SSID
     */
    sprintf(gCmdStr, "wpa_cli set_network 0 ssid '\"%s\"'", setIBSS->intf, setIBSS->ssid);
    sret = shell_execute_cmd(NULL,gCmdStr);

    /*
     * Set channel for IBSS
     */
    sprintf(gCmdStr, "iwconfig %s channel %i", setIBSS->intf, setIBSS->channel);
    sret = shell_execute_cmd(NULL,gCmdStr);

    /*
     * Tell the supplicant for IBSS mode (1)
     */
    sprintf(gCmdStr, "wpa_cli set_network 0 mode 1", setIBSS->intf);
    sret = shell_execute_cmd(NULL,gCmdStr);

    /*
     * set Key management to NONE (NO WPA) for plaintext or WEP
     */
    sprintf(gCmdStr, "wpa_cli set_network 0 key_mgmt NONE", setIBSS->intf);
    sret = shell_execute_cmd(NULL,gCmdStr);

    if(setIBSS->encpType == 1)
    {
        for(i=0; i<4; i++)
        {
            if(strlen(setIBSS->keys[i]) ==5 || strlen(setIBSS->keys[i]) == 13)
            {
                sprintf(gCmdStr, "wpa_cli set_network 0 wep_key%i \"%s\"",
                        setIBSS->intf, i, setIBSS->keys[i]);
                sret = shell_execute_cmd(NULL,gCmdStr);
            }
        }

        i = setIBSS->activeKeyIdx;
        if(strlen(setIBSS->keys[i]) ==5 || strlen(setIBSS->keys[i]) == 13)
        {
            sprintf(gCmdStr, "wpa_cli set_network 0 wep_tx_keyidx %i",
                    setIBSS->intf, setIBSS->activeKeyIdx);
            sret = shell_execute_cmd(NULL,gCmdStr);
        }
    }

     //IMG
    sprintf(gCmdStr, "wpa_cli sta_autoconnect 0 ", setIBSS->intf);
    sret = shell_execute_cmd(NULL,gCmdStr);

    sprintf(gCmdStr, "wpa_cli enable_network 0", setIBSS->intf);
    sret = shell_execute_cmd(NULL,gCmdStr);

    setIbssResp->status = STATUS_COMPLETE;
    wfaEncodeTLV(WFA_STA_SET_IBSS_RESP_TLV, 4, (BYTE *)setIbssResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + 4;

    return WFA_SUCCESS;
}

/*
 *  wfaSetMode():
 *  The function is to set the wireless interface with a given mode (possible
 *  adhoc)
 *  Input parameters:
 *    1. I/F
 *    2. ssid
 *    3. mode adhoc or managed
 *    4. encType
 *    5. channel
 *    6. key(s)
 *    7. active  key
 */
int wfaStaSetMode(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    caStaSetMode_t *setmode = (caStaSetMode_t *)caCmdBuf;
    dutCmdResponse_t *SetModeResp = &gGenericResp;
    int i;

    /*
     * bring down the interface
     */
    sprintf(gCmdStr, "ifconfig %s down",setmode->intf);
    sret = shell_execute_cmd(NULL,gCmdStr);

    /*
     * distroy the interface
     */
    sprintf(gCmdStr, "wlanconfig %s destroy",setmode->intf);
    sret = shell_execute_cmd(NULL,gCmdStr);


    /*
     * re-create the interface with the given mode
     */
    if(setmode->mode == 1)
        sprintf(gCmdStr, "wlanconfig %s create wlandev wifi0 wlanmode adhoc",setmode->intf);
    else
        sprintf(gCmdStr, "wlanconfig %s create wlandev wifi0 wlanmode managed",setmode->intf);

    sret = shell_execute_cmd(NULL,gCmdStr);
    if(setmode->encpType == ENCRYPT_WEP)
    {
        int j = setmode->activeKeyIdx;
        for(i=0; i<4; i++)
        {
            if(setmode->keys[i][0] != '\0')
            {
                sprintf(gCmdStr, "iwconfig  %s key  s:%s",
                        setmode->intf, setmode->keys[i]);
                sret = shell_execute_cmd(NULL,gCmdStr);
            }
            /* set active key */
            if(setmode->keys[j][0] != '\0')
                sprintf(gCmdStr, "iwconfig  %s key  s:%s",
                        setmode->intf, setmode->keys[j]);
            sret = shell_execute_cmd(NULL,gCmdStr);
        }

    }
    /*
     * Set channel for IBSS
     */
    if(setmode->channel)
    {
        sprintf(gCmdStr, "iwconfig %s channel %i", setmode->intf, setmode->channel);
        sret = shell_execute_cmd(NULL,gCmdStr);
    }


    /*
     * set SSID
     */
    sprintf(gCmdStr, "iwconfig %s essid %s", setmode->intf, setmode->ssid);
    sret = shell_execute_cmd(NULL,gCmdStr);

    /*
     * bring up the interface
     */
    sprintf(gCmdStr, "ifconfig %s up",setmode->intf);
    sret = shell_execute_cmd(NULL,gCmdStr);

    SetModeResp->status = STATUS_COMPLETE;
    wfaEncodeTLV(WFA_STA_SET_MODE_RESP_TLV, 4, (BYTE *)SetModeResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + 4;

    return WFA_SUCCESS;
}

int wfaStaSetPwrSave(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    caStaSetPwrSave_t *setps = (caStaSetPwrSave_t *)caCmdBuf;
    dutCmdResponse_t *SetPSResp = &gGenericResp;

    sprintf(gCmdStr, "wifi ps %s", setps->mode);
    sret = shell_execute_cmd(NULL,gCmdStr);

    printf("\n %s \n",gCmdStr);

    SetPSResp->status = STATUS_COMPLETE;
    wfaEncodeTLV(WFA_STA_SET_PWRSAVE_RESP_TLV, 4, (BYTE *)SetPSResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + 4;

    return WFA_SUCCESS;
}

int wfaStaUpload(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    caStaUpload_t *upload = &((dutCommand_t *)caCmdBuf)->cmdsu.upload;
    dutCmdResponse_t *upLoadResp = &gGenericResp;
    caStaUploadResp_t *upld = &upLoadResp->cmdru.uld;

    if(upload->type == WFA_UPLOAD_VHSO_RPT)
    {
        int rbytes;
        /*
         * if asked for the first packet, always to open the file
         */
        if(upload->next == 1)
        {
            if(e2efp != NULL)
            {
                fclose(e2efp);
                e2efp = NULL;
            }

            e2efp = fopen(e2eResults, "r");
        }

        if(e2efp == NULL)
        {
            upLoadResp->status = STATUS_ERROR;
            wfaEncodeTLV(WFA_STA_UPLOAD_RESP_TLV, 4, (BYTE *)upLoadResp, respBuf);
            *respLen = WFA_TLV_HDR_LEN + 4;
            return WFA_FAILURE;
        }

        rbytes = fread(upld->bytes, 1, 256, e2efp);

        if(rbytes < 256)
        {
            /*
             * this means no more bytes after this read
             */
            upld->seqnum = 0;
            fclose(e2efp);
            e2efp=NULL;
        }
        else
        {
            upld->seqnum = upload->next;
        }

        upld->nbytes = rbytes;

        upLoadResp->status = STATUS_COMPLETE;
        wfaEncodeTLV(WFA_STA_UPLOAD_RESP_TLV, sizeof(dutCmdResponse_t), (BYTE *)upLoadResp, respBuf);
        *respLen = WFA_TLV_HDR_LEN + sizeof(dutCmdResponse_t);
    }
    else
    {
        upLoadResp->status = STATUS_ERROR;
        wfaEncodeTLV(WFA_STA_UPLOAD_RESP_TLV, 4, (BYTE *)upLoadResp, respBuf);
        *respLen = WFA_TLV_HDR_LEN + 4;
    }

    return WFA_SUCCESS;
}
/*
 * wfaStaSetWMM()
 *  TO be ported on a specific plaform for the DUT
 *  This is to set the WMM related parameters at the DUT.
 *  Currently the function is used for GROUPS WMM-AC and WMM general configuration for setting RTS Threshhold, Fragmentation threshold and wmm (ON/OFF)
 *  It is expected that this function will set all the WMM related parametrs for a particular GROUP .
 */
int wfaStaSetWMM(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
#ifdef WFA_WMM_AC
    caStaSetWMM_t *setwmm = (caStaSetWMM_t *)caCmdBuf;
    char *ifname = setwmm->intf;
    dutCmdResponse_t *setwmmResp = &gGenericResp;

    switch(setwmm->group)
    {
    case GROUP_WMMAC:
        if (setwmm->send_trig)
        {
            int Sockfd;
            struct sockaddr_in psToAddr;
            unsigned int TxMsg[512];

            Sockfd = wfaCreateUDPSock(setwmm->dipaddr, 12346);
            memset(&psToAddr, 0, sizeof(psToAddr));
            psToAddr.sin_family = AF_INET;
            psToAddr.sin_addr.s_addr = inet_addr(setwmm->dipaddr);
            psToAddr.sin_port = htons(12346);


            switch (setwmm->trig_ac)
            {
            case WMMAC_AC_VO:
                wfaTGSetPrio(Sockfd, 7);
                create_apts_msg(APTS_CK_VO, TxMsg, 0);
                printf("\r\nSending AC_VO trigger packet\n");
                break;

            case WMMAC_AC_VI:
                wfaTGSetPrio(Sockfd, 5);
                create_apts_msg(APTS_CK_VI, TxMsg, 0);
                printf("\r\nSending AC_VI trigger packet\n");
                break;

            case WMMAC_AC_BK:
                wfaTGSetPrio(Sockfd, 2);
                create_apts_msg(APTS_CK_BK, TxMsg, 0);
                printf("\r\nSending AC_BK trigger packet\n");
                break;

            default:
            case WMMAC_AC_BE:
                wfaTGSetPrio(Sockfd, 0);
                create_apts_msg(APTS_CK_BE, TxMsg, 0);
                printf("\r\nSending AC_BE trigger packet\n");
                break;
            }

            sendto(Sockfd, TxMsg, 256, 0, (struct sockaddr *)&psToAddr,
                   sizeof(struct sockaddr));
            close(Sockfd);
            usleep(1000000);
        }
        else if (setwmm->action == WMMAC_ADDTS)
        {
            printf("ADDTS AC PARAMS: dialog id: %d, TID: %d, "
                   "DIRECTION: %d, PSB: %d, UP: %d, INFOACK: %d BURST SIZE DEF: %d"
                   "Fixed %d, MSDU Size: %d, Max MSDU Size %d, "
                   "MIN SERVICE INTERVAL: %d, MAX SERVICE INTERVAL: %d, "
                   "INACTIVITY: %d, SUSPENSION %d, SERVICE START TIME: %d, "
                   "MIN DATARATE: %d, MEAN DATA RATE: %d, PEAK DATA RATE: %d, "
                   "BURSTSIZE or MSDU Aggreg: %d, DELAY BOUND: %d, PHYRATE: %d, SPLUSBW: %f, "
                   "MEDIUM TIME: %d, ACCESSCAT: %d\n",
                   setwmm->actions.addts.dialog_token,
                   setwmm->actions.addts.tspec.tsinfo.TID,
                   setwmm->actions.addts.tspec.tsinfo.direction,
                   setwmm->actions.addts.tspec.tsinfo.PSB,
                   setwmm->actions.addts.tspec.tsinfo.UP,
                   setwmm->actions.addts.tspec.tsinfo.infoAck,
                   setwmm->actions.addts.tspec.tsinfo.bstSzDef,
                   setwmm->actions.addts.tspec.Fixed,
                   setwmm->actions.addts.tspec.size,
                   setwmm->actions.addts.tspec.maxsize,
                   setwmm->actions.addts.tspec.min_srvc,
                   setwmm->actions.addts.tspec.max_srvc,
                   setwmm->actions.addts.tspec.inactivity,
                   setwmm->actions.addts.tspec.suspension,
                   setwmm->actions.addts.tspec.srvc_strt_tim,
                   setwmm->actions.addts.tspec.mindatarate,
                   setwmm->actions.addts.tspec.meandatarate,
                   setwmm->actions.addts.tspec.peakdatarate,
                   setwmm->actions.addts.tspec.burstsize,
                   setwmm->actions.addts.tspec.delaybound,
                   setwmm->actions.addts.tspec.PHYrate,
                   setwmm->actions.addts.tspec.sba,
                   setwmm->actions.addts.tspec.medium_time,
                   setwmm->actions.addts.accesscat);

            //tspec should be set here.

            sret = shell_execute_cmd(NULL,gCmdStr);
        }
        else if (setwmm->action == WMMAC_DELTS)
        {
            // send del tspec
        }

        setwmmResp->status = STATUS_COMPLETE;
        break;

    case GROUP_WMMCONF:
        sprintf(gCmdStr, "iwconfig %s rts %d",
                ifname,setwmm->actions.config.rts_thr);

        sret = shell_execute_cmd(NULL,gCmdStr);
        sprintf(gCmdStr, "iwconfig %s frag %d",
                ifname,setwmm->actions.config.frag_thr);

        sret = shell_execute_cmd(NULL,gCmdStr);
        sprintf(gCmdStr, "iwpriv %s wmmcfg %d",
                ifname, setwmm->actions.config.wmm);

        sret = shell_execute_cmd(NULL,gCmdStr);
        setwmmResp->status = STATUS_COMPLETE;
        break;

    default:
        DPRINT_ERR(WFA_ERR, "The group %d is not supported\n",setwmm->group);
        setwmmResp->status = STATUS_ERROR;
        break;

    }

    wfaEncodeTLV(WFA_STA_SET_WMM_RESP_TLV, 4, (BYTE *)setwmmResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + 4;
#endif

    return WFA_SUCCESS;
}

int wfaStaSendNeigReq(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    dutCmdResponse_t *sendNeigReqResp = &gGenericResp;

    /*
     *  run your device to send NEIGREQ
     */

    sendNeigReqResp->status = STATUS_COMPLETE;
    wfaEncodeTLV(WFA_STA_SEND_NEIGREQ_RESP_TLV, 4, (BYTE *)sendNeigReqResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + 4;

    return WFA_SUCCESS;
}

int wfaStaSetEapFAST(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    caStaSetEapFAST_t *setFAST= (caStaSetEapFAST_t *)caCmdBuf;
    char *ifname = setFAST->intf;
    dutCmdResponse_t *setEapFastResp = &gGenericResp;

#ifdef WFA_NEW_CLI_FORMAT
    sprintf(gCmdStr, "wfa_set_eapfast %s %s %s %s %s %s", ifname, setFAST->ssid, setFAST->username,
            setFAST->passwd, setFAST->pacFileName,
            setFAST->innerEAP);
    sret = shell_execute_cmd(NULL,gCmdStr);
#else

    sprintf(gCmdStr, "wpa_cli add_network 0", ifname);
    sret = shell_execute_cmd(NULL,gCmdStr);
    sprintf(gCmdStr, "wpa_cli disable_network 0", ifname);
    sret = shell_execute_cmd(NULL,gCmdStr);

    sprintf(gCmdStr, "wpa_cli set_network 0 ssid '\"%s\"'", ifname, setFAST->ssid);
    sret = shell_execute_cmd(NULL,gCmdStr);

    sprintf(gCmdStr, "wpa_cli set_network 0 identity '\"%s\"'", ifname, setFAST->username);
    sret = shell_execute_cmd(NULL,gCmdStr);

    sprintf(gCmdStr, "wpa_cli set_network 0 password '\"%s\"'", ifname, setFAST->passwd);
    sret = shell_execute_cmd(NULL,gCmdStr);

    if(strcasecmp(setFAST->keyMgmtType, "wpa2-sha256") == 0)
    {
    }
    else if(strcasecmp(setFAST->keyMgmtType, "wpa2-eap") == 0)
    {
    }
    else if(strcasecmp(setFAST->keyMgmtType, "wpa2-ft") == 0)
    {

    }
    else if(strcasecmp(setFAST->keyMgmtType, "wpa") == 0)
    {
        sprintf(gCmdStr, "wpa_cli set_network 0 key_mgmt WPA-EAP", ifname);
    }
    else if(strcasecmp(setFAST->keyMgmtType, "wpa2") == 0)
    {
        // take all and device to pick one which is supported.
    }
    else
    {
        // ??
    }
    sret = shell_execute_cmd(NULL,gCmdStr);

    sprintf(gCmdStr, "wpa_cli set_network 0 eap FAST", ifname);
    sret = shell_execute_cmd(NULL,gCmdStr);

    sprintf(gCmdStr, "wpa_cli set_network 0 pac_file '\"%s/%s\"'", ifname, CERTIFICATES_PATH,     setFAST->pacFileName);
    sret = shell_execute_cmd(NULL,gCmdStr);

    sprintf(gCmdStr, "wpa_cli set_network 0 anonymous_identity '\"anonymous\"'", ifname);
    sret = shell_execute_cmd(NULL,gCmdStr);

    sprintf(gCmdStr, "wpa_cli set_network 0 phase1 '\"fast_provisioning=1\"'", ifname);
    sret = shell_execute_cmd(NULL,gCmdStr);

    sprintf(gCmdStr, "wpa_cli set_network 0 phase2 '\"auth=%s\"'", ifname,setFAST->innerEAP);
    sret = shell_execute_cmd(NULL,gCmdStr);

    //IMG
    sprintf(gCmdStr, "wpa_cli sta_autoconnect 0 ", ifname);
    sret = shell_execute_cmd(NULL,gCmdStr);



    sprintf(gCmdStr, "wpa_cli enable_network 0", ifname);
    sret = shell_execute_cmd(NULL,gCmdStr);
#endif

    setEapFastResp->status = STATUS_COMPLETE;
    wfaEncodeTLV(WFA_STA_SET_EAPFAST_RESP_TLV, 4, (BYTE *)setEapFastResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + 4;

    return WFA_SUCCESS;
}

int wfaStaSetEapAKA(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    caStaSetEapAKA_t *setAKA= (caStaSetEapAKA_t *)caCmdBuf;
    char *ifname = setAKA->intf;
    dutCmdResponse_t *setEapAkaResp = &gGenericResp;

#ifdef WFA_NEW_CLI_FORMAT
    sprintf(gCmdStr, "wfa_set_eapaka %s %s %s %s", ifname, setAKA->ssid, setAKA->username, setAKA->passwd);
    sret = shell_execute_cmd(NULL,gCmdStr);
#else

    sprintf(gCmdStr, "wpa_cli disable_network 0", ifname);
    sret = shell_execute_cmd(NULL,gCmdStr);

    sprintf(gCmdStr, "wpa_cli set_network 0 ssid '\"%s\"'", ifname, setAKA->ssid);
    sret = shell_execute_cmd(NULL,gCmdStr);

    if(strcasecmp(setAKA->keyMgmtType, "wpa2-sha256") == 0)
    {
    }
    else if(strcasecmp(setAKA->keyMgmtType, "wpa2-eap") == 0)
    {
    }
    else if(strcasecmp(setAKA->keyMgmtType, "wpa2-ft") == 0)
    {

    }
    else if(strcasecmp(setAKA->keyMgmtType, "wpa") == 0)
    {
        sprintf(gCmdStr, "wpa_cli set_network 0 key_mgmt WPA-EAP", ifname);
    }
    else if(strcasecmp(setAKA->keyMgmtType, "wpa2") == 0)
    {
        // take all and device to pick one which is supported.
    }
    else
    {
        // ??
    }
    sret = shell_execute_cmd(NULL,gCmdStr);

    sprintf(gCmdStr, "wpa_cli set_network 0 proto WPA2", ifname);
    sret = shell_execute_cmd(NULL,gCmdStr);
    sprintf(gCmdStr, "wpa_cli set_network 0 pairwise CCMP", ifname);
    sret = shell_execute_cmd(NULL,gCmdStr);

    sprintf(gCmdStr, "wpa_cli set_network 0 eap AKA", ifname);
    sret = shell_execute_cmd(NULL,gCmdStr);

    sprintf(gCmdStr, "wpa_cli set_network 0 phase1 \"result_ind=1\"", ifname);
    sret = shell_execute_cmd(NULL,gCmdStr);

    sprintf(gCmdStr, "wpa_cli set_network 0 identity '\"%s\"'", ifname, setAKA->username);
    sret = shell_execute_cmd(NULL,gCmdStr);

    sprintf(gCmdStr, "wpa_cli set_network 0 password '\"%s\"'", ifname, setAKA->passwd);
    sret = shell_execute_cmd(NULL,gCmdStr);

     //IMG
    sprintf(gCmdStr, "wpa_cli sta_autoconnect 0 ", ifname);
    sret = shell_execute_cmd(NULL,gCmdStr);

    sprintf(gCmdStr, "wpa_cli enable_network 0", ifname);
    sret = shell_execute_cmd(NULL,gCmdStr);
#endif

    setEapAkaResp->status = STATUS_COMPLETE;
    wfaEncodeTLV(WFA_STA_SET_EAPAKA_RESP_TLV, 4, (BYTE *)setEapAkaResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + 4;

    return WFA_SUCCESS;
}

int wfaStaSetSystime(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    caStaSetSystime_t *systime = (caStaSetSystime_t *)caCmdBuf;
    dutCmdResponse_t *setSystimeResp = &gGenericResp;

    DPRINT_INFO(WFA_OUT, "Entering wfaStaSetSystime ...\n");

    sprintf(gCmdStr, "date %d-%d-%d",systime->month,systime->date,systime->year);
    sret = shell_execute_cmd(NULL,gCmdStr);

    sprintf(gCmdStr, "time %d:%d:%d", systime->hours,systime->minutes,systime->seconds);
    sret = shell_execute_cmd(NULL,gCmdStr);

    setSystimeResp->status = STATUS_COMPLETE;
    wfaEncodeTLV(WFA_STA_SET_SYSTIME_RESP_TLV, 4, (BYTE *)setSystimeResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + 4;

    return WFA_SUCCESS;
}

#ifdef WFA_STA_TB
int wfaStaPresetParams(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    dutCmdResponse_t infoResp;
    int i,ret;
    caStaPresetParameters_t *preset = (caStaPresetParameters_t *)caCmdBuf;
    char *ifname = preset->intf;
	ret = shell_execute_cmd(NULL, "wpa_cli set mbo_cell_capa 1");
    DPRINT_INFO(WFA_OUT, "Inside wfaStaPresetParameters function ...\n");
    if((preset->Ch_Op_Class)!=0)
   {
     printf("\n Operating class= %d \n",preset->Ch_Op_Class);
     printf("\n Channel Pref Number=%d \n",preset->Ch_Pref_Num);
     printf("\n Channel Pref= %d \n",preset->Ch_Pref);
     printf("\n Reason code=%d \n",preset->Ch_Reason_Code);
	sprintf(gCmdStr, "wpa_cli -i wlan0 set non_pref_chan=%d:%d:%d:%d", preset->Ch_Op_Class,preset->Ch_Pref_Num,preset->Ch_Pref,preset->Ch_Reason_Code);
	ret = shell_execute_cmd(NULL, gCmdStr);
   }
    // Implement the function and its sub commands
    infoResp.status = STATUS_COMPLETE;

    wfaEncodeTLV(WFA_STA_PRESET_PARAMETERS_RESP_TLV, 4, (BYTE *)&infoResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + 4;

    return WFA_SUCCESS;
/* 	dutCmdResponse_t *PresetParamsResp = &gGenericResp;
    caStaPresetParameters_t *presetParams = (caStaPresetParameters_t *)caCmdBuf;
    BYTE presetDone = 1;
    int st = 0;
   char cmdStr[128];
   char string[256];
   FILE *tmpfd = NULL;
   long val;
   char *endptr;

    DPRINT_INFO(WFA_OUT, "Inside wfaStaPresetParameters function ...\n");
    wfaEncodeTLV(WFA_STA_PRESET_PARAMETERS_RESP_TLV, 4, (BYTE *)PresetParamsResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + 4;

    return WFA_SUCCESS;
 */  
 
}

int wfaStaSet11n(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    dutCmdResponse_t *v11nParamsResp = &gGenericResp;

    v11nParamsResp->status = STATUS_COMPLETE;
    wfaEncodeTLV(WFA_STA_SET_11N_RESP_TLV, 4, (BYTE *)v11nParamsResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + 4;
    return WFA_SUCCESS;
}
int wfaStaSetWireless(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    dutCmdResponse_t *staWirelessResp = &gGenericResp;

    staWirelessResp->status = STATUS_COMPLETE;
    wfaEncodeTLV(WFA_STA_SET_WIRELESS_RESP_TLV, 4, (BYTE *)staWirelessResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + 4;
    return WFA_SUCCESS;
}

int wfaStaSendADDBA(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    dutCmdResponse_t *staSendADDBAResp = &gGenericResp;
    staSendADDBAResp->status = STATUS_COMPLETE;

    wfaEncodeTLV(WFA_STA_SET_SEND_ADDBA_RESP_TLV, 4, (BYTE *)staSendADDBAResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + 4;
    return WFA_SUCCESS;
}

int wfaStaSetRIFS(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    dutCmdResponse_t *staSetRIFSResp = &gGenericResp;

    wfaEncodeTLV(WFA_STA_SET_RIFS_TEST_RESP_TLV, 4, (BYTE *)staSetRIFSResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + 4;

    return WFA_SUCCESS;
}

int wfaStaSendCoExistMGMT(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    dutCmdResponse_t *staSendMGMTResp = &gGenericResp;

    wfaEncodeTLV(WFA_STA_SEND_COEXIST_MGMT_RESP_TLV, 4, (BYTE *)staSendMGMTResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + 4;

    return WFA_SUCCESS;

}

int wfaStaResetDefault(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    caStaResetDefault_t *reset = (caStaResetDefault_t *)caCmdBuf;
    dutCmdResponse_t *ResetResp = &gGenericResp;


    // need to make your own command available for this, here is only an example
//    sprintf(gCmdStr, "myresetdefault %s program %s", reset->intf, reset->prog);
   // sret = shell_execute_cmd(NULL,"killall -9 wpa_supplicant");
//    sprintf(gCmdStr,"wpa_supplicant -Dnl80211 -c /etc/no_cfg.conf -i %s -d -K -f log_CALDER_SAE.txt &",reset->intf);
  //  sret=shell_execute_cmd(NULL,gCmdStr);
    //sprintf(gCmdStr, "wpa_cli disable_network 0", reset->intf);
    //sret = shell_execute_cmd(NULL,gCmdStr);
    //printf("\n %s \n",gCmdStr);
    sprintf(gCmdStr, " 'wpa_cli disconnect'", reset->intf);
    sret = shell_execute_cmd(NULL, gCmdStr);
    printf("\n %s \n",gCmdStr);
    sprintf(gCmdStr, " 'wpa_cli set mbo_cell_capa 1'", reset->intf);
    sret = shell_execute_cmd(NULL, gCmdStr);
    printf("\n %s \n",gCmdStr);


    ResetResp->status = STATUS_COMPLETE;
    wfaEncodeTLV(WFA_STA_RESET_DEFAULT_RESP_TLV, 4, (BYTE *)ResetResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + 4;

    return WFA_SUCCESS;
}

#else

int wfaStaTestBedCmd(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    dutCmdResponse_t *staCmdResp = &gGenericResp;

    wfaEncodeTLV(WFA_STA_DISCONNECT_RESP_TLV, 4, (BYTE *)staCmdResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + 4;

    return WFA_SUCCESS;
}
#endif

/*
 * This is used to send a frame or action frame
 */
int wfaStaDevSendFrame(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    /* uncomment it if needed */
    // char *ifname = cmd->intf;
    dutCmdResponse_t *devSendResp = &gGenericResp;
    caStaDevSendFrame_t *sf = (caStaDevSendFrame_t *)caCmdBuf;

    DPRINT_INFO(WFA_OUT, "Inside wfaStaDevSendFrame function ...\n");
    /* processing the frame */
    sret = shell_execute_cmd(NULL, "wpa_cli wnm_bss_query 1");

    devSendResp->status = STATUS_COMPLETE;
    wfaEncodeTLV(WFA_STA_DEV_SEND_FRAME_RESP_TLV, 4, (BYTE *)devSendResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + 4;

    return WFA_SUCCESS;
}

/*
 * This is used to set a temporary MAC address of an interface
 */
int wfaStaSetMacAddr(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    // Uncomment it if needed
    //dutCommand_t *cmd = (dutCommand_t *)caCmdBuf;
    // char *ifname = cmd->intf;
    dutCmdResponse_t *staCmdResp = &gGenericResp;
    // Uncomment it if needed
    //char *macaddr = &cmd->cmdsu.macaddr[0];

    wfaEncodeTLV(WFA_STA_SET_MAC_ADDRESS_RESP_TLV, 4, (BYTE *)staCmdResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + 4;

    return WFA_SUCCESS;
}


int wfaStaDisconnect(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    dutCommand_t *disc = (dutCommand_t *)caCmdBuf;
    char *intf = disc->intf;
    dutCmdResponse_t *staDiscResp = &gGenericResp;

    sprintf(gCmdStr, "wpa_cli disconnect");
	printf("\n %s \n", gCmdStr);		
    sret = shell_execute_cmd(NULL,gCmdStr);

    staDiscResp->status = STATUS_COMPLETE;

    wfaEncodeTLV(WFA_STA_DISCONNECT_RESP_TLV, 4, (BYTE *)staDiscResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + 4;

    return WFA_SUCCESS;
}

/* Execute CLI, read the status from Environment variable */
int wfaExecuteCLI(char *CLI)
{
    char *retstr;

    sret = shell_execute_cmd(NULL,CLI);

    retstr = getenv("WFA_CLI_STATUS");
    printf("cli status %s\n", retstr);
    return atoi(retstr);
}

/* Supporting Functions */
void wfaSendPing(tgPingStart_t *staPing, float *interval, int streamid)
{
    int totalpkts, tos=-1;
    char cmdStr[256];
//    char *addr = staPing->dipaddr;
    char addr[40];
    char bflag[] = "-b";
    char *tmpstr;
    int inum=0;
    int duration,frameSize;
	duration = (int)staPing->duration;
	frameSize = (int)staPing->frameSize;
    totalpkts = (int)(staPing->duration * staPing->frameRate);
    strcpy(addr,staPing->dipaddr);
	int ret;
	printf("duration = %d frameRate = %d\n",staPing->duration,staPing->frameSize);
	stmp = (int)staPing->duration;
    net_icmpv4_register_handler(&ping4_handler);
    	printf("Printing PING OUTPUT\n");
	sprintf(gCmdStr, "net ping -s %d -c %d %s", frameSize, duration, addr);
	ret = shell_execute_cmd(NULL, gCmdStr);
    	printf("Printing PING OUTPUT DONE: %d\n", ret);
}

int wfaStopPing(dutCmdResponse_t *stpResp, int streamid)
{
    char strout[256];
    FILE *tmpfile = NULL;
    char cmdStr[128];
    printf("\nIn func %s :: stream id=%d\n", __func__,streamid);

    stpResp->cmdru.pingStp.sendCnt = stmp-1;
    stpResp->cmdru.pingStp.repliedCnt = count_seq;
    printf("\nCount of the seq_num from NET SHELL is  %d",count_seq);
    printf("wfaStopPing send count %i\n", stpResp->cmdru.pingStp.sendCnt);
    printf("wfaStopPing replied count %i\n", stpResp->cmdru.pingStp.repliedCnt);
    remove_ipv4_ping_handler();
    count_seq = 0;
    return WFA_SUCCESS;
}

/*
 *  * wfaStaSetRFeature():
 *   */

int wfaStaSetRFeature(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
#ifndef CONFIG_USB_DEVICE_STACK
	caStaRFeat_t *rfeat = (caStaRFeat_t *)caCmdBuf;
	dutCmdResponse_t *caResp = &gGenericResp;
#endif
#ifdef CONFIG_USB_DEVICE_STACK
	dutCommand_t *dutCmd = (dutCommand_t *)caCmdBuf;
	caStaRFeat_t *rfeat = &dutCmd->cmdsu.rfeat;
	dutCmdResponse_t *caResp = &gGenericResp;
#endif
	DPRINT_INFO(WFA_OUT, "entering wfaStaSetRFeature ...\n");
#ifndef CONFIG_USB_DEVICE_STACK
	if(strcasecmp(rfeat->prog, "tdls") == 0)
	{

	}
	if(rfeat->prog == 7)
	{
		printf("\n------------INSIDE MBO--------\n");
		printf("\n------------rfeat->cellulardatacap =%d--------\n", rfeat->cellulardatacap);
		sprintf(gCmdStr, "wpa_cli set mbo_cell_capa %d", rfeat->cellulardatacap);
		sret = shell_execute_cmd(NULL, gCmdStr);
		printf("\n %s \n ",gCmdStr);
		sleep(5);
		if (chan_buf2 != NULL)
		{
			sprintf(gCmdStr, "wpa_cli set non_pref_chan %s %s",
					chan_buf1, chan_buf2);
			printf("\n %s \n", gCmdStr);
			sret = shell_execute_cmd(NULL, gCmdStr);
			wFREE(chan_buf1);
			chan_buf1 = NULL;
			wFREE(chan_buf2);
			chan_buf2 = NULL;
		}
		/* sprintf(gCmdStr, "wpa_cli set non_pref_chan %d:%d:%s:%d",
		   rfeat->ch_op_class, rfeat->ch_pref_num, rfeat->ch_pref,
		   rfeat->ch_reason_code);//AJAY
		   sprintf(gCmdStr, "wpa_cli set non_pref_chan 115:48:0:0", ifname);
		   sret = shell_execute_cmd(NULL, gCmdStr);
		   sprintf(gCmdStr, "wpa_cli set non_pref_chan 115:44:1:1", ifname);
		   sret = shell_execute_cmd(NULL,gCmdStr);*/
	}
	else if (rfeat->prog == 11)
	{
		printf("\n------------INSIDE HE--------\n");
		if (rfeat->Twt_Config.Twt_Setup == WFA_TWT_SETUP) {
			int twt_wake_interval_ms = (rfeat->Twt_Config.NominalMinWakeDur * 256)/1000;
			if (twt_wake_interval_ms > 65) {
				twt_wake_interval_ms = (twt_wake_interval_ms * 1024)/1000;
			}
			int twt_interval_ms = ((pow(2, rfeat->Twt_Config.WakeIntervalExp)) * rfeat->Twt_Config.WakeIntervalMantissa)/1000;

			sprintf(gCmdStr, "wifi twt setup %d %d 1 1 %d %d %d %d %d %d", rfeat->Twt_Config.NegotiationType,
					rfeat->Twt_Config.SetupCommand, rfeat->Twt_Config.RespPMMode,
					rfeat->Twt_Config.TWT_Trigger, rfeat->Twt_Config.Implicit,
					rfeat->Twt_Config.FlowType, twt_wake_interval_ms,
					twt_interval_ms);
			printf("\n------------%s--------\n", gCmdStr);
			sret = shell_execute_cmd(NULL, gCmdStr);
		}
		else if (rfeat->Twt_Config.Twt_Setup == WFA_TWT_TEARDOWN) {
			sprintf(gCmdStr, "wifi twt teardown %d 0 1 %d", rfeat->Twt_Config.NegotiationType,
					rfeat->Twt_Config.FlowID);
			printf("\n------------%s--------\n", gCmdStr);
			sret = shell_execute_cmd(NULL, gCmdStr);
		}
	}
#endif
#ifdef CONFIG_USB_DEVICE_STACK
	DPRINT_INFO(WFA_OUT, "Inside Netusb ...\n");
	if(strcmp(rfeat->prog, "mbo") == 0){
		DPRINT_INFO(WFA_OUT, "Inside MBO ...\n");
		sprintf(gCmdStr, "wpa_cli set mbo_cell_capa %d", rfeat->cellulardatacap);
		sret = shell_execute_cmd(NULL, gCmdStr);
		printf("\n %s \n ",gCmdStr);
		sleep(5);
		if (chan_buf2 != NULL)
		{
			sprintf(gCmdStr, "wpa_cli set non_pref_chan %s %s",
					chan_buf1, chan_buf2);
			printf("\n %s \n", gCmdStr);
			sret = shell_execute_cmd(NULL, gCmdStr);
			wFREE(chan_buf1);
			chan_buf1 = NULL;
			wFREE(chan_buf2);
			chan_buf2 = NULL;
		}
	}
	else if(strcmp(rfeat->prog, "HE") == 0){
		DPRINT_INFO(WFA_OUT, "Inside HE ...\n");
		printf("%s\n",rfeat->prog);
		printf("%s\n",rfeat->ppdutxtype);
		if((rfeat->he_ltf) && (rfeat->he_gi))
               {
                       int ltf,gi;
                       sprintf(gCmdStr, "wifi_util set_he_ltf_gi 1");
                       printf("\n------------%s--------\n", gCmdStr);
                       sret = shell_execute_cmd(NULL, gCmdStr);
                       if((rfeat->he_ltf) == 3.2f)
                       {
                               ltf = 0;
                       }
                       else if((rfeat->he_ltf) == 6.4f)
                       {
                               ltf = 1;
                       }
                       else if((rfeat->he_ltf) == 12.8f)
                       {
                               ltf = 2;
                       }
                       sprintf(gCmdStr, "wifi_util he_ltf %d",ltf);
                       printf("\n------------%s--------\n", gCmdStr);
                       sret = shell_execute_cmd(NULL, gCmdStr);
                       if((rfeat->he_gi) == 0.8f)
                       {
                               gi = 0;
                       }
                       else if((rfeat->he_gi) == 1.6f)
                       {
                               gi = 1;
                       }
                       else if((rfeat->he_gi) == 3.2f)
                       {
                               gi = 2;
                       }
                       sprintf(gCmdStr, "wifi_util he_gi %d",gi);
                       printf("\n------------%s--------\n", gCmdStr);
                       sret = shell_execute_cmd(NULL, gCmdStr);

               }
		int ppdu_len = strlen(rfeat->ppdutxtype);
		if(ppdu_len!='\0')
		if(rfeat->ppdutxtype)
		{
			int ppdutype,rualloctone;
			rfeat->rualloctone = 0; //ignoring the value from WTS and setting it to 0
			if(strcmp(rfeat->ppdutxtype, "LEGACY") == 0)
			{
				ppdutype = 0;
			}
			else if(strcmp(rfeat->ppdutxtype, "HT") == 0)
			{
				ppdutype = 1;
			}
			else if(strcmp(rfeat->ppdutxtype, "VHT") == 0)
			{
				ppdutype = 2;
			}
			else if(strcmp(rfeat->ppdutxtype, "HE-SU") == 0)
			{
				ppdutype = 3;
			}
			else if(strcmp(rfeat->ppdutxtype, "ER-SU") == 0)
			{
				ppdutype = 4;
			}
			else if(strcmp(rfeat->ppdutxtype, "AUTO") == 0)
			{
				ppdutype = 5;
			}
			sprintf(gCmdStr, "wifi_util tx_rate %d %d",ppdutype,rfeat->rualloctone);
			printf("\n------------%s--------\n", gCmdStr);
			sret = shell_execute_cmd(NULL, gCmdStr);

		}
		if(strcmp(rfeat->twt_setup, "request") == 0){
			int twt_wake_interval_ms = (rfeat->twt_nominalminwakedur * 256)/1000;
			if (twt_wake_interval_ms > 65) {
				twt_wake_interval_ms = (twt_wake_interval_ms * 1024)/1000;
			}
			int twt_interval_ms = ((pow(2, rfeat->twt_wakeintervalexp)) * rfeat->twt_wakeintervalmantissa)/1000;

			sprintf(gCmdStr, "wifi twt setup %d %d 1 1 %d %d %d %d %d %d", rfeat->twt_negotiationtype,
					rfeat->twt_setup_command, rfeat->twt_resppmmode,
					rfeat->twt_trigger, rfeat->twt_implicit,
					rfeat->twt_flowtype, twt_wake_interval_ms,
					twt_interval_ms);
			printf("\n------------%s--------\n", gCmdStr);
			sret = shell_execute_cmd(NULL, gCmdStr);
		}
		else if(strcmp(rfeat->twt_setup, "teardown") == 0){
			sprintf(gCmdStr, "wifi twt teardown %d 0 1 %d", rfeat->twt_negotiationtype,
					rfeat->twt_flowid);
			printf("\n------------%s--------\n", gCmdStr);
			sret = shell_execute_cmd(NULL, gCmdStr);
		}

	}
#endif
	caResp->status = STATUS_COMPLETE;
	wfaEncodeTLV(WFA_STA_SET_RFEATURE_RESP_TLV, 4, (BYTE *)caResp, respBuf);
	*respLen = WFA_TLV_HDR_LEN + 4;

	return WFA_SUCCESS;
}

static void handle_wfa_scan_result(struct net_mgmt_event_callback *cb)
{
	const struct wifi_scan_result *entry =
		(const struct wifi_scan_result *)cb->info;
	uint8_t mac_string_buf[sizeof("xx:xx:xx:xx:xx:xx")];
	int ret = 0;

	if (!entry) {
		return;
	}

	scan_result_offset = snprintf(scan_results_buffer + scan_result_offset,
				MAX_SCAN_RESULT_SIZE - scan_result_offset,
				"SSID%d,%s,BSSID%d,%s,",
				scan_result_index,
				entry->ssid,
				scan_result_index,
				entry->mac_length ?
					net_sprint_ll_addr_buf(entry->mac, WIFI_MAC_ADDR_LEN, mac_string_buf, sizeof(mac_string_buf)) :
					"00:00:00:00:00:00");
	if (scan_result_offset < 0) {
		printf("Failed to print scan results: %d\n", scan_result_offset);
		return;
	}
	scan_result_index++;
}

static void handle_wfa_scan_done(struct net_mgmt_event_callback *cb)
{
	const struct wifi_status *status =
		(const struct wifi_status *)cb->info;

	if (status->status) {
		printf("Scan request failed (%d)\n", status->status);
	} else {
		printf("Scan request done\n");
	}
	scan_result_index = 0;
	scan_result_offset = 0;
	k_sem_give(&sem_scan);
}

void wfa_mgmt_event_handler(struct net_mgmt_event_callback *cb,
		uint32_t mgmt_event, struct net_if *iface)
{
	switch (mgmt_event) {
		case NET_EVENT_WIFI_SCAN_RESULT:
			handle_wfa_scan_result(cb);
			break;
		case NET_EVENT_WIFI_SCAN_DONE:
			handle_wfa_scan_done(cb);
			break;
		default:
			break;
	}
}

static int wfa_scan(void)
{
	struct net_if *iface = net_if_get_default();

	if (net_mgmt(NET_REQUEST_WIFI_SCAN, iface, NULL, 0)) {
		printf("Scan request failed\n");
		return -1;
	}

	printf("Scan requested\n");

	return 0;
}

int wfaStaScan(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
	dutCmdResponse_t *infoResp = &gGenericResp;
	caStaScan_t *staScan = (caStaScan_t *)caCmdBuf;
	int ret = 0;

	printf("\n Entry wfaStaScan ...\n ");

	memset(scan_results_buffer, 0, ARRAY_SIZE(scan_results_buffer));
	memset(infoResp->cmdru.execAction.scan_res_buf, 0, 256);
	ret = wfa_scan();

	if (ret || k_sem_take(&sem_scan, K_SECONDS(SCAN_RESULTS_TIMEOUT))) {
		infoResp->status = STATUS_ERROR;
	} else {
		memcpy(infoResp->cmdru.execAction.scan_res_buf, scan_results_buffer, ARRAY_SIZE(scan_results_buffer));
		infoResp->status = STATUS_COMPLETE;
	}

	wfaEncodeTLV(WFA_STA_SCAN_RESP_TLV, sizeof(*infoResp), (BYTE *)infoResp, respBuf);
	*respLen = WFA_TLV_HDR_LEN + sizeof(*infoResp);

	return WFA_SUCCESS;
}

#define RSSI_FAILED 0
int get_wifi_rssi()
{
	struct net_if *iface = net_if_get_default();
	struct wifi_iface_status status = { 0 };

	if (net_mgmt(NET_REQUEST_WIFI_IFACE_STATUS, iface, &status,
				sizeof(struct wifi_iface_status))) {
		printf("%s: Status request failed\n", __func__);
		return RSSI_FAILED;
	}
    printf("RSSI = %d\n", status.rssi);
    return status.rssi;
}

int wfaStaGetParameter(int len, BYTE *caCmdBuf, int *respLen, BYTE *respBuf)
{
    dutCmdResponse_t infoResp;
    caStaGetParameter_t *staGetParam= (caStaGetParameter_t *)caCmdBuf; //uncomment and use it


    caStaGetParameterResp_t *paramList = &infoResp.cmdru.getParamValue;

    printf("\n Entry wfaStaGetParameter... ");

    // Check the program type
    if(staGetParam->program == PROG_TYPE_WFD)
    {
        if(staGetParam->getParamValue == eDiscoveredDevList )
        {
            // Get the discovered devices, make space seperated list and return, check list is not bigger than 128 bytes.
            paramList->getParamType = eDiscoveredDevList;
            strcpy((char *)&paramList->devList, "11:22:33:44:55:66 22:33:44:55:66:77 33:44:55:66:77:88");
        }
    }

    if(staGetParam->program == PROG_TYPE_WFDS)
    {
        if(staGetParam->getParamValue == eDiscoveredDevList )
        {
            // Get the discovered devices, make space seperated list and return, check list is not bigger than 128 bytes.
            paramList->getParamType = eDiscoveredDevList;
            strcpy((char *)&paramList->devList, "11:22:33:44:55:66 22:33:44:55:66:77 33:44:55:66:77:88");
        }
        if(staGetParam->getParamValue == eOpenPorts)
        {
            // Run the port checker tool
            // Get all the open ports and make space seperated list and return, check list is not bigger than 128 bytes.
            paramList->getParamType = eOpenPorts;
            strcpy((char *)&paramList->devList, "22 139 445 68 9700");
        }
    }

    if(staGetParam->program == PROG_TYPE_NAN)
    {
        if(staGetParam->getParamValue == eMasterPref )
        {
            // Get the master preference of the device and return the value
            paramList->getParamType = eMasterPref;
            strcpy((char *)&paramList->masterPref, "0xff");
        }
    }

    if (staGetParam->program == PROG_TYPE_HE)
    {
        if (staGetParam->getParamValue == eRSSI)
        {
            // Get the RSSI of the device and return the value
            paramList->getParamType = eRSSI;
            signed char rssi = get_wifi_rssi();
            snprintf((char *)&paramList->rssi, sizeof(paramList->rssi), "%d", rssi);
            printf("\n RSSI value is %s \n", paramList->rssi);
        }
    }

    infoResp.status = STATUS_COMPLETE;
    wfaEncodeTLV(WFA_STA_GET_PARAMETER_RESP_TLV, sizeof(infoResp), (BYTE *)&infoResp, respBuf);
    *respLen = WFA_TLV_HDR_LEN + sizeof(infoResp);

   return WFA_SUCCESS;
}

