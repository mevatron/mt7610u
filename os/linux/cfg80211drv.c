/*
 *************************************************************************
 * Ralink Tech Inc.
 * 5F., No.36, Taiyuan St., Jhubei City,
 * Hsinchu County 302,
 * Taiwan, R.O.C.
 *
 * (c) Copyright 2002-2010, Ralink Technology, Inc.
 *
 * This program is free software; you can redistribute it and/or modify  *
 * it under the terms of the GNU General Public License as published by  *
 * the Free Software Foundation; either version 2 of the License, or     *
 * (at your option) any later version.                                   *
 *                                                                       *
 * This program is distributed in the hope that it will be useful,       *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 * GNU General Public License for more details.                          *
 *                                                                       *
 * You should have received a copy of the GNU General Public License     *
 * along with this program; if not, write to the                         *
 * Free Software Foundation, Inc.,                                       *
 * 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 *                                                                       *
 *************************************************************************/


#ifdef RT_CFG80211_SUPPORT

#include "rt_config.h"

#define RT_CFG80211_DEBUG /* debug use */
#define CFG80211CB			(pAd->pCfg80211_CB)

#ifdef RT_CFG80211_DEBUG
#define CFG80211DBG(__Flg, __pMsg)		DBGPRINT(__Flg, __pMsg)
#else
#define CFG80211DBG(__Flg, __pMsg)
#endif /* RT_CFG80211_DEBUG */


extern INT RtmpIoctl_rt_ioctl_siwauth(
	IN      struct rtmp_adapter                   *pAd,
	IN      void                            *pData,
	IN      ULONG                            Data);

extern INT RtmpIoctl_rt_ioctl_siwauth(
	IN      struct rtmp_adapter                   *pAd,
	IN      void                            *pData,
	IN      ULONG                            Data);

INT CFG80211DRV_IoctlHandle(
	IN	void 				*pAdSrc,
	IN	RTMP_IOCTL_INPUT_STRUCT	*wrq,
	IN	INT						cmd,
	IN	USHORT					subcmd,
	IN	void 				*pData,
	IN	ULONG					Data)
{
	struct rtmp_adapter *pAd = (struct rtmp_adapter *)pAdSrc;


	switch(cmd)
	{
		case CMD_RTPRIV_IOCTL_80211_START:
		case CMD_RTPRIV_IOCTL_80211_END:
			/* nothing to do */
			break;

		case CMD_RTPRIV_IOCTL_80211_CB_GET:
			*(void **)pData = (void *)(pAd->pCfg80211_CB);
			break;

		case CMD_RTPRIV_IOCTL_80211_CB_SET:
			pAd->pCfg80211_CB = pData;
			break;

		case CMD_RTPRIV_IOCTL_80211_CHAN_SET:
			if (CFG80211DRV_OpsSetChannel(pAd, pData) != true)
				return NDIS_STATUS_FAILURE;
			break;

		case CMD_RTPRIV_IOCTL_80211_VIF_CHG:
			if (CFG80211DRV_OpsChgVirtualInf(pAd, pData, Data) != true)
				return NDIS_STATUS_FAILURE;
			break;

		case CMD_RTPRIV_IOCTL_80211_SCAN:
			CFG80211DRV_OpsScan(pAd);
			break;

		case CMD_RTPRIV_IOCTL_80211_IBSS_JOIN:
			CFG80211DRV_OpsJoinIbss(pAd, pData);
			break;

		case CMD_RTPRIV_IOCTL_80211_STA_LEAVE:
			CFG80211DRV_OpsLeave(pAd);
			break;

		case CMD_RTPRIV_IOCTL_80211_STA_GET:
			if (CFG80211DRV_StaGet(pAd, pData) != true)
				return NDIS_STATUS_FAILURE;
			break;

		case CMD_RTPRIV_IOCTL_80211_KEY_ADD:
			CFG80211DRV_KeyAdd(pAd, pData);
			break;

		case CMD_RTPRIV_IOCTL_80211_KEY_DEFAULT_SET:
#ifdef CONFIG_STA_SUPPORT
			pAd->StaCfg.DefaultKeyId = Data; /* base 0 */
#endif /* CONFIG_STA_SUPPORT */
			break;

		case CMD_RTPRIV_IOCTL_80211_CONNECT_TO:
			CFG80211DRV_Connect(pAd, pData);
			break;

#ifdef RFKILL_HW_SUPPORT
		case CMD_RTPRIV_IOCTL_80211_RFKILL:
		{
			u32 data = 0;
			bool active;

			/* Read GPIO pin2 as Hardware controlled radio state */
			mt7610u_read32(pAd, GPIO_CTRL_CFG, &data);
			active = !!(data & 0x04);

			if (!active)
			{
				RTMPSetLED(pAd, LED_RADIO_OFF);
				*(u8 *)pData = 0;
			}
			else
				*(u8 *)pData = 1;
		}
			break;
#endif /* RFKILL_HW_SUPPORT */

		case CMD_RTPRIV_IOCTL_80211_REG_NOTIFY_TO:
			CFG80211DRV_RegNotify(pAd, pData);
			break;

		case CMD_RTPRIV_IOCTL_80211_UNREGISTER:
			CFG80211_UnRegister(pAd, pData);
			break;

		case CMD_RTPRIV_IOCTL_80211_BANDINFO_GET:
		{
			CFG80211_BAND *pBandInfo = (CFG80211_BAND *)pData;
			CFG80211_BANDINFO_FILL(pAd, pBandInfo);
		}
			break;

		case CMD_RTPRIV_IOCTL_80211_SURVEY_GET:
			CFG80211DRV_SurveyGet(pAd, pData);
			break;

#ifdef RT_P2P_SPECIFIC_WIRELESS_EVENT
		case CMD_RTPRIV_IOCTL_80211_SEND_WIRELESS_EVENT:
			CFG80211_SendWirelessEvent(pAd, pData);
			break;
#endif /* RT_P2P_SPECIFIC_WIRELESS_EVENT */

		default:
			return NDIS_STATUS_FAILURE;
	}

	return NDIS_STATUS_SUCCESS;
}


bool CFG80211DRV_OpsSetChannel(
	void 					*pAdOrg,
	void 					*pData)
{
	struct rtmp_adapter *pAd = (struct rtmp_adapter *)pAdOrg;
	CMD_RTPRIV_IOCTL_80211_CHAN *pChan;
	u8 ChanId;
	u8 IfType;
	u8 ChannelType;
	STRING ChStr[5] = { 0 };
	u8 BW_Old;
	bool FlgIsChanged;


	/* init */
	pChan = (CMD_RTPRIV_IOCTL_80211_CHAN *)pData;
	ChanId = pChan->ChanId;
	IfType = pChan->IfType;
	ChannelType = pChan->ChanType;

	if (IfType != RT_CMD_80211_IFTYPE_MONITOR)
	{
		/* get channel BW */
		FlgIsChanged = false;
		BW_Old = pAd->CommonCfg.RegTransmitSetting.field.BW;

		/* set to new channel BW */
		if (ChannelType == RT_CMD_80211_CHANTYPE_HT20)
		{
			pAd->CommonCfg.RegTransmitSetting.field.BW = BW_20;
			FlgIsChanged = true;
		}
		else if ((ChannelType == RT_CMD_80211_CHANTYPE_HT40MINUS) ||
				(ChannelType == RT_CMD_80211_CHANTYPE_HT40PLUS))
		{
			/* not support NL80211_CHAN_HT40MINUS or NL80211_CHAN_HT40PLUS */
			/* i.e. primary channel = 36, secondary channel must be 40 */
			pAd->CommonCfg.RegTransmitSetting.field.BW = BW_40;
			FlgIsChanged = true;
		} /* End of if */

		CFG80211DBG(RT_DEBUG_ERROR, ("80211> New BW = %d\n",
					pAd->CommonCfg.RegTransmitSetting.field.BW));

		/* change HT/non-HT mode (do NOT change wireless mode here) */
		if (((ChannelType == RT_CMD_80211_CHANTYPE_NOHT) &&
			(pAd->CommonCfg.HT_Disable == 0)) ||
			((ChannelType != RT_CMD_80211_CHANTYPE_NOHT) &&
			(pAd->CommonCfg.HT_Disable == 1)))
		{
			if (ChannelType == RT_CMD_80211_CHANTYPE_NOHT)
				pAd->CommonCfg.HT_Disable = 1;
			else
				pAd->CommonCfg.HT_Disable = 0;
			/* End of if */

			FlgIsChanged = true;
			CFG80211DBG(RT_DEBUG_ERROR, ("80211> HT Disable = %d\n",
						pAd->CommonCfg.HT_Disable));
		} /* End of if */
	}
	else
	{
		/* for monitor mode */
		FlgIsChanged = true;
		pAd->CommonCfg.HT_Disable = 0;
		pAd->CommonCfg.RegTransmitSetting.field.BW = BW_40;
	} /* End of if */

	if (FlgIsChanged == true)
		SetCommonHT(pAd);
	/* End of if */

	/* switch to the channel */
	sprintf(ChStr, "%d", ChanId);
	if (Set_Channel_Proc(pAd, ChStr) == false)
	{
		CFG80211DBG(RT_DEBUG_ERROR, ("80211> Change channel fail!\n"));
	} /* End of if */

#ifdef CONFIG_STA_SUPPORT
	if ((IfType == RT_CMD_80211_IFTYPE_STATION) && (FlgIsChanged == true))
	{
		/*
			1. Station mode;
			2. New BW settings is 20MHz but current BW is not 20MHz;
			3. New BW settings is 40MHz but current BW is 20MHz;

			Re-connect to the AP due to BW 20/40 or HT/non-HT change.
		*/
		Set_SSID_Proc(pAd, (char *)pAd->CommonCfg.Ssid);
	} /* End of if */

	if (IfType == RT_CMD_80211_IFTYPE_ADHOC)
	{
		/* update IBSS beacon */
		MlmeUpdateTxRates(pAd, false, 0);
		MakeIbssBeacon(pAd);
		AsicEnableIbssSync(pAd);

		Set_SSID_Proc(pAd, (char *)pAd->CommonCfg.Ssid);
	} /* End of if */

	if (IfType == RT_CMD_80211_IFTYPE_MONITOR)
	{
		/* reset monitor mode in the new channel */
		Set_NetworkType_Proc(pAd, "Monitor");
		mt7610u_write32(pAd, RX_FILTR_CFG, pChan->MonFilterFlag);
	} /* End of if */
#endif /* CONFIG_STA_SUPPORT */

	return true;
}


bool CFG80211DRV_OpsChgVirtualInf(
	void 					*pAdOrg,
	void 					*pFlgFilter,
	u8						IfType)
{
#ifdef CONFIG_STA_SUPPORT
	struct rtmp_adapter *pAd = (struct rtmp_adapter *)pAdOrg;
	u32 FlgFilter = *(u32 *)pFlgFilter;


	/* change type */
	if (IfType == RT_CMD_80211_IFTYPE_ADHOC)
		Set_NetworkType_Proc(pAd, "Adhoc");
	else if (IfType == RT_CMD_80211_IFTYPE_STATION)
		Set_NetworkType_Proc(pAd, "Infra");
	else if (IfType == RT_CMD_80211_IFTYPE_MONITOR)
	{
		/* set packet filter */
		Set_NetworkType_Proc(pAd, "Monitor");

		if (FlgFilter != 0)
		{
			u32 Filter;


			Filter = mt7610u_read32(pAd, RX_FILTR_CFG);

			if ((FlgFilter & RT_CMD_80211_FILTER_FCSFAIL) == \
												RT_CMD_80211_FILTER_FCSFAIL)
			{
				Filter = Filter & (~0x01);
			}
			else
				Filter = Filter | 0x01;
			/* End of if */

			if ((FlgFilter & RT_CMD_80211_FILTER_PLCPFAIL) == \
												RT_CMD_80211_FILTER_PLCPFAIL)
			{
				Filter = Filter & (~0x02);
			}
			else
				Filter = Filter | 0x02;
			/* End of if */

			if ((FlgFilter & RT_CMD_80211_FILTER_CONTROL) == \
												RT_CMD_80211_FILTER_CONTROL)
			{
				Filter = Filter & (~0xFF00);
			}
			else
				Filter = Filter | 0xFF00;
			/* End of if */

			if ((FlgFilter & RT_CMD_80211_FILTER_OTHER_BSS) == \
												RT_CMD_80211_FILTER_OTHER_BSS)
			{
				Filter = Filter & (~0x08);
			}
			else
				Filter = Filter | 0x08;
			/* End of if */

			mt7610u_write32(pAd, RX_FILTR_CFG, Filter);
			*(u32 *)pFlgFilter = Filter;
		} /* End of if */

		return true; /* not need to set SSID */
	} /* End of if */

	CFG80211DBG(RT_DEBUG_ERROR, ("80211> SSID = %s\n", pAd->CommonCfg.Ssid));
	Set_SSID_Proc(pAd, (char *)pAd->CommonCfg.Ssid);
#endif /* CONFIG_STA_SUPPORT */

	return true;
}


bool CFG80211DRV_OpsScan(
	void 					*pAdOrg)
{
	struct rtmp_adapter *pAd = (struct rtmp_adapter *)pAdOrg;


	if (pAd->FlgCfg80211Scanning == true)
		return false; /* scanning */
	/* End of if */

	/* do scan */
	pAd->FlgCfg80211Scanning = true;
	return true;
}


bool CFG80211DRV_OpsJoinIbss(
	void 					*pAdOrg,
	void 					*pData)
{
#ifdef CONFIG_STA_SUPPORT
	struct rtmp_adapter *pAd = (struct rtmp_adapter *)pAdOrg;
	CMD_RTPRIV_IOCTL_80211_IBSS *pIbssInfo;


	pIbssInfo = (CMD_RTPRIV_IOCTL_80211_IBSS *)pData;

	pAd->CommonCfg.BeaconPeriod = pIbssInfo->BeaconInterval;
	Set_SSID_Proc(pAd, pIbssInfo->Ssid);
#endif /* CONFIG_STA_SUPPORT */
	return true;
}


bool CFG80211DRV_OpsLeave(
	void 					*pAdOrg)
{
#ifdef CONFIG_STA_SUPPORT
	struct rtmp_adapter *pAd = (struct rtmp_adapter *)pAdOrg;


	pAd->FlgCfg80211Connecting = false;
	LinkDown(pAd, false);
#endif /* CONFIG_STA_SUPPORT */
	return true;
}


bool CFG80211DRV_StaGet(
	void 					*pAdOrg,
	void 					*pData)
{
	struct rtmp_adapter *pAd = (struct rtmp_adapter *)pAdOrg;
	CMD_RTPRIV_IOCTL_80211_STA *pIbssInfo;


	pIbssInfo = (CMD_RTPRIV_IOCTL_80211_STA *)pData;

#ifdef CONFIG_STA_SUPPORT
{
	HTTRANSMIT_SETTING PhyInfo;
	ULONG DataRate = 0;
	u32 RSSI;


	/* fill tx rate */
    if ((!WMODE_CAP_N(pAd->CommonCfg.PhyMode)) ||
	 (pAd->MacTab.Content[BSSID_WCID].HTPhyMode.field.MODE <= MODE_OFDM))
	{
		PhyInfo.word = pAd->StaCfg.HTPhyMode.word;
	}
    else
		PhyInfo.word = pAd->MacTab.Content[BSSID_WCID].HTPhyMode.word;
	/* End of if */

	getRate(PhyInfo, &DataRate);

	if ((PhyInfo.field.MODE == MODE_HTMIX) ||
		(PhyInfo.field.MODE == MODE_HTGREENFIELD))
	{
		if (PhyInfo.field.BW)
			pIbssInfo->TxRateFlags |= RT_CMD_80211_TXRATE_BW_40;
		/* End of if */
		if (PhyInfo.field.ShortGI)
			pIbssInfo->TxRateFlags |= RT_CMD_80211_TXRATE_SHORT_GI;
		/* End of if */

		pIbssInfo->TxRateMCS = PhyInfo.field.MCS;
	}
	else
	{
		pIbssInfo->TxRateFlags = RT_CMD_80211_TXRATE_LEGACY;
		pIbssInfo->TxRateMCS = DataRate*10; /* unit: 100kbps */
	} /* End of if */

	/* fill signal */
	RSSI = (pAd->StaCfg.RssiSample.AvgRssi0 +
			pAd->StaCfg.RssiSample.AvgRssi1 +
			pAd->StaCfg.RssiSample.AvgRssi2) / 3;
	pIbssInfo->Signal = RSSI;
}
#endif /* CONFIG_STA_SUPPORT */

	return true;
}


bool CFG80211DRV_KeyAdd(
	void 					*pAdOrg,
	void 					*pData)
{
#ifdef CONFIG_STA_SUPPORT
	struct rtmp_adapter *pAd = (struct rtmp_adapter *)pAdOrg;
	CMD_RTPRIV_IOCTL_80211_KEY *pKeyInfo;


	pKeyInfo = (CMD_RTPRIV_IOCTL_80211_KEY *)pData;

	/*if (pKeyInfo->KeyType == RT_CMD_80211_KEY_WEP)
	{
		switch(pKeyInfo->KeyId)
		{
			case 1:
			default:
				Set_Key1_Proc(pAd, (char *)pKeyInfo->KeyBuf);
				break;

			case 2:
				Set_Key2_Proc(pAd, (char *)pKeyInfo->KeyBuf);
				break;

			case 3:
				Set_Key3_Proc(pAd, (char *)pKeyInfo->KeyBuf);
				break;

			case 4:
				Set_Key4_Proc(pAd, (char *)pKeyInfo->KeyBuf);
				break;
		}
	}
	else
		Set_WPAPSK_Proc(pAd, (char *)pKeyInfo->KeyBuf);*/

	if (pKeyInfo->KeyType == RT_CMD_80211_KEY_WEP40 || pKeyInfo->KeyType == RT_CMD_80211_KEY_WEP104)
	{
		DBGPRINT(RT_DEBUG_TRACE, ("RT_CMD_80211_KEY_WEP\n"));
	}
	else
	{
		RT_CMD_STA_IOCTL_SECURITY IoctlSec;

		DBGPRINT(RT_DEBUG_TRACE, ("Set_WPAPSK_Proc ==> %d, %d, %d...\n", pKeyInfo->KeyId, pKeyInfo->KeyType, (int) strlen(pKeyInfo->KeyBuf)));

		IoctlSec.KeyIdx = pKeyInfo->KeyId;
		IoctlSec.pData = pKeyInfo->KeyBuf;
		IoctlSec.length = pKeyInfo->KeyLen;

		/* YF@20120327: Due to WepStatus will be set in the cfg connect function.*/
		if (pAd->StaCfg.wdev.WepStatus == Ndis802_11Encryption2Enabled)
			IoctlSec.Alg = RT_CMD_STA_IOCTL_SECURITY_ALG_TKIP;
		else if (pAd->StaCfg.wdev.WepStatus == Ndis802_11Encryption3Enabled)
			IoctlSec.Alg = RT_CMD_STA_IOCTL_SECURITY_ALG_CCMP;
		IoctlSec.flags = RT_CMD_STA_IOCTL_SECURITY_ENABLED;
		if (pKeyInfo->bPairwise == false )
		{
			if (pAd->StaCfg.GroupCipher == Ndis802_11Encryption2Enabled)
				IoctlSec.Alg = RT_CMD_STA_IOCTL_SECURITY_ALG_TKIP;
			else if (pAd->StaCfg.GroupCipher == Ndis802_11Encryption3Enabled)
				IoctlSec.Alg = RT_CMD_STA_IOCTL_SECURITY_ALG_CCMP;

			DBGPRINT(RT_DEBUG_TRACE, ("Install GTK: %d\n", IoctlSec.Alg));
			IoctlSec.ext_flags = RT_CMD_STA_IOCTL_SECURTIY_EXT_GROUP_KEY;
		}
		else
		{
			if (pAd->StaCfg.PairCipher == Ndis802_11Encryption2Enabled)
				IoctlSec.Alg = RT_CMD_STA_IOCTL_SECURITY_ALG_TKIP;
			else if (pAd->StaCfg.PairCipher == Ndis802_11Encryption3Enabled)
				IoctlSec.Alg = RT_CMD_STA_IOCTL_SECURITY_ALG_CCMP;

			DBGPRINT(RT_DEBUG_TRACE, ("Install PTK: %d\n", IoctlSec.Alg));
			IoctlSec.ext_flags = RT_CMD_STA_IOCTL_SECURTIY_EXT_SET_TX_KEY;
		}

		/*Set_GroupKey_Proc(pAd, &IoctlSec) */
		RTMP_STA_IoctlHandle(pAd, NULL, CMD_RTPRIV_IOCTL_STA_SIOCSIWENCODEEXT, 0,
							  &IoctlSec, 0, INT_MAIN);
	}

#endif /* CONFIG_STA_SUPPORT */

	return true;
}


bool CFG80211DRV_Connect(
	void 					*pAdOrg,
	void 					*pData)
{
#ifdef CONFIG_STA_SUPPORT
	struct rtmp_adapter *pAd = (struct rtmp_adapter *)pAdOrg;
	CMD_RTPRIV_IOCTL_80211_CONNECT *pConnInfo;
	u8 SSID[NDIS_802_11_LENGTH_SSID + 1]; /* Add One for SSID_Len == 32 */
	u32 SSIDLen;
	RT_CMD_STA_IOCTL_SECURITY_ADV IoctlWpa;

	if (OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_INFRA_ON) &&
            OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_MEDIA_STATE_CONNECTED))
	{
		DBGPRINT(RT_DEBUG_TRACE, ("CFG80211: Connected, disconnect first !\n"));
	}
	else
	{
		DBGPRINT(RT_DEBUG_TRACE, ("CFG80211: No Connection\n"));
	}

	pConnInfo = (CMD_RTPRIV_IOCTL_80211_CONNECT *)pData;

	/* change to infrastructure mode if we are in ADHOC mode */
	Set_NetworkType_Proc(pAd, "Infra");

	SSIDLen = pConnInfo->SsidLen;
	if (SSIDLen > NDIS_802_11_LENGTH_SSID)
	{
		SSIDLen = NDIS_802_11_LENGTH_SSID;
	}

	memset(&SSID, 0, sizeof(SSID));
	memcpy(SSID, pConnInfo->Ssid, SSIDLen);

	if (pConnInfo->bWpsConnection)
	{
		DBGPRINT(RT_DEBUG_TRACE, ("WPS Connection onGoing.....\n"));
		/* YF@20120327: Trigger Driver to Enable WPS function. */
		pAd->StaCfg.WpaSupplicantUP |= WPA_SUPPLICANT_ENABLE_WPS;  /* Set_Wpa_Support(pAd, "3") */
		Set_AuthMode_Proc(pAd, "OPEN");
		Set_EncrypType_Proc(pAd, "NONE");
		Set_SSID_Proc(pAd, (char *)SSID);

		return true;
	}
	else
	{
		pAd->StaCfg.WpaSupplicantUP = WPA_SUPPLICANT_ENABLE; /* Set_Wpa_Support(pAd, "1")*/
	}



	/* set authentication mode */
	if (pConnInfo->WpaVer == 2)
	{
		if (pConnInfo->FlgIs8021x == true) {
			DBGPRINT(RT_DEBUG_TRACE, ("WPA2\n"));
			Set_AuthMode_Proc(pAd, "WPA2");
		}
		else
		{
			DBGPRINT(RT_DEBUG_TRACE, ("WPA2PSK\n"));
			Set_AuthMode_Proc(pAd, "WPA2PSK");
		}
	}
	else if (pConnInfo->WpaVer == 1)
	{
		if (pConnInfo->FlgIs8021x == true) {
			DBGPRINT(RT_DEBUG_TRACE, ("WPA\n"));
			Set_AuthMode_Proc(pAd, "WPA");
		}
		else
		{
			DBGPRINT(RT_DEBUG_TRACE, ("WPAPSK\n"));
			Set_AuthMode_Proc(pAd, "WPAPSK");
		}
	}
	else if (pConnInfo->AuthType == Ndis802_11AuthModeAutoSwitch)
		Set_AuthMode_Proc(pAd, "WEPAUTO");
        else if (pConnInfo->AuthType == Ndis802_11AuthModeShared)
		Set_AuthMode_Proc(pAd, "SHARED");
	else
		Set_AuthMode_Proc(pAd, "OPEN");

	CFG80211DBG(RT_DEBUG_TRACE,
				("80211> AuthMode = %d\n", pAd->StaCfg.wdev.AuthMode));


	/* set encryption mode */
	if (pConnInfo->PairwiseEncrypType & RT_CMD_80211_CONN_ENCRYPT_CCMP)
	{
		DBGPRINT(RT_DEBUG_TRACE, ("AES\n"));
		Set_EncrypType_Proc(pAd, "AES");
	}
	else if (pConnInfo->PairwiseEncrypType & RT_CMD_80211_CONN_ENCRYPT_TKIP)
	{
		DBGPRINT(RT_DEBUG_TRACE, ("TKIP\n"));
		Set_EncrypType_Proc(pAd, "TKIP");
	}
	else if (pConnInfo->PairwiseEncrypType & RT_CMD_80211_CONN_ENCRYPT_WEP)
	{
		DBGPRINT(RT_DEBUG_TRACE, ("WEP\n"));
		Set_EncrypType_Proc(pAd, "WEP");
	}
	else
	{
		DBGPRINT(RT_DEBUG_TRACE, ("NONE\n"));
		Set_EncrypType_Proc(pAd, "NONE");
	}

	/* Groupwise Key Information Setting */
	IoctlWpa.flags = RT_CMD_STA_IOCTL_WPA_GROUP;
	if (pConnInfo->GroupwiseEncrypType & RT_CMD_80211_CONN_ENCRYPT_CCMP)
	{
		DBGPRINT(RT_DEBUG_TRACE, ("GTK AES\n"));
		IoctlWpa.value = RT_CMD_STA_IOCTL_WPA_GROUP_CCMP;
		RtmpIoctl_rt_ioctl_siwauth(pAd, &IoctlWpa, 0);
	}
	else if (pConnInfo->GroupwiseEncrypType & RT_CMD_80211_CONN_ENCRYPT_TKIP)
	{
		DBGPRINT(RT_DEBUG_TRACE, ("GTK TKIP\n"));
		IoctlWpa.value = RT_CMD_STA_IOCTL_WPA_GROUP_TKIP;
		RtmpIoctl_rt_ioctl_siwauth(pAd, &IoctlWpa, 0);
	}


	CFG80211DBG(RT_DEBUG_ERROR,
				("80211> EncrypType = %d\n", pAd->StaCfg.WepStatus));


	/* set channel: STATION will auto-scan */

	/* set WEP key */
	/*if (pConnInfo->pKey &&
		((pConnInfo->GroupwiseEncrypType | pConnInfo->PairwiseEncrypType) &
												RT_CMD_80211_CONN_ENCRYPT_WEP))
	{
		u8 KeyBuf[50];

		// reset AuthMode and EncrypType
		Set_AuthMode_Proc(pAd, "SHARED");
		Set_EncrypType_Proc(pAd, "WEP");

		// reset key
#ifdef RT_CFG80211_DEBUG
#endif

		pAd->StaCfg.DefaultKeyId = pConnInfo->KeyIdx; // base 0
		if (pConnInfo->KeyLen >= sizeof(KeyBuf))
			return false;
		memcpy(KeyBuf, pConnInfo->pKey, pConnInfo->KeyLen);
		KeyBuf[pConnInfo->KeyLen] = 0x00;

		CFG80211DBG(RT_DEBUG_ERROR,
					("80211> pAd->StaCfg.DefaultKeyId = %d\n",
					pAd->StaCfg.DefaultKeyId));

		switch(pConnInfo->KeyIdx)
		{
			case 1:
			default:
				Set_Key1_Proc(pAd, (char *)KeyBuf);
				break;

			case 2:
				Set_Key2_Proc(pAd, (char *)KeyBuf);
				break;

			case 3:
				Set_Key3_Proc(pAd, (char *)KeyBuf);
				break;

			case 4:
				Set_Key4_Proc(pAd, (char *)KeyBuf);
				break;
		}
	}*/

	CFG80211DBG(RT_DEBUG_TRACE, ("80211> Key = %s\n", pConnInfo->Key));

	/* set channel: STATION will auto-scan */

	/* set WEP key */
	if (((pConnInfo->GroupwiseEncrypType | pConnInfo->PairwiseEncrypType) &
												RT_CMD_80211_CONN_ENCRYPT_WEP))
	{
		u8 KeyBuf[50];

		/* reset AuthMode and EncrypType */
		Set_EncrypType_Proc(pAd, "WEP");

		/* reset key */
#ifdef RT_CFG80211_DEBUG
#endif /* RT_CFG80211_DEBUG */

		pAd->StaCfg.DefaultKeyId = pConnInfo->KeyIdx; /* base 0 */
		if (pConnInfo->KeyLen >= sizeof(KeyBuf))
			return false;

		memcpy(KeyBuf, pConnInfo->Key, pConnInfo->KeyLen);
		KeyBuf[pConnInfo->KeyLen] = 0x00;

		CFG80211DBG(RT_DEBUG_ERROR,
					("80211> pAd->StaCfg.DefaultKeyId = %d\n",
					pAd->StaCfg.DefaultKeyId));

		Set_Wep_Key_Proc(pAd, (char *)KeyBuf, (INT)pConnInfo->KeyLen, (INT)pConnInfo->KeyIdx);

	} /* End of if */

	/* TODO: We need to provide a command to set BSSID to associate a AP */
	//pAd->cfg80211_ctrl.FlgCfg80211Connecting = true;

	pAd->FlgCfg80211Connecting = true;

	/*SSIDLen = pConnInfo->SsidLen;
	if (SSIDLen > NDIS_802_11_LENGTH_SSID)
		SSIDLen = NDIS_802_11_LENGTH_SSID;


	memset(&SSID, 0, sizeof(SSID));
	memcpy(SSID, pConnInfo->pSsid, SSIDLen);
	Set_SSID_Proc(pAd, (char *)SSID);
	CFG80211DBG(RT_DEBUG_ERROR, ("80211> SSID = %s\n", SSID));*/


	Set_SSID_Proc(pAd, (char *)SSID);
	CFG80211DBG(RT_DEBUG_TRACE, ("80211> Connecting SSID = %s\n", SSID));

#endif /* CONFIG_STA_SUPPORT */

	return true;
}


void CFG80211DRV_RegNotify(
	void 					*pAdOrg,
	void 					*pData)
{
	struct rtmp_adapter *pAd = (struct rtmp_adapter *)pAdOrg;
	CMD_RTPRIV_IOCTL_80211_REG_NOTIFY *pRegInfo;


	pRegInfo = (CMD_RTPRIV_IOCTL_80211_REG_NOTIFY *)pData;

	/* keep Alpha2 and we can re-call the function when interface is up */
	pAd->Cfg80211_Alpha2[0] = pRegInfo->Alpha2[0];
	pAd->Cfg80211_Alpha2[1] = pRegInfo->Alpha2[1];

	/* apply the new regulatory rule */
	if (RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_START_UP))
	{
		/* interface is up */
		CFG80211_RegRuleApply(pAd, pRegInfo->pWiphy, (u8 *)pRegInfo->Alpha2);
	}
	else
	{
		CFG80211DBG(RT_DEBUG_ERROR, ("crda> interface is down!\n"));
	} /* End of if */
}


void CFG80211DRV_SurveyGet(
	void 					*pAdOrg,
	void 					*pData)
{
	struct rtmp_adapter *pAd = (struct rtmp_adapter *)pAdOrg;
	CMD_RTPRIV_IOCTL_80211_SURVEY *pSurveyInfo;


	pSurveyInfo = (CMD_RTPRIV_IOCTL_80211_SURVEY *)pData;

	pSurveyInfo->pCfg80211 = pAd->pCfg80211_CB;

#ifdef AP_QLOAD_SUPPORT
	pSurveyInfo->ChannelTimeBusy = pAd->QloadLatestChannelBusyTimePri;
	pSurveyInfo->ChannelTimeExtBusy = pAd->QloadLatestChannelBusyTimeSec;
#endif /* AP_QLOAD_SUPPORT */
}


void CFG80211_UnRegister(
	IN void 					*pAdOrg,
	IN void 					*pNetDev)
{
	struct rtmp_adapter *pAd = (struct rtmp_adapter *)pAdOrg;


	/* sanity check */
	if (pAd->pCfg80211_CB == NULL)
		return;
	/* End of if */

	CFG80211OS_UnRegister(pAd->pCfg80211_CB, pNetDev);
	pAd->pCfg80211_CB = NULL;
	pAd->CommonCfg.HT_Disable = 0;
}


/*
========================================================================
Routine Description:
	Parse and handle country region in beacon from associated AP.

Arguments:
	pAdCB			- WLAN control block pointer
	pVIE			- Beacon elements
	LenVIE			- Total length of Beacon elements

Return Value:
	NONE

Note:
========================================================================
*/
void CFG80211_BeaconCountryRegionParse(
	IN void 					*pAdCB,
	IN NDIS_802_11_VARIABLE_IEs	*pVIE,
	IN UINT16					LenVIE)
{
	struct rtmp_adapter *pAd = (struct rtmp_adapter *)pAdCB;
	u8 *pElement = (u8 *)pVIE;
	u32 LenEmt;


	while(LenVIE > 0)
	{
		pVIE = (NDIS_802_11_VARIABLE_IEs *)pElement;

		if (pVIE->ElementID == IE_COUNTRY)
		{
			/* send command to do regulation hint only when associated */
			RTEnqueueInternalCmd(pAd, CMDTHREAD_REG_HINT_11D,
								pVIE->data, pVIE->Length);
			break;
		} /* End of if */

		LenEmt = pVIE->Length + 2;

		if (LenVIE <= LenEmt)
			break; /* length is not enough */
		/* End of if */

		pElement += LenEmt;
		LenVIE -= LenEmt;
	} /* End of while */
} /* End of CFG80211_BeaconCountryRegionParse */


/*
========================================================================
Routine Description:
	Hint to the wireless core a regulatory domain from driver.

Arguments:
	pAd				- WLAN control block pointer
	pCountryIe		- pointer to the country IE
	CountryIeLen	- length of the country IE

Return Value:
	NONE

Note:
	Must call the function in kernel thread.
========================================================================
*/
void CFG80211_RegHint(
	IN void 					*pAdCB,
	IN u8 				*pCountryIe,
	IN ULONG					CountryIeLen)
{
	struct rtmp_adapter *pAd = (struct rtmp_adapter *)pAdCB;


	CFG80211OS_RegHint(CFG80211CB, pCountryIe, CountryIeLen);
} /* End of CFG80211_RegHint */


/*
========================================================================
Routine Description:
	Hint to the wireless core a regulatory domain from country element.

Arguments:
	pAdCB			- WLAN control block pointer
	pCountryIe		- pointer to the country IE
	CountryIeLen	- length of the country IE

Return Value:
	NONE

Note:
	Must call the function in kernel thread.
========================================================================
*/
void CFG80211_RegHint11D(
	IN void 					*pAdCB,
	IN u8 				*pCountryIe,
	IN ULONG					CountryIeLen)
{
	/* no regulatory_hint_11d() in 2.6.32 */
	struct rtmp_adapter *pAd = (struct rtmp_adapter *)pAdCB;


	CFG80211OS_RegHint11D(CFG80211CB, pCountryIe, CountryIeLen);
} /* End of CFG80211_RegHint11D */


/*
========================================================================
Routine Description:
	Apply new regulatory rule.

Arguments:
	pAdCB			- WLAN control block pointer
	pWiphy			- Wireless hardware description
	pAlpha2			- Regulation domain (2B)

Return Value:
	NONE

Note:
	Can only be called when interface is up.

	For general mac80211 device, it will be set to new power by Ops->config()
	In rt2x00/, the settings is done in rt2x00lib_config().
========================================================================
*/
void CFG80211_RegRuleApply(
	IN void 					*pAdCB,
	IN void 					*pWiphy,
	IN u8 				*pAlpha2)
{
	struct rtmp_adapter *pAd = (struct rtmp_adapter *)pAdCB;
	void *pBand24G, *pBand5G;
	u32 IdBand, IdChan, IdPwr;
	u32 ChanNum, ChanId, Power, RecId, DfsType;
	bool FlgIsRadar;
	ULONG IrqFlags;
#ifdef DFS_SUPPORT
	RADAR_DETECT_STRUCT	*pRadarDetect;
#endif /* DFS_SUPPORT */


	CFG80211DBG(RT_DEBUG_ERROR, ("crda> CFG80211_RegRuleApply ==>\n"));

	/* init */
	pBand24G = NULL;
	pBand5G = NULL;

	if (pAd == NULL)
		return;

	spin_lock_bh(&pAd->irq_lock);

	/* zero first */
	memset(pAd->ChannelList, 0,
					MAX_NUM_OF_CHANNELS * sizeof(struct CHANNEL_TX_POWER));

	/* 2.4GHZ & 5GHz */
	RecId = 0;
#ifdef DFS_SUPPORT
	pRadarDetect = &pAd->CommonCfg.RadarDetect;
#endif /* DFS_SUPPORT */

	/* find the DfsType */
	DfsType = CE;

	pBand24G = NULL;
	pBand5G = NULL;

	if (CFG80211OS_BandInfoGet(CFG80211CB, pWiphy, &pBand24G, &pBand5G) == false)
		return;

#ifdef AUTO_CH_SELECT_ENHANCE
#ifdef EXT_BUILD_CHANNEL_LIST
	if ((pAlpha2[0] != '0') && (pAlpha2[1] != '0'))
	{
		u32 IdReg;

		if (pBand5G != NULL)
		{
			for(IdReg=0; ; IdReg++)
			{
				if (ChRegion[IdReg].CountReg[0] == 0x00)
					break;
				/* End of if */

				if ((pAlpha2[0] == ChRegion[IdReg].CountReg[0]) &&
					(pAlpha2[1] == ChRegion[IdReg].CountReg[1]))
				{
					if (pAd->CommonCfg.DfsType != MAX_RD_REGION)
						DfsType = pAd->CommonCfg.DfsType;
					else
					DfsType = ChRegion[IdReg].DfsType;

					CFG80211DBG(RT_DEBUG_ERROR,
								("crda> find region %c%c, DFS Type %d\n",
								pAlpha2[0], pAlpha2[1], DfsType));
					break;
				} /* End of if */
			} /* End of for */
		} /* End of if */
	} /* End of if */
#endif /* EXT_BUILD_CHANNEL_LIST */
#endif /* AUTO_CH_SELECT_ENHANCE */

	for(IdBand=0; IdBand<2; IdBand++)
	{
		if (((IdBand == 0) && (pBand24G == NULL)) ||
			((IdBand == 1) && (pBand5G == NULL)))
		{
			continue;
		} /* End of if */

		if (IdBand == 0)
		{
			CFG80211DBG(RT_DEBUG_ERROR, ("crda> reset chan/power for 2.4GHz\n"));
		}
		else
		{
			CFG80211DBG(RT_DEBUG_ERROR, ("crda> reset chan/power for 5GHz\n"));
		} /* End of if */

		ChanNum = CFG80211OS_ChanNumGet(CFG80211CB, pWiphy, IdBand);

		for(IdChan=0; IdChan<ChanNum; IdChan++)
		{
			if (CFG80211OS_ChanInfoGet(CFG80211CB, pWiphy, IdBand, IdChan,
									&ChanId, &Power, &FlgIsRadar) == false)
			{
				/* the channel is not allowed in the regulatory domain */
				/* get next channel information */
				continue;
			}

			if (!WMODE_CAP_2G(pAd->CommonCfg.PhyMode))
			{
				/* 5G-only mode */
				if (ChanId <= CFG80211_NUM_OF_CHAN_2GHZ)
					continue;
			}

			if (!WMODE_CAP_5G(pAd->CommonCfg.PhyMode))
			{
				/* 2.4G-only mode */
				if (ChanId > CFG80211_NUM_OF_CHAN_2GHZ)
					continue;
			}

			for(IdPwr=0; IdPwr<MAX_NUM_OF_CHANNELS; IdPwr++)
			{
				if (ChanId == pAd->TxPower[IdPwr].Channel)
				{
					/* init the channel info. */
					memmove(&pAd->ChannelList[RecId],
									&pAd->TxPower[IdPwr],
									sizeof(struct CHANNEL_TX_POWER));

					/* keep channel number */
					pAd->ChannelList[RecId].Channel = ChanId;

					/* keep maximum tranmission power */
					pAd->ChannelList[RecId].MaxTxPwr = Power;

					/* keep DFS flag */
					if (FlgIsRadar == true)
						pAd->ChannelList[RecId].DfsReq = true;
					else
						pAd->ChannelList[RecId].DfsReq = false;
					/* End of if */

					/* keep DFS type */
					pAd->ChannelList[RecId].RegulatoryDomain = DfsType;

					/* re-set DFS info. */
					pAd->CommonCfg.RDDurRegion = DfsType;

					CFG80211DBG(RT_DEBUG_ERROR,
								("Chan %03d:\tpower %d dBm, "
								"DFS %d, DFS Type %d\n",
								ChanId, Power,
								((FlgIsRadar == true)?1:0),
								DfsType));

					/* change to record next channel info. */
					RecId ++;
					break;
				} /* End of if */
			} /* End of for */
		} /* End of for */
	} /* End of for */

	pAd->ChannelListNum = RecId;
	spin_unlock_bh(&pAd->irq_lock);

	CFG80211DBG(RT_DEBUG_ERROR, ("crda> Number of channels = %d\n", RecId));
} /* End of CFG80211_RegRuleApply */


/*
========================================================================
Routine Description:
	Inform us that a scan is got.

Arguments:
	pAdCB				- WLAN control block pointer

Return Value:
	NONE

Note:
	Call RT_CFG80211_SCANNING_INFORM, not CFG80211_Scaning
========================================================================
*/
void CFG80211_Scaning(
	IN void 						*pAdCB,
	IN u32						BssIdx,
	IN u32						ChanId,
	IN u8 					*pFrame,
	IN u32						FrameLen,
	IN INT32						RSSI)
{
#ifdef CONFIG_STA_SUPPORT
	struct rtmp_adapter *pAd = (struct rtmp_adapter *)pAdCB;
	void *pCfg80211_CB = pAd->pCfg80211_CB;
	bool FlgIsNMode;
	u8 BW;


	CFG80211DBG(RT_DEBUG_ERROR, ("80211> CFG80211_Scaning ==>\n"));

	if (!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_INTERRUPT_IN_USE))
	{
		DBGPRINT(RT_DEBUG_TRACE, ("80211> Network is down!\n"));
		return;
	} /* End of if */

	/*
		In connect function, we also need to report BSS information to cfg80211;
		Not only scan function.
	*/
	if ((pAd->FlgCfg80211Scanning == false) &&
		(pAd->FlgCfg80211Connecting == false))
	{
		return; /* no scan is running */
	} /* End of if */

	/* init */
	/* Note: Can not use local variable to do pChan */
	if (WMODE_CAP_N(pAd->CommonCfg.PhyMode))
		FlgIsNMode = true;
	else
		FlgIsNMode = false;

	if (pAd->CommonCfg.RegTransmitSetting.field.BW == BW_20)
		BW = 0;
	else
		BW = 1;

	CFG80211OS_Scaning(pCfg80211_CB,
						ChanId,
						pFrame,
						FrameLen,
						RSSI,
						FlgIsNMode,
						BW);
#endif /* CONFIG_STA_SUPPORT */
} /* End of CFG80211_Scaning */


/*
========================================================================
Routine Description:
	Inform us that scan ends.

Arguments:
	pAdCB			- WLAN control block pointer
	FlgIsAborted	- 1: scan is aborted

Return Value:
	NONE

Note:
========================================================================
*/
void CFG80211_ScanEnd(
	IN void 					*pAdCB,
	IN bool 				FlgIsAborted)
{
#ifdef CONFIG_STA_SUPPORT
	struct rtmp_adapter *pAd = (struct rtmp_adapter *)pAdCB;


	if (!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_INTERRUPT_IN_USE))
	{
		DBGPRINT(RT_DEBUG_TRACE, ("80211> Network is down!\n"));
		return;
	} /* End of if */

	if (pAd->FlgCfg80211Scanning == false)
	{
		DBGPRINT(RT_DEBUG_TRACE, ("80211> No scan is running!\n"));
		return; /* no scan is running */
	} /* End of if */

	if (FlgIsAborted == true)
		FlgIsAborted = 1;
	else
		FlgIsAborted = 0;
	/* End of if */

	CFG80211OS_ScanEnd(CFG80211CB, FlgIsAborted);

	pAd->FlgCfg80211Scanning = false;
#endif /* CONFIG_STA_SUPPORT */
} /* End of CFG80211_ScanEnd */


/*
========================================================================
Routine Description:
	Inform CFG80211 about association status.

Arguments:
	pAdCB			- WLAN control block pointer
	pBSSID			- the BSSID of the AP
	pReqIe			- the element list in the association request frame
	ReqIeLen		- the request element length
	pRspIe			- the element list in the association response frame
	RspIeLen		- the response element length
	FlgIsSuccess	- 1: success; otherwise: fail

Return Value:
	None

Note:
========================================================================
*/
void CFG80211_ConnectResultInform(
	IN void 					*pAdCB,
	IN u8 				*pBSSID,
	IN u8 				*pReqIe,
	IN u32					ReqIeLen,
	IN u8 				*pRspIe,
	IN u32					RspIeLen,
	IN u8 				FlgIsSuccess)
{
	struct rtmp_adapter *pAd = (struct rtmp_adapter *)pAdCB;


	CFG80211DBG(RT_DEBUG_ERROR, ("80211> CFG80211_ConnectResultInform ==>\n"));

	CFG80211OS_ConnectResultInform(CFG80211CB,
								pBSSID,
								pReqIe,
								ReqIeLen,
								pRspIe,
								RspIeLen,
								FlgIsSuccess);

	pAd->FlgCfg80211Connecting = false;
} /* End of CFG80211_ConnectResultInform */


/*
========================================================================
Routine Description:
	Re-Initialize wireless channel/PHY in 2.4GHZ and 5GHZ.

Arguments:
	pAdCB			- WLAN control block pointer

Return Value:
	true			- re-init successfully
	false			- re-init fail

Note:
	CFG80211_SupBandInit() is called in xx_probe().
	But we do not have complete chip information in xx_probe() so we
	need to re-init bands in xx_open().
========================================================================
*/
bool CFG80211_SupBandReInit(
	IN void 					*pAdCB)
{
	struct rtmp_adapter *pAd = (struct rtmp_adapter *)pAdCB;
	CFG80211_BAND BandInfo;


	CFG80211DBG(RT_DEBUG_ERROR, ("80211> re-init bands...\n"));

	/* re-init bands */
	memset(&BandInfo, 0, sizeof(BandInfo));
	CFG80211_BANDINFO_FILL(pAd, &BandInfo);

	return CFG80211OS_SupBandReInit(CFG80211CB, &BandInfo);
} /* End of CFG80211_SupBandReInit */

#ifdef RT_P2P_SPECIFIC_WIRELESS_EVENT
INT CFG80211_SendWirelessEvent(
	IN void                                         *pAdCB,
	IN u8 					*pMacAddr)
{
	struct rtmp_adapter *pAd = (struct rtmp_adapter *)pAdCB;

	P2pSendWirelessEvent(pAd, RT_P2P_CONNECTED, NULL, pMacAddr);

	return 0;
}
#endif /* RT_P2P_SPECIFIC_WIRELESS_EVENT */


#endif /* RT_CFG80211_SUPPORT */

/* End of cfg80211drv.c */
