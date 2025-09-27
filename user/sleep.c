#include "user/user.h"
#include "kernel/types.h"
#include "kernel/stat.h"

int main(int argc, char *argv[]){
    if(argc != 2){
      fprintf(2, "sleep: wrong args\n");
    }else{
      int time = atoi(argv[0]);
      sleep(time);
    }
    fprintf(1, "(nothing happens for a little while)\n");
    exit(0);
}
