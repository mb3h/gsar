#ifndef CONFIG_H_INCLUDED__
#define CONFIG_H_INCLUDED__

#include "iface/config_access.h"

struct config {
#ifdef __x86_64__
	uint8_t priv[32]; // gcc's value to x86_64
#else //def __i386__
	uint8_t priv[20]; // gcc's value to i386
#endif
};
struct config *config_get_singlton ();
void config_cleanup();
void config_warmup();
struct config_access_i *config_query_config_access_i (struct config *this_);

#endif // CONFIG_H_INCLUDED__
