#include "monitor.h"
#define NANOSEC 1000000000L
#define MICRO 1000000
enum State{
	IDLE,
	UP,
	DOWN,
};
enum PersonState{
	NOT_REQESTED,
	REQUESTED,
	MISSED,
	IN,
	OUT,
};
enum Priority{
	HIGH,
	LOW,
};

typedef struct person{
	int weight;
	int initFloor;
	int destFloor;
	int id;
	Priority priority;
	State movesTo;
	PersonState state;


} Person;

typedef struct elevator_state{
	State moveState;
	int carriedWeight;
	int carriedPeople;
	int currentFloor;
	int totalCarried;

} ElevatorState;