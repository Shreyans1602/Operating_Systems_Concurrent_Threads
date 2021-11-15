#define _XOPEN_SOURCE 600   //To support -std=c99 in place of -std=gnu99

//Libraries
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <semaphore.h>
#include <pthread.h>
#include <time.h>

//Defines
#define CODING_SLEEP_TIME 200
#define TUTORING_SLEEP_TIME 200

// Data structure arguments
int *studentsInWaitingAreaQueue = NULL;  //Students in the queue
int *studentIdsQueue = NULL;             //ID of students
int *studentPriorities = NULL;           //Priority of students
int *tutorIdsQueue = NULL;               //ID of tutors
int *tutoringFinishedQueue = NULL;       //Queue to indicate that tutoring finished
int **priorityQueueForTutoring = NULL;   //Priority queue

//Input arguments
int numberOfChairsInWaitingArea = 0; //Number of chairs
int numberOfStudents = 0;            //Number of students 
int numberOfTutors = 0;              //Number of tutors
int numberOfTimesHelpRequired = 0;   //Number of times each student will take help

//Program related arguments
int numberOfOccupiedChairs = 0;
int numberOfStudentsHelped = 0;
int totalTutoringRequests = 0;
int totalTutoringSessionsHeld = 0;
int studentsBeingTutoredNow = 0;

// thread-functions
void *coordinatorThread();
void *studentThread(void *studentId);
void *tutorThread(void *tutorId);

sem_t semCoordinatorIsWaitingForStudent;
sem_t semTutorIsWaitingForCoordinator;

pthread_mutex_t chairsLock;
pthread_mutex_t queueLock;
pthread_mutex_t tutoringFinishedQueueLock;

void *coordinatorThread()
{
    int tIterator = 0;

    while(1)
    {
        //If all students are helped out, terminate the coordinatorThread and tutorThread
        if(numberOfStudentsHelped == numberOfStudents)
        {
            //Terminate the tutors first
            for(tIterator = 0; tIterator < numberOfTutors; tIterator++)
            {
                //Sending a signal informing tutors to terminate
                sem_post(&semTutorIsWaitingForCoordinator);
            }

            //Then coordinator terminates itself
            pthread_exit(NULL);
        }

        //Wait for student's availability notification
        sem_wait(&semCoordinatorIsWaitingForStudent);

        //Acquire lock for shared variable
        pthread_mutex_lock(&chairsLock);
        for(tIterator = 0; tIterator < numberOfStudents; tIterator++)
        {
            //Adding each student to the 2-d priority queue
            if(studentsInWaitingAreaQueue[tIterator] > -1)
            {
                //priorityQueueForTutoring contains 2 variables for each student
                //0th Index: Student's priority
                //1st Index: Student's position in the waiting queue
                priorityQueueForTutoring[tIterator][0] = studentPriorities[tIterator];
                priorityQueueForTutoring[tIterator][1] = studentsInWaitingAreaQueue[tIterator];

                printf("C: Student %d with priority %d added to the queue. Waiting students now = %d. Total requests = %d\n", studentIdsQueue[tIterator], studentPriorities[tIterator], numberOfOccupiedChairs, totalTutoringRequests);

                //Clearing the student's position in the waitingAreaQueue and resetting it
                studentsInWaitingAreaQueue[tIterator] = -1;

                //Send signal to tutor to call the student with highest priority for tutoring
                sem_post(&semTutorIsWaitingForCoordinator);
            }
        }
        //Release lock for shared variable
        pthread_mutex_unlock(&chairsLock);
    }
}

void *studentThread(void *studentId)
{
    int studentIdOfCurrentStudent = *(int *)studentId;

    while(1)
    {
        if(studentPriorities[studentIdOfCurrentStudent - 1] >= numberOfTimesHelpRequired)
        {
            //Acquire lock for shared variable
            pthread_mutex_lock(&chairsLock);

            numberOfStudentsHelped++;

            //Release lock for shared variable
            pthread_mutex_unlock(&chairsLock);

            //Notify coordinate to terminate
            sem_post(&semCoordinatorIsWaitingForStudent);

            pthread_exit(NULL);
        }

        //Student is coding for a random period upto 2ms
        float codingTime = (float)(rand() % CODING_SLEEP_TIME) / 100;
        usleep(codingTime);

        //Acquire lock for shared variable
        pthread_mutex_lock(&chairsLock);

        if(numberOfOccupiedChairs >= numberOfChairsInWaitingArea)
        {
            printf("S: Student %d found no empty chair. Will try again later.\n", studentIdOfCurrentStudent);
            pthread_mutex_unlock(&chairsLock);
            continue;
        }

        numberOfOccupiedChairs++;
        totalTutoringRequests++;

        //All incoming students are initialised with 0 or the current value of totalTutoringRequests.
        studentsInWaitingAreaQueue[studentIdOfCurrentStudent - 1] = totalTutoringRequests;

        printf("S: Student %d takes a seat. Empty chairs = %d.\n", studentIdOfCurrentStudent, numberOfChairsInWaitingArea - numberOfOccupiedChairs);

        //Release lock for shared variable
        pthread_mutex_unlock(&chairsLock);

        //Inform coordinator that student is waiting
        sem_post(&semCoordinatorIsWaitingForStudent);

        //Wait for tutor to be available
        while(tutoringFinishedQueue[studentIdOfCurrentStudent - 1] == -1);

        int tutorIdCurrentlyTutoring = (tutoringFinishedQueue[studentIdOfCurrentStudent - 1] - numberOfStudents);

        printf("S: Student %d received help from Tutor %d.\n", studentIdOfCurrentStudent, tutorIdCurrentlyTutoring);

        //Acquire lock for shared variable
        pthread_mutex_lock(&tutoringFinishedQueueLock);

        tutoringFinishedQueue[studentIdOfCurrentStudent - 1] = -1;

        //Release lock for shared variable
        pthread_mutex_unlock(&tutoringFinishedQueueLock);

        //Decrease the priority of student after providing help
        //Acquire lock for shared variable
        pthread_mutex_lock(&chairsLock);

        studentPriorities[studentIdOfCurrentStudent - 1]++;

        //Release lock for shared variable
        pthread_mutex_unlock(&chairsLock);
    }
}

void *tutorThread(void *tutorId)
{
    int tutorIdOfCurrentTutor = *(int *)tutorId;
    int numberOfTimesStudentIsTutored;
    int tIterator = 0;

    //For students with same number of times being tutored, the one who comes first has higher priority
    int studentSequence;
    int studentId;

    while(1)
    {
        //If all students are helped out, terminate the tutorThread
        if(numberOfStudentsHelped == numberOfStudents)
        {
            pthread_exit(NULL);
        }

        numberOfTimesStudentIsTutored = numberOfTimesHelpRequired - 1;
        studentSequence = numberOfStudents * numberOfTimesHelpRequired + 1;
        studentId = -1;

        //Wait for signal from coordinatorThread to be woken up
        sem_wait(&semTutorIsWaitingForCoordinator);

        //Acquire lock for shared variable
        pthread_mutex_lock(&chairsLock);

        //Getting the latest values of numberOfTimesStudentIsTutored, studentSequence, studentId for each student 
        for(tIterator = 0; tIterator < numberOfStudents; tIterator++)
        {
            //priorityQueueForTutoring contains 2 variables for each student
            //0th Index: Student's priority
            //1st Index: Student's position in the waiting queue
            if(priorityQueueForTutoring[tIterator][0] > -1 && 
            priorityQueueForTutoring[tIterator][0] <= numberOfTimesStudentIsTutored && 
            priorityQueueForTutoring[tIterator][1] < studentSequence)
            {
                numberOfTimesStudentIsTutored = priorityQueueForTutoring[tIterator][0];
                studentSequence = priorityQueueForTutoring[tIterator][1];
                studentId = studentIdsQueue[tIterator];
            }
        }

        //If the studentId was not updated, he/she is not in the queue
        if(studentId == -1)
        {
            //Release lock for shared variable
            pthread_mutex_unlock(&chairsLock);
            continue;
        }

        //Resetting the priority queue 
        priorityQueueForTutoring[studentId - 1][0] = -1;
        priorityQueueForTutoring[studentId - 1][1] = -1;

        //Decreasing occupied chair count as the student is leaving the chair and will proceed for tutoring
        numberOfOccupiedChairs--;

        //Since the student left the chair and is moving for tutoring, increment its count
        studentsBeingTutoredNow++;

        //Release lock for shared variable
        pthread_mutex_unlock(&chairsLock);

        //Student is being tutored (0.2 ms)
        usleep(TUTORING_SLEEP_TIME);

        //After tutoring the student
        //Acquire lock for shared variable
        pthread_mutex_lock(&chairsLock);

        //Since student's tutoring is done, decrement tutoringNow after tutoring.
        studentsBeingTutoredNow--;

        //Increment the number of sessions held after tutoring
        totalTutoringSessionsHeld++;

        printf("T: Student %d tutored by Tutor %d. Students tutored now = %d. Total sessions tutored = %d\n", studentId, tutorIdOfCurrentTutor - numberOfStudents, studentsBeingTutoredNow, totalTutoringSessionsHeld);

        //Release lock for shared variable
        pthread_mutex_unlock(&chairsLock);

        //Acquire lock for shared variable
        pthread_mutex_lock(&tutoringFinishedQueueLock);
        
        //Update shared data so student can know who tutored him.
        tutoringFinishedQueue[studentId - 1] = tutorIdOfCurrentTutor;

        //Release lock for shared variable
        pthread_mutex_unlock(&tutoringFinishedQueueLock);
    }
}

void initializeVariables(int iNumberOfStudents, int iNumberOfTutors, int iNumberOfChairsInWaitingArea, int iNumberOfTimesHelpRequired)
{
    int tIterator = 0;

    if(iNumberOfStudents < 1)
    {
        fprintf(stderr, "ERROR! There should be at least 1 student\n");
        exit(-1);
    }

    if(iNumberOfTutors < 1)
    {
        fprintf(stderr, "ERROR! There should be at least 1 tutor\n");
        exit(-1);
    }

    if(iNumberOfChairsInWaitingArea < 1)
    {
        fprintf(stderr, "ERROR! There should be at least 1 chair in waiting area\n");
        exit(-1);
    }

    if(iNumberOfTimesHelpRequired < 0)
    {
        fprintf(stderr, "ERROR! No negative values of help allowed\n");
        exit(-1);
    }

    studentsInWaitingAreaQueue = (int *) malloc(iNumberOfStudents * sizeof(int));
    studentIdsQueue = (int *) malloc(iNumberOfStudents * sizeof(int));
    studentPriorities = (int *) malloc(iNumberOfStudents * sizeof(int));
    tutorIdsQueue = (int *) malloc(iNumberOfTutors * sizeof(int));
    tutoringFinishedQueue = (int *) malloc(iNumberOfStudents * sizeof(int));

    //priorityQueueForTutoring contains 2 variables for each student
    //0th Index: Student's priority
    //1st Index: Student's position in the waiting queue
    priorityQueueForTutoring = (int **) malloc(iNumberOfStudents * sizeof(int *));

    if(NULL == priorityQueueForTutoring)
    {
        fprintf(stderr, "ERROR! Memory allocation failed\n");
        exit(-1);
    }

    for(tIterator = 0; tIterator < iNumberOfStudents; tIterator++)
    {
        priorityQueueForTutoring[tIterator] = (int *) malloc(2 * sizeof(int));

        if(NULL == priorityQueueForTutoring)
        {
            fprintf(stderr, "ERROR! Memory allocation failed\n");
            exit(-1);
        }
    }

    if((NULL == studentsInWaitingAreaQueue) || (NULL == studentIdsQueue) || (NULL == studentPriorities) || (NULL == tutorIdsQueue) || (NULL == tutoringFinishedQueue))
    {
        fprintf(stderr, "ERROR! Memory allocation failed\n");
        exit(-1);
    }
}

int main(int argc, char *argv[])
{
    int tIterator = 0;

    //Check for number of passed arguments
    if(argc != 5)
    {
        fprintf(stderr, "ERROR! Please provide sufficient arguments: #students, #tutors, #chairs, #help\n");
        exit(-1);
    }

    //Convert arguments from character to integer
    numberOfStudents = atoi(argv[1]);
    numberOfTutors = atoi(argv[2]);
    numberOfChairsInWaitingArea = atoi(argv[3]);
    numberOfTimesHelpRequired = atoi(argv[4]);

    //Argument validation and dynamic memory allocation
    initializeVariables(numberOfStudents, numberOfTutors, numberOfChairsInWaitingArea, numberOfTimesHelpRequired);

    //Fill default values
    for(tIterator = 0; tIterator < numberOfStudents; tIterator++)
    {
        studentsInWaitingAreaQueue[tIterator] = -1;
        tutoringFinishedQueue[tIterator] = -1;
        priorityQueueForTutoring[tIterator][0] = -1;
        priorityQueueForTutoring[tIterator][1] = -1;
        studentPriorities[tIterator] = 0;
    }

    //Initialize lock and semaphores
    //Initialized to 0 as on 1st wait call to sem, the current thread should be allowed and other threads should be blocked
    sem_init(&semCoordinatorIsWaitingForStudent, 0, 0);
    sem_init(&semTutorIsWaitingForCoordinator, 0, 0);
    pthread_mutex_init(&chairsLock, NULL);
    pthread_mutex_init(&queueLock, NULL);
    pthread_mutex_init(&tutoringFinishedQueueLock, NULL);

    //Initialize threads
    pthread_t students[numberOfStudents];
    pthread_t tutors[numberOfTutors];
    pthread_t coordinator;

    //Create threads
    //Coordinator thread
    pthread_create(&coordinator, NULL, coordinatorThread, NULL);

    for(tIterator = 0; tIterator < numberOfStudents; tIterator++)
    {
        studentIdsQueue[tIterator] = tIterator + 1;
        //Student thread
        pthread_create(&students[tIterator], NULL, studentThread, (void *)&studentIdsQueue[tIterator]);
    }

    for(tIterator = 0; tIterator < numberOfTutors; tIterator++)
    {
        tutorIdsQueue[tIterator] = tIterator + numberOfStudents + 1;
        //Tutor thread
        pthread_create(&tutors[tIterator], NULL, tutorThread, (void *)&tutorIdsQueue[tIterator]);
    }

    //Join threads
    //Coordinator thread
    pthread_join(coordinator, NULL);

    for(tIterator = 0; tIterator < numberOfStudents; tIterator++)
    {
        //Student thread
        pthread_join(students[tIterator], NULL);
    }

    for(tIterator = 0; tIterator < numberOfTutors; tIterator++)
    {
        //Tutor thread
        pthread_join(tutors[tIterator], NULL);
    }

    return 0;
}
