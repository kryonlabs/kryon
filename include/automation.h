#ifndef AUTOMATION_H
#define AUTOMATION_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Generic automation/test-run options. Native builds read environment
 * variables named AUTOMATION_<KEY>; web builds also read query/hash
 * parameters named <key>, automation_<key>, or automation-<key>.
 */
int AutomationGetOption(const char *key, const char *fallback,
                        char *out, int out_size);
int AutomationGetInt(const char *key, int fallback, int *out);
unsigned int AutomationGetSeed(unsigned int fallback);

#ifdef __cplusplus
}
#endif

#endif /* AUTOMATION_H */
