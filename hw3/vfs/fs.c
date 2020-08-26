#include "fs.h"
#include "ext2_fs/ext2.h"

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int init_fs(const char *image_path) {
  current_fs = initialize_ext2(image_path);
  current_sb = current_fs->get_superblock(current_fs);
  return !(current_fs && current_sb);
}

struct file *openfile(char *path) {
  struct file *f = malloc(sizeof(struct file));
  f->f_path = malloc(strlen(path) + 1);
  strcpy(f->f_path, path);
  struct dentry *dir = pathwalk(path);
  if (!dir) {
    return NULL;
  }
  f->f_inode = dir->d_inode;
  free(dir);
  if (f->f_inode->f_op->open(f->f_inode, f)) {
    return NULL;
  }
  return f;
}

int closefile(struct file *f) {
  if (f->f_op->release(f->f_inode, f)) {
    printf("Error closing file\n");
  }
  free(f);
  f = NULL;
  return 0;
}

int readfile(struct file *f, char *buf, int size, loffset_t *offset) {
  if (*offset >= f->f_inode->i_size) {
    return 0;
  }
  if (*offset + size >= f->f_inode->i_size) {
    size = f->f_inode->i_size - *offset;
  }
  // May add llseek call
  return f->f_op->read(f, buf, size, offset);
}

char* parseString(char* string){

	int size = strlen(string);

	if(!size){
		return NULL;
	}else if(size == 1){
		
		char* tmp = malloc(sizeof(char)*(size+1));
		tmp[size-1] = string[0];
		tmp[size] = '\0';
		return tmp;

	}else{
		string++;
		char found = 0;
		int index = 0;
		while(!found){
			if(string[index]=='/' || string[index]=='\0'){
				found = 1;
			}
			else{
				index++;
			}

		}

		char* tmp = malloc(sizeof(char)*(index+1));
		tmp[index] = '\0';
		for(int i=0;i<index;i++){
			tmp[i]=string[i];
		}
		
		return tmp;
	}

}
struct dentry *pathwalk(char *path) {
  /* Allocates and returns a new dentry for a given path */
  
  char* pathCpy = path;
	char* retVal;
  struct dentry* dentRetVal;
  struct dentry* current = current_sb->s_root;
  
  if(!path){
    return NULL;
  }

  if(!strcmp(".",path) || !strcmp("/",path)){
    
    
    struct dentry* test = malloc(sizeof(struct dentry));
    test-> d_name = malloc(sizeof(char)*(strlen("/")+1));
    strcpy(test->d_name, "/");
    test ->d_name[strlen("/")] = '\0';
    test ->d_parent = test;
    test -> d_sb = current_sb;
    test -> d_fsdata = 0;
    test -> d_flags = current->d_flags;
    test -> d_inode = malloc(sizeof(struct inode));
    test -> d_inode -> i_ino = current -> d_inode->i_ino;           
    test -> d_inode -> i_mode = current -> d_inode->i_mode;                
    test -> d_inode -> i_nlink = current -> d_inode->i_nlink;            
    test -> d_inode ->i_uid = current -> d_inode->i_uid;                    
    test -> d_inode -> i_gid = current -> d_inode->i_gid;                    
    test -> d_inode -> i_size = current -> d_inode->i_size;               
    test -> d_inode -> i_atime = current -> d_inode->i_atime;           
    test -> d_inode -> i_mtime = current -> d_inode->i_mtime;           
    test -> d_inode -> i_ctime = current -> d_inode->i_ctime;            
    test -> d_inode -> i_blocks = current -> d_inode->i_blocks; 
    for(int i=0;i<15;i++){
      
      test -> d_inode -> i_block[i] = current -> d_inode->i_block[i];  

    }        
    test -> d_inode ->i_op = current->d_inode->i_op;
    test -> d_inode ->f_op = current-> d_inode->f_op;  
    test -> d_inode ->i_sb = current_sb;
    test -> d_inode ->i_state = current -> d_inode->i_state;         
    test -> d_inode -> i_flags = current -> d_inode->i_flags;  
    return test;
  }

  struct dentry* root = malloc(sizeof(struct dentry));
  root-> d_name = malloc(sizeof(char)*(strlen("/")+1));
  strcpy(root->d_name, "/");
  root ->d_name[strlen("/")] = '\0';
  root ->d_parent = root;
  root -> d_sb = current_sb;
  root -> d_fsdata = 0;
  root -> d_flags = current_sb->s_root->d_flags;
  root -> d_inode = malloc(sizeof(struct inode));
  root -> d_inode -> i_ino = current -> d_inode->i_ino;           
  root -> d_inode -> i_mode = current -> d_inode->i_mode;                
  root -> d_inode -> i_nlink = current -> d_inode->i_nlink;            
  root -> d_inode ->i_uid = current -> d_inode->i_uid;                    
  root -> d_inode -> i_gid = current -> d_inode->i_gid;                    
  root -> d_inode -> i_size = current -> d_inode->i_size;               
  root -> d_inode -> i_atime = current -> d_inode->i_atime;           
  root -> d_inode -> i_mtime = current -> d_inode->i_mtime;           
  root -> d_inode -> i_ctime = current -> d_inode->i_ctime;            
  root -> d_inode -> i_blocks = current -> d_inode->i_blocks; 
  for(int i=0;i<15;i++){
    
    root -> d_inode -> i_block[i] = current -> d_inode->i_block[i];  

  }        
  root -> d_inode ->i_op = current->d_inode->i_op;   
  root -> d_inode ->f_op = current->d_inode->f_op;
  root -> d_inode ->i_sb = current_sb;      
  root -> d_inode ->i_state = current -> d_inode->i_state;         
  root -> d_inode -> i_flags = current -> d_inode->i_flags;  


  current = root;
  while((retVal = parseString(pathCpy))){

    struct dentry* test = malloc(sizeof(struct dentry));
    test-> d_name = malloc(sizeof(char)*(strlen(retVal)+1));
    strcpy(test->d_name, retVal);
    test ->d_name[strlen(retVal)] = '\0';
    test -> d_sb = current_sb;
    test -> d_fsdata = 0;
    dentRetVal = lookup(current->d_inode, test); //????
    if(!dentRetVal){
     free(test);
      return NULL;
    }
    
    test->d_parent = current;
    current = test;
		pathCpy += strlen(retVal)+1;
    if(!strcmp("\0",pathCpy)){
      return test;
    }
		free(retVal);
		
	}
  return NULL;
}

int statfile(struct dentry *d_entry, struct kstat *k_stat) {
  return d_entry->d_inode->i_op->getattr(d_entry, k_stat);
}
