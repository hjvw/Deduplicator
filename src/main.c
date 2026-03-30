#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <omp.h>
#include <sys/types.h>
#include <stdatomic.h>

#define MAX_PATH_LENGH 1024
#define TABLE_SIZE 1000



typedef struct File{
    char path[MAX_PATH_LENGH];
    off_t size;
    struct File *next;
} File;

_Atomic(File*) containers[TABLE_SIZE] = {NULL};





void search_folder(const char *path){
    DIR *folder=opendir(path);
    if(folder==NULL) return;
    
    struct dirent *entry;

    
    
    char new_path[1024];
    
    while((entry=readdir(folder))!=NULL){
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

        snprintf(new_path,sizeof(new_path),"%s/%s",path,entry->d_name);
        

        if(entry->d_type==DT_DIR){
            #pragma omp task firstprivate(new_path)
            {
                search_folder(new_path);
            }
        }else if(entry->d_type==DT_REG){

            struct stat st;
        if (stat(new_path, &st) == 0) {
            off_t size = st.st_size; 
            File *new_file=malloc(sizeof(File));
            if(new_file==NULL){
                printf("OUT OF RAM");
                continue;
            }

            new_file->size=size;
            strncpy(new_file->path,new_path,MAX_PATH_LENGH);
            new_file->next=NULL;

            int container_index=size % TABLE_SIZE;
            
            File *old_beggining=atomic_load(&containers[container_index]);

do {
    new_file->next = old_beggining;
} while (!atomic_compare_exchange_weak((_Atomic(File*)*)&containers[container_index], &old_beggining, new_file));





 } else {
            printf("Couldn`t get file info: %s\n", new_path);
        }

        }

      
    }

    closedir(folder);

    


 }



int comp(const void *a,const void *b){
    File *fileA=*(File**)a;
    File *fileB=*(File**)b;

    if(fileA->size<fileB->size) return -1;
    if(fileA->size>fileB->size) return 1;
    return 0;
}

void get_md5(const char *path, char *output_hash) {
    char command[1100];
    snprintf(command, sizeof(command), "md5sum \"%s\"", path);

 
    FILE *fp = popen(command, "r");
    if (fp == NULL) {
        strcpy(output_hash, "ERROR");
        return;
    }

  
    if (fgets(output_hash, 33, fp) == NULL) {
        strcpy(output_hash, "ERROR");
    }
    
    output_hash[32] = '\0'; 
    pclose(fp); 
}

int main(int argc, char *argv[]) {


    char *initial_path=".";
    if(argc>1){
        initial_path=argv[1];
    }

    #pragma omp parallel
    {
        #pragma omp single
        {
            search_folder(initial_path);
        }

    }

    


    for (int i = 0; i < TABLE_SIZE; i++) {
        File *recent = atomic_load(&containers[i]);
        if (recent == NULL) continue;

        int counter = 0;
        File *temp = recent;
        while (temp != NULL) { 
            counter++; 
            temp = temp->next; 
        }

        if (counter < 2) continue; 

        File **file_table = malloc(counter * sizeof(File*));
        if (file_table == NULL) continue; 

        temp = recent;
        for (int j = 0; j < counter; j++) {
            file_table[j] = temp;
            temp = temp->next;
        }

        qsort(file_table, counter, sizeof(File*), comp);

        for (int j = 0; j < counter - 1; j++) {
            if (file_table[j]->size == file_table[j+1]->size) {
                
                char base_hash[33];
                get_md5(file_table[j]->path, base_hash);
                
                if (strcmp(base_hash, "ERROR") == 0) continue;

                int k = j + 1;
                while (k < counter && file_table[k]->size == file_table[j]->size) {
                    char next_hash[33];
                    get_md5(file_table[k]->path, next_hash);
                    
                    if (strcmp(base_hash, next_hash) != 0) {
                        break; 
                    }
                    k++;
                }

                if (k > j + 1) {
                    printf("\nFOUND SOME  DUPES (HASH: %s):\n", base_hash);
                    
                    for (int m = j; m < k; m++) {
                        printf("  - %s\n", file_table[m]->path);
                    }
                    printf("     (SIZE: %ld BITES)\n", (long)file_table[j]->size);
                    
                    j = k - 1; 
                }
            }
        }
        free(file_table);
    }
    
    return EXIT_SUCCESS;
}