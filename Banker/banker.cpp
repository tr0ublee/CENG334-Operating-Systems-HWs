#include <iostream>
#include <vector>
#include <fstream>
using namespace std;

typedef struct resource_struct  {
	int available;
	int fakeAvail;
	string resName;
} resource; 

typedef struct process_struct{
	
	vector<pair<string, int>> max;
	vector<pair<string, int>> allocation; 
	vector<pair<string, int>> need;
	string procName;
	bool finished;

} process;


void printRes(vector<resource*>& resVec){
	for(int i=0;i<resVec.size();i++){
		cout<<"Name: "<<resVec[i]->resName<<" Avail: "<<resVec[i]->available<<endl;
	}

}

void printProc(vector<process*>& procVec){
	for(int i=0;i<procVec.size();i++){
		cout<<"Name: "<<procVec[i]->procName<<endl;
		for(int j=0;j<procVec[i]->max.size();j++){
			cout<<"Resource: "<<procVec[i]->max[j].first;
			cout<<" MAX "<< procVec[i]->max[j].second;
			cout<<" Alloc "<< procVec[i]->allocation[j].second;
			cout<<" Need "<< procVec[i]->need[j].second;
			cout<<endl;

		}
	}

	cout<< endl;
}

inline int getResourceIndexByName(vector<resource*>& allResources, const string& name){

	int vecSize= allResources.size();
	for(int i=0;i<vecSize;i++){
		if(allResources[i]->resName==name) {
			return i;
		}
	}

	return -1;
}

inline int getProcessIndexByName(vector<process*>& allProcesses, const string& name){

	int vecSize= allProcesses.size();
	for(int i=0;i<vecSize;i++){
		if(allProcesses[i]->procName==name) {
			
			return i;
		}
	}

	return -1;
}

inline int getPairIndexWithName(vector<pair<string, int>> vec, const string& name){

	size_t vecSize= vec.size();
	for(size_t i=0;i<vecSize;i++){
		if(vec[i].first==name) {
			return i;
		}
	}
	return -1;
}

inline void initResource(resource* created, int available, string resName){

	created-> available = available;
	created-> resName = resName;
	created-> fakeAvail = available;
}

inline void addToProcess(process* created, string resName, int resMax, int resAlloc) {

	pair<string, int> maxPushed(resName,resMax);
	created->max.push_back(maxPushed);
	pair<string, int> allocPushed(resName, resAlloc);
	created->allocation.push_back(allocPushed);
	pair<string, int> needPushed(resName, resMax - resAlloc);
	created->need.push_back(needPushed);

}

bool safeState(vector<resource*>& resVec, vector<process*>& procVec, vector<pair<string, int>>& reqVec){
	vector<bool> finished(procVec.size(),false);
	vector<int> work(resVec.size());
	vector <string> safeSeq(procVec.size());
	for(int i=0;i<resVec.size();i++){
		work[i]=resVec[i]->fakeAvail;
	}

	for(int i=0;i<finished.size();i++){
		process* tmp = procVec[i];

		if(!finished[i]){
			bool found=true;
			for(int j=0;j<tmp->need.size();j++){
				if(tmp->need[j].second>work[j]){
					found=false;
					break;
				}
			}

			if(found){

				finished[i]=true;
				safeSeq.push_back(tmp->procName);
				for(int j=0; j < work.size();j++){
					work[j] += tmp->allocation[j].second;
				
				}
				i=-1;

			}


		}

		

		


	}
	bool retVal=true;
	for(int i=0;i<finished.size();i++){
		if(!finished[i]){
			retVal=false;
			return retVal;
		}
	}

	cout<<"Safe Sequence : <";
	for(string i: safeSeq){
		cout<<i<<" ";

	}
	cout<<">"<<endl;
	return retVal;

}



/*
===== INPUT STYLE =====

read number of process from FILE
read number of resources from FILE
for each resource, read its name and available amount, format = Resname amount from FILE
for each process, read its name, max needed and allocation, format = Pname res1 res1max res1allocated res2  res2max res2alloc from FILE
for each request, read it in the form ProcName Resname1 Amount1 Resname2 amount2 from CLI
for each kill, read it as kill procName from CLI

*/


void banker(vector<resource*>& resVec, vector<process*>& procVec,string procName, vector<pair<string, int>>& reqVec ){
	process* currentProc = procVec[getProcessIndexByName(procVec, procName)];
	for(int i=0 ; i<reqVec.size();i++){
		if(reqVec[i].second > currentProc-> need[i].second){
			cout<<"Request of "<<procName << " on resource "<<reqVec[i].first<< " with amount "<< reqVec[i].second << "rejected due to capacity"<<endl;
			return;
		}

		int resIndex= getResourceIndexByName(resVec, reqVec[i].first);
		resource* curRes = resVec[i];
		if(reqVec[i].second > curRes->available){
			cout << "Not enough resource. Returning"<<endl;
			return;
		}

	}

	vector<pair<string, int>> backupNeed(currentProc->allocation.size());
	vector<pair<string, int>> backupAlloc(currentProc->allocation.size());
	for(int i=0;i<backupNeed.size();i++){
		backupAlloc[i]=currentProc->allocation[i];
		backupNeed[i]=currentProc->need[i];

	}

	for(int i=0; i< reqVec.size() ; i++){
		int resIndex= getResourceIndexByName(resVec, reqVec[i].first);
		resource* curRes = resVec[i];
		curRes->fakeAvail = curRes -> available - reqVec[i].second;
		currentProc->allocation[i]=make_pair(reqVec[i].first, currentProc->allocation[i].second+ reqVec[i].second);
		currentProc->need[i]=make_pair(reqVec[i].first, currentProc->need[i].second-reqVec[i].second);
		
	}

	bool isSafe= safeState(resVec,procVec, reqVec);
	if(isSafe){
		for(int i=0; i< reqVec.size() ; i++){
			int resIndex= getResourceIndexByName(resVec, reqVec[i].first);
			resource* curRes = resVec[i];
			curRes->available=curRes->fakeAvail;
		
		}

		cout<<endl;
		cout << "+++++++++++ Granted +++++++++++++++"<<endl;
		cout<<endl;
		printProc(procVec);
		printRes(resVec);
		cout<<"++++++++++++++++++++++++++"<<endl;

	}


	else{
		cout<<endl;
		cout<<"XXXXXXXXXXXX Rejected XXXXXXXXXX"<< endl;
		cout<<endl;
		cout<<"No safe state with "<<endl;
		for(int i=0;i<resVec.size();i++){
			cout<<resVec[i]->resName<<" : "<< resVec[i] -> fakeAvail<<endl;
		}
		for(int i=0;i<backupNeed.size();i++){
			currentProc->allocation[i]=backupAlloc[i];
			currentProc->need[i]=backupNeed[i];
		

		}
		printProc(procVec);
		printRes(resVec);

		cout<<"XXXXXXXXXXXXXXXXXXXXXXXXXXXX"<<endl;
		return;
	}

}



int main(){
	
	int numOfProcess;
	int numOfResources;
	int numOfRequests;
	ifstream inFile;
	inFile.open("./inp");
	if(!inFile){
		cerr<<"Read error"<<endl;
		return 0;
	}
	vector<resource*> allResources;
	vector<process*> allProcesses;
	inFile>>numOfProcess>>numOfResources;
	
	allResources.resize(numOfResources);
	allProcesses.resize(numOfProcess);

	for(int i=0;i<numOfResources;i++){
		string readResName;
		int readResAmount;
		inFile>>readResName>>readResAmount;
		resource* created = new resource();
		initResource(created,readResAmount,readResName);
		allResources[i]=created;
	}

	for(int i=0;i<numOfProcess;i++){
		string readProcName;
		process* createdProc = new process();
		inFile>> readProcName;
		createdProc->procName = readProcName;
		createdProc->finished = false;
		for(int j=0;j < numOfResources; j++){
			string resName;
			int resMax;
			int resAlloc;
			inFile >> resName >> resMax >> resAlloc;
			addToProcess(createdProc, resName, resMax, resAlloc);
		}

		allProcesses[i]=createdProc;

	}


		//cin>>numOfRequests;
	printProc(allProcesses);
	printRes(allResources);
	const string kill("x");
	while(true){

		string procName;
		vector<pair<string, int>> reqVec;
		cin >> procName;
		if(procName==kill){
			string realName;
			cin >> realName;
			int index =getProcessIndexByName(allProcesses, realName);
			for(int j=0;j<allResources.size();j++){
				allResources[j]->available+=allProcesses[index]->allocation[j].second;

			}
			cout<<"============KILL========"<<endl;
			cout<<realName<<" killed"<<endl;
			printProc(allProcesses);
			printRes(allResources);
			cout<<"========================"<<endl;

			delete allProcesses[index];
			allProcesses.erase(allProcesses.begin()+index);
			if(allProcesses.empty()){
				for(int j=0;j<allResources.size();j++){
					delete allResources[j];
				}
				inFile.close();
				return 0;
			}
			continue;

		}

		for(int j=0; j< numOfResources; j++){

			string resReqName;
			int reqAmount;
			cin >> resReqName >> reqAmount;
			pair<string,int> pushed(resReqName, reqAmount);
			reqVec.push_back(pushed);

		}
		banker(allResources, allProcesses, procName, reqVec );

	}



	inFile.close();
	return 0;
}