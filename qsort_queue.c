/*
* Quicksort algorithm (Sequential version)
* Author: Karl Hiner
* CS415
*/

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <pthread.h>
#include <semaphore.h>

#define MIN_SIZE 10

pthread_mutex_t mutex, count_mutex;
// sem_t queue_sem;

struct task_node
{
    int low, high;
    struct task_node *next;
};

struct task_queue
{
    struct task_node *front, *rear;
};

// add node to the back of queue.
// since this function effects the global resource 'queue',
// it is recommended to enforce as a critical section,
// i.e. use mutex_lock before and mutex_unlock after
void add(struct task_queue *queue, struct task_node *node)
{
    if (isEmpty(queue)) {
        queue->front = node;
        queue->rear = node;
    } else {
        queue->rear->next = node;
        queue->rear = node;
    }
}

// remove and return the node at the front of the queue
// returns NULL if the queue is empty
// since this function effects the global resource 'queue',
// it is recommended to enforce as a critical section,
// i.e. use mutex_lock before and mutex_unlock after
struct task_node* delete(struct task_queue *queue)
{
    if (isEmpty(queue)) {
        return NULL;
    }
    struct task_node *front = queue->front;
    queue->front = front->next;
    return front;
}

int isEmpty(struct task_queue *queue)
{    
    return (queue->front == NULL);
}

struct task_queue *queue;

int N;
int sortedCount = -1;

int *array = NULL;
pthread_t *thread_array = NULL;

/* Swap two array elements 
*/
void swap(int a, int b)
{
    if (a == b) return;
    int tmp = array[a];
    array[a] = array[b];
    array[b] = tmp;
}
 /* Bubble sort for the base cases
 */
void bubble_sort(int low, int high)
{
    if (low > high) return;
    int i, j;
    for (i = low; i <= high; i++)
        for (j = i+1; j <= high; j++) 
            if (array[i] > array[j])
                swap(i, j);
}
 /* Partition the array into two halves and return
 * the middle index
 */
int partition(int low, int high)
{
    /* Use the lowest element as pivot */
    int pivot = array[low], middle = low, i;

    swap(low, high);
    for(i=low; i<high; i++) {
        if(array[i] < pivot) {
            swap(i, middle);
            middle++;
        }
    }
    swap(high, middle);
 
    return middle;
}

/*
 * Quicksort the array 
 */
void quicksort(int low, int high)
{
    if (high - low < MIN_SIZE) {
        bubble_sort(low, high);
        
        // critical section - increasing global var sortedCount
        pthread_mutex_lock(&count_mutex);
        // +1 for partition
        sortedCount += high - low + 2;        
        pthread_mutex_unlock(&count_mutex);
        
        return;
    }
    int middle = partition(low, high);
    // place the first section on the task queue,
    // and recurse on the second section
    struct task_node *new_node = (struct task_node *)malloc(sizeof(struct task_node));
    new_node->low = low;
    new_node->high = middle - 1;
    
    // critical section - adding task to global queue
    pthread_mutex_lock(&mutex);    
    add(queue, new_node);
    // increment semaphore to wake up waiting threads
    // sem_post(&queue_sem);        
    pthread_mutex_unlock(&mutex);    
    
    quicksort(middle+1, high);
}

/*
 * Every thread executes this routine, including ’main’ 
 */
void worker(long rank)
{
#ifdef DEBUG
    printf("Thread %ld starts ....\n", rank);
#endif
    while (sortedCount < N || !isEmpty(queue)) {
        // wait for the queue to be non-empty
        // sem_wait(&queue_sem);        

        // critical section - removing task from global queue
        pthread_mutex_lock(&mutex);
        struct task_node *node = delete(queue);        
        pthread_mutex_unlock(&mutex);

        if (node != NULL) {
             quicksort(node->low, node->high);
        }
        
#ifdef DEBUG
        printf("%ld.", rank);
#endif
    }
} 

// A global array of size N contains the integers to be sorted,
// using <num_threads> threads
// global task queue is initialized with the given sort range [0,N-1].
int main(int argc, char **argv)
{
    int num_threads, i, j;
    char ans;
    
    /* check command line first */
    if (argc < 2) {
        printf("Usage : qsort_seq <array_size> <num_threads: default=1>\n");
        exit(0);
    }
    if ((N=atoi(argv[1])) < 2) {
        printf ("<array_size> must be greater or equal to 2\n");
        exit(0);
    }
    if (argc >= 3) {
        if ((num_threads=atoi(argv[2])) < 1) {
            printf("<num_threads> must be greater or equal to 1\n");
            exit(0);
        }
    } else {
        printf("No num_threads arg provided.  Defaulting to 1 thread.\n");
        num_threads = 1;
    }

    // initialize mutexes and semaphore
    pthread_mutex_init(&mutex, NULL);
    pthread_mutex_init(&count_mutex, NULL);
    // init semaphore to 0, since no tasks are in the queue to start
    // sem_init(&queue_sem, 0, 0);

    // init an array with values 1. N-1
    array = (int *)malloc(sizeof(int) * N);
    for (i = 0; i < N; i++)
        array[i] = i + 1;
    
    // randomly permute array elements
    srand(time(NULL));
    for (i = 0; i < N; i++) {
        j = (rand()*1./RAND_MAX)*(N - 1);
        swap(i, j);
    }

    // init task queue with initial task node of 0-(N-1)
    queue = (struct task_queue *)malloc(sizeof(struct task_queue));
    struct task_node *firstNode = (struct task_node *)malloc(sizeof(struct task_node));
    firstNode->low = 0;
    firstNode->high = N - 1;

    // only main thread so far.  no need to lock.
    add(queue, firstNode);
    
    // increment semaphore to wake up waiting threads
    // sem_post(&queue_sem);    
    
    // create (num_threads-1) threads, each will execute the worker() routine;
    thread_array = (pthread_t *)malloc(sizeof(pthread_t)*(num_threads));
    for (i = 0; i < num_threads - 1; i++)
        pthread_create(&thread_array[i], NULL, (void *) &worker, /*THREAD_NUM*/ (void *)i);
    
    // execute worker() - ’main’ is also a member of the thread pool
    // last thread, so thread_num == num_threads - 1
    worker(num_threads - 1);
    
    // wait for other threads to join;
    for (i = 0; i < num_threads; i++)
        pthread_join(thread_array[i], NULL);

    /* Verify the result */
    for (i=0; i<N-1; i++) {
        if (array[i]>array[i+1]) {
            printf("Verification failed, array[%d]=%d, array[%d]=%d\n",
                   i, array[i], i+1, array[i+1]);
            return;
        }
    }
    printf("\nSorting result verified! (cnt = %d)\n", sortedCount);
}
