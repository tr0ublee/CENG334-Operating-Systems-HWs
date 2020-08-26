#include "constructors.h"

void cmp_cons(cmp& write, int val){
	write.status=val;
}

void cm_cons(cm& write, cmp& params, int message_id){
	write.params=params;
	write.message_id=message_id;

}

void cei_cons(cei& write, int client_id, int starting_bid, int current_bid, int minimum_increment){
	write.client_id=client_id;
	write.starting_bid=starting_bid;
	write.current_bid=current_bid;
	write.minimum_increment=minimum_increment;

}

void bi_cons(bi& write,int result, int current_bid){
	write.result=result;
	write.current_bid=current_bid;
}

void wi_cons(wi& write,int winner_id, int winning_bid){
	write.winner_id=winner_id;
	write.winning_bid=winning_bid;
}

void smp_cei_cons(smp& write, cei& start_info){
	write.start_info=start_info;
}

void smp_bi_cons(smp& write, bi& result_info){
	write.result_info=result_info;
}

void smp_wi_cons(smp& write, wi& winner_info){
	write.winner_info=winner_info;
}

void sm_cons(sm& write, int message_id, smp& params){
	write.message_id=message_id;
	write.params=params;

}

void oi_cons(oi& write, int type, int pid, smp& info){
	write.type=type;
	write.pid=pid;
	write.info=info;
}


void ii_cons(ii&write, int type, int pid,cmp& info){
	write.type=type;
	write.pid=pid;
	write.info=info;

}