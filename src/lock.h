#ifndef LOCK_H_INCLUDED__
#define LOCK_H_INCLUDED__

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#ifdef WIN32 // windows
// TODO: change into equivalent inteface to gnu-linux.
typedef CRITICAL_SECTION Lock;
#define lock_dtor DeleteCriticalSection
#define lock_ctor(m_) (InitializeCriticalSection (m_), true)
#define lock_lock EnterCriticalSection
#define lock_unlock LeaveCriticalSection
#else // gnu-linux

struct lock {
#ifdef __x86_64__
	uint8_t priv[40]; // gcc's value to x86_64
#else // __i386__
	uint8_t priv[24]; // gcc's value to i386
#endif
};
void lock_dtor (struct lock *this_);
void lock_ctor (struct lock *this_, unsigned cb);
void lock_lock (struct lock *this_);
void lock_unlock (struct lock *this_);

#endif

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // LOCK_H_INCLUDED__
