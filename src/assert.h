#ifndef ASSERT_H_INCLUDED__
#define ASSERT_H_INCLUDED__

# if 1 // prior to performance(0) or safety(1)
#define ASSERTE(expr) \
	if (! (expr)) { \
printf (VTRR "ASSERT" VTO "! " #expr "\n"); \
		exit (-1); \
	}
#define ASSERT2(expr, fmt, a, b) \
	if (! (expr)) { \
printf (VTRR "ASSERT" VTO "! " #expr fmt "\n", a, b); \
		exit (-1); \
	}
# else
#define ASSERTE(expr) 
#define ASSERT2(expr, fmt, a, b) 
# endif
#define BUG ASSERTE
#define BUG2 ASSERT2

#endif // ASSERT_H_INCLUDED__
