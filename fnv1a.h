#ifndef FNV1A_H
#define FNV1A_H 1

unsigned int fnv1a_hash32(const char *key, const char *end);
unsigned short fnv1a_hash16(const char *key, const char *end);

#endif
