#include "Elevator.hpp"


ElevatorControl::ElevatorControl(int nof, int nop, int wc, 
				int pc, int tt, int it, int iot)
				:timedWait(this), direction(this),//,wantsToLeave(this),wantsToEnter(this),
				up(this),down(this),missSleep(this){

	numOfFloors=nof;
	numOfPeople=nop;
	weightCapacity=wc;
	personCapacity=pc;
	
	wannaEnter=0;
	wannaLeave=0;

	inOutTime={0,0};
	idleTime={0,0};
	travelTime={0,0};

	createTimeStruct(travelTime,tt);
	createTimeStruct(idleTime,it);
	createTimeStruct(inOutTime,iot);

	normalizeTimeStruct(travelTime);
	normalizeTimeStruct(idleTime);
	normalizeTimeStruct(inOutTime);

	highPriorityQueue.resize(numOfFloors); //????
	lowPriorityQueue.resize(numOfFloors);
	exitArray.resize(numOfFloors);
	inArray.resize(numOfFloors);
	state.moveState=IDLE;
	state.carriedWeight=0;
	state.carriedPeople=0;
	state.currentFloor=0;
	state.totalCarried=0;
	for(int i=0;i<numOfFloors;i++){

		Condition wantsToEnterHigh(this);
		Condition wantsToEnterLow(this);
		Condition wantsToLeave(this);

		highWaitingPeople.push_back(wantsToEnterHigh); // take adv of copy constructor?
		lowWaitingPeople.push_back(wantsToEnterLow);
		exitingPeople.push_back(wantsToLeave);
		willExit.push_back(false);
		exitArray[i]=0;
		inArray[i]=0;

	}
	personId=0;
}

ElevatorControl::~ElevatorControl(){
	for(int i=0;i<numOfPeople;i++){
		delete people[i];
	}

}

void ElevatorControl::addPeopletoFloors(int personWeight, int initFloor, int destFloor, int pri){
	Person* added= new Person;
	added->weight=personWeight;
	added->initFloor= initFloor;
	added->destFloor= destFloor;
	added->priority= (pri==2) ? LOW: HIGH;
	added->id= personId++;
	added->movesTo = (initFloor<destFloor) ? UP : DOWN;
	added->state=NOT_REQESTED;
	people.push_back(added);
}

void ElevatorControl::normalizeTimeStruct(struct timespec& toWrite){
	while(toWrite.tv_nsec>=NANOSEC){
		toWrite.tv_sec++;
		toWrite.tv_nsec-=NANOSEC;
	
	}

	while(toWrite.tv_nsec<0){
		toWrite.tv_nsec+=NANOSEC;
		toWrite.tv_sec--;
	}
}

void ElevatorControl::createTimeStruct(struct timespec& toWrite, int microsecond){
	int second=microsecond/MICRO;
	int nano= (microsecond%MICRO)*1000;
	toWrite.tv_sec=second;
	toWrite.tv_nsec=nano;
}

void ElevatorControl::printQueue(){
	for(int i=0;i<destQueue.size();i++){
		cout<<destQueue[i]<<" ";
	}
	cout<<endl;

}

int ElevatorControl::getIndex(vector<int>& looked,int key){
	for(int i=0;i<looked.size();i++){
		if(looked[i]==key) return i;
	}
	return 0;
}

bool ElevatorControl::isIn(vector<int>& looked,int key){
	for(int i=0;i<looked.size();i++){
		if(looked[i]==key) return true;
	}
	return false;
}	


void ElevatorControl::printElevatorStatus(){
	cout<<"Elevator (";
	switch(state.moveState){
		case IDLE:
			cout<<"Idle";
			break;
		case UP:
			cout<<"Moving-up";
			break;
		case DOWN:
			cout<<"Moving-down";
			break;
	}
	cout<<", "<<state.carriedWeight<<", "<<state.carriedPeople<<", "<<state.currentFloor<<" ->";
	if(!destQueue.empty()) {
		cout<<" ";
	}
	for(int i=0;i<destQueue.size();i++) {
		cout<<destQueue[i];
		if(i!=destQueue.size()-1) cout<<",";
	}
	cout<<")"<<endl;	

}


void ElevatorControl::tryRequest(int personId){

	Person* tmp =people[personId];
	
	if(state.moveState==IDLE){
		makeRequest(personId);
			
	}
	else if(state.moveState==tmp->movesTo){
		
		if(state.moveState==UP && tmp->initFloor >= state.currentFloor){
			makeRequest(personId);
		}

		else if(state.moveState==DOWN && tmp->initFloor <=  state.currentFloor){
			makeRequest(personId);

		}	
		else if(tmp->movesTo==DOWN) {
			down.wait();
		}
		else if (tmp->movesTo==UP) {
			up.wait();		
		}
	}

	else if(tmp->movesTo==DOWN) {
		down.wait();
	}
		
	else if (tmp->movesTo==UP) {
		up.wait();
	}	
}

void ElevatorControl::makeRequest(int personId){
	Person* tmp=people[personId];
	cout<<"Person ("<<tmp->id;
	
	if(tmp->priority==HIGH){
		cout<<", hp, "<<tmp->initFloor<<" -> "<<tmp->destFloor<<", "<<tmp->weight<<") made a request"<<endl;
	} else{
		cout<<", lp, "<<tmp->initFloor<<" -> "<<tmp->destFloor<<", "<<tmp->weight<<") made a request"<<endl;
	}

	tmp->state=REQUESTED;
	wannaEnter++;
	inArray[tmp->initFloor]++;

	if(state.currentFloor==tmp->initFloor){

		if(state.moveState==IDLE){
			state.moveState=tmp->movesTo;
		}
		printElevatorStatus();
		if(tmp->priority==HIGH){
			if(!willExit[state.currentFloor]){
				getIn(personId);
			}else{
				highPriorityQueue[state.currentFloor].push_back(personId);
				highWaitingPeople[state.currentFloor].wait();
			}
			
		}else{
			if(!willExit[state.currentFloor] && highPriorityQueue[state.currentFloor].empty()){
				getIn(personId);
			}else{
				lowPriorityQueue[state.currentFloor].push_back(personId);
				lowWaitingPeople[state.currentFloor].wait();
			}
		}
		
	}else{

		if(state.moveState==IDLE){
			if(tmp->initFloor > state.currentFloor){
				state.moveState=UP;
			}else{
				state.moveState=DOWN;
			}
		}

		if(state.moveState==UP){
			if(!isIn(destQueue,tmp->initFloor)){
				destQueue.push_back(tmp->initFloor);
				std::sort(destQueue.begin(),destQueue.end());
			}
			printElevatorStatus();

		}else{
			if(!isIn(destQueue,tmp->initFloor)){
				destQueue.push_back(tmp->initFloor);
				std::sort(destQueue.begin(), destQueue.end(), std::greater<int>());
			}
			printElevatorStatus();

		}


		if(tmp->priority==HIGH){
			highPriorityQueue[tmp->initFloor].push_back(personId);
			highWaitingPeople[tmp->initFloor].wait();
		}else{
			lowPriorityQueue[tmp->initFloor].push_back(personId);
			lowWaitingPeople[tmp->initFloor].wait();
		}

	}

}


void ElevatorControl::getOut(int personId){
	Person* tmp= people[personId];
	if(tmp->destFloor != state.currentFloor) return;
	state.carriedPeople--;
	state.carriedWeight-=tmp->weight;
	wannaLeave--;
	state.totalCarried++;
	exitArray[state.currentFloor]--;
	cout<<"Person ("<<tmp->id<<", ";
		if(tmp->priority==HIGH){
			cout<<"hp, ";

		}
		else{
			cout<<"lp, ";
		}
	cout<<tmp->initFloor<<" -> "<<tmp->destFloor<<", "<<tmp->weight<<") has left the elevator"<<endl;
	tmp->state=OUT;
	missSleep.notifyAll();
	if(!exitArray[state.currentFloor]) {
		willExit[tmp->destFloor]=false;
		highWaitingPeople[state.currentFloor].notifyAll();
	}
	
	if(state.totalCarried==numOfPeople) {
		state.moveState=IDLE;
		printElevatorStatus();

	}else{
		printElevatorStatus();
	}
}


void ElevatorControl::getIn(int personId){

	Person* tmp= people[personId];
	bool directionMiss=false;

	if(tmp->priority==HIGH && isIn(highPriorityQueue[tmp->initFloor],personId)){
		int index=getIndex(highPriorityQueue[tmp->initFloor],personId);
		highPriorityQueue[tmp->initFloor].erase(highPriorityQueue[tmp->initFloor].begin()+index);			
	}

	else if(tmp->priority==LOW && isIn(lowPriorityQueue[tmp->initFloor],personId)){
		int index=getIndex(lowPriorityQueue[tmp->initFloor],personId);
		lowPriorityQueue[tmp->initFloor].erase(lowPriorityQueue[tmp->initFloor].begin()+index);
	}

	if(highPriorityQueue[tmp->initFloor].size()==0){
		lowWaitingPeople[tmp->initFloor].notifyAll();
	} 

	wannaEnter--;
	inArray[tmp->initFloor]--;

	if(state.moveState!=tmp->movesTo){
		tmp->state=MISSED;
		directionMiss=true;

	}
	
	else if(state.carriedPeople<personCapacity && weightCapacity - state.carriedWeight >= tmp->weight){
	
		wannaLeave++;
		exitArray[tmp->destFloor]++;
		state.carriedPeople++;
		state.carriedWeight+=tmp->weight;
		willExit[tmp->destFloor]=true;
		cout<<"Person ("<<tmp->id<<", ";
		if(tmp->priority==HIGH){
			cout<<"hp, ";

		}else{
			cout<<"lp, ";
		}
		cout<<tmp->initFloor<<" -> "<<tmp->destFloor<<", "<<tmp->weight<<") entered the elevator"<<endl;
		
		if(!isIn(destQueue,tmp->destFloor)){
			destQueue.push_back(tmp->destFloor);
		}

		if(state.moveState==IDLE){
			state.moveState=tmp->movesTo;
		}
		if(state.moveState==DOWN){
			std::sort(destQueue.begin(), destQueue.end(), std::greater<int>());
		}
		else if(state.moveState==UP) {
			std::sort(destQueue.begin(), destQueue.end());
		}
		printElevatorStatus();
		tmp->state=IN;
		exitingPeople[tmp->destFloor].wait(); //when it arrives to the destination, wait for the people
	}

	else{

		if(directionMiss){
			direction.wait();
		}else{
			tmp->state=MISSED;
			missSleep.wait();
		}	
	}
}

void ElevatorControl::personMove(int personId){
	__synchronized__;
	Person* tmp = people[personId];
	while(tmp->state!=OUT){
		retry:
			while(tmp->state==NOT_REQESTED || tmp->state==MISSED)
				tryRequest(personId);
		
		if(tmp->state==REQUESTED && tmp->state!=IN) {
			getIn(personId);
			if(people[personId]->state==MISSED) goto retry;
		}
		if(tmp->state==IN) getOut(personId);
	}
}


void ElevatorControl::waitFor(const struct timespec* addedTime){

	struct timespec waitTime={0,0};
    int currentTime;
	currentTime=clock_gettime(CLOCK_REALTIME,&waitTime);
	waitTime.tv_sec+=addedTime->tv_sec;
	waitTime.tv_nsec+=addedTime->tv_nsec;

	while(waitTime.tv_nsec>=NANOSEC){
		waitTime.tv_sec++;
		waitTime.tv_nsec-=NANOSEC;
	}
    
    while(waitTime.tv_nsec<0){
		waitTime.tv_nsec+=NANOSEC;
		waitTime.tv_sec--;
	}

	
	timedWait.wait_time(&waitTime);

}

void ElevatorControl::loadUnload(){
	if(!willExit[state.currentFloor]){
		if(!highPriorityQueue[state.currentFloor].empty()){
			highWaitingPeople[state.currentFloor].notifyAll();
		}else{ 
			lowWaitingPeople[state.currentFloor].notifyAll();
		}
	
	}else {
		exitingPeople[state.currentFloor].notifyAll();
	}
    
	waitFor(&inOutTime);	
}


void ElevatorControl::moveElevator(){
	__synchronized__;
	while(state.totalCarried < numOfPeople){
		if(state.moveState==IDLE){

			if(state.totalCarried==numOfPeople) break;

			direction.notifyAll();
			up.notifyAll();
			down.notifyAll();
			while(destQueue.empty()){
				if(state.totalCarried==numOfPeople) break;
				else if(exitArray[state.currentFloor] || inArray[state.currentFloor]) {
					loadUnload();	
				}

				waitFor(&idleTime);
			}
		}

		if(state.moveState==UP){

				up.notifyAll();
				waitFor(&travelTime);
					
				if(state.currentFloor< numOfFloors && state.currentFloor<*max_element(destQueue.begin(),destQueue.end())){
					state.currentFloor++;
				}	

				if(destQueue.size() && state.currentFloor==destQueue[0]){
					destQueue.erase(destQueue.begin());	
					if(destQueue.empty()) {
						state.moveState=IDLE;
						printElevatorStatus();
					}
					else{
						printElevatorStatus();
					}
					loadUnload();
				}else{
					printElevatorStatus();
				}
				if(state.totalCarried==numOfPeople) break;
		}

		if(state.moveState==DOWN){
			
			down.notifyAll();
			
			waitFor(&travelTime);

			if(state.currentFloor>0 && state.currentFloor>*min_element(destQueue.begin(),destQueue.end())){
				state.currentFloor--;
			}


			if(destQueue.size() && state.currentFloor==destQueue[0]){
				destQueue.erase(destQueue.begin());	
				if(destQueue.empty()) {
					state.moveState=IDLE;
					printElevatorStatus();
				}
				else{
					printElevatorStatus();
				}
				loadUnload();
			}
			else{
				printElevatorStatus();
			}
			
			
			if(state.totalCarried==numOfPeople) break;
		}			
	}
}