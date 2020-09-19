/*
 * Quicksort with Uncontrolled forking. Spawn as many processes as needed in order to completely sort an array of ints.
 */
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <sys/time.h>

/* Prototypes */
double wctime()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + 1E-6 * tv.tv_usec;
}

/* Declarations */
/*
 * Swap elements in the array that is being sorted.
 */
void swap(int array[], int left, int right)
{
    int temp = array[left];
    array[left] = array[right];
    array[right] = temp;
}

/*
 * Dump to the stdout the contents of the array.
 */
void display(int array[], int length)
{
    printf("Array:");
    for (int i = 0; i < length; i++)
        printf(" %d", array[i]);
    printf("\n");
}

/*
 * Partition the data into two parts, elements smaller than the pivot to the 'left' and elements larger to the 'right'.
 */
int partition(int array[], int left, int right, int pivot_index)
{
    int pivot_value = array[pivot_index];
    int store_index = left;

    swap(array, pivot_index, right);
    for (int i = left; i < right; i++)
        if (array[i] <= pivot_value)
        {
            swap(array, i, store_index);
            store_index++;
        }
    swap(array, store_index, right);

    return store_index;
}

/*
 * Parallel Quicksort Algorithm
 */
void quicksortParallel(int array[], int left, int right)
{
    int pivot_index = left;
    int pivot_new_index;

    /*
	 * Use -1 to initialize because fork() uses 0 to identify a process as a child.
	 */
    int lchild = -1;
    int rchild = -1;

    if (left < right)
    {
        int status; // For waitpid() only

        /*
         * Select pivot position and put all the elements smaller than pivot on left and greater than pivot on right
         */
        pivot_new_index = partition(array, left, right, pivot_index);

        /*
		 * Parallelize by processing the left and right partition simultaneously.
         * Start by spawning the 'left' child.
		 */
        lchild = fork();
        if (lchild < 0)
        {
            perror("fork");
            exit(1);
        }
        else if (lchild == 0)
        {
            // The 'left' child starts processing
            quicksortParallel(array, left, pivot_new_index - 1);
            exit(0);
        }
        else
        {
            // The parent spawns the 'right' child
            rchild = fork();
            if (rchild < 0)
            {
                perror("fork");
                exit(1);
            }
            if (rchild == 0)
            {
                // The 'right' child starts processing
                quicksortParallel(array, pivot_new_index + 1, right);
                exit(0);
            }
        }

        // Parent waits for children to finish
        waitpid(lchild, &status, 0);
        waitpid(rchild, &status, 0);
    }
}

/*
 * Serial Quicksort Algorithm
 */
void quicksortSerial(int array[], int left, int right)
{
    int pivot_index = left;
    int pivot_new_index;

    if (left < right)
    {
        /*
         * Select pivot position and put all the elements smaller than pivot on left and greater than pivot on right
         */
        pivot_new_index = partition(array, left, right, pivot_index);

        // Sort the elements on the left of pivot
        quicksortSerial(array, left, pivot_new_index - 1);

        // Sort the elements on the right of pivot
        quicksortSerial(array, pivot_new_index + 1, right);
    }
}

/*
 * Main function
 */
int main()
{
    // Initialize data
    srand(time(0));
    int length = rand() % 1000 + 1;
    printf("%d elements to sort\n", length);

    int array[length];
    for (int i = 0; i < length; i++)
        array[i] = rand() % 10;
    // display(array, length);

    // Use this process's pid as the shared memory key identifier
    key_t key = IPC_PRIVATE;

    // Create the shared memory segment
    int shm_id;
    size_t shm_size = length * sizeof(int);
    if ((shm_id = shmget(key, shm_size, IPC_CREAT | 0666)) == -1)
    {
        perror("shmget");
        exit(1);
    }

    // Attached to the shared memory segment in order to use it
    int *shm_array;
    if ((shm_array = shmat(shm_id, NULL, 0)) == (int *)-1)
    {
        perror("shmat");
        exit(1);
    }

    /*
	 * Copy the data to be sorted from the local memory into the shared memory.
	 */
    for (int i = 0; i < length; i++)
        shm_array[i] = array[i];
    // display(shm_array, length);

    printf("\n\n--------------------PARALLEL--------------------\n");

    /*
     * Call quicksortParallel and calculate the time it takes
     */
    double start, end;
    start = wctime();
    quicksortParallel(shm_array, 0, length - 1);
    end = wctime();
    printf("Done parallel sorting\n");

    // display(shm_array, length);

    // Detach from the shared memory now that we are done using it
    if (shmdt(shm_array) == -1)
    {
        perror("shmdt");
        exit(1);
    }

    // Delete the shared memory segment
    if (shmctl(shm_id, IPC_RMID, NULL) == -1)
    {
        perror("shmctl");
        exit(1);
    }

    printf("Run time: %f secs\n", (end - start));

    printf("\n\n--------------------SERIAL--------------------\n");

    /*
     * Call quicksortSerial and calculate the time it takes
     */
    start = wctime();
    quicksortSerial(array, 0, length - 1);
    end = wctime();
    printf("Done serial sorting\n");

    // display(array, length);

    printf("Run time: %f secs\n", (end - start));

    return 0;
}
