#include <inttypes.h>
#include <tee_internal_api.h>
#include <tee_internal_api_extensions.h>
#include "aes.h"

static TEE_Result prepare_operation(struct aes_cipher *sess, const char *key) {
	/* prepare for AES operation, e.g., allocate transient objects to hold the key,
	set the operation mode, etc */
    TEE_Result rc = TEE_ERROR_GENERIC;
    /* Free potential previous transient object */
    if (sess->op_handle != TEE_HANDLE_NULL)
		TEE_FreeOperation(sess->op_handle);

	/* Allocate operation: AES/CTR, mode and size from params */
	IMSG("Allocate sess->op_handle");
    rc = TEE_AllocateOperation(&sess->op_handle,
				    sess->algo,
				    sess->mode,
				    sess->key_size * 8);
	if (rc != TEE_SUCCESS) {
		EMSG("Failed to allocate operation");
		sess->op_handle = TEE_HANDLE_NULL;
		goto err;
	}

	/* Free potential previous transient object */
	if (sess->key_handle != TEE_HANDLE_NULL)
		TEE_FreeTransientObject(sess->key_handle);

	IMSG("Allocate sess->key_handle");
	/* Allocate transient object according to target key size */
	rc = TEE_AllocateTransientObject(TEE_TYPE_AES,
					  sess->key_size * 8,
					  &sess->key_handle);
	if (rc != TEE_SUCCESS) {
		EMSG("Failed to allocate transient object");
		sess->key_handle = TEE_HANDLE_NULL;
		goto err;
	}

	IMSG("Set sess->key_handle");
    TEE_Attribute attr;
	TEE_InitRefAttribute(&attr, TEE_ATTR_SECRET_VALUE, key, sess->key_size);

	rc = TEE_PopulateTransientObject(sess->key_handle, &attr, 1);
	if (rc != TEE_SUCCESS) {
		EMSG("TEE_PopulateTransientObject failed, %x", rc);
        goto err;
	}

	IMSG("Set sess->op_handle");
	rc = TEE_SetOperationKey(sess->op_handle, sess->key_handle);
	if (rc != TEE_SUCCESS) {
		EMSG("TEE_SetOperationKey failed %x", rc);
        goto err;
	}
err:
	/*
	if (sess->op_handle != TEE_HANDLE_NULL)
		TEE_FreeOperation(sess->op_handle);
	sess->op_handle = TEE_HANDLE_NULL;

	if (sess->key_handle != TEE_HANDLE_NULL)
		TEE_FreeTransientObject(sess->key_handle);
	sess->key_handle = TEE_HANDLE_NULL;
	*/
	return rc;
}

static TEE_Result do_operation(uint32_t op_type, const char *key, const char *iv,
                               void *input, size_t input_size,
                               void *output, size_t *output_size) {
	/* execute the AES crypto operation decrypt or encrypt depending on the
	parameter `op_type` */
	TEE_Result rc = TEE_ERROR_GENERIC;
    const char *operation;
	if (op_type == TEE_MODE_DECRYPT) {
		operation = "decrypt";
	} else if (op_type == TEE_MODE_ENCRYPT) {
		operation = "encrypt";
	} else {
		EMSG("operation type %d is undefined", op_type);
		return rc;
	}
    struct aes_cipher cipher = {
        TEE_ALG_AES_CTR,
        op_type,
        AES256_KEY_BYTE_SIZE,
        TEE_HANDLE_NULL,
        TEE_HANDLE_NULL
    };
    struct aes_cipher *sess = &cipher;

    // prepare cipher object
    rc = prepare_operation(sess, key);
    
    if (rc != TEE_SUCCESS) {
        EMSG("Prepare %s session failed %x", operation, rc);
        goto err;
    }
    
    // initialize cipher state
    TEE_CipherInit(sess->op_handle, (void*)iv, AES_BLOCK_SIZE);

    // do cipher
    rc = TEE_CipherUpdate(sess->op_handle,
				          input, input_size,
				          output, output_size);
    
    if (rc != TEE_SUCCESS) {
        EMSG("Execute %s operation failed %x", operation, rc);
		if (rc == TEE_ERROR_SHORT_BUFFER)
			EMSG("Needed %d bytes\n", *output_size);
        goto err;
    }
err:
	if (sess->op_handle != TEE_HANDLE_NULL)
		TEE_FreeOperation(sess->op_handle);
	sess->op_handle = TEE_HANDLE_NULL;

	if (sess->key_handle != TEE_HANDLE_NULL)
		TEE_FreeTransientObject(sess->key_handle);
	sess->key_handle = TEE_HANDLE_NULL;
    return rc;
}

TEE_Result encrypt(const char *key, const char *iv,
                   void *input, size_t input_size,
                   void *output, size_t *output_size) {
	/*
	encrypt plaintext `input` and write to ciphertext `output`
	- `key`: AES key for the encryption operation
	- `iv`: initialization vector (nonce) for the encryption operation
	- `input`: pointer to the input buffer
	- `input_size`: size of input buffer
	- `output`: pointer to the output buffer
	- `output_size`: pointer to the variable holding size of output buffer,
	  it will be changed by the following function call to reflect the size
	  of ciphertext `output`
	input_size == *output_size should always hold after calling this function
	*/
    return do_operation(TEE_MODE_ENCRYPT, key, iv, input, input_size, output, output_size);
}

TEE_Result decrypt(const char *key, const char *iv,
                   void *input, size_t input_size,
                   void *output, size_t *output_size) {
	/*
	decrypt ciphertext `input` and write to plaintext `output`
	- `key`: AES key for the decryption operation
	- `iv`: initialization vector (nonce) for the decryption operation
	- `input`: pointer to the input buffer
	- `input_size`: size of input buffer
	- `output`: pointer to the output buffer
	- `output_size`: pointer to the variable holding size of output buffer,
	  it will be changed by the following function call to reflect the size
	  of plaintext `output`
	input_size == *output_size should always hold after calling this function
	*/
    return do_operation(TEE_MODE_DECRYPT, key, iv, input, input_size, output, output_size);
}