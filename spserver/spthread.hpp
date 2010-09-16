
#ifndef __spthread_hpp__
#define __spthread_hpp__

#ifndef WIN32

/// pthread

#include <pthread.h>
#include <unistd.h>

typedef void * sp_thread_result_t;
typedef pthread_mutex_t sp_thread_mutex_t;
typedef pthread_cond_t  sp_thread_cond_t;
typedef pthread_t       sp_thread_t;
typedef pthread_attr_t  sp_thread_attr_t;

#define sp_thread_mutex_init     pthread_mutex_init
#define sp_thread_mutex_destroy  pthread_mutex_destroy
#define sp_thread_mutex_lock     pthread_mutex_lock
#define sp_thread_mutex_unlock   pthread_mutex_unlock

#define sp_thread_cond_init      pthread_cond_init
#define sp_thread_cond_destroy   pthread_cond_destroy
#define sp_thread_cond_wait      pthread_cond_wait
#define sp_thread_cond_signal    pthread_cond_signal

#define sp_thread_attr_init           pthread_attr_init
#define sp_thread_attr_destroy        pthread_attr_destroy
#define sp_thread_attr_setdetachstate pthread_attr_setdetachstate
#define SP_THREAD_CREATE_DETACHED     PTHREAD_CREATE_DETACHED
#define sp_thread_attr_setstacksize   pthread_attr_setstacksize

#define sp_thread_self    pthread_self
#define sp_thread_create  pthread_create

#define SP_THREAD_CALL
typedef sp_thread_result_t ( * sp_thread_func_t )( void * args );

#ifndef sp_sleep
#define sp_sleep(x) sleep(x)
#endif

#else ///////////////////////////////////////////////////////////////////////

// win32 thread

#include <winsock2.h>
#include <process.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned sp_thread_t;

typedef unsigned sp_thread_result_t;
#define SP_THREAD_CALL __stdcall
typedef sp_thread_result_t ( __stdcall * sp_thread_func_t )( void * args );

typedef HANDLE  sp_thread_mutex_t;
typedef HANDLE  sp_thread_cond_t;

//typedef DWORD   sp_thread_attr_t;
typedef struct tagsp_thread_attr {
	int stacksize;
	int detachstate;
} sp_thread_attr_t;

#define SP_THREAD_CREATE_DETACHED 1

#ifndef sp_sleep
#define sp_sleep(x) Sleep(1000*x)
#endif

int sp_thread_mutex_init( sp_thread_mutex_t * mutex, void * attr );
int sp_thread_mutex_destroy( sp_thread_mutex_t * mutex );
int sp_thread_mutex_lock( sp_thread_mutex_t * mutex );
int sp_thread_mutex_unlock( sp_thread_mutex_t * mutex );

int sp_thread_cond_init( sp_thread_cond_t * cond, void * attr );
int sp_thread_cond_destroy( sp_thread_cond_t * cond );
int sp_thread_cond_wait( sp_thread_cond_t * cond, sp_thread_mutex_t * mutex );
int sp_thread_cond_signal( sp_thread_cond_t * cond );

int sp_thread_attr_init( sp_thread_attr_t * attr );
int sp_thread_attr_destroy( sp_thread_attr_t * attr );
int sp_thread_attr_setdetachstate( sp_thread_attr_t * attr, int detachstate );
int sp_thread_attr_setstacksize( sp_thread_attr_t * attr, size_t stacksize );

int sp_thread_create( sp_thread_t * thread, sp_thread_attr_t * attr,
		sp_thread_func_t myfunc, void * args );
sp_thread_t sp_thread_self();

#ifdef __cplusplus
}
#endif

#endif

#endif
