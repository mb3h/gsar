
#include <stdint.h>
#include "lock.h"

#include "vt100.h"
#include "assert.h"

#include <stdbool.h>
#include <stdio.h> // printf
#include <stdlib.h> // exit
#include <pthread.h>

///////////////////////////////////////////////////////////////////////////////
#ifdef WIN32 // win32

// TODO: implements by CriticalSection.

///////////////////////////////////////////////////////////////////////////////
#else // linux

typedef struct lock_ {
	pthread_mutex_t mtx;
} lock_s;

void lock_dtor (struct lock *this_)
{
lock_s *m_;
	m_ = (lock_s *)this_;
BUG(m_)
	pthread_mutex_destroy (&m_->mtx);
}
void lock_ctor (struct lock *this_, unsigned cb)
{
ASSERT2(sizeof(lock_s) <= cb, " %d <= " VTRR "%d" VTO, (unsigned)sizeof(lock_s), cb)
lock_s *m_;
	m_ = (lock_s *)this_;
BUG(m_)
	pthread_mutex_init (&m_->mtx, NULL);
}
void lock_lock (struct lock *this_)
{
lock_s *m_;
	m_ = (lock_s *)this_;
BUG(m_)
	pthread_mutex_lock (&m_->mtx);
}
void lock_unlock (struct lock *this_)
{
lock_s *m_;
	m_ = (lock_s *)this_;
BUG(m_)
	pthread_mutex_unlock (&m_->mtx);
}

///////////////////////////////////////////////////////////////////////////////
#endif
