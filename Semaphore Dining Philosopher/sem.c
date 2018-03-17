/*
 * OS Assignment #3
 *
 * @file sem.c
 */

#include "sem.h"
#include <stdlib.h>

struct test_semaphore
{
  int             value;
  pthread_mutex_t mutex;
  pthread_cond_t  cond;
};

tsem_t *
tsem_new (int value)
{
  tsem_t *sem;

  sem = malloc (sizeof (tsem_t));
  if (!sem)
    return NULL;

  sem->value = value;
  pthread_mutex_init (&sem->mutex, NULL);
  pthread_cond_init (&sem->cond, NULL);

  return sem;
}

void
tsem_free (tsem_t *sem)
{
  if (!sem)
    return;

  pthread_cond_destroy (&sem->cond);
  pthread_mutex_destroy (&sem->mutex);
  free (sem);
}

void
tsem_wait (tsem_t *sem)
{
  if (!sem)
    return;

  pthread_mutex_lock (&sem->mutex);
  sem->value--;
  if (sem->value < 0)
    pthread_cond_wait (&sem->cond, &sem->mutex);
  pthread_mutex_unlock (&sem->mutex);
}

int
tsem_try_wait (tsem_t *sem)
{
  /* TODO: not yet implemented. */
	int result;//반환값 
	if (!sem)
		return;
	pthread_mutex_lock(&sem->mutex);//우선 락을 잡는다.
	if (sem->value > 0)//value가 1 이상의 값일 경우(value는 int형이므로)
	{
		sem->value--;//value를 -1 해주고(P연산), result를 0으로 설정한다.
		result = 0;
	}
	else//value가 0이하의 값일 경우 
	{
		result = 1;//1을 반환한다.
	}
	pthread_mutex_unlock(&sem->mutex); //락을 해제한다.
	return result;//값을 반환한다.

	
}

void tsem_signal (tsem_t *sem)
{
  if (!sem)
    return;

  pthread_mutex_lock (&sem->mutex);
  sem->value++;
  if (sem->value <= 0)
    pthread_cond_signal (&sem->cond);
  pthread_mutex_unlock (&sem->mutex);  
}
