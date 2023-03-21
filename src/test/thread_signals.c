// cc -pthread -g -Wall -std=c99 concepts/thread_signals.c -o thread_signals

#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>

void *
signal_thread (void *arg)
{
  sigset_t *mask = (sigset_t *) arg;

  /* Wait for a signal to be delivered */

  int sig;

  while (1)
    {
      sigwait (mask, &sig);

      /* Handle the signal */
      switch (sig)
	{
	case SIGINT:
	  printf ("Received SIGINT\n");
	  break;
	case SIGQUIT:
	  printf ("Received SIGQUIT\n");
	  break;
	default:
	  printf ("Received signal %d\n", sig);
	  break;
	}
    }

  return NULL;
}

int
main (int argc, char *argv[])
{
  sigset_t mask;

  /* Block all signals */
  sigfillset (&mask);
  sigprocmask (SIG_BLOCK, &mask, NULL);

  /* Create a thread to handle signals */
  pthread_t thread;
  pthread_create (&thread, NULL, signal_thread, &mask);

  printf ("%d %u\n", getpid (), pthread_self ());

  pthread_exit (0);
}
