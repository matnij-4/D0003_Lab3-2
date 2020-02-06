#include <setjmp.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include "tinythreads.h"

#define NULL            0
#define DISABLE()       cli()
#define ENABLE()        sei()
#define STACKSIZE       80
#define NTHREADS        4
#define SETSTACK(buf,a) *((unsigned int *)(buf)+8) = (unsigned int)(a) + STACKSIZE - 4; \
                        *((unsigned int *)(buf)+9) = (unsigned int)(a) + STACKSIZE - 4

struct thread_block {
    void (*function)(int);   // code to run
    int arg;                 // argument to the above
    thread next;             // for use in linked lists
    jmp_buf context;         // machine state
    char stack[STACKSIZE];   // execution stack space
};

struct thread_block threads[NTHREADS];

struct thread_block initp;

thread freeQ   = threads;
thread readyQ  = NULL;
thread current = &initp;

int initialized = 0;

//Create a global Counter.
int clockTimmerBlink = 0;


static void initialize(void) {
    int i;
    for (i=0; i<NTHREADS-1; i++)
        threads[i].next = &threads[i+1];
    threads[NTHREADS-1].next = NULL;
	
	//Button settings.
	
	//Activate the button
	PORTB = 0x80;
	
	//Enabel Interrupt Enabel 1 on the PCIE1
	EIMSK = (1 << PCIE1);
	PCMSK1 = (1 << PCINT15);
	
	
	// The clock settings. 
	
	//OC1A is set high on compare match.
	TCCR1A = (1 << COM1A0) | (1 << COM1A1);
	
	// Set timer to CTC and prescale Factor on 1024.
	TCCR1B = (1 << WGM12) | (1 << CS10) |(1 << CS12);
	
	// Set Value to around 1s.
	OCR1A = 3906;
	
	//clearing the TCNT1 register during initialization.
	TCNT1 = 0x0;
	
	//Compare a match interrupt Enable.
	TIMSK1 = (1 << OCIE1A);
	
    initialized = 1;
}

static void enqueue(thread p, thread *queue) {
	p->next = NULL;
	if (*queue == NULL)
	{
		*queue = p;
	} 
	//Put it first in the queue.
	else
	{
		//save the old queue to next.
		thread oldQ = *queue;
		p->next = oldQ;
		
		//Put the new threads first.
		*queue = p;
	}
}


static thread dequeue(thread *queue) {
    thread p = *queue;
    if (*queue) {
        *queue = (*queue)->next;
    } else {
        // Empty queue, kernel panic!!!
        while (1) ;  // not much else to do...
    }
    return p;
}

static void dispatch(thread next) {
    if (setjmp(current->context) == 0) {
        current = next;
        longjmp(next->context,1);
    }
}

void spawn(void (* function)(int), int arg) {
    thread newp;

    DISABLE();
    if (!initialized) initialize();

    newp = dequeue(&freeQ);
    newp->function = function;
    newp->arg = arg;
    newp->next = NULL;
    if (setjmp(newp->context) == 1) {
        ENABLE();
        current->function(current->arg);
        DISABLE();
        enqueue(current, &freeQ);
        dispatch(dequeue(&readyQ));
    }
    SETSTACK(&newp->context, &newp->stack);

    enqueue(newp, &readyQ);
    ENABLE();
}

void yield(void) {
	ENABLE();
	enqueue(current, &readyQ);
	dispatch(dequeue(&readyQ));
	DISABLE();
}

void lock(mutex *m) 
{
	DISABLE();
	
	//Take the mutex
	if (m->locked == 0)
	{
		m->locked = 1;
	}
	//Wait if it is already locked.
	else
	{
		enqueue(current, &(m->waitQ));
		dispatch(dequeue(&readyQ));
	}
	ENABLE();

}

void unlock(mutex *m) 
{
	DISABLE();
	
	
	//Check it it is non empty.
	if (m->waitQ != NULL) 
	{
		enqueue(current, &readyQ);
		dispatch(dequeue(&(m->waitQ)));
	} 
	//Realese the mutex.
	else
	{
		m->locked = 0;
	}
	
	ENABLE();
}