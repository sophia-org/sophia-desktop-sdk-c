#ifndef SOPHIA_ADMIN_FILES_H
#define SOPHIA_ADMIN_FILES_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define SOPHIA_AF_LINE_MAX_BYTES 512u
struct sophia_af_info {
  uint64_t session_low, session_high, connection;
  uint32_t command_ms, frame_ms, idle_ms;
};
/* Parse a complete LF-terminated info snapshot without allocation. Callers
 * establish EOF separately. Returns 0 on success, -1 on invalid input;
 * failure leaves out unchanged. Parsing grants no authority. */
int sophia_af_decode_info(const void *bytes, size_t length,
                          struct sophia_af_info *out);
#ifdef __cplusplus
}
#endif
#endif
