#include <iostream>
#include <stdio.h>
#include "logging.h"
#include "message.h"
#include "unistd.h"
#include <sys/socket.h>
#include <sys/wait.h>
#include <poll.h>
#include "constructors.h"
#include <cstring>
#include <vector>
#include <stdlib.h>
#include <string.h>


#define SERVER_SIDE 0
#define BIDDER_SIDE 1
#define PIPE(fd) socketpair(AF_UNIX,SOCK_STREAM,PF_UNIX,fd)


int cid=0;
int minDelay=0;
using namespace std;



struct serverInfo{

	int current_bid;
	int winner_id;
	int winning_bid;
	int done;
	int reggedUser;

} serverStatInfo;


int getClientID(int** pidCid, int pid){

	for(int i=0;i<serverStatInfo.reggedUser;i++){
		if(pidCid[i][0]==pid) return pidCid[i][1];
	}

	return 0;
}

bool checkConnect(struct pollfd* pfd, int numOfBids){
	for(int i=0;i<numOfBids;i++){
		if(pfd[i].fd>=0)	 return true;
	}
	return false;
}

bool checkStat(int** pidStat, int currentStat, int pid){
	for(int i=0;i<serverStatInfo.reggedUser;i++){
		if(pidStat[i][0]==pid && pidStat[i][1]==currentStat) return true;
		else if(pidStat[i][0]==pid && pidStat[i][1]!=currentStat) return false;
	}
	return false;
}

void buildArgs(char** args,int numOfParams,std::string& execPath){
	   	args[0]=new char[strlen(execPath.c_str())+1];
    	strcpy(args[0],execPath.c_str());
    	args[0][strlen(execPath.c_str())]='\0';
    	for(int j=1;j<numOfParams+1;j++){
    		std::string param;
    		cin>>param;
    		args[j]=new char[strlen(param.c_str())+1];
    		strcpy(args[j],param.c_str());
    		args[j][strlen(param.c_str())]='\0';
    	}
    	args[numOfParams+1]=NULL;
}

void server(pid_t* pids, int** fdTable, 
			struct pollfd* pfd, int startingBid, 
			int minInc, int numOfBids,int pid,
			int** pidCid,
			int** pidStat){
	cm message;
	while(checkConnect(pfd,numOfBids)){
		if(serverStatInfo.done==numOfBids){
			sm serverMsg;
	    	smp serverParams;
	    	print_server_finished(serverStatInfo.winner_id,serverStatInfo.winning_bid);
	    	wi winnerInfo;
	    	wi_cons(winnerInfo,serverStatInfo.winner_id,serverStatInfo.winning_bid);
	    	smp_wi_cons(serverParams,winnerInfo);
	    	sm_cons(serverMsg,SERVER_AUCTION_FINISHED,serverParams);
	    	for(int j=0;j<numOfBids;j++){
		    	oi response;
				oi_cons(response,SERVER_AUCTION_FINISHED,pids[j],serverParams);
				print_output(&response,getClientID(pidCid,pids[j]));
				write(pfd[j].fd,&serverMsg,sizeof(serverMsg));
			}
			return ;
	    }

		poll(pfd,numOfBids,minDelay);

		for(int i=0;i<numOfBids;i++){
			if (pfd[i].revents && POLLIN) {
				int r = read(pfd[i].fd, &message, sizeof(message));
				if (r == 0) continue;//EOF
				else{
					ii user;
    				cmp msgInfo;
    				cmp_cons(msgInfo,message.params.delay);
    				ii_cons(user,message.message_id,pids[i],msgInfo);
    				
    				if(message.message_id==CLIENT_CONNECT) print_input(&user,cid);
    				else print_input(&user,getClientID(pidCid,pids[i]));
    				
    				sm serverMsg;
	    			smp serverParams;
 
    				if(message.message_id==CLIENT_CONNECT){		
	   					cei connectionInfo;
	   					int* arr= new int[2];
	   					arr[0]=pids[i];
	   					arr[1]=cid;
	   					pidCid[cid]=arr;
					    cei_cons(connectionInfo,cid,startingBid,serverStatInfo.current_bid,minInc);
					    
					    if(cid==0){
					    	minDelay=message.params.delay;
					    }
					    else{
					    	if(message.params.delay<minDelay) minDelay=message.params.delay;
					    }


					    cid++;
					    serverStatInfo.reggedUser++;
					    smp_cei_cons(serverParams,connectionInfo);
					    sm_cons(serverMsg,SERVER_CONNECTION_ESTABLISHED,serverParams);
					    oi response;
					    oi_cons(response,SERVER_CONNECTION_ESTABLISHED,pids[i],serverParams);
					    print_output(&response,serverParams.start_info.client_id);
						write(pfd[i].fd,&serverMsg,sizeof(serverMsg));
					}

					else if(message.message_id==CLIENT_BID){
						int bid=message.params.bid;
						int answer;
						if(bid<startingBid) answer=BID_LOWER_THAN_STARTING_BID;
						else if(bid<serverStatInfo.current_bid) answer=BID_LOWER_THAN_CURRENT;
						else if(bid-serverStatInfo.current_bid < minInc) answer= BID_INCREMENT_LOWER_THAN_MINIMUM;
						else{
							answer= BID_ACCEPTED;
							serverStatInfo.current_bid=bid;
							serverStatInfo.winner_id=getClientID(pidCid,pids[i]);
							serverStatInfo.winning_bid=bid;
						}

						bi bidInfo;
						bi_cons(bidInfo,answer,serverStatInfo.current_bid);
						smp_bi_cons(serverParams,bidInfo);
						sm_cons(serverMsg,SERVER_BID_RESULT,serverParams);
						oi response;
						oi_cons(response,SERVER_BID_RESULT,pids[i],serverParams);
					    print_output(&response,getClientID(pidCid,pids[i]));
						write(pfd[i].fd,&serverMsg,sizeof(serverMsg));


					}

					else  {
				
						int* arr=new int[2];
						arr[0]=pids[i];
						arr[1]=message.params.delay;
						pidStat[serverStatInfo.done++]=arr;
						//CLIENT FINISHED
					}
				}
			}
		}
	}
}

int main(){
	
	int** fdTable;
	int** pidCid;
	int** pidStat;

    int startingBid,minInc,numOfBids;
    int c;	

    cin>>startingBid;
    cin>>minInc;
    cin>>numOfBids;
    serverStatInfo.current_bid=startingBid;
    serverStatInfo.done=0;
    serverStatInfo.reggedUser=0;
    fdTable= new int*[numOfBids];
    pidCid= new int*[numOfBids];
    pidStat= new int*[numOfBids];
    pid_t pids[numOfBids];
    pid_t pid;

    struct pollfd pfd[numOfBids];

    for(int i=0;i<numOfBids;i++){
    	fdTable[i]=new int[2];
    	PIPE(fdTable[i]);
    	pfd[i]={fdTable[i][0],POLLIN,0};

    }
    for(int i=0;i<numOfBids;i++){
    	std::string execName;
    	int numOfParams;
    	cin>>execName;
    	cin>>numOfParams;
    	char* args[numOfParams+2];
    	buildArgs(args,numOfParams,execName);

    	pid = fork();
    	
    	if (pid == 0) { 
    		dup2(fdTable[i][BIDDER_SIDE],1);
    		dup2(fdTable[i][BIDDER_SIDE],0);
    		close(fdTable[i][BIDDER_SIDE]);
        	execv(execName.c_str(),args);
    	} 
		else {
			pids[i]=pid;
		}
		for(int j=0;j<numOfParams+2;j++){
    		delete  [] args[j]; //free up arg vector
    	}
    }

    server(pids,fdTable,pfd,startingBid,minInc,numOfBids,pid,pidCid,pidStat);
    
    for(int i=0;i<numOfBids;i++) {
    	waitpid(pids[i],&c,0);
    	print_client_finished(getClientID(pidCid,pids[i]), c, checkStat(pidStat,c,pids[i]));
    }

    for(int i=0;i<numOfBids;i++) {
    	 close(fdTable[i][0]);
    	 close(fdTable[i][1]);
    	 delete [] fdTable[i];
    	 delete [] pidCid[i];
    	 delete [] pidStat[i];
    }
    delete [] fdTable;
    delete [] pidCid;
    delete [] pidStat;

    // close(0);
    // close(1);
    // close(2);
    return 0; 

}	