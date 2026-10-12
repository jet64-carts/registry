// runtime/jet_thread.h
#ifndef JET_THREAD_H
#define JET_THREAD_H

// Run some of your game's heavy work on a second worker, at the same time as
// the game loop, so the loop never has to stop and wait for it. A thread is a
// second worker that runs a function of yours while the main game carries on.
// A mutex is a lock: only one worker can hold it at a time, so two workers can
// share one piece of data without both changing it at once. A condition
// variable is a way for one worker to sleep until another says "ready".
//
// For one long job, start a worker with jet_thread_create and later call
// jet_thread_join, which waits for it to finish. For many small jobs, set up
// the pool once with jet_thread_pool_init, add short jobs that do not depend on
// each other with jet_thread_pool_add, call jet_thread_pool_wait before you
// read what they produced, and shut the pool down from _end with
// jet_thread_pool_shutdown. The pool has a fixed number of workers and a
// fixed-size queue.
//
// Two rules keep this safe. Every pointer you hand to a worker must stay valid
// until that worker has finished, because the worker reads it later, not now.
// Any data that more than one worker can touch, where at least one of them
// writes to it, needs a mutex around it.
//
// The web build has no threads, and neither does any other build without them.
// There the same calls run your work on the spot, one job after another, so
// the results come out the same. That is why a cartridge must never depend on
// how fast, or in what order, its workers run.
//
// Complete parallel-batch example (copy into main.c):
//
//   #include "jet64.h"
//   #include "jet_thread.h"
//
//   enum { JOB_COUNT = 4, WORKER_COUNT = 2 };
//   static int job_ids[JOB_COUNT] = { 0, 1, 2, 3 };
//   static int squares[JOB_COUNT];
//
//   static void calculate_square(void* arg) {
//       const int id = *(const int*)arg;
//       squares[id] = id * id;
//   }
//
//   void _init(void) {
//       jet_thread_pool_init(WORKER_COUNT);
//       for (int i = 0; i < JOB_COUNT; i++) jet_thread_pool_add(calculate_square, &job_ids[i]);
//       jet_thread_pool_wait(); // every job has finished writing before the main thread reads
//   }
//
//   void _update(float dt) { (void)dt; }
//
//   void _draw(void) {
//       char line[48];
//       snprintf(line, sizeof line, "SQUARES: %d %d %d %d", squares[0], squares[1], squares[2], squares[3]);
//       jet_cls(0);
//       jet_print(line, 16, 16, 15);
//   }
//
//   void _end(void) {
//       jet_thread_pool_shutdown();
//   }

/**
 * Type: jet_mutex_t
 * Summary: A handle for one mutex, a lock that lets only one worker at a time touch some shared data.
 * Include: jet_thread.h
 * Notes: Make one with jet_mutex_create; NULL means the runtime could not make it. Keep the handle, pass it to the lock and unlock calls, and do not look inside it. A handle that is not NULL is yours until you call jet_mutex_destroy on it, exactly once, after every worker has stopped locking it. After that the value means nothing and must not be used. On the web build, or any other build without threads, the runtime hands back a harmless stand-in handle, and locking and unlocking do nothing.
 * Example:
 *   jet_mutex_t lock = jet_mutex_create();
 *   if (lock != NULL) { jet_mutex_destroy(lock); }
 */
typedef void* jet_mutex_t;

/**
 * Type: jet_cond_t
 * Summary: A handle for one condition variable, a way for a worker to sleep until another worker says "ready".
 * Include: jet_thread.h
 * Notes: Make one with jet_cond_create; NULL means the runtime could not make it. Keep the handle, pass it to the wait and signal calls, and do not look inside it. A handle that is not NULL is yours until you call jet_cond_destroy on it, exactly once, after no worker can still be waiting on it. After that the value means nothing and must not be used. On the web build, or any other build without threads, the runtime hands back a harmless stand-in handle, and waiting and signalling do nothing.
 * Example:
 *   jet_cond_t ready = jet_cond_create();
 *   if (ready != NULL) { jet_cond_destroy(ready); }
 */
typedef void* jet_cond_t;

/**
 * Type: jet_thread_t
 * Summary: A handle for one worker thread started by jet_thread_create.
 * Include: jet_thread.h
 * Notes: NULL means the runtime could not start the worker. A handle that is not NULL is yours until you call jet_thread_join on it, exactly once. Join waits for the worker to finish and then lets the handle go, so the value must not be used after that. On the web build, or any other build without threads, the worker's function runs to the end on the spot, and the stand-in handle you get back is still safe to join once.
 * Example:
 *   jet_thread_t worker = jet_thread_create(worker_main, &job);
 *   if (worker != NULL) { jet_thread_join(worker); }
 */
typedef void* jet_thread_t;

/**
 * Function: jet_mutex_create
 * Summary: Makes a new mutex, a lock that lets only one worker at a time touch some shared data.
 * Parameters:
 *   - None.
 * Returns: The mutex handle, or NULL if the runtime could not allocate it or set it up.
 * Include: jet_thread.h
 * Notes: On the web build, or any other build without threads, you get a harmless stand-in handle and the lock and unlock calls do nothing. When you are finished with a real mutex, call jet_mutex_destroy on it.
 * Example:
 *   jet_mutex_t lock = jet_mutex_create();
 */
jet_mutex_t jet_mutex_create(void);

/**
 * Function: jet_mutex_lock
 * Summary: Takes hold of a mutex. If another worker holds it, this waits until they let go.
 * Parameters:
 *   - mutex: The mutex to lock, from jet_mutex_create. NULL is ignored on native builds.
 * Returns: Nothing.
 * Include: jet_thread.h
 * Notes: Every lock needs a matching jet_mutex_unlock on the same path through your code, so the mutex is never left locked by mistake. Do not hold a mutex while you run long cartridge or file work, because every other worker that wants it has to wait.
 * Example:
 *   jet_mutex_lock(lock);
 *   shared_value++;
 *   jet_mutex_unlock(lock);
 */
void jet_mutex_lock(jet_mutex_t mutex);

/**
 * Function: jet_mutex_unlock
 * Summary: Lets go of a mutex you locked with jet_mutex_lock, so another worker can take it.
 * Parameters:
 *   - mutex: The mutex to unlock. NULL is ignored on native builds.
 * Returns: Nothing.
 * Include: jet_thread.h
 * Notes: Only the worker that locked the mutex may unlock it. Beyond that, the mutex behaves the way the platform's own mutex does.
 * Example:
 *   jet_mutex_unlock(lock);
 */
void jet_mutex_unlock(jet_mutex_t mutex);

/**
 * Function: jet_mutex_destroy
 * Summary: Destroys a mutex and gives its resources back to the system.
 * Parameters:
 *   - mutex: The mutex handle from jet_mutex_create. NULL is ignored.
 * Returns: Nothing.
 * Include: jet_thread.h
 * Notes: Never destroy a mutex while another worker might still lock it, unlock it, or wait on it.
 * Example:
 *   jet_mutex_destroy(lock);
 */
void jet_mutex_destroy(jet_mutex_t mutex);

/**
 * Function: jet_cond_create
 * Summary: Makes a new condition variable, a way for one worker to sleep until another worker says "ready".
 * Parameters:
 *   - None.
 * Returns: The condition variable's handle, or NULL if the runtime could not allocate it or set it up.
 * Include: jet_thread.h
 * Notes: On the web build, or any other build without threads, you get a harmless stand-in handle and the wait and signal calls do nothing. When you are finished with a real condition variable, call jet_cond_destroy on it.
 * Example:
 *   jet_cond_t ready = jet_cond_create();
 */
jet_cond_t jet_cond_create(void);

/**
 * Function: jet_cond_wait
 * Summary: Puts this worker to sleep until another worker signals the condition variable. The mutex is let go while it sleeps and taken back before it returns, in one step that nothing can slip between.
 * Parameters:
 *   - cond: The condition variable from jet_cond_create.
 *   - mutex: A mutex you have already locked, the one that guards the thing you are waiting for.
 * Returns: Nothing.
 * Include: jet_thread.h
 * Notes: Always call it inside a loop that checks the thing you are waiting for, as the example does. A worker can wake for no reason, and can miss a change that happened before it went to sleep, so only the check tells the truth. On builds without threads this does nothing.
 * Example:
 *   while (!ready_flag) jet_cond_wait(ready, lock);
 */
void jet_cond_wait(jet_cond_t cond, jet_mutex_t mutex);

/**
 * Function: jet_cond_signal
 * Summary: Wakes one worker that is sleeping in jet_cond_wait on this condition variable.
 * Parameters:
 *   - cond: The condition variable to signal. NULL is ignored on native builds.
 * Returns: Nothing.
 * Include: jet_thread.h
 * Notes: First change the shared thing the sleeper is waiting for, while holding its mutex, and signal after.
 * Example:
 *   ready_flag = true;
 *   jet_cond_signal(ready);
 */
void jet_cond_signal(jet_cond_t cond);

/**
 * Function: jet_cond_broadcast
 * Summary: Wakes every worker that is sleeping in jet_cond_wait on this condition variable.
 * Parameters:
 *   - cond: The condition variable to broadcast on. NULL is ignored on native builds.
 * Returns: Nothing.
 * Include: jet_thread.h
 * Notes: Use it when one change might let several workers carry on at once, such as shutting down, or a queue that has just opened up for everyone.
 * Example:
 *   shutting_down = true;
 *   jet_cond_broadcast(ready);
 */
void jet_cond_broadcast(jet_cond_t cond);

/**
 * Function: jet_cond_destroy
 * Summary: Destroys a condition variable and gives its resources back to the system.
 * Parameters:
 *   - cond: The condition variable's handle from jet_cond_create. NULL is ignored.
 * Returns: Nothing.
 * Include: jet_thread.h
 * Notes: Never destroy a condition variable while a worker might still be waiting on it.
 * Example:
 *   jet_cond_destroy(ready);
 */
void jet_cond_destroy(jet_cond_t cond);

/**
 * Function: jet_thread_create
 * Summary: Starts a worker thread that runs a function of yours at the same time as the game.
 * Parameters:
 *   - func: The function the worker runs. It takes one void* and gives back a void*.
 *   - arg: A pointer of yours, handed to func as its one argument, unchanged.
 * Returns: A handle to give jet_thread_join later, or NULL if the runtime could not start the thread.
 * Include: jet_thread.h
 * Notes: On the web build, or any other build without threads, func runs to the end right here, before this call returns, and you get a stand-in handle.
 * Example:
 *   jet_thread_t worker = jet_thread_create(worker_main, &job_data);
 */
jet_thread_t jet_thread_create(void* (*func)(void*), void* arg);

/**
 * Function: jet_thread_join
 * Summary: Waits for a worker thread to finish, then lets go of its handle.
 * Parameters:
 *   - thread: The handle from jet_thread_create. NULL is ignored.
 * Returns: Nothing.
 * Include: jet_thread.h
 * Notes: Join each real thread handle exactly once. On builds without threads this does nothing.
 * Example:
 *   jet_thread_join(worker);
 */
void jet_thread_join(jet_thread_t thread);

// ----------------------------------------------------
// The thread pool: a fixed team of workers that take
// small jobs from one shared queue
// ----------------------------------------------------
/**
 * Callback: jet_job_fn
 * Summary: The shape of a job function for the thread pool. It does one piece of work on the data you point it at.
 * Parameters:
 *   - arg: The pointer you gave jet_thread_pool_add, handed on unchanged.
 * Returns: Nothing.
 * Include: jet_thread.h
 * Notes: On a native build the job may run on a pool worker at the same time as other jobs and as the game. On a build without threads it runs inside jet_thread_pool_wait, on the main thread. So it must be safe to run beside other work, it must not call any drawing function, and it must finish before the data it points at goes away.
 * Example:
 *   static void build_chunk(void* arg) {
 *       Chunk* chunk = (Chunk*)arg;
 *       generate_chunk_pixels(chunk);
 *   }
 */
typedef void (*jet_job_fn)(void* arg);

/**
 * Function: jet_thread_pool_init
 * Summary: Sets up the thread pool, a fixed team of workers that run jobs at the same time.
 * Parameters:
 *   - num_threads: How many workers you want. A native build holds this to the runtime's maximum, and to at least one.
 * Returns: Nothing.
 * Include: jet_thread.h
 * Notes: Call it once, before you add any job, and pair it with jet_thread_pool_shutdown. On the web build, or any other build without threads, jobs are queued up and then run one after another, on the main thread, when you call jet_thread_pool_wait.
 * Example:
 *   jet_thread_pool_init(4);
 */
void jet_thread_pool_init(int num_threads);

/**
 * Function: jet_thread_pool_add
 * Summary: Puts one job in the pool's queue, for a worker to run.
 * Parameters:
 *   - func: The job function to run.
 *   - arg: A pointer of yours, handed to func unchanged.
 * Returns: Nothing.
 * Include: jet_thread.h
 * Notes: The queue has a fixed size. On a native build, if it is full, this call waits until a worker takes a job and makes room. The job must be safe to run beside other work, and whatever arg points at must still exist when the job runs, so never point it at a local variable of a function that will have returned by then.
 * Example:
 *   jet_thread_pool_add(build_chunk_job, &chunks[i]);
 */
void jet_thread_pool_add(jet_job_fn func, void* arg);

/**
 * Function: jet_thread_pool_wait
 * Summary: Waits until every job in the queue, and every job a worker is running, has finished.
 * Parameters:
 *   - None.
 * Returns: Nothing.
 * Include: jet_thread.h
 * Notes: Call it before you read anything the jobs wrote; it is the fence in a frame between their work and yours. On the web build, or any other build without threads, this is where the queued jobs actually run, one after another, on the thread that called it.
 * Example:
 *   jet_thread_pool_wait();
 */
void jet_thread_pool_wait(void);

/**
 * Function: jet_thread_pool_shutdown
 * Summary: Stops the thread pool cleanly, once the jobs already in the queue have finished.
 * Parameters:
 *   - None.
 * Returns: Nothing.
 * Include: jet_thread.h
 * Notes: On a native build it wakes every worker and waits for each one to end. Do not add a job after this unless you call jet_thread_pool_init again.
 * Example:
 *   jet_thread_pool_shutdown();
 */
void jet_thread_pool_shutdown(void);

// Short names for the calls above, kept so older cartridge code still builds.
#define mutex_create jet_mutex_create
#define mutex_lock jet_mutex_lock
#define mutex_unlock jet_mutex_unlock
#define mutex_destroy jet_mutex_destroy
#define cond_create jet_cond_create
#define cond_wait jet_cond_wait
#define cond_signal jet_cond_signal
#define cond_broadcast jet_cond_broadcast
#define cond_destroy jet_cond_destroy
#define thread_create jet_thread_create
#define thread_join jet_thread_join
#define thread_pool_init jet_thread_pool_init
#define thread_pool_add jet_thread_pool_add
#define thread_pool_wait jet_thread_pool_wait
#define thread_pool_shutdown jet_thread_pool_shutdown

#endif // JET_THREAD_H
