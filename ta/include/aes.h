#ifndef __AES_H__
#define __AES_H__

#include <inttypes.h>
#include <tee_internal_api.h>
#include <tee_internal_api_extensions.h>

struct aes_cipher {
	uint32_t algo;			/* AES flavour */
	uint32_t mode;			/* Encode or decode */
	uint32_t key_size;		/* AES key size in byte */
	TEE_OperationHandle op_handle;	/* AES ciphering operation */
	TEE_ObjectHandle key_handle;	/* transient object to load the key */
};

#define AES128_KEY_BIT_SIZE		128
#define AES128_KEY_BYTE_SIZE	(AES128_KEY_BIT_SIZE / 8)
#define AES256_KEY_BIT_SIZE		256
#define AES256_KEY_BYTE_SIZE	(AES256_KEY_BIT_SIZE / 8)
#define AES_BLOCK_SIZE          16

TEE_Result encrypt(const char *key, const char *iv, void *input, size_t input_size, void *output, size_t *output_size);
TEE_Result decrypt(const char *key, const char *iv, void *input, size_t input_size, void *output, size_t *output_size);

#endif