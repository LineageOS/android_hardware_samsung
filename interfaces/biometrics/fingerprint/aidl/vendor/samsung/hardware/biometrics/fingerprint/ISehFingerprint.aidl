package vendor.samsung.hardware.biometrics.fingerprint;

import vendor.samsung.hardware.biometrics.fingerprint.ISehFingerprintCallback;

@VintfStability
interface ISehFingerprint {
    void registerCallback(in ISehFingerprintCallback callback);

    void unregisterCallback(in ISehFingerprintCallback callback);

    void setFodEnable(int mode);

    void updateDisplayInfo(int displayWidth, int displayHeight, float xdpi);

    int[] getSensorInfo();

    int[] getFodRect();

    void setFodRect(int left, int top, int right, int bottom);

    void setFodIconVisible(boolean visible);

    void setMaskBrightness(int brightness);

    int getSensorTargetNits();

    String getSensorLightColor();

    boolean isHbmEnabled();

    void notifyTouchEvent(boolean down, int currentAlpha, boolean screenOn);

    boolean isHwLightSourceSupported();

    void setHwLightSource(boolean enable);
}
