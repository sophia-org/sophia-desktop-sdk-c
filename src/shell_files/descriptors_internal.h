#ifndef SOPHIA_SF_DESCRIPTORS_INTERNAL_H
#define SOPHIA_SF_DESCRIPTORS_INTERNAL_H
#include "roles_internal.h"
static inline int sf_descriptor_kind(unsigned k)
{
    return (k >= 46 && k <= 53) || k == 274 || k == 278;
}
size_t sf_descriptor_size(unsigned);
int sf_descriptor_check(const struct sophia_sf_record *);
void sf_descriptor_put(uint8_t *, const struct sophia_sf_record *);
int sf_descriptor_take(const uint8_t *, size_t, struct sophia_sf_record *);
#endif
