#ifndef CONFIG_ACCESS_H_INCLUDED__
#define CONFIG_ACCESS_H_INCLUDED__

#ifndef config_access_s
# define config_access_s void
#endif

struct config_access_i {
	unsigned (*get_u) (config_access_s *this_, const char *key, unsigned defval);
	void (*set_u) (config_access_s *this_, const char *key, unsigned val);
	_Bool (*commit) (config_access_s *this_);
};

#endif // CONFIG_ACCESS_H_INCLUDED__
