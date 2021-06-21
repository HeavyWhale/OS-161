/* map.c
 * ----------------------------------------------------------
 *  CS350
 *  Assignment 1
 *  Question 1
 *
 *  Purpose:  Gain experience with threads and basic 
 *  synchronization.
 *
 *  YOU MAY ADD WHATEVER YOU LIKE TO THIS FILE.
 *  YOU CANNOT CHANGE THE SIGNATURE OF CountOccurrences.
 * ----------------------------------------------------------
 */
#include "data.h"
#include <stdlib.h>

#define NTHREADS 10

volatile unsigned int wordOccurrence = 0;
volatile unsigned int finishedThreadCount = 0;

pthread_mutex_t mutex;
pthread_cond_t cv;

// The input arg struct for function CountOccurrencesThread
typedef struct 
{
    struct Library* lib;
    char* word;
    unsigned int pos;
    unsigned int length;
} Args;


/* --------------------------------------------------------------------
 * CountOccurrencesThread
 * --------------------------------------------------------------------
 * Takes a struct Args which contains the information for a slice of 
 * Library and update the global counter `wordOccurrence` for each 
 * occurrence of desired word.
 * --------------------------------------------------------------------
 */
void* CountOccurrencesThread( void* arg ) 
{
    Args* args = (Args*) arg;

    for ( unsigned int i = args->pos; i < args->pos + args->length; i++ ) 
    {
        struct Article * art = args->lib->articles[i];
        for ( unsigned int j = 0; j < art->numWords; j++ ) 
        {
            char * givenWord = art->words[j];
            if ( strcmp( givenWord, args->word ) == 0 ) 
            {
                // Protect update on shared variable wordOccurrence
                pthread_mutex_lock( &mutex );
                wordOccurrence++;
                pthread_mutex_unlock( &mutex );
            }
        }
    }

    // Update finished thread count and notify master thread 
    // if all worker threads are finished
    pthread_mutex_lock( &mutex );
    finishedThreadCount++;
    if ( finishedThreadCount == NTHREADS ) 
    {
        pthread_cond_signal( &cv );
    }
    pthread_mutex_unlock( &mutex );

    // Explicitly terminate the thread (to avoid control flow warning)
    pthread_exit( NULL );
}

/* --------------------------------------------------------------------
 * CountOccurrences
 * --------------------------------------------------------------------
 * Takes a Library of articles containing words and a word.
 * Returns the total number of times that the word appears in the 
 * Library.
 *
 * For example, "There the thesis sits on the theatre bench.", contains
 * 2 occurences of the word "the".
 * --------------------------------------------------------------------
 */
int CountOccurrences( struct  Library * lib, char * word )
{
    pthread_t workerThreads[NTHREADS];
    Args* args_list[NTHREADS];
    int rc;

    // Initialize mutex lock and conditional variable
    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&cv, NULL);

    // Partition articles and store the result into args_list
    unsigned int q = lib->numArticles / NTHREADS;  // workload for each worker thread
    unsigned int r = lib->numArticles % NTHREADS;  // remaining workload for last worker thread
    for ( unsigned int i = 0; i < NTHREADS; i++ )
    {
        Args* args = malloc(sizeof(Args));
        args->lib = lib;
        args->word = word;
        args->pos = i*q;
        args->length = ((i == NTHREADS-1) ? q+r : q);

        args_list[i] = args;
    }

    // Create threads and feed corresponding args
    for ( unsigned int i = 0; i < NTHREADS; i++ )
    {
        rc = pthread_create( &workerThreads[i], NULL, CountOccurrencesThread, (void*)args_list[i] );
        if (rc != 0) 
        {
            printf("ERROR: pthread_create failed on i=%d\n", i);
        }
    }

    // Using conditional variables to mimic pthread_join so that 
    // we wait for all worker threads to finish their work
    pthread_mutex_lock( &mutex );
    while ( finishedThreadCount < NTHREADS ) {
        pthread_cond_wait( &cv, &mutex );
    }
    pthread_mutex_unlock( &mutex );

    // Free all resources
    for ( unsigned int i = 0; i < NTHREADS; i++ ) { free(args_list[i]); }
    pthread_mutex_destroy( &mutex );
    pthread_cond_destroy( &cv );

    return wordOccurrence;
}

int naiveCountOccur( struct Library * lib, char * word ) {

    unsigned int occ = 0;

    for ( unsigned int i = 0; i < lib->numArticles; i ++ )
    {
        struct Article * art = lib->articles[i];
        for ( unsigned int j = 0; j < art->numWords; j ++ )
        {
            char * givenWord = art->words[j];
            if (strcmp(givenWord, word) == 0) occ++;
        }
    }

    return occ;
}
