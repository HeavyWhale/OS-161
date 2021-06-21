#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*
 * Note: Error handling is deliberatly gratuitous; we are a short lived 
 * userspace utility, and we exit on every error, so we will be properly cleaned 
 * up anyway. Providing a good example of clean C is worth the extra effort,
 * though.
 */

pthread_mutex_t done_mutex = PTHREAD_MUTEX_INITIALIZER;
bool done = false;

// atomic checking value of global variable `done` (by returning its value)
bool
check_done(void)
{
	bool ret;

	pthread_mutex_lock(&done_mutex);
	ret = done;
	pthread_mutex_unlock(&done_mutex);

	return (ret);
}

// atomic setting value of global variable `done` to new value `val`
void
set_done(bool val)
{
	pthread_mutex_lock(&done_mutex);
	done = val;
	pthread_mutex_unlock(&done_mutex);
}

struct resource {
	long num_consumers;		/* Number of active consumers in the resource */
	long num_producers;		/* Number of active producers in the resource */
	int ratio;			/* Ratio of producers to consumers */
	pthread_cond_t cond;		/* Resource condition variable */
	pthread_mutex_t mutex;		/* Resource mutex */
};

void
assert_capacity(struct resource *resource)
{
	// if (resource->num_consumers > resource->num_producers * resource->ratio) {
	// 	printf("assert_capacity: num_consumers=%ld, num_producers=%ld, num_producers * ratio=%ld\n", 
	// 		resource->num_consumers, resource->num_producers, resource->num_producers * resource->ratio);
	// }
	
	assert(resource->num_consumers <= resource->num_producers * resource->ratio);
}

/* Sleep for an arbitrary amount of time. */
void
compute(void)
{
	usleep(1000 * (time(NULL) % 10));
}

/* Sleep for an arbitrary amount of time. */
void
rest(void)
{
	usleep(1000 * (time(NULL) % 50));
}


/* --------------------------------------------------------------------
 * Invariant for following four functions:
 *                   num_consumers <= num_producers * ratio
 * --------------------------------------------------------------------
 */


/* --------------------------------------------------------------------
 * consume_enter
 * --------------------------------------------------------------------
 * Precondition:
 *     The action of
 *                        num_consumers += 1;
 *     must not violate the invariant. This means the thread must
 * 	   enter wait queue (by calling `pthread_cond_wait`) when the 
 *     following is true:
 *                   num_consumers >= num_producers * ratio
 *     so that when (presumably) an entering producer signals this 
 *     waiting thread, there is at least one free spot (of num_consumer)
 *     for it to increment.
 * --------------------------------------------------------------------
 * Postcondition:
 *     The action of
 *                        num_consumers += 1;
 * 	   will have no effect on other waiting threads.
 * --------------------------------------------------------------------
 */
void
consume_enter(struct resource *resource)
{
	pthread_mutex_lock( &resource->mutex );
    while ( resource->num_consumers >= resource->num_producers * resource->ratio )
    {
        pthread_cond_wait( &resource->cond, &resource->mutex );
    }
	resource->num_consumers += 1;
}

/* --------------------------------------------------------------------
 * consume_exit
 * --------------------------------------------------------------------
 * Precondition:
 *     The action of
 *                        num_consumers -= 1;
 *     will have no effect on other waiting threads.
 * --------------------------------------------------------------------
 * Postcondition:
 *     The action of
 *                        num_consumers -= 1;
 * 	   should notify (signal) the procuder threads who desire to exit
 *     (the critical section).
 * --------------------------------------------------------------------
 */
void
consume_exit(struct resource *resource)
{
	resource->num_consumers -= 1;
    pthread_cond_broadcast( &resource->cond );
    pthread_mutex_unlock( &resource->mutex );
}

/* --------------------------------------------------------------------
 * produce_enter
 * --------------------------------------------------------------------
 * Precondition:
 *     The action of
 *                        num_producers += 1;
 *     will have no effect on other waiting threads.
 * --------------------------------------------------------------------
 * Postcondition:
 *     The action of
 *                        num_producers += 1;
 * 	   should notify (signal) the consumer threads who desire to enter
 *     (the critical section).
 * --------------------------------------------------------------------
 */
void
produce_enter(struct resource *resource)
{
	pthread_mutex_lock( &resource->mutex );
	resource->num_producers += 1;
    pthread_cond_broadcast( &resource->cond );
}

/* --------------------------------------------------------------------
 * produce_exit
 * --------------------------------------------------------------------
 * Precondition:
 *     The action of
 *                        num_producers -= 1;
 *     must not violate the invariant. This means the thread must
 * 	   enter wait queue (by calling `pthread_cond_wait`) when the 
 *     following is true:
 *               num_consumers > (num_producers-1) * ratio
 *     so that when (presumably) an exiting consumer signals this 
 *     waiting thread, there is at least one free spot (of num_consumer)
 *     for it to decrement.
 * --------------------------------------------------------------------
 * Postcondition:
 *     The action of
 *                        num_producers -= 1;
 * 	   will have no effect on other waiting threads.
 * --------------------------------------------------------------------
 */
void
produce_exit(struct resource *resource)
{
	while ( resource->num_consumers > (resource->num_producers-1) * resource->ratio )
    {
        pthread_cond_wait( &resource->cond, &resource->mutex );
    }
	resource->num_producers -= 1;
    pthread_mutex_unlock( &resource->mutex );
}

/* Functions for the consumers and producers. */
void *
consume(void *data)
{
	struct resource *resource = (struct resource *) data;
	int *ret;

	ret = malloc(sizeof(*ret));
	if (ret == NULL) {
		perror("malloc");
		pthread_exit(NULL);
	}

	while (!check_done()) {
		consume_enter(resource);
		assert_capacity(resource);

		/* Computation happens outside the critical section.*/
		pthread_mutex_unlock(&resource->mutex);
		compute();
		pthread_mutex_lock(&resource->mutex);

		assert_capacity(resource); // moved above consume_exit to protect accessing resource
		consume_exit(resource);

		/* Wait for a bit. */
		rest();
	}

	*ret = 0;
	pthread_exit(ret);
}

void *
produce(void *data)
{
	struct resource *resource = (struct resource *) data;
	int *ret;

	ret = malloc(sizeof(*ret));
	if (ret == NULL) {
		perror("malloc");
		pthread_exit(NULL);
	}

	while (!check_done()) {
		produce_enter(resource);
		assert_capacity(resource);

		/* Computation happens outside the critical section.*/
		pthread_mutex_unlock(&resource->mutex);
		compute();
		pthread_mutex_lock(&resource->mutex);

		assert_capacity(resource); // moved above produce_exit to protect accessing resource
		produce_exit(resource);

		/* Wait for a bit. */
		rest();
	}

	/* Fix edge condition. */
	pthread_mutex_lock(&resource->mutex);
	resource->num_producers += 1;
	pthread_cond_broadcast(&resource->cond);
	pthread_mutex_unlock(&resource->mutex);

	*ret = 0;
	pthread_exit(ret);
}

struct resource *
resource_setup(long num_consumers, long num_producers, long ratio)
{
	struct resource *resource;
	int error;

	resource = calloc(1, sizeof(*resource));
	if (resource == NULL) {
		perror("calloc");
		return (NULL);
	}

	error = pthread_cond_init(&resource->cond, NULL);
	if (error != 0) {
		fprintf(stderr, "pthread_cond_init: %s", strerror(error));
		return (NULL);
	}

	error = pthread_mutex_init(&resource->mutex, NULL);
	if (error != 0) {
		fprintf(stderr, "pthread_mutex_init: %s", strerror(error));
		pthread_cond_destroy(&resource->cond);
		return (NULL);
	}

	/* Make sure the values are sane. */
	assert(num_consumers > 0);
	assert(num_producers > 0);
	assert(ratio > 0);
	assert(num_producers * ratio >= num_consumers);

	/* No active producers or consumers. */
	resource->num_consumers = 0;
	resource->num_producers = 0;
	resource->ratio = ratio;

	/* The upper limit of consumers starts at 0, there are no producers. */

	return (resource);
}

/*
 * This function gets called either before thread creation or after exit, so
 * there is no concurrency.
 */
void
resource_teardown(struct resource *resource)
{
	pthread_cond_destroy(&resource->cond);
	pthread_mutex_destroy(&resource->mutex);

	free(resource);
}

void
thread_teardown(pthread_t *threads, struct resource *resource, int nthreads)
{
	int *ret;
	int i;

	for (i = 0; i < nthreads; i++) {
		pthread_join(threads[i], (void *)&ret);
		assert(ret != NULL);
		assert(*ret == 0);
		free(ret);
	}

	free(threads);
}

int
thread_setup(struct resource *resource ,int num_producers, int num_consumers,
    pthread_t **threadsp)
{
	pthread_t *threads;
	int nthreads = 0;
	int error;
	int i;

	threads = malloc(sizeof(*threads) * (num_producers + num_consumers));
	if (threads == NULL) {
		perror("malloc");
		return (ENOMEM);
	}

	for (i = 0; i < num_consumers; i++) {
		error = pthread_create(&threads[nthreads], NULL, consume, (void *)resource);
		if (error != 0) {
			fprintf(stderr, "pthread_create: %s\n", strerror(error));
			goto error;
		}

		nthreads += 1;
	}

	for (i = 0; i < num_producers; i++) {
		error = pthread_create(&threads[nthreads], NULL, produce, (void *)resource);
		if (error != 0) {
			fprintf(stderr, "pthread_create: %s\n", strerror(error));
			goto error;
		}

		nthreads += 1;
	}

	*threadsp = threads;

	return (0);

error:

	thread_teardown(threads, resource, nthreads);
	return (error);
}

int
main(int argc, char **argv)
{
	long num_producers, num_consumers, ratio;
	struct resource* resource;
	pthread_t *threads;
	int error;

	if (argc != 4) {
		fprintf(stderr, "Usage: ./condition <# consumers> <# producers> <ratio>\n");
		exit(0);
	}

	num_consumers = strtol(argv[1], NULL, 10);
	num_producers = strtol(argv[2], NULL, 10);
	ratio = strtol(argv[3], NULL, 10);

	resource = resource_setup(num_consumers, num_producers, ratio);
	if (resource == 0) {
		fprintf(stderr, "Failed to acquire resource\n");
		exit(0);
	}

	error = thread_setup(resource, num_producers, num_consumers, &threads);
	if (error != 0) {
		fprintf(stderr, "Failed to set up threads\n");
		resource_teardown(resource);
		exit(0);
	}

	sleep(3);

	/* Mark operation as done. */
	set_done(true);

	/* Free the resources. */
	thread_teardown(threads, resource, num_producers + num_consumers);
	resource_teardown(resource);

	return (0);
}
