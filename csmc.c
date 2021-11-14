//#define _BSD_SOURCE
#define _XOPEN_SOURCE 600
#define _POSIX_C_SOURCE 200112L

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <semaphore.h>
#include <pthread.h>
#include <time.h>
#include <assert.h>

#define QUEUE_SIZE 10 // Max_stu_size
#define CODING_SLEEP_TIME 2000
#define TUTORING_SLEEP_TIME 200

int studentsInWaitingAreaQueue[QUEUE_SIZE];  // newArrivedStudentQueue
int studentIdsQueue[QUEUE_SIZE];             // student_ids
int tutorIdsQueue[QUEUE_SIZE];               // tutor_ids
int tutoringFinishedQueue[QUEUE_SIZE];       // tutorFinishedQueue
int priorityQueueForTutoring[QUEUE_SIZE][2]; // priorityQueue
int studentPriorities[QUEUE_SIZE];           // student_priority

int numberOfChairsInWaitingArea = 0; // chair_num
int numberOfStudents = 0;            // student_num
int numberOfTutors = 0;              // tutor_num
int numberOfTimesHelpRequired = 0;   // help_num
int numberOfOccupiedChairs = 0;      // occupied_chair_num

int numberOfStudentsHelped = 0;    // done
int totalTutoringRequests = 0;     // totalRequests
int totalTutoringSessionsHeld = 0; // totalSessions
int studentsBeingTutoredNow = 0;   // tutoringNow

void *studentThread(void *studentId);
void *coordinatorThread();
void *tutorThread(void *tutorId);

sem_t semCoordinatorIsWaitingForStudent; // sem_student
sem_t semTutorIsWaitingForCoordinator;   // sem_coordinator

pthread_mutex_t chairsLock;                // seatLock
pthread_mutex_t queueLock;                 // queueLock
pthread_mutex_t tutoringFinishedQueueLock; // tutorFinishedQueueLock

// the above variable will be equal to the size of queue for students in waiting area?
/*
int headOfQueue = -1;
int tailOfQueue = -1;


// insert the student's student_id in the queue. 
// studentPriority to be incorporated into this
void enqueue(int id)
{
    if( tailOfQueue == QUEUE_SIZE - 1 )
    {
        printf("\nOVERFLOW");
        return;
    }
    else
    {
        if( headOfQueue == -1 )
        {
            headOfQueue = 0;
        }
        tailOfQueue += 1;
        studentsInWaitingAreaQueue[tailOfQueue] = id;
    }
}

void dequeue()
{
    if ( headOfQueue == -1 || headOfQueue > tailOfQueue ) 
    {
        printf("\nUNDERFLOW");
        return;
    }
    else
    {
        headOfQueue += 1;
    }
}

struct studentInfo
{
    int id;
    int numberOfTimesHelpReceived;
    int studentPriority;
};

*/

void studentIsCoding()
{
    float codingTime = (float)(rand() % CODING_SLEEP_TIME) / 1000;
    printf("\nStudent is coding for %f ms.", codingTime);
    usleep(codingTime);
}

void studentIsBeingTutored()
{
    float tutoringTime = (float)(rand() % TUTORING_SLEEP_TIME) / 1000;
    printf("\nTutored for %f ms", tutoringTime);
    usleep(tutoringTime);
}

void printArray(int *array, int length)
{
    int i = 0;
    for (i = 0; i < length; i++)
    {
        printf("%d ", array[i]);
    }
}

void *studentThread(void *studentId)
{
    int studentIdOfCurrentStudent = *(int *)studentId;

    while (1)
    {

        if (studentPriorities[studentIdOfCurrentStudent - 1] >= numberOfTimesHelpRequired)
        {

            // ----- why chairs are locked for numberOfStudentsHelped?
            // A: we only want to lock this shared variable, name doesn't matter
            pthread_mutex_lock(&chairsLock);
            numberOfStudentsHelped++;
            pthread_mutex_unlock(&chairsLock);

            //notify coordinate to terminate
            sem_post(&semCoordinatorIsWaitingForStudent);

            printf("\n------student %d terminates------\n", studentIdOfCurrentStudent);
            pthread_exit(NULL);
        }

        // ---------- should i implement random sleep?
        // implemented
        studentIsCoding();

        pthread_mutex_lock(&chairsLock);
        if (numberOfOccupiedChairs >= numberOfChairsInWaitingArea)
        {
            printf("\nS: Student %d found no empty chair. Will try again later.\n", studentIdOfCurrentStudent);
            pthread_mutex_unlock(&chairsLock);
            continue;
        }

        numberOfOccupiedChairs++;
        totalTutoringRequests++;

        // ---------- why?
        // A: all incoming students are initialised with 0 or the current value of totalTutoringRequests.
        // ++++++++++++
        studentsInWaitingAreaQueue[studentIdOfCurrentStudent - 1] = totalTutoringRequests;
        printArray(studentsInWaitingAreaQueue, QUEUE_SIZE);
        printf("\nstudentIds\n");
        printArray(studentIdsQueue, QUEUE_SIZE);
        printf("\ntutorIds\n");
        printArray(tutorIdsQueue, QUEUE_SIZE);
        printf("\nstudentpriorities\n");
        printArray(studentPriorities, QUEUE_SIZE);
        printf("\ntutfinished\n");
        printArray(tutoringFinishedQueue, QUEUE_SIZE);

        printf("\nS: Student %d takes a seat. Empty chairs = %d.", studentIdOfCurrentStudent, numberOfChairsInWaitingArea - numberOfOccupiedChairs);
        pthread_mutex_unlock(&chairsLock);

        // inform coordinator that student is waiting
        sem_post(&semCoordinatorIsWaitingForStudent);

        // wait for tutor to be available
        // ---------- how does it work?
        while (tutoringFinishedQueue[studentIdOfCurrentStudent - 1] == -1)
            ;

        // lock the numberOfOccupiedChairs
        //    pthread_mutex_lock(&chairsLock);
        //    numberOfOccupiedChairs--;
        //    pthread_mutex_unlock(&chairsLock);
        // unlock the numberOfOccupiedChairs

        int tutorIdCurrentlyTutoring = (tutoringFinishedQueue[studentIdOfCurrentStudent - 1] - numberOfStudents);

        //    studentIsBeingTutored(studentIdOfCurrentStudent);

        printf("\nS: Student %d received help from Tutor %d.\n", studentIdOfCurrentStudent, tutorIdCurrentlyTutoring);

        pthread_mutex_lock(&tutoringFinishedQueueLock);
        tutoringFinishedQueue[studentIdOfCurrentStudent - 1] = -1;
        pthread_mutex_unlock(&tutoringFinishedQueueLock);

        //decrease the priority of student after providing help
        pthread_mutex_lock(&chairsLock);
        studentPriorities[studentIdOfCurrentStudent - 1]++;
        pthread_mutex_unlock(&chairsLock);
    }
}

void *coordinatorThread()
{

    while (1)
    {

        // if all students are helped out, terminate the coordinatorThread and tutorThread
        if (numberOfStudentsHelped == numberOfStudents)
        {
            // terminate the tutors
            int i = 0;
            for (i = 0; i < numberOfTutors; i++)
            {
                // inform tutors to terminate
                sem_post(&semTutorIsWaitingForCoordinator);
            }

            // coordinator terminates itself
            printf("\nCoordinator terminates");
            pthread_exit(NULL);
        }

        // wait for student's availability notification
        sem_wait(&semCoordinatorIsWaitingForStudent);

        int i = 0;
        for (i = 0; i < numberOfStudents; i++)
        {
            pthread_mutex_lock(&chairsLock);
            // *********************************************
            if (studentsInWaitingAreaQueue[i] > -1)
            {
                priorityQueueForTutoring[i][0] = studentPriorities[i];
                priorityQueueForTutoring[i][1] = studentsInWaitingAreaQueue[i];

                printf("\nC: Student %d with priority %d added to the queue. Waiting students now = %d. Total requests = %d\n", studentIdsQueue[i], studentPriorities[i], numberOfOccupiedChairs, totalTutoringRequests);

                // clearing the student's position in the waitingAreaQueue
                studentsInWaitingAreaQueue[i] = -1;

                sem_post(&semTutorIsWaitingForCoordinator);
            }
            pthread_mutex_unlock(&chairsLock);
        }
    }
}

void *tutorThread(void *tutorId)
{

    int tutorIdOfCurrentTutor = *(int *)tutorId;

    int numberOfTimesStudentIsTutored;
    // students with same tutored times, who comes first has higher priority
    int studentSequence;
    int studentId;

    while (1)
    {

        // if all students are helped out, terminate the tutorThread
        if (numberOfStudentsHelped == numberOfStudents)
        {
            printf("\nTutor is exiting");
            pthread_exit(NULL);
        }

        numberOfTimesStudentIsTutored = numberOfTimesHelpRequired - 1;
        studentSequence = numberOfStudents * numberOfTimesHelpRequired + 1;
        studentId = -1;

        // wait for signal from coordinatorThread to be woken up
        sem_wait(&semTutorIsWaitingForCoordinator);

        // lock the numberOfOccupiedChairs
        pthread_mutex_lock(&chairsLock);
        int i;
        for (i = 0; i < numberOfStudents; i++)
        {
            if (priorityQueueForTutoring[i][0] > -1 && priorityQueueForTutoring[i][0] <= numberOfTimesStudentIsTutored && priorityQueueForTutoring[i][1] < studentSequence)
            {
                numberOfTimesStudentIsTutored = priorityQueueForTutoring[i][0];
                studentSequence = priorityQueueForTutoring[i][1];
                studentId = studentIdsQueue[i];
            }
        }

        // in case no student in the queue.
        if (studentId == -1)
        {
            pthread_mutex_unlock(&chairsLock);
            continue;
        }

        //pop the student(reset the priority queue)
        priorityQueueForTutoring[studentId - 1][0] = -1;
        priorityQueueForTutoring[studentId - 1][1] = -1;

        //occupied chair--
        numberOfOccupiedChairs--;
        // //all the students who are receiving tutoring now, since each tutor time slice is very tiny, so it's common that the tutoringNow is 0.
        studentsBeingTutoredNow++;

        pthread_mutex_unlock(&chairsLock);

        studentIsBeingTutored();

        // after tutoring
        pthread_mutex_lock(&chairsLock);

        //need to do tutoringNow-- after tutoring.
        studentsBeingTutoredNow--;
        totalTutoringSessionsHeld++;
        printf("\nT: Student %d tutored by Tutor %d. Students tutored now = %d. Total sessions tutored = %d\n", studentId, tutorIdOfCurrentTutor - numberOfStudents, studentsBeingTutoredNow, totalTutoringSessionsHeld);

        pthread_mutex_unlock(&chairsLock);

        //update shared data so student can know who tutored him.
        pthread_mutex_lock(&tutoringFinishedQueueLock);
        tutoringFinishedQueue[studentId - 1] = tutorIdOfCurrentTutor;
        pthread_mutex_unlock(&tutoringFinishedQueueLock);
    }
}

int main(int argc, char *argv[])
{

    if (argc != 5)
    {
        fprintf(stderr, "\nERROR! Please provide these 4 arguments: #students, #tutors, #chairs, #help to the code");
        exit(-1);
    }

    numberOfStudents = atoi(argv[1]);
    numberOfTutors = atoi(argv[2]);
    numberOfChairsInWaitingArea = atoi(argv[3]);
    numberOfTimesHelpRequired = atoi(argv[4]);

    if (numberOfStudents < 1)
    {
        fprintf(stderr, "\nError. There should be at least 1 student");
        exit(-1);
    }

    if (numberOfTutors < 1)
    {
        fprintf(stderr, "\nError. There should be at least 1 tutor");
        exit(-1);
    }

    if (numberOfChairsInWaitingArea < 1)
    {
        fprintf(stderr, "\nError. There should be at least 1 chair in waiting area");
        exit(-1);
    }

    if (numberOfTimesHelpRequired < 0)
    {
        fprintf(stderr, "\nError. No negative values of help allowed");
        exit(-1);
    }

    // struct studentInfo studentInfo;

    int i = 0;
    for (i = 0; i < QUEUE_SIZE; i++)
    {
        studentsInWaitingAreaQueue[i] = -1;
        tutoringFinishedQueue[i] = -1;
        priorityQueueForTutoring[i][0] = -1;
        priorityQueueForTutoring[i][1] = -1;
        studentPriorities[i] = 0;
    }

    //init lock and semaphore
    // initialized to 0 as on 1st wait call to sem, the current thread should be allowed and other threads should be blocked
    sem_init(&semCoordinatorIsWaitingForStudent, 0, 0);
    sem_init(&semTutorIsWaitingForCoordinator, 0, 0);
    pthread_mutex_init(&chairsLock, NULL);
    pthread_mutex_init(&queueLock, NULL);
    pthread_mutex_init(&tutoringFinishedQueueLock, NULL);

    //allocate threads
    pthread_t students[numberOfStudents];
    pthread_t tutors[numberOfTutors];
    pthread_t coordinator;

    // //create threads
    assert(pthread_create(&coordinator, NULL, coordinatorThread, NULL) == 0);

    for (i = 0; i < numberOfStudents; i++)
    {
        studentIdsQueue[i] = i + 1;
        assert(pthread_create(&students[i], NULL, studentThread, (void *)&studentIdsQueue[i]) == 0);
    }

    for (i = 0; i < numberOfTutors; i++)
    {
        tutorIdsQueue[i] = i + numberOfStudents + 1;
        assert(pthread_create(&tutors[i], NULL, tutorThread, (void *)&tutorIdsQueue[i]) == 0);
    }

    //join threads
    pthread_join(coordinator, NULL);

    for (i = 0; i < numberOfStudents; i++)
    {
        pthread_join(studentIdsQueue[i], NULL);
    }

    for (i = 0; i < numberOfTutors; i++)
    {
        pthread_join(tutorIdsQueue[i], NULL);
    }

    return 0;
}
