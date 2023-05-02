/* boilerplate code

	WARNING: YOU MUST UNDERSTAND EVERY LINE OF CODE BEFORE ATTEMPTING CHANGING 

	You can change anything. Search for "TODO" for suggested places of change 
*/

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <dirent.h>

/* OP-TEE TEE client API (built by optee_client) */
#include <tee_client_api.h>

/* To the the UUID (found the the TA's h-file(s)) */
#include "cnn_ta.h"

#define OUTPUT_BUFFER_SIZE 512 * 1024 // at most 512KB
static char tmp_filepath[256];        // temporary file name for read/write

void* read_file(char *filename, size_t *size) {
    /*
    read the content from `filename` to buffer
    - the buffer is `malloc`ed and the pointer to it is returned
    - the number of bytes being read is saved to *size and may be used by caller
    - caller is responsible for `free`ing the allocated memory
    */
    FILE *f = fopen(filename, "rb");
    void* buffer = NULL;
    size_t fsize = 0;
    if (f) {
        fseek(f, 0, SEEK_END);
        fsize = ftell(f);
        fseek(f, 0, SEEK_SET);
        buffer = malloc(fsize);
        if (buffer) {
            fread(buffer, sizeof(char), fsize, f);
        } else {
            errx(1, "\033[31mnot enough memory\n\033[0m");
        }
        fclose(f);
    } else {
        fprintf(stderr, "\033[31mno such file: %s\n\033[0m", filename);
    }
    *size = fsize;
    printf("\033[34mread %d bytes from file %s\n\033[0m", *size, filename);
    return buffer;
}

size_t write_file(void *buffer, char *filename, size_t size) {
    /*
    write `size` bytes of buffer to `filename`
    - the number of bytes being written is returned
    - `size` == `bytes` should always hold when return
    */
    FILE *f = fopen(filename, "wb");
    size_t bytes = fwrite((unsigned char*)buffer, 1, size, f);
    printf("\033[34mwrite %d bytes to file %s\n\033[0m", bytes, filename);
    fclose(f);
    return bytes;
}

int main(int argc, char *argv[]) {
    char *arg1 = argv[1];
    char *arg2 = argv[2];

    TEEC_Context ctx;
    TEEC_Session sess;
    TEEC_UUID uuid = TA_CNN_UUID;   // uuid of the TA 
	uint32_t origin;
	TEEC_Result res;
    TEEC_Operation op;
	memset(&op, 0, sizeof(op));
    void *file_buffer = NULL;
    DIR *dir = NULL;

    /* Initialize a context connecting us to the TEE */
	res = TEEC_InitializeContext(NULL, &ctx);
	if (res != TEEC_SUCCESS)
		errx(1, "TEEC_InitializeContext failed with code 0x%x", res);

	/* Open a session with the TA */
	res = TEEC_OpenSession(&ctx, &sess, &uuid, TEEC_LOGIN_PUBLIC, NULL, NULL, &origin);
	if (res != TEEC_SUCCESS) {
        TEEC_FinalizeContext(&ctx);
		errx(1, "TEEC_Opensession failed with code 0x%x origin 0x%x",
			 res, origin);
    }

    if (strcmp(arg1, "-p") == 0) {
        /* TODO: implement TA command to load encrypted CNN parameter file */
        /////////////////// I WROTE START
        op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_INPUT,    // input from CA to TA
                                         TEEC_NONE,    // input from CA to TA
	    				                 TEEC_NONE,   // output from TA to CA
                                         TEEC_NONE);                // not used
                                    
        size_t cnn_buffer_size;
        void *cnn_buffer = read_file(arg2, &cnn_buffer_size);
	    op.params[0].tmpref.buffer = cnn_buffer;        // buffer contains cnn parameters
	    op.params[0].tmpref.size = cnn_buffer_size;     // size of cnn buffer
        res = TEEC_InvokeCommand(&sess, TA_CNN_CMD_LOAD, &op, &origin); // invoke command TA_CNN_CMD_LOAD

        if (res != TEEC_SUCCESS) {
	        fprintf(stderr, "\033[31mTEEC_InvokeCommand ([cnn_param img]) failed 0x%x origin 0x%x\033[0m FAILED IN -p", res, origin);
            goto exit;
        }

/*
        if (cnn_buffer_sec){
            free(cnn_buffer_sec);
        }*/
        //////////////// I WROTE END

        /* I WROTE TO COMMENT OUT
        res = TEEC_ERROR_NOT_SUPPORTED;
	    if (res == TEEC_ERROR_NOT_SUPPORTED) {
	    	fprintf(stderr, "\033[31mTEEC_InvokeCommand (optee_example_cnn -p [cnn_param]) not implemented IS IT STILL GOINGN HERE\033[0m");
            goto exit;
        }
        */
    } else if (strcmp(arg1, "-i") == 0) {
        /* TODO: implement TA command to load encrypted image and detect faces */

        //// I WROTE START
        void *output_buffer = malloc(OUTPUT_BUFFER_SIZE);
        if (output_buffer = NULL){
            err(1, "Not enough memory \n");

        }
        op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_INPUT, TEEC_MEMREF_TEMP_INPUT, TEEC_NONE, TEEC_NONE);

        // list all jpg png files under the directory
        dir = opendir(arg2);
        if (dir == NULL) {
            fprintf("\033[31mno such directory %s\033[0m", arg2);
        }

        /* HINT: you can refer to this while for how to loop through every
        file that has a specified extension name, and read its content, 
        invoke TEE command on the content
        */
        struct dirent *ent;
        while ((ent = readdir(dir)) != NULL)
        {
            if (ent->d_type == DT_REG &&   // ignore DT_REG undefined error // a regular file (not dir) AND
                ((strstr(ent->d_name, ".png") != NULL)                      // (jpg image file, *.jpg
                    ||                                                      //  OR
                 (strstr(ent->d_name, ".jpg") != NULL))                     //  png image file, *.png)
                ) {
                sprintf(tmp_filepath, "%s/%s", arg2, ent->d_name);
                size_t file_size;
                file_buffer = read_file(tmp_filepath, &file_size);
	            op.params[1].tmpref.buffer = file_buffer;       // buffer contains image
	            op.params[1].tmpref.size = file_size;           // size of image buffer
	            op.params[2].tmpref.buffer = output_buffer;     // buffer holding output from TA
	            op.params[2].tmpref.size = OUTPUT_BUFFER_SIZE;  // size of output buffer
	            res = TEEC_InvokeCommand(&sess, TA_CNN_CMD_DETECT, &op, &origin); // invoke command TA_CNN_CMD_EXEC
                // release file_buffer and set it to NULL
                free(file_buffer);
                file_buffer = NULL;
                if (res != TEEC_SUCCESS) {
	            	fprintf(stderr, "\033[31mTEEC_InvokeCommand ([cnn_param img]) failed 0x%x origin 0x%x\033[0m",
	            	        res, origin);
                    goto exit;
                }
                printf("\033[32msuccessfully use parameters %s to detect faces in %s\n\033[0m", arg1, tmp_filepath);
                sprintf(tmp_filepath, "%s/out_%s", arg2, ent->d_name);
                size_t bytes = write_file(op.params[2].tmpref.buffer, tmp_filepath, op.params[2].tmpref.size);
            }
        }
     /*
        res = TEEC_ERROR_NOT_SUPPORTED;
        if (res == TEEC_ERROR_NOT_SUPPORTED) {
	    	fprintf(stderr, "\033[31mTEEC_InvokeCommand (optee_example_cnn -i [img]) not implemented NOT IMPLEMENTED\033[0m");
            goto exit;
        }*/

        ///////// I WROTE END
    } else {
        // optee_example_cnn [cnn_param] [img_dir]
        void *output_buffer = malloc(OUTPUT_BUFFER_SIZE); // the max output file size is 512KB
        if (output_buffer == NULL) {
            err(1, "Not enough memory\n");
        }
        op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_INPUT,    // input from CA to TA
                                         TEEC_MEMREF_TEMP_INPUT,    // input from CA to TA
	    				                 TEEC_MEMREF_TEMP_OUTPUT,   // output from TA to CA
                                         TEEC_NONE);                // not used
        size_t cnn_buffer_size;
        void *cnn_buffer = read_file(arg1, &cnn_buffer_size);
	    op.params[0].tmpref.buffer = cnn_buffer;        // buffer contains cnn parameters
	    op.params[0].tmpref.size = cnn_buffer_size;     // size of cnn buffer

        // list all jpg png files under the directory
        dir = opendir(arg2);
        if (dir == NULL) {
            fprintf("\033[31mno such directory %s\033[0m", arg2);
        }

        /* HINT: you can refer to this while for how to loop through every
        file that has a specified extension name, and read its content, 
        invoke TEE command on the content
        */
        struct dirent *ent;
        while ((ent = readdir(dir)) != NULL)
        {
            if (ent->d_type == DT_REG &&   // ignore DT_REG undefined error // a regular file (not dir) AND
                ((strstr(ent->d_name, ".png") != NULL)                      // (jpg image file, *.jpg
                    ||                                                      //  OR
                 (strstr(ent->d_name, ".jpg") != NULL))                     //  png image file, *.png)
                ) {
                sprintf(tmp_filepath, "%s/%s", arg2, ent->d_name);
                size_t file_size;
                file_buffer = read_file(tmp_filepath, &file_size);
	            op.params[1].tmpref.buffer = file_buffer;       // buffer contains image
	            op.params[1].tmpref.size = file_size;           // size of image buffer
	            op.params[2].tmpref.buffer = output_buffer;     // buffer holding output from TA
	            op.params[2].tmpref.size = OUTPUT_BUFFER_SIZE;  // size of output buffer
	            res = TEEC_InvokeCommand(&sess, TA_CNN_CMD_EXEC, &op, &origin); // invoke command TA_CNN_CMD_EXEC
                // release file_buffer and set it to NULL
                free(file_buffer);
                file_buffer = NULL;
                if (res != TEEC_SUCCESS) {
	            	fprintf(stderr, "\033[31mTEEC_InvokeCommand ([cnn_param img]) failed 0x%x origin 0x%x\033[0m",
	            	        res, origin);
                    goto exit;
                }
                printf("\033[32msuccessfully use parameters %s to detect faces in %s\n\033[0m", arg1, tmp_filepath);
                sprintf(tmp_filepath, "%s/out_%s", arg2, ent->d_name);
                size_t bytes = write_file(op.params[2].tmpref.buffer, tmp_filepath, op.params[2].tmpref.size);
            }
        }
        
        // free cnn_buffer
        if (cnn_buffer) {
            free(cnn_buffer);
        }
        // free output_buffer
        if (output_buffer) {
            free(output_buffer);
        }
    }

exit: 
    // free `file_buffer` if it's in use
    if (file_buffer) {
        free(file_buffer);
    }
    // close `dir` if it's open
    if (dir) {
        closedir(dir);
    }
    // destroy TEE connection
    TEEC_CloseSession(&sess);
	TEEC_FinalizeContext(&ctx);
    return 0;
}



