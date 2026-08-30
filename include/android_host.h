#ifndef KRYON_ANDROID_HOST_H
#define KRYON_ANDROID_HOST_H

#if defined(__cplusplus)
extern "C" {
#endif

enum {
    ANDROID_SECURE_STORE_IDLE = 0,
    ANDROID_SECURE_STORE_PENDING = 1,
    ANDROID_SECURE_STORE_OK = 2,
    ANDROID_SECURE_STORE_ERROR = 3
};

void AndroidHostInit(void);
void AndroidHostApplySystemTheme(void);
void AndroidHostSetSoftKeyboardVisible(int visible);

int AndroidHostLeftReserved(void);
int AndroidHostTopReserved(void);
int AndroidHostRightReserved(void);
int AndroidHostBottomReserved(void);

int AndroidSecureStoreBiometricAvailable(void);
int AndroidSecureStoreBiometricSetupRequired(void);
int AndroidSecureStoreHasSecret(const char *key);
int AndroidSecureStoreSecretUsesBiometric(const char *key);
void AndroidSecureStoreSaveSecret(const char *key, const char *secret,
                                  int require_biometric,
                                  const char *label);
void AndroidSecureStoreUnlockSecret(const char *key, const char *label);
void AndroidSecureStoreClearSecret(const char *key);
int AndroidSecureStoreStatus(const char *key);
int AndroidSecureStoreTakeResult(const char *key, char *out, int out_size);

#if defined(__cplusplus)
}
#endif

#endif /* KRYON_ANDROID_HOST_H */
