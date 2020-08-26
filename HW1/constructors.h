#include "logging.h"
#include "message.h"


void cmp_cons(cmp& write,  int val);

void cm_cons(cm& write, cmp& params, int message_id);

void cei_cons(cei& write, int client_id, int starting_bid, int current_bid, int minimum_increment);

void bi_cons(bi& write,int result, int current_bid);

void wi_cons(wi& write,int winner_id, int winning_bid);

void smp_cei_cons(smp& write, cei& start_info);

void smp_bi_cons(smp& write, bi& result_info);

void smp_wi_cons(smp& write, wi& winner_info);

void sm_cons(sm& write, int message_id, smp& params);

void oi_cons(oi& write, int type, int pid, smp& info);


void ii_cons(ii&write, int type, int pid,cmp& info);