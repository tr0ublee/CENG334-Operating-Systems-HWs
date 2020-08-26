#include "ext2.h"

#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <math.h>
#define BASE_OFFSET 1024 /* location of the superblock in the first group */
#define DIRECT_POINTER 11
#define INDIRECT_POINTER 12
#define DOUBLE_POINTER 13
#define TRIPLE_POINTER 14
#define SUPERBLOCK_OFFSET 1024
struct super_operations s_op;
struct inode_operations i_op;
struct file_operations f_op;

char fs_name[] = "ext2";


typedef enum CompareMode{
  COMPARE,
  DONT_COMPARE,
}compare_mode;

/* +++++++++++++++++++++++++ ==== Super Operations Start ==== +++++++++++++++++++++++++ */

int statfs(struct super_block *sb, struct kstatfs *stats){

  stats -> name = malloc(sizeof(char)*(strlen(sb -> s_type -> name)+1));
  strcpy(stats -> name, sb->s_type->name);
  stats -> f_magic = sb -> s_magic;          
  stats -> f_bsize = sb ->s_blocksize;                     
  stats -> f_blocks = sb -> s_blocks_count ;               
  stats -> f_bfree = sb -> s_free_blocks_count;               
  stats -> f_inodes = sb -> s_inodes_count;                
  stats -> f_finodes = sb -> s_free_inodes_count;               
  stats -> f_inode_size = sb -> s_inode_size;      
  stats -> f_minor_rev_level = sb -> s_minor_rev_level; 
  stats -> f_rev_level = sb -> s_rev_level;         
  stats -> f_namelen = strlen(stats -> name);            
  return 0;

}

void getInodeFromNum(unsigned int inode, struct ext2_inode* readData, struct super_block* superBlock){

  unsigned long blockSize = superBlock->s_blocksize;
  unsigned int blockGroup = (inode - 1) / superBlock->s_inodes_per_group;
  unsigned int indexInBlockGroup = (inode-1) % superBlock->s_inodes_per_group;
  unsigned int inodeContainerBlock = (indexInBlockGroup * superBlock-> s_inode_size)/ superBlock->s_blocksize;
  unsigned int inodeSize;
  struct ext2_group_desc groupDesc;

  unsigned long groupTableSeekIndex = blockSize* ceil((1024+sizeof(struct ext2_super_block))/blockSize);
  lseek(superBlock->s_type->file_descriptor,groupTableSeekIndex, SEEK_SET);
  lseek(superBlock->s_type->file_descriptor,blockGroup*sizeof(struct ext2_group_desc) , SEEK_CUR);
  read(superBlock->s_type->file_descriptor, &groupDesc, sizeof(struct ext2_group_desc));

  

  lseek(superBlock->s_type->file_descriptor, blockSize*groupDesc.bg_inode_table,SEEK_SET);
  lseek(superBlock->s_type->file_descriptor, superBlock->s_inode_size*((inode-1)%superBlock->s_inodes_per_group),SEEK_CUR);
  read(superBlock->s_type->file_descriptor, readData, superBlock->s_inode_size);

}

void read_inode(struct inode *i){

  struct ext2_inode readInode;
  getInodeFromNum(i->i_ino,&readInode, i->i_sb);
  i -> i_mode = readInode.i_mode;
  i -> i_nlink = readInode.i_links_count;
  i -> i_uid = readInode.i_uid;                   /* user id */
  i -> i_gid = readInode.i_gid;                   /* group id */
  i -> i_size = readInode.i_size;              /* size */
  i -> i_atime = readInode.i_atime;          /* access time */
  i -> i_mtime = readInode.i_mtime;          /* modify time */
  i -> i_ctime = readInode.i_ctime;          /* create time */
  i -> i_blocks = readInode.i_blocks;        /* Blocks count */

  for(int j=0; j<15; j++){
    i -> i_block[j] = readInode.i_block[j];      /* Pointers to data blocks */
  }
  
  i -> i_op = &i_op; /* inode operations */
  i -> f_op = &f_op;  /* file operations */
  i -> i_state = 0;         /*  */
  i -> i_flags = readInode.i_flags;          /* Permissions */

}


/* ------------------------- ==== Super Operations End ==== -------------------------*/



/* +++++++++++++++++++++++++ ==== Inode Operations Start ==== +++++++++++++++++++++++++ */

// Helper function for lookup
void clearAndAlloc(unsigned char* readBlock, unsigned long blockSize){
    
    free(readBlock);
    readBlock = malloc(blockSize);
}
// Helper for lookup
void readBlockwithOffset(unsigned char* readBlock, unsigned long blockOffset, int fd, int blockSize){
    
    lseek(fd, blockOffset, SEEK_SET);
    read(fd, readBlock, blockSize); 
 
}

void readCurrentDirEntry(struct ext2_dir_entry* currentDirEntry, unsigned char** readBlock){
  
  unsigned char* begin = *readBlock;
  unsigned int inode;
  unsigned short rec_len;
  unsigned char name_len;
  unsigned char file_type;
  char* name = NULL;

  // inode = **readBlock;
  memcpy(&inode,&(**readBlock),sizeof(unsigned int));

  *readBlock+=sizeof(unsigned int);

  // rec_len=**readBlock;
  memcpy(&rec_len,*readBlock,sizeof(unsigned short));

  *readBlock+=sizeof(unsigned short);

  // name_len = **readBlock;
  memcpy(&name_len,*readBlock,sizeof(unsigned char));

  *readBlock+=sizeof(unsigned char);

  // file_type = **readBlock;
  memcpy(&file_type,*readBlock,sizeof(unsigned char));

  *readBlock+=sizeof(unsigned char);

  name = malloc(sizeof(char)*(name_len+1));
  name[name_len]='\0';
  for(unsigned char i =0; i<name_len;i++){
    name[i] = **readBlock;
    *readBlock+=sizeof(unsigned char);
  }

  currentDirEntry -> rec_len = rec_len;
  currentDirEntry -> name_len = name_len;
  currentDirEntry -> file_type = file_type;
  currentDirEntry ->inode = inode;
  strcpy(currentDirEntry->name, name);
  currentDirEntry->name[name_len] = '\0';
  *readBlock = begin + rec_len;
}

unsigned long getBlockOffset(unsigned int block, unsigned long blockSize){
  return block*blockSize; //(block-1)??? //+SUPERBLOCK??

}


struct ext2_dir_entry* findDentryWithName(char* name, struct inode* i_node, compare_mode comp, filldir_t ptr, int* total){

  struct ext2_dir_entry* currentDirEntry;//=  malloc(sizeof(unsigned int) +sizeof(unsigned short));

  int fd = i_node ->i_sb->s_type->file_descriptor;
  int blockPtr = 0;
  int blockNumber = 0;
  unsigned int oneLevelPtr = 0;
  unsigned int secLevelPtr = 0;
  unsigned int thirdLevelPtr = 0;
  
  loffset_t totalRead = 0;
  loffset_t inBlockRead = 0;

  unsigned long blockOffset;
  unsigned long blockSize = i_node->i_sb->s_blocksize;
   
  unsigned long numOfIndirEntries = i_node -> i_sb -> s_blocksize / (sizeof(unsigned int));
  unsigned long numOfIndirEntriesSquared = numOfIndirEntries * numOfIndirEntries;
  unsigned long numOfIndirEntriesCubed = numOfIndirEntries * numOfIndirEntries * numOfIndirEntries;

  
  unsigned char* readBlock= malloc(blockSize*sizeof(unsigned char)); //read the entire block
  unsigned char* blockStartPtr = readBlock;
  unsigned char* indirectReadBlock;
  
  

  while(totalRead < i_node->i_size && blockPtr<15){

    while(!i_node->i_block[blockPtr]){
      blockPtr++;
      if(blockPtr>14){
        free(blockStartPtr);
        return NULL;
      }
    }
    if(blockPtr<12){
      blockOffset = getBlockOffset(i_node->i_block[blockPtr],blockSize);
      readBlockwithOffset(readBlock, blockOffset, fd, blockSize);
      blockPtr++;

    }else if(blockPtr == 12){
      unsigned int* table = malloc(numOfIndirEntries*sizeof(unsigned int));
      blockOffset = getBlockOffset(i_node->i_block[blockPtr], blockSize);
      lseek(fd,blockOffset,SEEK_SET);
      read(fd, table, numOfIndirEntries*sizeof(unsigned int));
      blockOffset = getBlockOffset(table[oneLevelPtr], blockSize);
      readBlockwithOffset(readBlock,blockOffset,fd,blockSize);
     
      oneLevelPtr++;
      
      if(oneLevelPtr >= numOfIndirEntries){
        blockPtr++;
        oneLevelPtr = 0;
      }
      free(table);  

    }else if(blockPtr == 13){
      
      unsigned int* table = malloc(numOfIndirEntries*sizeof(unsigned int));
      blockOffset = getBlockOffset(i_node->i_block[blockPtr], blockSize);
      lseek(fd,blockOffset,SEEK_SET);
      read(fd, table, numOfIndirEntries*sizeof(unsigned int) );
      blockOffset = getBlockOffset(table[secLevelPtr], blockSize);
      lseek(fd,blockOffset,SEEK_SET);
      read(fd, table, numOfIndirEntries*sizeof(unsigned int) );
      blockOffset = getBlockOffset(table[oneLevelPtr], blockSize);
      readBlockwithOffset(readBlock, blockOffset, fd, blockSize); // read Data
      
      oneLevelPtr++;
      if(oneLevelPtr >= numOfIndirEntries){
        oneLevelPtr = 0;
        secLevelPtr++;
        if(secLevelPtr >= numOfIndirEntries){
          secLevelPtr = 0;
          blockPtr++;          
        }
      }
      free(table);

    }else if(blockPtr==14){
      unsigned int* table = malloc(numOfIndirEntries*sizeof(unsigned int));
      blockOffset = getBlockOffset(i_node->i_block[blockPtr], blockSize);
      lseek(fd,blockOffset,SEEK_SET);
      read(fd, table, numOfIndirEntries*sizeof(unsigned int) );
      blockOffset = getBlockOffset(table[thirdLevelPtr], blockSize);
      lseek(fd,blockOffset,SEEK_SET);
      read(fd, table, numOfIndirEntries*sizeof(unsigned int) );
      blockOffset = getBlockOffset(table[secLevelPtr], blockSize);
      lseek(fd,blockOffset,SEEK_SET);
      read(fd, table, numOfIndirEntries*sizeof(unsigned int) );
      blockOffset = getBlockOffset(table[oneLevelPtr], blockSize);
      readBlockwithOffset(readBlock, blockOffset, fd , blockSize);
      oneLevelPtr++;
      if(oneLevelPtr >= numOfIndirEntries){
        oneLevelPtr = 0;
        secLevelPtr++;
        if(secLevelPtr >= numOfIndirEntries){
          secLevelPtr = 0;
          thirdLevelPtr++;
          if(thirdLevelPtr >= numOfIndirEntries){
            thirdLevelPtr = 0;
            blockPtr++;
          }
        }
      }
      free(table);
    }
    inBlockRead = 0;
    while(inBlockRead < blockSize){
      
      currentDirEntry = malloc(*(readBlock+sizeof(unsigned int)));
      readCurrentDirEntry(currentDirEntry, &readBlock);
      if(currentDirEntry->rec_len == 0 ){
        break;
      } 
      
      inBlockRead+=currentDirEntry->rec_len;
      totalRead += currentDirEntry->rec_len;

      if(currentDirEntry->inode==0){
        // free(blockStartPtr);
        // free(currentDirEntry);
        continue;
      }

      
      if(comp == COMPARE && !strcmp(name, currentDirEntry->name) ){
        
        // readBlock = blockStartPtr;
        free(blockStartPtr);
        return currentDirEntry;
      }
  
      if(comp == DONT_COMPARE){
        (*total)++;
        ptr(currentDirEntry->name, currentDirEntry->name_len,currentDirEntry->inode);
      }
      
      free(currentDirEntry);

    }
    // readBlock = blockStartPtr;
    free(blockStartPtr);
    readBlock= malloc(blockSize*sizeof(unsigned char)); //read the entire block
    blockStartPtr = readBlock;


  
  }
  // readBlock = blockStartPtr;
  free(blockStartPtr);
  return NULL;

}


struct dentry* lookup(struct inode *i, struct dentry *dir){

  if(!i || !dir){
    return dir;
  }
  
  void* dummyPtr = NULL;
  int dummyInt = 0;
  struct ext2_dir_entry* currentDirEntry = findDentryWithName(dir->d_name,i,COMPARE,dummyPtr,dummyInt);

  if(!strcmp(dir->d_name,"/")){
    return dir;

  }
  if(currentDirEntry){
    dir -> d_inode = malloc(sizeof(struct inode));
    dir -> d_inode ->i_op = &i_op;
    dir -> d_inode -> i_ino = currentDirEntry->inode;
    dir -> d_inode ->i_sb = i ->i_sb;

    dir->d_sb = i ->i_sb;
    read_inode(dir->d_inode);
    dir -> d_flags = dir->d_inode->i_flags;
    dir->d_fsdata = (char) dir->d_fsdata;
    dir->d_fsdata = 1;

  }else{
    free(dir->d_name);


    return NULL;
  }
  
  return dir;
 
}

ssize_t readNBytes(struct inode *inodePtr, char *buf, int len,loffset_t *o){

  loffset_t inodeSize = inodePtr->i_size; 

  unsigned long blockSize = inodePtr ->i_sb ->s_blocksize;
  unsigned long blockNumber = (*o) / blockSize;
  unsigned long dataStartPosInBlock = (*o) % blockSize;
  unsigned long numOfIndirEntries = inodePtr -> i_sb -> s_blocksize / (sizeof(unsigned int));
  unsigned long numOfIndirSquared = numOfIndirEntries * numOfIndirEntries;
  unsigned long numOfIndirCubed = numOfIndirEntries * numOfIndirEntries * numOfIndirEntries;

  int totalBytesRead  = 0;
  int inBlockRead = 0;
  int blockPtr = 0 ;

  if(blockNumber<12){
    blockPtr = blockNumber;
  }else if(blockNumber<numOfIndirEntries+12){
    blockPtr = 12;
  }else if(blockNumber < numOfIndirSquared+numOfIndirEntries+12){
    blockPtr = 13;
  }else{
    blockPtr =14;
  }


  int fd = inodePtr ->i_sb ->s_type->file_descriptor; 
  char* readBlock = malloc(blockSize*sizeof(char));
  unsigned int oneLevelPtr = 0;
  unsigned int secLevelPtr = 0;
  unsigned int thirdLevelPtr = 0;
  unsigned long blockOffset = getBlockOffset(inodePtr->i_block[blockPtr], blockSize);
  readBlockwithOffset(readBlock,blockOffset,fd,blockSize);  

     
  if(blockPtr == 12){
    unsigned int* table = malloc(numOfIndirEntries*sizeof(unsigned int));
    lseek(fd,blockOffset,SEEK_SET);
    read(fd, table, numOfIndirEntries*sizeof(unsigned int) );
    oneLevelPtr = blockNumber-12;
    blockOffset = getBlockOffset(table[oneLevelPtr], blockSize);
    readBlockwithOffset(readBlock,blockOffset,fd,blockSize);
    free(table);  
  }else if(blockPtr == 13){
    unsigned int* table = malloc(numOfIndirEntries*sizeof(unsigned int));
    lseek(fd,blockOffset,SEEK_SET);
    read(fd, table, numOfIndirEntries*sizeof(unsigned int) );
    secLevelPtr = (blockNumber - 12 - numOfIndirEntries) / numOfIndirEntries;
    blockOffset = getBlockOffset(table[secLevelPtr], blockSize);
    lseek(fd,blockOffset,SEEK_SET);
    read(fd, table, numOfIndirEntries*sizeof(unsigned int) );
    oneLevelPtr = (blockNumber- 12 - numOfIndirEntries) % numOfIndirEntries;//(blockNumber - 12 - numOfIndirEntries) % blockSize;
    blockOffset = getBlockOffset(table[oneLevelPtr], blockSize);
    readBlockwithOffset(readBlock, blockOffset, fd, blockSize); // read Data
    free(table);
  }else if(blockPtr==14){
    unsigned int* table = malloc(numOfIndirEntries*sizeof(unsigned int));
    lseek(fd,blockOffset,SEEK_SET);
    read(fd, table, numOfIndirEntries*sizeof(unsigned int) );
    thirdLevelPtr =  (blockNumber - 12 - numOfIndirEntries -numOfIndirSquared)/numOfIndirSquared;
    blockOffset = getBlockOffset(table[thirdLevelPtr], blockSize);
    lseek(fd,blockOffset,SEEK_SET);
    read(fd, table, numOfIndirEntries*sizeof(unsigned int) );
    secLevelPtr = ((blockNumber-12-numOfIndirEntries-numOfIndirSquared)%numOfIndirSquared)/numOfIndirEntries;
    //secLevelPtr = (blockNumber - 12 - numOfIndirEntries -numOfIndirSquared - thirdLevelPtr* numOfIndirSquared)%numOfIndirEntries;
    blockOffset = getBlockOffset(table[secLevelPtr], blockSize);
    lseek(fd,blockOffset,SEEK_SET);
    read(fd, table, numOfIndirEntries*sizeof(unsigned int) );
    //oneLevelPtr = (blockNumber- 12 - numOfIndirEntries - secLevelPtr*numOfIndirEntries-thirdLevelPtr*numOfIndirSquared) % numOfIndirEntries;//(blockNumber - 12 - numOfIndirEntries) % blockSize;
    oneLevelPtr = ((blockNumber-12-numOfIndirEntries-numOfIndirSquared)%numOfIndirSquared)%numOfIndirEntries;
    blockOffset = getBlockOffset(table[oneLevelPtr], blockSize);
    readBlockwithOffset(readBlock, blockOffset, fd , blockSize);
  }
  while(totalBytesRead < len && totalBytesRead < inodeSize){
    
    if(inBlockRead >= blockSize){
      
      inBlockRead = 0;
      dataStartPosInBlock=0;
      
      if(blockPtr < 12){
        free(readBlock);
        readBlock = malloc(blockSize);
        blockPtr++;
        if(blockPtr == 12){
          unsigned int* table = malloc(numOfIndirEntries*sizeof(unsigned int));
          blockOffset = getBlockOffset(inodePtr->i_block[blockPtr], blockSize);
          lseek(fd,blockOffset,SEEK_SET);
          read(fd, table, numOfIndirEntries*sizeof(unsigned int) );
          blockOffset = getBlockOffset(table[oneLevelPtr], blockSize);
          readBlockwithOffset(readBlock,blockOffset,fd,blockSize);
          free(table);  
          continue;
        }
       
        blockOffset = getBlockOffset(inodePtr->i_block[blockPtr], blockSize);
        readBlockwithOffset(readBlock, blockOffset, fd, blockSize);
        continue;

      }else if(blockPtr == 12 ){
        free(readBlock);
        readBlock = malloc(blockSize);
        oneLevelPtr++;
        if(oneLevelPtr >= numOfIndirEntries){
          oneLevelPtr = 0;
          blockPtr++;
          unsigned int* table = malloc(numOfIndirEntries*sizeof(unsigned int));
          blockOffset = getBlockOffset(inodePtr->i_block[blockPtr], blockSize);
          lseek(fd,blockOffset,SEEK_SET);
          read(fd, table, numOfIndirEntries*sizeof(unsigned int) );
          blockOffset = getBlockOffset(table[secLevelPtr], blockSize);
          lseek(fd,blockOffset,SEEK_SET);
          read(fd, table, numOfIndirEntries*sizeof(unsigned int) );
          blockOffset = getBlockOffset(table[oneLevelPtr], blockSize);
          readBlockwithOffset(readBlock, blockOffset, fd, blockSize); // read Data
          free(table);
          continue;
        }

        unsigned int* table = malloc(numOfIndirEntries*sizeof(unsigned int));
        blockOffset = getBlockOffset(inodePtr->i_block[blockPtr], blockSize);
        lseek(fd,blockOffset,SEEK_SET);
        read(fd, table, numOfIndirEntries*sizeof(unsigned int) );
        //master table read;
        blockOffset = getBlockOffset(table[oneLevelPtr], blockSize);
        readBlockwithOffset(readBlock,blockOffset,fd,blockSize);
        free(table);  
        continue;

      }else if (blockPtr == 13 ){
        free(readBlock);
        readBlock = malloc(blockSize);
        oneLevelPtr++;
        if(oneLevelPtr >= numOfIndirEntries){
          oneLevelPtr = 0;
          secLevelPtr++;
          if(secLevelPtr >= numOfIndirEntries){
            secLevelPtr = 0;
            blockPtr++;
            unsigned int* table = malloc(numOfIndirEntries*sizeof(unsigned int));
            blockOffset = getBlockOffset(inodePtr->i_block[blockPtr], blockSize);
            lseek(fd,blockOffset,SEEK_SET);
            read(fd, table, numOfIndirEntries*sizeof(unsigned int) );
            blockOffset = getBlockOffset(table[thirdLevelPtr], blockSize);
            lseek(fd,blockOffset,SEEK_SET);
            read(fd, table, numOfIndirEntries*sizeof(unsigned int) );
            blockOffset = getBlockOffset(table[secLevelPtr], blockSize);
            lseek(fd,blockOffset,SEEK_SET);
            read(fd, table, numOfIndirEntries*sizeof(unsigned int) );
            blockOffset = getBlockOffset(table[oneLevelPtr], blockSize);
            readBlockwithOffset(readBlock, blockOffset, fd , blockSize);
            free(table);
            continue;
          }
        }

        unsigned int* table = malloc(numOfIndirEntries*sizeof(unsigned int));
        blockOffset = getBlockOffset(inodePtr->i_block[blockPtr], blockSize);
        lseek(fd,blockOffset,SEEK_SET);
        read(fd, table, numOfIndirEntries*sizeof(unsigned int) );
        blockOffset = getBlockOffset(table[secLevelPtr], blockSize);
        lseek(fd,blockOffset,SEEK_SET);
        read(fd, table, numOfIndirEntries*sizeof(unsigned int) );
        blockOffset = getBlockOffset(table[oneLevelPtr], blockSize);
        readBlockwithOffset(readBlock, blockOffset, fd, blockSize); // read Data
        free(table);

      }else if (blockPtr==14){
        free(readBlock);
        readBlock = malloc(blockSize);
        oneLevelPtr++;
        if(oneLevelPtr >= numOfIndirEntries){
          oneLevelPtr = 0;
          secLevelPtr++;
          if(secLevelPtr >= numOfIndirEntries){
            secLevelPtr = 0;
            oneLevelPtr = 0;
            thirdLevelPtr++;
            if(thirdLevelPtr >= numOfIndirEntries){
              free(readBlock);
              return totalBytesRead;
            }
          }
        }
      unsigned int* table = malloc(numOfIndirEntries*sizeof(unsigned int));
      blockOffset = getBlockOffset(inodePtr->i_block[blockPtr], blockSize);
      lseek(fd,blockOffset,SEEK_SET);
      read(fd, table, numOfIndirEntries*sizeof(unsigned int) );
      blockOffset = getBlockOffset(table[thirdLevelPtr], blockSize);
      lseek(fd,blockOffset,SEEK_SET);
      read(fd, table, numOfIndirEntries*sizeof(unsigned int) );
      blockOffset = getBlockOffset(table[secLevelPtr], blockSize);
      lseek(fd,blockOffset,SEEK_SET);
      read(fd, table, numOfIndirEntries*sizeof(unsigned int) );
      blockOffset = getBlockOffset(table[oneLevelPtr], blockSize);
      readBlockwithOffset(readBlock, blockOffset, fd , blockSize);
      free(table);

      }
    }else if(inBlockRead<dataStartPosInBlock){
      inBlockRead++;
    }else{

      buf[totalBytesRead] = readBlock[inBlockRead];
      totalBytesRead++;
      inBlockRead++; 
      
    }


  }
  free(readBlock);
  return totalBytesRead;

}



int myreadlink(struct dentry *dir, char *buf, int len){

  if(!dir || !buf){
    return 0;
  }
  
  struct inode* inodePtr = dir->d_inode;

  loffset_t inodeSize = dir->d_inode->i_size; 
  if( inodeSize <= 60){
    memcpy(buf, dir->d_inode->i_block, inodeSize);
    return (int) inodeSize;
  }

  return readNBytes(inodePtr, buf, len,0);




  



}

int myreaddir(struct inode *i, filldir_t callback){

  if(!i){
    return -1;
  }
  
  int totalEntries = 0;
  findDentryWithName(NULL, i, DONT_COMPARE,callback,&totalEntries);
  return totalEntries;


}


int getattr(struct dentry *dir, struct kstat *stats){

  if(!dir || !stats){
    return -1;
  }

  struct inode *tmp_inode = dir->d_inode;

  stats -> ino = tmp_inode ->i_ino;
  stats -> mode = tmp_inode -> i_mode;
  stats -> nlink = tmp_inode -> i_nlink;
  stats -> uid = tmp_inode -> i_uid;
  stats -> gid = tmp_inode -> i_gid;
  stats -> size = tmp_inode -> i_size;
  stats -> atime = tmp_inode -> i_atime;
  stats -> mtime = tmp_inode -> i_mtime;
  stats -> ctime = tmp_inode -> i_ctime;
  stats -> blksize = tmp_inode -> i_sb -> s_blocksize;
  stats -> blocks = tmp_inode ->i_blocks;

  return 0;
}



/* ------------------------- ==== Inode Operations End ==== -------------------------*/


/* Implement functions in s_op, i_op, f_op here */

// implements file_operations :: llseek




struct super_block *get_superblock(struct file_system_type *fs){

  if(!fs){
    return NULL;
  }
  struct ext2_super_block tmp;
  // struct ext2_group_desc* tmp2 = malloc(sizeof(struct ext2_group_desc));
  char rootName[]="/";

  unsigned long indirectSize;

  lseek(myfs.file_descriptor, 1024, SEEK_SET);
  read(myfs.file_descriptor, &tmp, sizeof(tmp));
  // read(myfs.file_descriptor, tmp2, sizeof(tmp2));


  struct super_block* created = malloc(sizeof(struct super_block));

  created -> s_inodes_count = tmp.s_inodes_count;
  created -> s_blocks_count = tmp.s_blocks_count;
  created -> s_free_blocks_count = tmp.s_free_blocks_count;
  created -> s_free_inodes_count = tmp. s_free_inodes_count;
  created -> s_first_data_block = tmp.s_first_data_block;

  created -> s_blocksize = 1024 << tmp.s_log_block_size;
  created -> s_blocksize_bits = log(created->s_blocksize)/log(2);
  created -> s_blocks_per_group  = tmp.s_blocks_per_group;
  created -> s_inodes_per_group = tmp.s_inodes_per_group;
  created -> s_minor_rev_level = tmp.s_minor_rev_level;

  created -> s_rev_level = tmp.s_rev_level;
  created -> s_first_ino = tmp.s_first_ino;
  created -> s_inode_size = tmp.s_inode_size;
  
  created -> s_block_group_nr = tmp.s_block_group_nr;
  indirectSize = created->s_blocksize/(sizeof(unsigned int));
  created -> s_maxbytes = created->s_blocksize*(12+indirectSize+indirectSize*indirectSize+indirectSize*indirectSize*indirectSize);
  created -> s_type = fs;
  created -> s_op = &s_op;

  // created -> s_flags NOT USED

  created -> s_magic = tmp.s_magic;
  
  struct inode* dentry_rootInode = malloc(sizeof(struct inode));
  dentry_rootInode->i_ino = 2;
  dentry_rootInode->i_sb = created;
  read_inode(dentry_rootInode);
  // created -> s_root 
  created -> s_root = malloc(sizeof(struct dentry));
  created -> s_root -> d_flags = dentry_rootInode -> i_flags;
  created -> s_root -> d_inode = dentry_rootInode;
  created -> s_root -> d_parent = created -> s_root;
  created -> s_root ->d_name = malloc(sizeof(rootName)+1);
  strcpy(created -> s_root -> d_name, rootName);
  created -> s_root -> d_sb = created;
  created -> s_root -> d_fsdata = (char) created -> s_root -> d_fsdata;
  created -> s_root -> d_fsdata = 1;


  return created;
  

}







loffset_t llSeek(struct file *filePointer, loffset_t offset, int whence){

  if(!filePointer){
    return 0;
  }

  switch(whence){
    
    case SEEK_SET:
      filePointer->f_pos = offset;
      if(filePointer->f_pos > filePointer->f_inode->i_size){
        filePointer->f_pos = filePointer->f_inode->i_size;
      }else if(filePointer->f_pos<0){
        filePointer->f_pos = 0;
      }
      break;

    case SEEK_CUR:
      filePointer->f_pos += offset;
      if(filePointer->f_pos > filePointer->f_inode->i_size){
        filePointer->f_pos = filePointer->f_inode->i_size;
      }else if(filePointer->f_pos<0){
        filePointer->f_pos = 0;
      }
      break;
    default: //SEEK_END
      filePointer->f_pos = filePointer->f_inode->i_size;
      break;
  }

  return filePointer->f_pos;

}


// implements file_operations :: read
ssize_t fsRead(struct file *filePointer, char *buf, size_t len, loffset_t *o){

  if(!filePointer){
    return 0;
  }
  char isNull = 0;
  if(!o){
    isNull =1;
    o = &filePointer->f_pos;
  }
  
  if(S_ISLNK(filePointer->f_inode->i_mode)){
    struct dentry* dirEntPtr = pathwalk(filePointer->f_path);
    while(S_ISLNK(dirEntPtr->d_inode->i_mode)){
      
      char buf[256];
      int read = myreadlink(dirEntPtr, buf, 254);
      buf[read] = '\0';
      free(dirEntPtr);
      dirEntPtr = pathwalk(buf);
    }
    if(S_ISDIR(dirEntPtr->d_inode->i_mode)){
      if(isNull){
        o=NULL;
      }
      return 0;
    }
    if(dirEntPtr->d_inode->i_size < *o){
       if(isNull){
        o=NULL;
      }
      return 0;
    }
    if(isNull){
      o=NULL;
    }
    return readNBytes(dirEntPtr->d_inode, buf, len,o);
  }else if(S_ISDIR(filePointer->f_inode->i_mode)){
    if(isNull){
      o=NULL;
    }
    return 0;
  }


  if(filePointer->f_inode->i_size <= *o){
    if(isNull){
      o=NULL;
    }
    return 0;
  }

  ssize_t totalBytesRead = 0;
  struct inode* inode = filePointer-> f_inode;
  
  totalBytesRead = readNBytes(inode, buf,len, o);

  filePointer -> f_pos = *o + totalBytesRead;
  
  if(filePointer->f_pos > filePointer->f_inode->i_size){
    filePointer->f_pos = filePointer->f_inode->i_size;
  }else if(filePointer->f_pos<0){
    filePointer->f_pos = 0;
  }
  if(isNull){
      o=NULL;
    }
  return totalBytesRead;

}


// implements file_operations :: open
int fsOpen(struct inode * inodePtr, struct file *filePtr){

  if(!inodePtr || !filePtr){
    return -1; //error
  }
  
  filePtr -> f_op = &f_op;
  filePtr -> f_flags = filePtr -> f_inode -> i_flags;
  filePtr -> f_mode = filePtr -> f_inode -> i_mode;
  filePtr -> f_pos = 0;

  return 0;

}

// implements file_operations :: close
int fsRelease(struct inode* inodePtr, struct file *filePtr){

  if(!inodePtr || !filePtr){
    return -1; //error
  }

  free (filePtr -> f_path); 
  free (filePtr-> f_inode);  /// @@@ AFTER PATHWALK @@@

  return 0;

}


struct file_system_type *initialize_ext2(const char *image_path) {
  /* fill super_operations s_op */
  /* fill inode_operations i_op */
  /* fill file_operations f_op */
  /* for example:
      s_op = (struct super_operations){
        .read_inode = read_inode_function,
        .statfs = your_statfs_function,
      };
  */

  if(!image_path){
    return NULL;
  }

  myfs.file_descriptor = open(image_path, O_RDONLY);
  myfs.name = malloc((strlen(fs_name)+1)*sizeof(char));
  strcpy(myfs.name, "ext2");

  s_op = (struct super_operations){
    .read_inode = read_inode,
    .statfs = statfs,
    };
  f_op = (struct file_operations){

    .llseek = llSeek,
    .read = fsRead,
    .open = fsOpen,
    .release = fsRelease,

  };
  i_op = (struct inode_operations){
    .lookup = lookup,
    .readlink = myreadlink,
    .readdir = myreaddir,
    .getattr = getattr,
  };
  // myfs.name = fs_name;
  myfs.get_superblock = get_superblock;
  /* assign get_superblock function
     for example:
       
  */

  return &myfs;
}
