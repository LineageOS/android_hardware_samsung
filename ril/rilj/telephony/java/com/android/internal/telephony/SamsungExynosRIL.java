/*
 * Copyright (c) 2014, The CyanogenMod Project. All rights reserved.
 * Copyright (c) 2017, The LineageOS Project. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.android.internal.telephony;

import static com.android.internal.telephony.RILConstants.*;

import android.content.Context;
import android.telephony.Rlog;
import android.os.AsyncResult;
import android.os.Message;
import android.os.Parcel;
import android.os.SystemProperties;
import android.telephony.PhoneNumberUtils;
import com.android.internal.telephony.uicc.IccCardApplicationStatus;
import com.android.internal.telephony.uicc.IccCardStatus;
import com.android.internal.telephony.uicc.IccUtils;
import java.util.ArrayList;
import java.util.Collections;

/**
 * RIL customization for Samsung Exynos devices
 *
 * {@hide}
 */
public class SamsungExynosRIL extends RIL {
    static final boolean RILJ_LOGD = true;
    static final boolean RILJ_LOGV = true;

    public SamsungExynosRIL(Context context, int preferredNetworkType,
                   int cdmaSubscription, Integer instanceId) {
        super(context, preferredNetworkType, cdmaSubscription, instanceId);
    }

    /**********************************************************
     * SAMSUNG RESPONSE
     **********************************************************/
    private static final int SAMSUNG_UNSOL_RESPONSE_BASE = 11000;
    private static final int RIL_UNSOL_STK_SEND_SMS_RESULT = 11002;
    private static final int RIL_UNSOL_AM = 11010;
    private static final int RIL_UNSOL_DUN_PIN_CONTROL_SIGNAL = 11011;
    private static final int RIL_UNSOL_SIM_SWAP_STATE_CHANGED = 11057;

    // Property to determine whether we are using this subclass on a nextgen modem or not.
    protected boolean mSamsungNextGenModem = SystemProperties.getBoolean(
        "ro.ril.samsung_nextgen_modem", false);
    // Property to determine whether the modem needs the videocall field or not.
    protected boolean mNeedsVideocallField = SystemProperties.getBoolean(
        "ro.ril.needs_videocall_field", false);

    @Override
    public void
    acceptCall(Message result) {
        acceptCall(0, result);
    }

    public void
    acceptCall(int index, Message result) {
        RILRequest rr =
            RILRequest.obtain(RIL_REQUEST_ANSWER, result);

        if (RILJ_LOGD) riljLog(rr.serialString() + "> " + requestToString(rr.mRequest));

        rr.mParcel.writeInt(1);
        rr.mParcel.writeInt(index);

        send(rr);
    }

    @Override
    public void
    dial(String address, int clirMode, UUSInfo uusInfo, Message result) {
        RILRequest rr = RILRequest.obtain(RIL_REQUEST_DIAL, result);
        rr.mParcel.writeString(address);
        rr.mParcel.writeInt(clirMode);
        if (mSamsungNextGenModem) { // Samsung CallDetails
            rr.mParcel.writeInt(0);
            rr.mParcel.writeInt(1);
            rr.mParcel.writeString("");
        }
        if (uusInfo == null) {
            rr.mParcel.writeInt(0);
        } else {
            rr.mParcel.writeInt(1);
            rr.mParcel.writeInt(uusInfo.getType());
            rr.mParcel.writeInt(uusInfo.getDcs());
            rr.mParcel.writeByteArray(uusInfo.getUserData());
        }
        if (RILJ_LOGD) riljLog(rr.serialString() + "> " +
                               requestToString(rr.mRequest));
        send(rr);
    }

    @Override
    protected Object
    responseIccCardStatus(Parcel p) {
        IccCardStatus cardStatus = new IccCardStatus();
        cardStatus.setCardState(p.readInt());
        cardStatus.setUniversalPinState(p.readInt());
        cardStatus.mGsmUmtsSubscriptionAppIndex = p.readInt();
        cardStatus.mCdmaSubscriptionAppIndex = p.readInt();
        cardStatus.mImsSubscriptionAppIndex = p.readInt();
        int numApplications = p.readInt();
        if (numApplications > IccCardStatus.CARD_MAX_APPS) {
            numApplications = IccCardStatus.CARD_MAX_APPS;
        }
        cardStatus.mApplications = new IccCardApplicationStatus[numApplications];
        for (int i = 0; i < numApplications; i++) {
            IccCardApplicationStatus appStatus = new IccCardApplicationStatus();
            appStatus.app_type = appStatus.AppTypeFromRILInt(p.readInt());
            appStatus.app_state = appStatus.AppStateFromRILInt(p.readInt());
            appStatus.perso_substate = appStatus.PersoSubstateFromRILInt(p.readInt());
            appStatus.aid = p.readString();
            appStatus.app_label = p.readString();
            appStatus.pin1_replaced = p.readInt();
            appStatus.pin1 = appStatus.PinStateFromRILInt(p.readInt());
            appStatus.pin2 = appStatus.PinStateFromRILInt(p.readInt());
            p.readInt(); // pin1_num_retries
            p.readInt(); // puk1_num_retries
            p.readInt(); // pin2_num_retries
            p.readInt(); // puk2_num_retries
            p.readInt(); // perso_unblock_retries

            cardStatus.mApplications[i] = appStatus;
        }
        return cardStatus;
    }

    @Override
    protected Object
    responseCallList(Parcel p) {
        int num = p.readInt();
        ArrayList<DriverCall> response = new ArrayList(num);
        for (int i = 0; i < num; i++) {
            DriverCall dc = new DriverCall();
            dc.state = DriverCall.stateFromCLCC(p.readInt());
            dc.index = p.readInt() & 0xff;
            dc.TOA = p.readInt();
            dc.isMpty = (p.readInt() != 0);
            dc.isMT = (p.readInt() != 0);
            dc.als = p.readInt();
            dc.isVoice = (p.readInt() == 0);

            if (mNeedsVideocallField) {
                boolean isVideo = (p.readInt() != 0);
            }
            if (mSamsungNextGenModem) { // Samsung CallDetails
                int type = p.readInt();
                int domain = p.readInt();
                String extras = p.readString();
            }

            dc.isVoicePrivacy = (p.readInt() != 0);
            dc.number = p.readString();
            dc.numberPresentation = DriverCall.presentationFromCLIP(p.readInt());
            dc.name = p.readString();
            dc.namePresentation = DriverCall.presentationFromCLIP(p.readInt());
            if (p.readInt() == 1) {
                // UUS Information is present
                dc.uusInfo = new UUSInfo();
                dc.uusInfo.setType(p.readInt());
                dc.uusInfo.setDcs(p.readInt());
                dc.uusInfo.setUserData(p.createByteArray());
                if (RILJ_LOGV) {
                    riljLogv(String.format("Incoming UUS : type=%d, dcs=%d, length=%d",
                             new Object[]{Integer.valueOf(dc.uusInfo.getType()),
                             Integer.valueOf(dc.uusInfo.getDcs()),
                             Integer.valueOf(dc.uusInfo.getUserData().length)}));
                    riljLogv("Incoming UUS : data (string)=" +
                             new String(dc.uusInfo.getUserData()));
                    riljLogv("Incoming UUS : data (hex): " +
                             IccUtils.bytesToHexString(dc.uusInfo.getUserData()));
                }
            } else {
                if (RILJ_LOGV) riljLogv("Incoming UUS : NOT present!");
            }
            dc.number = PhoneNumberUtils.stringFromStringAndTOA(dc.number, dc.TOA);
            response.add(dc);
            if (dc.isVoicePrivacy) {
                mVoicePrivacyOnRegistrants.notifyRegistrants();
                if (RILJ_LOGD) riljLog("InCall VoicePrivacy is enabled");
            } else {
                mVoicePrivacyOffRegistrants.notifyRegistrants();
                if (RILJ_LOGD) riljLog("InCall VoicePrivacy is disabled");
            }
        }
        Collections.sort(response);
        if (num == 0 && mTestingEmergencyCall.getAndSet(false) &&
            mEmergencyCallbackModeRegistrant != null) {
            if (RILJ_LOGD) riljLog("responseCallList: call ended, testing emergency call," +
                                   " notify ECM Registrants");
            mEmergencyCallbackModeRegistrant.notifyRegistrant();
        }
        return response;
    }

    private void constructGsmSendSmsRilRequest(RILRequest rr, String smscPDU, String pdu) {
        rr.mParcel.writeInt(2);
        rr.mParcel.writeString(smscPDU);
        rr.mParcel.writeString(pdu);
    }

    /**
     * The RIL can't handle the RIL_REQUEST_SEND_SMS_EXPECT_MORE
     * request properly, so we use RIL_REQUEST_SEND_SMS instead.
     */
    @Override
    public void sendSMSExpectMore(String smscPDU, String pdu, Message result) {

        RILRequest rr = RILRequest.obtain(RIL_REQUEST_SEND_SMS, result);
        constructGsmSendSmsRilRequest(rr, smscPDU, pdu);

        if (RILJ_LOGD) riljLog(rr.serialString() + "> " + requestToString(rr.mRequest));

        send(rr);
    }

    // This method is used in the search network functionality.
    // See mobile network setting -> network operators
    @Override
    protected Object
    responseOperatorInfos(Parcel p) {
        String strings[] = (String[])responseStrings(p);
        ArrayList<OperatorInfo> ret;

        if (strings.length % mQANElements != 0) {
            throw new RuntimeException("RIL_REQUEST_QUERY_AVAILABLE_NETWORKS: " +
                                       "invalid response. Got " + strings.length +
                                       " strings, expected multiple of " + mQANElements);
        }

        ret = new ArrayList<OperatorInfo>(strings.length / mQANElements);
        for (int i = 0 ; i < strings.length ; i += mQANElements) {
            String strOperatorLong = strings[i+0];
            String strOperatorNumeric = strings[i+2];
            String strState = strings[i+3].toLowerCase();

            Rlog.v(RILJ_LOG_TAG,
                   "SamsungExynosRIL: Add OperatorInfo: " + strOperatorLong +
                   ", " + strOperatorLong +
                   ", " + strOperatorNumeric +
                   ", " + strState);

            ret.add(new OperatorInfo(strOperatorLong, // operatorAlphaLong
                                     strOperatorLong, // operatorAlphaShort
                                     strOperatorNumeric,    // operatorNumeric
                                     strState));  // stateString
        }

        return ret;
    }

    // This method is used to forward non OEM or remapped responses to the super class.
    // Set newResponse to -1 if you dont need to remap the response.
    public void
    superProcessUnsolicited(Parcel p, int type, int dataPosition, int newResponse) {
        p.setDataPosition(dataPosition); // Lets rewind the parcel
        if (newResponse != -1) {
            p.writeInt(newResponse);
        }
        super.processUnsolicited(p, type); // Now lets forward the response to the super class.
    }

    @Override
    protected void
    processUnsolicited(Parcel p, int type) {
        int dataPosition = p.dataPosition(); // Let's save the parcel data position for later
        Object ret;

        int response = p.readInt();
        if (response >= SAMSUNG_UNSOL_RESPONSE_BASE) {
            try {
                switch (response) {
                    case RIL_UNSOL_STK_SEND_SMS_RESULT:
                        ret = responseInts(p);
                        break;
                    case RIL_UNSOL_AM:
                        ret = responseString(p);
                        break;
                    case RIL_UNSOL_DUN_PIN_CONTROL_SIGNAL:
                        ret = responseVoid(p);
                        break;
                    case RIL_UNSOL_SIM_SWAP_STATE_CHANGED: // To be remapped
                        superProcessUnsolicited(p, type, dataPosition,
                                                RIL_UNSOL_RESPONSE_SIM_STATUS_CHANGED);
                        return;
                    default:
                        Rlog.e(RILJ_LOG_TAG, "Unhandled OEM unsolicited response: " + response);
                        return;
                }
            } catch (Throwable tr) {
                Rlog.e(RILJ_LOG_TAG, "Exception processing unsol response: " + response +
                    "Exception:" + tr.toString());
                return;
            }

            switch (response) {
                case RIL_UNSOL_STK_SEND_SMS_RESULT:
                    unsljLogRet(response, ret);
                    if (mCatSendSmsResultRegistrant != null) {
                        mCatSendSmsResultRegistrant.notifyRegistrant(
                          new AsyncResult(null, ret, null));
                        return;
                    }
                    return;
                case RIL_UNSOL_AM:
                    String str = (String)ret;
                    Rlog.d(RILJ_LOG_TAG, "Am= " + str);
                    return;
                default:
                    return;
            }
        } else {
            // Not an OEM response.
            superProcessUnsolicited(p, type, dataPosition, -1);
            return;
        }
    }
}
