#include "Elevator.hpp"

ElevatorControl* control;

void* elevatorThread(void* param){
	control->moveElevator();
	return 0;
}

void* personControl (void* param){
	int personId=*(int*) param;
	control->personMove(personId);
	return 0;
}

int main(int arg, char* argv[]){
	pthread_t lift;
	pthread_t* personThreads;
	int floor,ppl,wc,pc,tt,it,iot;
	ifstream inputFile;
	inputFile.open(argv[1]);
	inputFile>>floor>>ppl>>wc>>pc>>tt>>it>>iot;
	personThreads= new pthread_t[ppl];
	//ElevatorControl system(floor,ppl,wc,pc,tt,it,iot);
	control= new ElevatorControl(floor,ppl,wc,pc,tt,it,iot);
	pthread_create(&lift, nullptr, elevatorThread, nullptr);
	int* tids=new int[ppl];
	for(int i=0;i<ppl;i++){
		tids[i]=i;
		int w,ifl,dfl,p;
		inputFile>>w>>ifl>>dfl>>p;
		control->addPeopletoFloors(w,ifl,dfl,p);
		pthread_create(personThreads+i, nullptr, personControl, &tids[i]);
	
	}
	pthread_join(lift,NULL);
	for(int i=0;i<ppl;i++){

		pthread_join(personThreads[i], NULL);
	} 

	delete [] tids;
	delete [] personThreads;
	delete control;

	return 0;
}