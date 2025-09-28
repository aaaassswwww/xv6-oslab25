#include "user/user.h"
#include "kernel/types.h"
#include "kernel/stat.h"

int main(){
  int c2f[2],f2c[2];
  int pid;
  int r_buffer[2], w_buffer[2];

  if(pipe(c2f) == -1 || pipe(f2c) == -1){
    fprintf(2, "pipe creation failed\n");
    exit(0);
  }

  pid = fork();

  if(pid == -1){
    fprintf(2, "fork error\n");
    exit(0);
  }
  
  if(pid == 0){
    while(read(f2c[0], r_buffer, 1) == 0){

    }
    fprintf(1, "%d: received ping from pid %d\n", getpid(), r_buffer[0]);
    close(f2c[0]);
    close(f2c[1]);
    w_buffer[0] = 1;
    write(c2f[1], w_buffer, 1);
    exit(0);
  }else{
    w_buffer[0] = getpid();
    write(f2c[1], w_buffer, 1);
    while(read(c2f[0], r_buffer, 1) == 0){

    }
    fprintf(1, "%d: received pong from pid %d\n", getpid(), pid);
    close(c2f[0]);
    close(c2f[1]);
    exit(0);
  }
}
