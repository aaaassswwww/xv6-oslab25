#include "user/user.h"
#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "kernel/fs.h"

void add_path(const char *path, const char *name, char *buf){
  char *p;
  strcpy(buf, path);
  p = buf + strlen(buf);
  *p++ = '/';
  memmove(p, name, DIRSIZ);
  p[DIRSIZ] = 0;
  
}

void traverse_directory(const char *path, const char *name, int dir_fd){
  char buf[512], n[512], *p;
  int fd;
  struct dirent de;
  struct stat st;

  while(read(dir_fd, &de, sizeof(de)) == sizeof(de)){
    if(de.inum == 0) return;
    if((strcmp(de.name, ".") == 0) || (strcmp(de.name, "..") == 0)){
      // printf("%s\n", de.name);
      continue;
    } 
    
    add_path(path, de.name, n);

    if((fd = open(n, O_RDONLY)) < 0){
      fprintf(2, "t find: cannot open %s\n", n);
      close(fd);
      return;
    }
    // printf("%s\n", path);

    if((fstat(fd, &st)) < 0){
      fprintf(2, "find: cannot stat %s\n", path);
      close(fd);
      return;
    }

    switch (st.type) {
      case T_DIR:
        if(strcmp(de.name, name) == 0){
          printf("%s/%s\n", path, name);
        }
        add_path(path, de.name, buf);
        p = buf;
        traverse_directory(p, name, fd);
        break;
      case T_FILE:
      case T_DEVICE:
        if(strcmp(de.name, name) == 0){
          printf("%s/%s\n", path, name);
        }
        break;
      default:
        fprintf(2, "find: stat error %s\n", de.name);
    }
    close(fd);
  }
}

void find(const char *path, const char *name){
  int fd;
  struct stat st;

  if((fd = open(path, O_RDONLY)) < 0){
    fprintf(2, "find: cannot open %s\n", path);
    close(fd);
    return;
  }

  if((fstat(fd, &st)) < 0){
    fprintf(2, "find: cannot stat %s\n", path);
    close(fd);
    return;
  }

  switch(st.type) {
    case T_FILE:
      fprintf(2, "find: wrong path %s %d\n", path, st.type);
      break;
    case T_DIR:
      traverse_directory(path, name, fd);
      close(fd);
      break;
    default:
      fprintf(2, "find: stat error\n");
      
  }
  return;
  
}

int main(int argc, char *argv[]){
  if(argc != 3){
    fprintf(2, "find: use find <path> <name>\n");
    exit(0);
  }
  find(argv[1], argv[2]);
  exit(0);
}
