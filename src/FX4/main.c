
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "debug_printf.h"
#include "bytecode.h"
#include "parser.h"

#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>


void show_help(void){

    printf("Usage:\n");
    printf("fx4 <filename>\n");
    printf("Options:\n");
    printf("-h / --help: Show this help message\n");
    printf("-d / --debug: Print debug listing\n");
    printf("-r / --run: Run script\n");

}

bool is_file_exists(const char *path){

    struct stat sb;

    if(stat(path, &sb) < 0){
        
        return false;
    }

    return true;
}

bool is_dir(const char *path){

    struct stat sb;

    if(stat(path, &sb) < 0){

        perror("lstat");
        exit(EXIT_FAILURE);
    }

    return S_ISDIR(sb.st_mode);
}

bool is_fx4(const char *fname){

    fname = strrchr(fname, '.');

    if(fname != NULL){

        if(strcmp(fname, ".fx4") == 0){

            return true;
        }
    }
    
    return false;
}


void get_output_filename(const char *fname, char *output_buf, int buf_len){

    memset(output_buf, 0, buf_len);
    strncpy(output_buf, fname, buf_len);

    // split filename
    char *c = output_buf;
    int len = 0;

    while((*c != '.') && (*c != '\0')){

        len++;
        c++;
    }

    if((len + 5) > buf_len){

        printf("filename too long\n");
        exit(EXIT_FAILURE);
    }

    *c++ = '.';
    *c++ = 'f';
    *c++ = '4';
    *c++ = 'b';
    *c++ = '\0';
}

int compile_file(const char *fname){

    FILE* fp = fopen(fname, "r");
    
    if(fp == NULL){

        printf("File read error\n");

        return -1;
    }

    fseek(fp, 0, SEEK_END);
    int fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    // printf("Size: %d bytes\n", fsize);

    char* buffer = malloc(fsize + 1);
   
    fread(buffer, fsize, 1, fp);
    buffer[fsize] = 0; // null terminate

    fclose(fp);

    vm_t vm = {0};

    int status = parse_str(buffer, fname, &vm);

    free(buffer);
    buffer = NULL;

    if(status < 0){

        printf("Error: %d\n", status);

        vm_deinit(&vm);

        return status;
    }

    // set up output file anme
    char output_file[128] = {0};
    get_output_filename(fname, output_file, sizeof(output_file));
    
    // serialize VM to file

    vm_serialize(&vm, output_file);
    printf("VM serialized to: %s\n", output_file);

    vm_deinit(&vm);

    return 0;
}

int main(int argc, char **argv){

    // printf("\n");
    printf("FX4 Compiler\n");


    // parse options
    
    bool opt_help = false;
    bool opt_debug = false;
    // bool opt_verbose = false;
    bool opt_run = false;
    const char *fname = NULL;

    if(argc == 1){

        opt_help = true;
    }

    for(int i = 1; i < argc; i++){

        if((strcmp(argv[i], "-h") == 0) || (strcmp(argv[i], "--help") == 0)){

            opt_help = true;
        }
        else if((strcmp(argv[i], "-d") == 0) || (strcmp(argv[i], "--debug") == 0)){

            opt_debug = true;
        }
        // else if((strcmp(argv[i], "-v") == 0) || (strcmp(argv[i], "--verbose") == 0)){

        //     opt_verbose = true;
        // }
        else if((strcmp(argv[i], "-r") == 0) || (strcmp(argv[i], "--run") == 0)){

            opt_run = true;
        }
        else if((argv[i][0] != '-') && (argv[i][1] != '-')){

            fname = argv[i];
        }
    }

    if(opt_help){

        show_help();

        return 0;
    }

    if(fname == NULL){

        printf("No filename given.\n");

        show_help();

        return 0;
    }

    printf("Compiling: %s\n", fname);

    if(opt_debug){

        printf("Debug prints enabled\n");
        enable_debug_print();
    }

    if(!is_file_exists(fname)){

        printf("Path does not exist\n");
        exit(EXIT_FAILURE);
    }


    int status = 0;

    if(is_dir(fname)){

        const struct dirent *de;  // Pointer for directory entry

        // opendir() returns a pointer of DIR type. 
        DIR *dr = opendir(fname);

        // opendir returns NULL if couldn't open directory
        if(dr == NULL){
            printf("Could not open current directory\n");
            exit(EXIT_FAILURE);
        }

        char cwd[1024];
        getcwd(cwd, sizeof(cwd)); // remember CWD      

        chdir(fname); // change into target folder

        int count = 0;

        // Refer https://pubs.opengroup.org/onlinepubs/7990989775/xsh/readdir.html
        // for readdir()
        while((de = readdir(dr)) != NULL){

            // skip hidden files
            if(de->d_name[0] == '.'){

                continue;
            }

            if(!is_fx4(de->d_name)){

                continue;
            }

            printf("%s\n", de->d_name);
            status = compile_file(de->d_name);

            count++;

            if(status < 0){

                printf("Error: %d\n", status);
                exit(EXIT_FAILURE);
            }

            if(opt_run){

                // get output file anme
                char output_file[128] = {0};
                get_output_filename(de->d_name, output_file, sizeof(output_file));

                vm_t new_vm = {0};
                vm_deserialize(&new_vm, output_file);

                status = vm_run_instructions(&new_vm, -1, 0);

                if(status == VM4_STATUS_OK){

                    printf("VM run OK\n");
                }
                else if(status == VM4_STATUS_HALT){

                    printf("VM run OK HALT\n");
                }
                else if(status < 0){

                    printf("!!!!!!!!!!!!!!!!!!\n");
                    printf("VM run failed: %d\n", status);

                    vm_deinit(&new_vm);

                    return status;
                }

                print_vm_state(&new_vm);

                vm_deinit(&new_vm);
            }
        }
        
        printf("Compiled %d files\n", count);

        chdir(cwd); // change back to original CWD

        closedir(dr);
    }
    else{

        if(!is_fx4(fname)){

            printf("File extension is not .fx4!\n");
            exit(EXIT_FAILURE);
        }

        status = compile_file(fname);

        if(status < 0){

            printf("Error: %d\n", status);
            exit(EXIT_FAILURE);
        }

        if(opt_run){

            printf("Starting VM...\n");

            // get output file anme
            char output_file[128] = {0};
            get_output_filename(fname, output_file, sizeof(output_file));

            vm_t new_vm = {0};
            vm_deserialize(&new_vm, output_file);

            status = vm_run_instructions(&new_vm, -1, 0);

            if(status < 0){

                printf("!!!!!!!!!!!!!!!!!!\n");
                printf("VM run failed: %d\n", status);

                vm_deinit(&new_vm);

                return status;
            }

            print_vm_state(&new_vm);

            vm_deinit(&new_vm);

            printf("VM finished\n");
        }
    }


    return status;
}
