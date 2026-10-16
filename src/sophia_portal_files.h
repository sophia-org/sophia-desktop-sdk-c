#ifndef SOPHIA_PORTAL_FILES_H
#define SOPHIA_PORTAL_FILES_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define SOPHIA_PF_FRAME_INFO_BYTES 64u
#define SOPHIA_PF_IMAGE_MAX_BYTES UINT64_C(67108864)
/* Passive RGBA8 metadata; no pixel storage or authority is retained. */
struct sophia_pf_frame_info {
  uint64_t output, output_generation, presentation_generation;
  uint32_t width, height;
  uint64_t stride, image_bytes;
};
/* Exact complete record; caller establishes EOF and same-object frame reads.
 * No allocation. Returns -1 on invalid input and leaves out unchanged. */
int sophia_pf_decode_frame_info(const void *bytes, size_t length,
                                struct sophia_pf_frame_info *out);
#ifdef __cplusplus
}
#endif
#endif
