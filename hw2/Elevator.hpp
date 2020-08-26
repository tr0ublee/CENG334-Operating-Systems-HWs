#include "DataTypes.hpp"
#include <iostream>
#include <vector>
#include <algorithm>
#include <unistd.h>
#include <fstream>

using namespace std;
class ElevatorControl: public Monitor{

private:
	
	Condition timedWait;//,wantsToEnter,wantsToLeave;	
	Condition up,down, missSleep, direction;	
	
	ElevatorState state;						//	state of elevator, carried weight, number of people inside, current floor -> destination
	
	std::vector<int> destQueue;
	std::vector<int> exitArray;
	std::vector<int> inArray;
	std::vector<Condition> highWaitingPeople;
	std::vector<Condition> lowWaitingPeople;
	std::vector<Condition> exitingPeople;
	std::vector <bool> willExit;
	std::vector<Person*> people; // a std::vector to hold all people
	
	std::vector<std::vector<int>> highPriorityQueue; // a std::vector of std::vectors for each floor, representing high priority people that have made request
	std::vector<std::vector<int>> lowPriorityQueue;  // a std::vector of std::vectors for each floor, representing low  priority people that have made request
	
	int personId,numOfFloors,numOfPeople,weightCapacity,personCapacity,wannaEnter,wannaLeave;

	struct timespec inOutTime;
	struct timespec idleTime;
	struct timespec travelTime;
public:

	ElevatorControl(int nof, int nop, int wc, 
					int pc, int tt, int it, int iot);

	~ElevatorControl();
	void moveElevator();
	void personMove(int personId);
	void addPeopletoFloors(int personWeight, int initFloor, int destFloor, int pri);

private:

	void normalizeTimeStruct(struct timespec& toWrite);
	void createTimeStruct(struct timespec& toWrite, int microsecond);
	void printQueue();
	int getIndex(std::vector<int>& looked,int key);
	bool isIn(std::vector<int>& looked,int key);
	void printElevatorStatus();
	void makeRequest(int personId);
	void tryRequest(int personId);
	void getOut(int personId);
	void getIn(int personId);
	void waitFor(const struct timespec* addedTime);
	void loadUnload();


};