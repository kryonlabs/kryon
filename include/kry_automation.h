#ifndef KRYON_AUTOMATION_H
#define KRYON_AUTOMATION_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Generic automation/test-run options. Native builds read environment
 * variables named KRYON_AUTOMATION_<KEY>; web builds also read query/hash
 * parameters named <key>, kryon_<key>, or kryon-<key>.
 */
int KryAutomationGetOption(const char *key, const char *fallback,
                           char *out, int out_size);
int KryAutomationGetInt(const char *key, int fallback, int *out);
unsigned int KryAutomationGetSeed(unsigned int fallback);

#ifdef __cplusplus
}
#endif

#endif /* KRYON_AUTOMATION_H */
