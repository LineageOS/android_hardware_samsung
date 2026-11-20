package vendor.samsung.hardware.biometrics.fingerprint;

@VintfStability
oneway interface ISehFingerprintCallback {
    void onFingerDown(int x, int y);

    void onFingerUp();

    void onOpticalCaptureStarted();

    void onOpticalCaptureFinished();

    void onHbmChanged(boolean enabled);
}
