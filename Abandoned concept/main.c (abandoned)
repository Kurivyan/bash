#include "src/structures_bash.h"
#include "src/reader.h"
#include "src/executor.h"
#include "src/jobs_control.h"
#include "src/groups.h"
#include <signal.h>

int main(){
	signal(SIGINT, SIG_IGN);
	while(1){
		read_string();
		execute();
	}
	return 0;
}