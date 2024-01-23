#include "structures_bash.h"
#include "reader.h"
#include "executor.h"
#include "job_controller.h"
#include "signals_sender.h"
#include <signal.h>

int main(){
	signal(SIGINT, SIG_IGN);
	while(1){
		read_string();
		execute();
	}
	return 0;
}