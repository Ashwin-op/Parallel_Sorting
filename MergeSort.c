/*
 * Mergesort with Uncontrolled forking. Spawn as many processes as needed in order to completely sort an array of ints.
 */
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
 * Dump to the stdout the contents of the array to be sorted.
 */
void display(int array[], int length)
{
    printf(">");
    for (int i = 0; i < length; i++)
        printf(" %d", array[i]);
    printf("\n");
}

/*
 * Helper function to mergesort
 */
void merge(int *left, int left_length, int *right, int rlength)
{
    // Temporary memory locations for the 2 segments of the array to merge.
    int *ltmp = (int *)malloc(left_length * sizeof(int));
    int *rtmp = (int *)malloc(rlength * sizeof(int));

    /*
	 * Pointers to the elements being sorted in the temporary memory locations.
	 */
    int *ll = ltmp;
    int *rr = rtmp;

    int *result = left;

    /*
	 * Copy the segment of the array to be merged into the temporary memory locations.
	 */
    memcpy(ltmp, left, left_length * sizeof(int));
    memcpy(rtmp, right, rlength * sizeof(int));

    while (left_length > 0 && rlength > 0)
    {
        if (*ll <= *rr)
        {
            /*
			 * Merge the first element from the left back into the main array if it is smaller or equal to the one on the right.
			 */
            *result = *ll;
            ++ll;
            --left_length;
        }
        else
        {
            /*
			 * Merge the first element from the right back into the main array if it is smaller than the one on the left.
			 */
            *result = *rr;
            ++rr;
            --rlength;
        }
        ++result;
    }
    /*
	 * All the elements from either the left or the right temporary array segment have been merged back into the main array.
     * Append the remaining elements from the other temporary array back into the main array.
	 */
    if (left_length > 0)
        while (left_length > 0)
        {
            // Appending the rest of the left temporary array
            *result = *ll;
            ++result;
            ++ll;
            --left_length;
        }
    else
        while (rlength > 0)
        {
            // Appending the rest of the right temporary array
            *result = *rr;
            ++result;
            ++rr;
            --rlength;
        }

    // Release the memory used for the temporary arrays
    free(ltmp);
    free(rtmp);
}

/*
 * Parallel Mergesort Algorithm
 */
void mergesortParallel(int array[], int length)
{
    // This is the middle index and also the length of the right array
    int middle;

    /*
	 * Pointers to the beginning of the left and right segment of the array to be merged.
	 */
    int *left, *right;

    // Length of the left segment of the array to be merged
    int left_length;

    int lchild = -1;
    int rchild = -1;

    int status;

    if (length <= 1)
        return;

    // Let integer division truncate the value
    middle = length / 2;

    left_length = length - middle;

    /*
	 * Set the pointers to the appropriate segments of the array to be merged.
	 */
    left = array;
    right = array + left_length;

    lchild = fork();
    if (lchild < 0)
    {
        perror("fork");
        exit(1);
    }
    if (lchild == 0)
    {
        mergesortParallel(left, left_length);
        exit(0);
    }
    else
    {
        rchild = fork();
        if (rchild < 0)
        {
            perror("fork");
            exit(1);
        }
        if (rchild == 0)
        {
            mergesortParallel(right, middle);
            exit(0);
        }
    }
    waitpid(lchild, &status, 0);
    waitpid(rchild, &status, 0);

    merge(left, left_length, right, middle);
}

/*
 * Serial Mergesort Algorithm
 */
void mergesortSerial(int array[], int length)
{
    // This is the middle index and also the length of the right array
    int middle;

    /*
	 * Pointers to the beginning of the left and right segment of the array to be merged.
	 */
    int *left, *right;

    // Length of the left segment of the array to be merged
    int left_length;

    if (length <= 1)
        return;

    // Let integer division truncate the value
    middle = length / 2;

    left_length = length - middle;

    /*
	 * Set the pointers to the appropriate segments of the array to be merged.
	 */
    left = array;
    right = array + left_length;

    mergesortSerial(left, left_length);
    mergesortSerial(right, middle);

    merge(left, left_length, right, middle);
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
        array[i] = rand() % 10000;
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
	 * Copy the data to be sorted from the local memory into the shared memory
	 */
    for (int i = 0; i < length; i++)
        shm_array[i] = array[i];
    // display(shm_array, length);

    printf("\n\n--------------------PARALLEL--------------------\n");

    /*
     * Call mergesortParallel and calculate the time it takes
     */
    double start, end;
    start = wctime();
    mergesortParallel(shm_array, length);
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
     * Call mergesortSerial and calculate the time it takes
     */
    start = wctime();
    mergesortSerial(array, length);
    end = wctime();
    printf("Done serial sorting\n");

    // display(array, length);

    printf("Run time: %f secs\n", (end - start));

    return 0;
}
