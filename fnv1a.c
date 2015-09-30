#include "fnv1a.h"

#define FNV_32_PRIME 0x01000193U
#define FNV_32_INIT 0x811c9dc5U

/**
 * Computes the hash for key in a 32-bit unsigned integer space.
 **/
unsigned int
fnv1a_hash32(const char *key, const char *end)
{
	unsigned int hash = FNV_32_INIT;
	for (; key < end; key++)
		hash = (hash ^ (unsigned char)*key) * FNV_32_PRIME;
	return hash;
}

/**
 * Computes the hash position for key in a 16-bit unsigned integer
 * space.  Returns a number between 0 and 65535 based on the FNV1a hash
 * algorithm.
 */
unsigned short
fnv1a_hash16(const char *key, const char *end)
{
	unsigned int hash = fnv1a_hash32(key, end);
	return (unsigned short)((hash >> 16) ^ (hash & (unsigned short)0xFFFF));
}

