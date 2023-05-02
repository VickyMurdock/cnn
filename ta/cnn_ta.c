/* boilerplate code

	WARNING: YOU MUST UNDERSTAND EVERY LINE OF CODE BEFORE ATTEMPTING CHANGING 

	You can change anything. Search for "TODO" for suggested places of change 
*/


#include <inttypes.h>
#include <tee_internal_api.h>
#include <tee_internal_api_extensions.h>
#include "cnn_ta.h"
#include "aes.h"
#include "sod.h"

static const char key[AES256_KEY_BYTE_SIZE] = "$B&E)H+MbQeThWmZq4t7w!z%C*F-JaNc";
static const char iv[AES_BLOCK_SIZE] = "an iv here";
static const char obj_name[] = "face_cnn.sod";
static sod_cnn *pNet = NULL;

/*
create a CNN and set pNet as its handler
- `pNet`: sod_cnn **, the address of a pointer to sod_cnn
upon successfully finishing this function call, pNet will point to a functional sod_cnn
*/

static TEE_Result (sod_cnn **pNet) {
	TEE_Result rc = TEE_ERROR_GENERIC;
	TEE_ObjectHandle object;
	TEE_ObjectInfo object_info;

	rc = TEE_OpenPersistentObject(TEE_STORAGE_PRIVATE,
					obj_name, sizeof(obj_name),
					TEE_DATA_FLAG_ACCESS_READ |
					TEE_DATA_FLAG_SHARE_READ,
					&object);
	if (rc != TEE_SUCCESS) {
		EMSG("Failed to open persistent object, res=0x%08x", rc);
		goto exit;
	}

	rc = TEE_GetObjectInfo1(object, &object_info);
	if (rc != TEE_SUCCESS) {
		EMSG("Failed to get info of persistent object, res=0x%08x", rc);
		goto exit;
	}

	char *buffer = (char *)TEE_Malloc(object_info.dataSize, TEE_MALLOC_FILL_ZERO);
	char *buffer_ptr = buffer;	
	size_t read_bytes;
	
	rc = TEE_ReadObjectData(object, buffer, object_info.dataSize,
				 &read_bytes);

	if (rc != TEE_SUCCESS || read_bytes != object_info.dataSize) {
		EMSG("TEE_ReadObjectData failed 0x%08x, read %" PRIu32 " over %u",
				rc, read_bytes, object_info.dataSize);
		goto exit;
	}

	const char *zErr;
	rc = sod_cnn_create(pNet, ":face", &buffer_ptr, &zErr);
	if (rc != SOD_OK) {
		EMSG("Can't create cnn, check if parameters are valid");
		rc = TEE_ERROR_GENERIC;
		goto exit;
	}
exit:
	TEE_Free(buffer);
	TEE_CloseObject(object);
	return rc;
}

/*
use `pNet` to detect the face in the image referenced by `buffer`
`pNet`: sod_cnn *, MUST point to a functional `sod_cnn`, or the program will crash
`buffer`: plaintext image dumped as a byte string
`buffer_size`: size of the image in byte 
`output_size`: pointer to the variable holding the output image size (after this function detects faces
	and circles them, the output size is likely to differ from buffer_size)
*/

static unsigned char *detect_face(sod_cnn *pNet, void *buffer, size_t buffer_size, size_t *output_size) {

	sod_img img_in = sod_img_load_from_mem(buffer, buffer_size, 0);
	float *blob = NULL;
	unsigned char *img_out = NULL;
	if (img_in.data == 0) {
		EMSG("cannot load input image");
		goto exit;
	}
	sod_box *box;
	int i, nbox;
	/* Prepare our input image for the detection process which 
	 * is resized to the network dimension (This op is always very fast)
	 */
	blob = sod_cnn_prepare_image(pNet, img_in);
	if (!blob) {
		/* Very unlikely this happen: Invalid architecture, out-of-memory */
		EMSG("Something went wrong while preparing image..");
		goto exit;
	}
	EMSG("Starting CNN face detection");
	/* Detect.. */
	sod_cnn_predict(pNet, blob, &box, &nbox);
	/* Report the detection result. */
	EMSG("%d face(s) were detected..\n",nbox);
	for (i = 0; i < nbox; i++) {
		/* Report the coordinates and score of the current detected face */
		EMSG("(%s) X:%d Y:%d Width:%d Height:%d score:%f%%\n", box[i].zName, box[i].x, box[i].y, box[i].w, box[i].h, box[i].score * 100);
		if( box[i].score < 0.3) continue;   /* Discard low score detection, remove if you want to report all faces */
		/*
		 * Draw a UVA orange (RGB: 229,114,0) circle of width 5 on the object coordinates. */
		sod_image_draw_circle_thickness(img_in, box[i].x + (box[i].w / 2), box[i].y + (box[i].h / 2), box[i].w, 5, 229., 114., 0);
		/* Of course, one could draw a box via sod_image_draw_bbox_width() or 
		 * crop the entire region via sod_crop_image() instead of drawing a circle. */
	}
	/* Finally save our output image with the circles drawn on it */
exit:
	// if no error occurs
	if (img_in.data && blob) {
		// dump the output image to buffer
		img_out = sod_img_dump_as_png(img_in, output_size);
    	EMSG("Finish dumping image to char, size: %d", *output_size);
	}
	/* Free the memory allocated to img_in */
	sod_free_image(img_in);
	return img_out;
}

/* function body responsible for TA command optee_example_cnn [cnn_param] [img] */
static TEE_Result cmd_exec(uint32_t types,
			     TEE_Param params[TEE_NUM_PARAMS]) {

	TEE_Result rc = TEE_ERROR_GENERIC;
	const int cnn_idx = 0;		/* highlight nonsecure buffer index */
    const int img_idx = 1;
	const int img_out_idx = 2;

	if (types != TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_INPUT,
				                 TEE_PARAM_TYPE_MEMREF_INPUT,
				                 TEE_PARAM_TYPE_MEMREF_OUTPUT,
				                 TEE_PARAM_TYPE_NONE)) {
		EMSG("bad parameters %x", (unsigned)types);
		return TEE_ERROR_BAD_PARAMETERS;
	}

    uint32_t cnn_size = params[cnn_idx].memref.size;
	/*
	 * We could rely on the TEE to provide consistent buffer/size values
	 * to reference a buffer with a unique and consistent secure attribute
	 * value. Hence it is safe enough (and more optimal) to test only the
	 * secure attribute of a single byte of it. Yet, since the current
	 * test does not deal with performance, let check the secure attribute
	 * of each byte of the buffer.
	 */
	rc = TEE_CheckMemoryAccessRights(TEE_MEMORY_ACCESS_ANY_OWNER |
					 TEE_MEMORY_ACCESS_READ |
					 TEE_MEMORY_ACCESS_NONSECURE,
					 params[cnn_idx].memref.buffer,
					 params[cnn_idx].memref.size);
	if (rc != TEE_SUCCESS) {
		EMSG("TEE_CheckMemoryAccessRights (nsec) failed %x", rc);
		return rc;
	}

	char *cnn_buffer = (char*)params[cnn_idx].memref.buffer;
	const char *zErr;
	// create CNN if it doesn't exist
	if (!pNet) {
		// the third parameter should be char **
		rc = sod_cnn_create(&pNet, ":face", &cnn_buffer, &zErr);			/////////// I THINK I WROTE BY ACCIDENT
		if (rc != SOD_OK) {
			EMSG("Can't create cnn, check if parameters are valid");
			rc = TEE_ERROR_GENERIC;
			return rc;
		}
	}
	void *img_buffer = params[img_idx].memref.buffer;
	size_t img_size = params[img_idx].memref.size;
	unsigned char *img_out = detect_face(pNet, img_buffer, img_size, &params[img_out_idx].memref.size);
	TEE_MemMove(params[img_out_idx].memref.buffer, img_out, params[img_out_idx].memref.size);
	TEE_Free(img_out);
	
	return rc;
}

/* function body responsible for TA command optee_example_cnn -p [cnn_param] */
static TEE_Result cmd_load(uint32_t types,
			     TEE_Param params[TEE_NUM_PARAMS]) {
	TEE_Result rc = TEE_ERROR_GENERIC;
	const int input_idx = 0;		/* highlight nonsecure buffer index */

	if (types != TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_INPUT,
				                 TEE_PARAM_TYPE_NONE,
				                 TEE_PARAM_TYPE_NONE,
				                 TEE_PARAM_TYPE_NONE)) {
		EMSG("bad parameters %x", (unsigned)types);
		return TEE_ERROR_BAD_PARAMETERS;
	}

	/*
	 * We could rely on the TEE to provide consistent buffer/size values
	 * to reference a buffer with a unique and consistent secure attribute
	 * value. Hence it is safe enough (and more optimal) to test only the
	 * secure attribute of a single byte of it. Yet, since the current
	 * test does not deal with performance, let check the secure attribute
	 * of each byte of the buffer.
	 */
	rc = TEE_CheckMemoryAccessRights(TEE_MEMORY_ACCESS_ANY_OWNER |
					 TEE_MEMORY_ACCESS_READ |
					 TEE_MEMORY_ACCESS_NONSECURE,
					 params[input_idx].memref.buffer,
					 params[input_idx].memref.size);
	if (rc != TEE_SUCCESS) {
		EMSG("TEE_CheckMemoryAccessRights (nsec) failed %x", rc);
		return rc;
	}
	char *cnn_buffer_sec = (char*)params[input_idx].memref.buffer;
	size_t cnn_size_sec = params[input_idx].memref.size;

	char *cnn_buffer = (char*)TEE_Malloc(cnn_size_sec, TEE_MALLOC_FILL_ZERO);
	char *cnn_buffer_ptr = cnn_buffer;
	size_t cnn_size = cnn_size_sec;

	/* TODO: decrypt the CNN parameters in `cnn_buffer_sec` to `cnn_buffer`
	         and create the CNN model to pNet
	cnn_buffer = decrypt(cnn_buffer_sec)
	create_cnn(pNet, cnn_buffer)
	*/
	////////// I WROTE START
	rc = decrypt(key, iv, cnn_buffer_sec, cnn_size_sec, cnn_buffer_ptr, &cnn_size);
	//const char *zErr;
	// create CNN if it doesn't exist
	if (rc != TEE_SUCCESS){
		EMSG("Decrypting the CNN parameters in 'cnn_buffer_sec' to 'cnn_buffer' failed");
		return rc;
	}
	const char *zErr;
	/*
	if (!pNet){
		rc = create_cnn(&pNet);
	}
	*/
	
	if (!pNet) {
		// the third parameter should be char **
		rc = sod_cnn_create(&pNet, ":face", &cnn_buffer_ptr, &zErr);
	//	rc = create_cnn(&pNet, ":face", &cnn_buffer, &zErr);
		if (rc != SOD_OK) {
			EMSG("Can't create cnn, check if parameters are valid");
			rc = TEE_ERROR_GENERIC;
			return rc;
		}
	}
///// I WROTE END
	/* it's not necessary to change the following block, but you can if needed */
	// save the decrypted parameter `cnn_buffer` as a TEE persistent object
	TEE_ObjectHandle object;
	uint32_t obj_data_flag;
	obj_data_flag = TEE_DATA_FLAG_ACCESS_READ |			/* we can later read the oject */
					TEE_DATA_FLAG_ACCESS_WRITE |		/* we can later write into the object */
					TEE_DATA_FLAG_ACCESS_WRITE_META |	/* we can later destroy or rename the object */
					TEE_DATA_FLAG_OVERWRITE;			/* destroy existing object of same ID */
	
	rc = TEE_CreatePersistentObject(TEE_STORAGE_PRIVATE,
					obj_name, sizeof(obj_name),
					obj_data_flag,
					TEE_HANDLE_NULL,
					NULL, 0,		/* we may not fill it right now */
					&object);
	if (rc != TEE_SUCCESS) {
		EMSG("TEE_CreatePersistentObject failed 0x%08x", rc);
		return rc;
	}

	// save the decrypted CNN params
	rc = TEE_WriteObjectData(object, cnn_buffer, cnn_size);
	if (rc != TEE_SUCCESS) {
		EMSG("TEE_WriteObjectData failed 0x%08x", rc);
		TEE_CloseAndDeletePersistentObject1(object);
	} else {
		TEE_CloseObject(object);
	}
	// free decrypted cnn_buffer
	TEE_Free(cnn_buffer);
	return rc;
}

/* function body responsible for TA command optee_example_cnn -i [img] */
static TEE_Result cmd_detect(uint32_t types,
			     TEE_Param params[TEE_NUM_PARAMS]) {
	TEE_Result rc = TEE_ERROR_GENERIC;
    const int img_idx = 0;
	const int img_out_idx = 1;

	if (types != TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_INPUT,
				                 TEE_PARAM_TYPE_MEMREF_OUTPUT,
				                 TEE_PARAM_TYPE_NONE, TEE_PARAM_TYPE_NONE)) {
		EMSG("bad parameters %x", (unsigned)types);
		return TEE_ERROR_BAD_PARAMETERS;
	}

	/*
	 * We could rely on the TEE to provide consistent buffer/size values
	 * to reference a buffer with a unique and consistent secure attribute
	 * value. Hence it is safe enough (and more optimal) to test only the
	 * secure attribute of a single byte of it. Yet, since the current
	 * test does not deal with performance, let check the secure attribute
	 * of each byte of the buffer.
	 */
	rc = TEE_CheckMemoryAccessRights(TEE_MEMORY_ACCESS_ANY_OWNER |
					 TEE_MEMORY_ACCESS_READ |
					 TEE_MEMORY_ACCESS_NONSECURE,
					 params[img_idx].memref.buffer,
					 params[img_idx].memref.size);
	if (rc != TEE_SUCCESS) {
		EMSG("TEE_CheckMemoryAccessRights (nsec) failed %x", rc);
		return rc;
	}

	char *img_buffer_sec = (char*)params[img_idx].memref.buffer;
	size_t img_size_sec = params[img_idx].memref.size;

	char *img_buffer = (char*)TEE_Malloc(img_size_sec, TEE_MALLOC_FILL_ZERO);
	size_t img_size = img_size_sec;

	/* TODO: decrypt the image buffer `img_buffer_sec` to `img_buffer`
	img_buffer = decrypt(img_buffer_sec)
	*/

	// I WROTE START

	rc = decrypt(key, iv, img_buffer_sec, img_size_sec, img_buffer, &img_size);
	if (rc != TEE_SUCCESS){
		EMSG("Decrypting image buffer img buffer sec to img buffer");
		return rc;
	}


/// I WROTE END





	
	/* it's not necessary to change the following block, but you can if needed */
	// create CNN
	if (!pNet) {
		rc = create_cnn(&pNet);
	}
	size_t img_out_size;
	// detect face
	unsigned char *img_out = detect_face(pNet, img_buffer, img_size, &img_out_size);
	// adjust buffer size based on output
	img_buffer = TEE_Realloc(img_buffer, img_out_size);
	img_size = img_out_size;

	/* TODO: encrypt the output image `img_out`
	img_buffer = encrypt(img_out)	
	*/	

	// I WROTE START
	rc = encrypt(key, iv, img_out, img_out_size, img_buffer, &img_size);
	if (rc != TEE_SUCCESS){
		EMSG("Encrypting output image failed");
		return rc;
	}






	
	/* it's not necessary to change the following block, but you can if needed */
	// check if the output buffer is large enough
	if (img_size > params[img_out_idx].memref.size) {
		EMSG("output buffer (%u bytes) is less than output image (%u bytes)",
		     params[img_out_idx].memref.size, img_size);
		rc = TEE_ERROR_SHORT_BUFFER;
		goto exit;
	}
	// copy the encrypted img_buffer to output buffer
	TEE_MemMove(params[img_out_idx].memref.buffer, img_buffer, img_size);
	params[img_out_idx].memref.size = img_size;
exit:
	// clear unencrypted `img_out`
	TEE_Free(img_out);
	// clear the encrypted `img_buffer`
	TEE_Free(img_buffer);
	return rc;
}

TEE_Result TA_CreateEntryPoint(void)
{
	/* Nothing to do */
    IMSG("Create TA\n");
	return TEE_SUCCESS;
}

void TA_DestroyEntryPoint(void)
{
	// destroy pNet if it exits when close the TA and destroy the context
	if (pNet) {
		sod_cnn_destroy(pNet);
	}
}

TEE_Result TA_OpenSessionEntryPoint(uint32_t __unused param_types,
					TEE_Param __unused params[4],
					void __unused **session)
{
	// *session = (void *)sess;
	DMSG("Entry");
    return TEE_SUCCESS;
}

void TA_CloseSessionEntryPoint(void __unused *session)
{
    DMSG("Exit");
}

TEE_Result TA_InvokeCommandEntryPoint(void __unused *session,
					uint32_t cmd,
					uint32_t param_types,
					TEE_Param params[4])
{
	switch (cmd) {
	case TA_CNN_CMD_EXEC:
		return cmd_exec(param_types, params);
	case TA_CNN_CMD_LOAD:
		/* TODO: execute load CNN command */
		return cmd_load(param_types, params);												////////////// I WROTE
		
	case TA_CNN_CMD_DETECT:
		/* TODO: execute load image and detect face command */
		return cmd_detect(param_types, params);												/////////////// I WROTE
		
	default:
		EMSG("Command ID 0x%x is not supported", cmd);
		return TEE_ERROR_NOT_SUPPORTED;
	}
}