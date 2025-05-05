#define _GNU_SOURCE
#include <dlfcn.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

char tmpbuff[1024];
unsigned long tmppos = 0;
unsigned long tmpallocs = 0;

void *memset(void*,int,size_t);
void *memmove(void *to, const void *from, size_t size);

/*=========================================================
 * interception points
 */

static void * (*myfn_calloc)(size_t nmemb, size_t size);
static void * (*myfn_malloc)(size_t size);
static void   (*myfn_free)(void *ptr);
static void * (*myfn_realloc)(void *ptr, size_t size);
static void * (*myfn_memalign)(size_t blocksize, size_t bytes);
static void * (*myfn_mmap)(void *ptr,  size_t length, int prot, int flags, int fd, off_t offset);
static int (*myfn_munmap)(void *ptr, size_t length);


static void init(){
  myfn_malloc     = dlsym(RTLD_NEXT, "malloc");
  myfn_free       = dlsym(RTLD_NEXT, "free");
  myfn_calloc     = dlsym(RTLD_NEXT, "calloc");
  myfn_realloc    = dlsym(RTLD_NEXT, "realloc");
  myfn_memalign   = dlsym(RTLD_NEXT, "memalign");
  myfn_mmap       = dlsym(RTLD_NEXT, "mmap");
  myfn_munmap     = dlsym(RTLD_NEXT, "munmap");
  
  if (!myfn_malloc || !myfn_free || !myfn_calloc || !myfn_realloc || !myfn_memalign || !myfn_mmap || !myfn_munmap ) 
    {
      fprintf(stderr, "Error in `dlsym`: %s\n", dlerror());
      exit(1);
    }
}

void *malloc(size_t size){

  static int initializing = 0;
  if (myfn_malloc == NULL){
    if (!initializing){
      initializing = 1;
      init();
      initializing = 0;
      fprintf(stdout, "rMALLOC(%lu)\n", size);      
      fprintf(stdout, "jcheck: allocated %lu bytes of temp memory in %lu chunks during initialization\n", tmppos, tmpallocs);
    }
    else {
      if (tmppos + size < sizeof(tmpbuff)) {
	void *retptr = tmpbuff + tmppos;
	tmppos += size;
	++tmpallocs;
	return retptr;
      }
      else {
	fprintf(stdout, "jcheck: too much memory requested during initialisation - increase tmpbuff size\n");
	exit(1);
      }
    }
  }

  void *ptr = myfn_malloc(size);
  char buffer[50];
  int len=sprintf(buffer,"rMALLOc (%ld) at %p\n",size,ptr);
  write(1,buffer,len);
  return ptr;
}

void free(void *ptr){
  // something wrong if we call free before one of the allocators!
  //  if (myfn_malloc == NULL)
  //      init();
  
  if (ptr >= (void*) tmpbuff && ptr <= (void*)(tmpbuff + tmppos))
    fprintf(stdout, "freeing temp memory\n");
  else
    myfn_free(ptr);

  char buffer[50];
  int len=sprintf(buffer,"rFREE at %p\n",ptr);
  write(1,buffer,len);
}

void *realloc(void *ptr, size_t size)
{
  char buffer[70];
  int len=sprintf(buffer,"rREALLOC-> (%ld) at %p \n",size,ptr);
  write(1,buffer,len);
    if (myfn_malloc == NULL)
    {
        void *nptr = malloc(size);
        if (nptr && ptr)
        {
            memmove(nptr, ptr, size);
            free(ptr);
        }
        return nptr;
    }

    void *nptr = myfn_realloc(ptr, size);


    len=sprintf(buffer,"rREALLOC (%ld) at %p -> %p\n",size,ptr,nptr);
    write(1,buffer,len);
    return nptr;
}

void *calloc(size_t nmemb, size_t size)
{
    if (myfn_malloc == NULL)
    {
        void *ptr = malloc(nmemb*size);
        if (ptr)
            memset(ptr, 0, nmemb*size);
        return ptr;
    }

    void *ptr = myfn_calloc(nmemb, size);

    char buffer[70];
    int len=sprintf(buffer,"rCALLOC (%ld,%ld) \n",nmemb, size);
    write(1,buffer,len);
    
    return ptr;
}

void *memalign(size_t blocksize, size_t bytes)
{
    void *ptr = myfn_memalign(blocksize, bytes);

    char buffer[70];
    int len=sprintf(buffer,"rMEMALING (%ld, %ld) @ %p\n",blocksize, bytes,ptr);
    write(1,buffer,len);
    
    return ptr;
}

void *mmap(void *ptr,  size_t length, int prot, int flags, int fd, off_t offset)
{
  static int initializing = 0;
  if (myfn_mmap == NULL) {
    if (!initializing){
      initializing = 1;
      init();
      initializing = 0;
      fprintf(stdout, "rMMAP(%lu)\n", length);      
      fprintf(stdout, "jcheck: allocated %lu bytes of temp memory in %lu chunks during initialization\n", tmppos, tmpallocs);
    }
    else {
     if (tmppos + length < sizeof(tmpbuff)) {
	void *retptr = tmpbuff + tmppos;
	tmppos += length;
	++tmpallocs;
	return retptr;
      }
      else {
	fprintf(stdout, "jcheck: too much memory requested during initialisation - increase tmpbuff size\n");
	exit(1);
      }
    }
  }
  void *ptr2 = myfn_mmap(ptr, length, prot, flags, fd, offset);
    
  char buffer[70];
  int len=sprintf(buffer,"rMMAP (%ld) at %p\n", length, ptr2);
  write(1,buffer,len);
  
  return ptr2;
}


int munmap(void *ptr, size_t length){
  char buffer[70];
  int len=sprintf(buffer,"rMUNMMAP-> (%p,%ld) => \n",ptr, length);
  write(1,buffer,len);
  
  int resp=myfn_munmap(ptr, length);

  len=sprintf(buffer,"rMUNMMAP (%p,%ld) => %d\n",ptr, length, resp);
  write(1,buffer,len);

  return resp;
}
