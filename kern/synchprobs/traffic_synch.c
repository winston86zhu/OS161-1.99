#include <types.h>
#include <lib.h>
#include <synchprobs.h>
#include <synch.h>
#include <opt-A1.h>
#include <array.h>

/* 
 * This simple default synchronization mechanism allows only vehicle at a time
 * into the intersection.   The intersectionSem is used as a a lock.
 * We use a semaphore rather than a lock so that this code will work even
 * before locks are implemented.
 */

/* 
 * Replace this default synchronization mechanism with your own (better) mechanism
 * needed for your solution.   Your mechanism may use any of the available synchronzation
 * primitives, e.g., semaphores, locks, condition variables.   You are also free to 
 * declare other global variables if your solution requires them.
 */

/*
 * replace this with declarations of any synchronization and other variables you need here
 */
//static struct semaphore *intersectionSem;
static struct cv *intersectioncv;
static struct lock* intersection_lock; 
static struct array * intersection_all;
static struct cv * end_cv;
static volatile int waiting_v = 0; 
static volatile int passing_v = 0; 
//static struct array * all_from;
//static struct array * all_to;


typedef struct vehicle
{
	Direction from;
	Direction dest;
} vehicle;


/*bool all_same_dir(Direction v_from, Direction v_to){
	KASSERT(array_num(intersection_all) != 0);
	for(unsigned int i=0; i < array_num(intersection_all); i++){
		if(!((array_get(intersection_all,i)->from == v_from && array_get(intersection_all,i)->from == v_to)||
		 (array_get(intersection_all,i)->from == v_to && array_get(intersection_all,i)->from == v_from))){
			return false;
		}
	}
	return true;
}*/

static bool test_rightturn(Direction v_from, Direction v_to){
	return ((v_from == east && v_to == north)||
		(v_from == north && v_to == west)||
		(v_from == west && v_to == south)||
		(v_from == south && v_to == east));
} 
static bool can_enter_single(vehicle * new_v, vehicle * exist_v){
	/* When vehicles are in the same road*/
	if(new_v->from == exist_v->from || 
		(new_v->from == exist_v->dest && new_v->dest == exist_v->from)){
			return true;
		}
	/*Different Destination and at least one is making right turns */
	else if ((new_v->dest != exist_v->dest) &&
		(test_rightturn(new_v->from, new_v->dest) ||
		test_rightturn(exist_v->from, exist_v->dest))){
		return true;
	} else {
		return false;
	}
}

static bool can_enter(vehicle * new_v){
	if(passing_v == 0){
		return true;
	} 
	for(unsigned int i=0; i < array_num(intersection_all); i++){
		if(!can_enter_single(new_v,array_get(intersection_all,i))){
			return false;
		}
	}
	return true;
}

/* 
 * The simulation driver will call this function once before starting
 * the simulation
 *
 * You can use it to initialize synchronization and other variables
 * 
 */
void
intersection_sync_init(void)
{
	/* replace this default implementation with your own implementation */

	/*intersectionSem = sem_create("intersectionSem",1);
	if (intersectionSem == NULL) {
		panic("could not create intersection semaphore");
	}
	return;*/
	intersectioncv = cv_create("My_CV");
	if (intersectioncv == NULL){
		panic("Could not create CV");
	}
	// cv will need a lock as parameter
	intersection_lock = lock_create("My_lock");
	if (intersection_lock == NULL) {
		panic("could not initialize lock for cv");
	}

	intersection_all = array_create();
	if (intersection_all == NULL) {
		panic("could not initialize array for vehicles");
	}

	end_cv = cv_create("End cv");
	array_init(intersection_all);

	return;
}

/* 
 * The simulation driver will call this function once after
 * the simulation has finished
 *
 * You can use it to clean up any synchronization and other variables.
 *
 */
void
intersection_sync_cleanup(void)
{
	/* replace this default implementation with your own implementation */
	KASSERT(intersectioncv != NULL);
	KASSERT(intersection_lock != NULL);
	KASSERT(intersection_all != NULL);
	lock_destroy(intersection_lock);
	cv_destroy(intersectioncv);
	cv_destroy(end_cv);
	array_cleanup(intersection_all);
	array_destroy(intersection_all);
	return;
}


/*
 * The simulation driver will call this function each time a vehicle
 * tries to enter the intersection, before it enters.
 * This function should cause the calling simulation thread 
 * to block until it is OK for the vehicle to enter the intersection.
 *
 * parameters:
 *    * origin: the Direction from which the vehicle is arriving
 *    * destination: the Direction in which the vehicle is trying to go
 *
 * return value: none
 */

void
intersection_before_entry(Direction origin, Direction destination) 
{
	/* replace this default implementation with your own implementation */
	KASSERT(intersection_lock != NULL);
	KASSERT(intersectioncv != NULL);
	KASSERT(intersection_all != NULL);

	/*Create a new vehicle */
	vehicle *new_v = kmalloc(sizeof(struct vehicle));
	new_v->from = origin;
	new_v->dest = destination;

	lock_acquire(intersection_lock);
	while (!can_enter(new_v)) {
    waiting_v++;
    cv_wait(intersectioncv, intersection_lock);
    waiting_v--;
  }

  array_add(intersection_all,new_v,NULL);
  passing_v++;
  lock_release(intersection_lock);

	/*Check whether a vehicle can enter, if not, then it waits */

	//P(intersectionSem);
}


/*
 * The simulation driver will call this function each time a vehicle
 * leaves the intersection.
 *
 * parameters:
 *    * origin: the Direction from which the vehicle arrived
 *    * destination: the Direction in which the vehicle is going
 *
 * return value: none
 */

void
intersection_after_exit(Direction origin, Direction destination) 
{
	/* replace this default implementation with your own implementation */
	KASSERT(intersection_lock != NULL);
	KASSERT(intersectioncv != NULL);
	KASSERT(intersection_all != NULL);

	lock_acquire(intersection_lock);
	/*Iterate to find such a car with origin and dest */
	for(unsigned int i=0; i < array_num(intersection_all); i++){
		vehicle * this_v = array_get(intersection_all,i);
		if(this_v->from == origin &&
			this_v->dest == destination){
			array_remove(intersection_all,i);
			passing_v--;
			cv_broadcast(intersectioncv, intersection_lock);
      break;
		}
	}
/*	if(waiting_v > 0){
		cv_broadcast(intersectioncv, intersection_lock);
	}*/
	if(waiting_v + passing_v > 0){
  	cv_wait(end_cv, intersection_lock);
	} 
		cv_broadcast(end_cv, intersection_lock);
	
	lock_release(intersection_lock);
}

