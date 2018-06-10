/**
* Name:    Francesco
* Surname: Longo
* ID:      223428
* Lab:     11
* Ex:      2
*
* Realize in the Windows environment a producer/consumer application
* with the following characteristics:
*
* 1. There are P producers and C consumers
* 2. Producers and consumers communicate using a circular buffer.
*    The circular buffer is implemented as a queue on a dynamic array of
*    size N.
* 3. Each producer inserts in the queue integer values (randomly
*    generated) at random time intervals (randomly generated and
*    varying from 0 to T seconds).
* 4. Each consumer extracts from the queue an integer value at random
*    time intervals (randomly generated and varying from 0 to T seconds)
*    and prints it out on standard output with its personal thread
*    identifier.
*
* Note that:
* - The integer values P, C, N, and T are passed to the program on the 
*   command line
* - The circular queue has to be properly protected, to avoid:
*   - insertions in a full queue
*   - extractions from an empty queue
*   - insertions by two producers at the same time
*   - extractions by two consumers at the same time
* - Find a reasonable way to terminate all producers and all consumers.
*
* Suggestions
* -----------
*
* - Refer to the Producer/Consumer logical scheme with more than
*   one producer and more than one consumer working concurrently.
* - To stop the application in a reasonable way use the following
*  strategy:
*   - Each producer terminates after a predefined number of products
*     has been produced
*   - The main thread awaits for all producers to end, and when all
*     producers have terminated it terminates all consumers
*   - To terminate all consumers (after they have consumed all produced
*     elements *not* before !) the main thread may insert in the queue
*     "termination" (sentinel) values.
*
**/

// !UNICODE
#ifndef UNICODE
#define UNICODE
#define _UNICODE
#endif

// !_CRT_SECURE_NO_WARNINGS
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif 

// include
#include <Windows.h>
#include <tchar.h>
#include <stdio.h>
#include <assert.h>

// define
#define MAX_THREAD_ITERATIONS 200
#define MESSAGE_CONTENT_MAX_LEN 256
#define DEBUG 0

// global variables
// this is a global variable used for protecting the message_id
CRITICAL_SECTION message_cs;

// this global variable is used for generating messages with unique id
// instead of using that, i could have created an ADT for managing the messages,
// and pass it to each thread
DWORD message_id;

// typedef
typedef struct {
	DWORD id;
	TCHAR content[MESSAGE_CONTENT_MAX_LEN];
} MESSAGE, *LPMESSAGE;

// a thread-safe queue
typedef struct {
	DWORD dimension;
	DWORD insertPosition;
	DWORD removePosition;
	DWORD nMessages;
	LPMESSAGE messages;
	CONDITION_VARIABLE full;
	CONDITION_VARIABLE empty;
	CRITICAL_SECTION cs;	// guarantees the atomicity on the structure
} QUEUE, *LPQUEUE;

typedef struct {
	LPQUEUE q;
	DWORD maxWaitTime;
} PARAM, *LPPARAM;

// prototypes
MESSAGE writeMessage(LPTSTR content);
LPTSTR printMessage(MESSAGE m);

LPQUEUE createQueue(DWORD dimension);
VOID enqueue(LPQUEUE, MESSAGE);
MESSAGE dequeue(LPQUEUE);
VOID destroyQueue(LPQUEUE);

DWORD WINAPI consumerThreadFunction(LPVOID);
DWORD WINAPI producerThreadFunction(LPVOID);

// main
INT _tmain(INT argc, LPTSTR argv[]) {
	DWORD P, C, N, T;
	DWORD i;
	PARAM param;
	LPQUEUE q;
	LPHANDLE producers, consumers;

	q = NULL;
	producers = consumers = NULL;

	// check number of parameters
	if (argc != 5) {
		_ftprintf(stderr, _T("Usage: %s P C N T\nP = number of producers\nC = number of consumers\nN = size of queue\nT = maximum time between subsequent productions / consuptions\n"), argv[0]);
		return 1;
	}

	// take parameters from command line
	P = _tstol(argv[1]);
	C = _tstol(argv[2]);
	N = _tstol(argv[3]);
	T = _tstol(argv[4]);

	// check parameters
	if (P * C * N * T == 0) {
		_ftprintf(stderr, _T("Error: some parameters are invalid / equal to 0\n"));
		return 2;
	}

	// create and check array for producers
	producers = (LPHANDLE)calloc(P, sizeof(HANDLE));
	if (producers == NULL) {
		_ftprintf(stderr, _T("Error allocating handles for producers\n"));
		return 3;
	}

	// create and check array for consumers
	consumers = (LPHANDLE)calloc(C, sizeof(HANDLE));
	if (consumers == NULL) {
		_ftprintf(stderr, _T("Error allocating handles for consumers"));
		free(producers);
		return 4;
	}

	// create and check the queue
	q = createQueue(N);
	if (q == NULL) {
		_ftprintf(stderr, _T("queue not created\n"));
		free(producers);
		free(consumers);
		return 5;
	}

	message_id = 0;
	InitializeCriticalSection(&message_cs);

	param.maxWaitTime = T;
	param.q = q;

	for (i = 0; i < P; i++) {
		// create producers
		producers[i] = CreateThread(NULL, 0, producerThreadFunction, &param, THREAD_TERMINATE, NULL);
		if (producers[i] == INVALID_HANDLE_VALUE) {
			_ftprintf(stderr, _T("Impossible to create a producer thread. Error: %x"), GetLastError());
			free(producers);
			free(consumers);
			destroyQueue(q);
			return 6;
		}
	}

	for (i = 0; i < C; i++) {
		// create consumers
		consumers[i] = CreateThread(NULL, 0, consumerThreadFunction, &param, THREAD_TERMINATE, NULL);
		if (consumers[i] == INVALID_HANDLE_VALUE) {
			_ftprintf(stderr, _T("Impossible to create a consumer thread. Error: %x"), GetLastError());
			free(producers);
			free(consumers);
			destroyQueue(q);
			return 7;
		}
	}

	// wait for all terminating thread for infinite time
	if (WaitForMultipleObjects(P, producers, TRUE, INFINITE) == WAIT_FAILED) {
		_ftprintf(stderr, _T("Error waiting for the producers\n"));
		free(producers);
		free(consumers);
		destroyQueue(q);
		return 8;
	}

	// close all handles & frees array
	for (i = 0; i < P; i++) {
		assert(producers[i]);
		CloseHandle(producers[i]);
	}
	free(producers);

	// wait for all terminating thread for infinite time
	if (WaitForMultipleObjects(C, consumers, TRUE, INFINITE) == WAIT_FAILED) {
		_ftprintf(stderr, _T("Error waiting for the consumers\n"));
		free(producers);
		free(consumers);
		destroyQueue(q);
		return 9;
	}

	// close all handles & frees array
	for (i = 0; i < C; i++) {
		assert(consumers[i]);
		CloseHandle(consumers[i]);
	}
	free(consumers);

	DeleteCriticalSection(&message_cs);

	return 0;
}

DWORD WINAPI consumerThreadFunction(LPVOID p) {
	LPPARAM param = (LPPARAM)p;
	MESSAGE m;
	DWORD i;
	LPTSTR messagePrint;
	DWORD timeToSleep;
	DWORD tId = GetCurrentThreadId();

	srand(tId);

	for (i = 0; i < MAX_THREAD_ITERATIONS; i++) {
		timeToSleep = 1000 * (rand() % (param->maxWaitTime + 1));

#if DEBUG
		_tprintf(_T("consumer %u going to sleep for %u\n"), tId, timeToSleep);
#endif // DEBUG

		// sleep
		Sleep(timeToSleep);

		// consume a message
		m = dequeue(param->q);
		messagePrint = printMessage(m);
		_tprintf(_T("consumer thread %u received a message: %s\n"), tId, messagePrint);
		
		free(messagePrint);
	}
	return 0;
}

DWORD WINAPI producerThreadFunction(LPVOID p) {
	LPPARAM param = (LPPARAM)p;
	MESSAGE m;
	DWORD i;
	DWORD tId;
	DWORD timeToSleep;
	TCHAR msgContent[MESSAGE_CONTENT_MAX_LEN];
	INT randomValue;

	tId = GetCurrentThreadId();
	
	srand(tId);
	
	for (i = 0; i < MAX_THREAD_ITERATIONS; i++) {
		timeToSleep = 1000 * (rand() % (param->maxWaitTime + 1));

#if DEBUG
		_tprintf(_T("producer %u going to sleep for %u\n"), tId, timeToSleep);
#endif // DEBUG

		// sleep 
		Sleep(timeToSleep);
		
		// produce a message
		randomValue = rand();
		_stprintf(msgContent, _T("hello from thread %u. My random number is %d"), tId, randomValue);
		m = writeMessage(msgContent);
		enqueue(param->q, m);
		_tprintf(_T("producer %u sent a message\n"), tId);
	}
	return 0;
}

// queue management
LPQUEUE createQueue(DWORD dimension) {
	LPQUEUE q = NULL;

	// check the parameter (a queue of size 0 has no meaning).
	// For the maximum value, dimension is unsigned long, so when passing this parameter
	// to the CreateSemaphore, it will be interpreted as signed long: if the value is greater
	// than 0x7fffffff it will be seen as a negative value. Better to remain in the range.
	if (dimension == 0 || dimension > MAXLONG) {
		_ftprintf(stderr, _T("CreateQueue error with the specified dimension\n"));
		return NULL;
	}

	// allocate the structure
	q = (LPQUEUE)calloc(1, sizeof(QUEUE));
	if (q == NULL) {
		_ftprintf(stderr, _T("CreateQueue error allocating the queue\n"));
		return NULL;
	}

	q->dimension = dimension;

	// allocate the buffer for messages
	q->messages = (LPMESSAGE)calloc(dimension, sizeof(MESSAGE));
	if (q->messages == NULL) {
		free(q);
		_ftprintf(stderr, _T("CreateQueue error allocating the buffer for messages\n"));
		return NULL;
	}

	q->insertPosition = 0;
	q->removePosition = 0;
	q->nMessages = 0;

	InitializeCriticalSection(&q->cs);
	InitializeConditionVariable(&q->empty);
	InitializeConditionVariable(&q->full);

	return q;
}

VOID enqueue(LPQUEUE q, MESSAGE m) {
	if (q == NULL) {
		_ftprintf(stderr, _T("enqueue called on a NULL queue\n"));
		return;
	}

	EnterCriticalSection(&q->cs);

	while (q->nMessages == q->dimension) {

#if DEBUG > 1
		_tprintf(_T("enqueue going to sleep\n"));
#endif // DEBUG > 1

		SleepConditionVariableCS(&q->full, &q->cs, INFINITE);
	}
	assert(q->nMessages < q->dimension);

#if DEBUG > 1
	_tprintf(_T("enqueuing now\n"));
#endif // DEBUG > 1

	q->messages[q->insertPosition++] = m;
	q->insertPosition %= q->dimension;
	q->nMessages++;

	WakeConditionVariable(&q->empty);

	LeaveCriticalSection(&q->cs);

	return;
}

MESSAGE dequeue(LPQUEUE q) {
	MESSAGE m;

	if (q == NULL) {
		_ftprintf(stderr, _T("dequeue called on a NULL queue\n"));
		m.id = 0;
		return m;
	}

	EnterCriticalSection(&q->cs);

	while (q->nMessages == 0) {

#if DEBUG > 1
		_tprintf(_T("dequeue going to sleep\n"));
#endif // DEBUG > 1

		SleepConditionVariableCS(&q->empty, &q->cs, INFINITE);
	}
	assert(q->nMessages > 0);

#if DEBUG > 1
	_tprintf(_T("dequeuing now\n"));
#endif // DEBUG > 1

	m = q->messages[q->removePosition++];
	q->removePosition %= q->dimension;
	q->nMessages--;

	WakeConditionVariable(&q->full);

	LeaveCriticalSection(&q->cs);

	return m;
}

VOID destroyQueue(LPQUEUE q) {
	if (q == NULL) {
		_ftprintf(stderr, _T("destroyQueue called on a NULL queue\n"));
		return;
	}

	assert(q->messages);

	free(q->messages);

	DeleteCriticalSection(&q->cs);

	free(q);
}

MESSAGE writeMessage(LPTSTR content) {
	MESSAGE m;

	_tcsncpy(m.content, content, MESSAGE_CONTENT_MAX_LEN);

	EnterCriticalSection(&message_cs);
	m.id = message_id++;
	LeaveCriticalSection(&message_cs);

	return m;
}

// free the output
LPTSTR printMessage(MESSAGE m) {
	LPTSTR output;

	// 20 more TCHAR should be enough for printing other things
	DWORD message_output_len = MESSAGE_CONTENT_MAX_LEN + 20;

	output = (LPTSTR)calloc(message_output_len, sizeof(TCHAR));

	_stprintf(output, _T("id: %u content: %s"), m.id, m.content);

	return output;
}
