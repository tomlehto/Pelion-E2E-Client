// ----------------------------------------------------------------------------
// Copyright 2016-2017 ARM Ltd.
//  
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//  
//     http://www.apache.org/licenses/LICENSE-2.0
//  
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
// ----------------------------------------------------------------------------

#include "pv_error_handling.h"
#include "cs_der_keys_and_csrs.h"
#include "cs_der_certs.h"
#include "pal.h"
#include "cs_utils.h"
#include "cs_hash.h"
#include "pk.h"
#include "kcm_internal.h"
#include "fcc_malloc.h"



/*! Frees key handle.
*    @param[in] grp                       curve handle
*    @param[in] key_handle                   key handle.
*    @void
*/
static kcm_status_e cs_free_pal_key_handle(palCurveHandle_t *grp, palECKeyHandle_t *key_handle)
{
    //Free curve handler
    (void)pal_ECGroupFree(grp);
    //Free key handler
    (void)pal_ECKeyFree(key_handle);

    SA_PV_ERR_RECOVERABLE_RETURN_IF((*grp != NULLPTR || *key_handle != NULLPTR), KCM_STATUS_ERROR, "Free handle failed ");

    return KCM_STATUS_SUCCESS;
}


/*! Creates and initializes key handle according to passed parameters.
*    @param[in] key_data                 pointer to  key buffer.
*    @param[in] key_data_size            size of key buffer.
*    @param[in] key_type                 pal key type(public or private)
*    @param[in/out] grp                  curve handle
*    @param[in/out] key_handle           key handle.
*    @returns
*        KCM_STATUS_SUCCESS in case of success or one of the `::kcm_status_e` errors otherwise.
*/

static kcm_status_e cs_init_and_set_pal_key_handle(const uint8_t *key_data, size_t key_data_size, palKeyToCheck_t key_type, palCurveHandle_t *grp, palECKeyHandle_t *key_handle)
{
    kcm_status_e kcm_status = KCM_STATUS_SUCCESS;
    kcm_status_e kcm_free_status = KCM_STATUS_SUCCESS;
    palStatus_t pal_status = PAL_SUCCESS;
    palGroupIndex_t pal_grp_idx;

    SA_PV_ERR_RECOVERABLE_RETURN_IF((key_data == NULL), KCM_STATUS_INVALID_PARAMETER, "Invalid key pointer");
    SA_PV_ERR_RECOVERABLE_RETURN_IF((key_data_size == 0), KCM_STATUS_INVALID_PARAMETER, "Invalid key size");
    SA_PV_ERR_RECOVERABLE_RETURN_IF((key_type != PAL_CHECK_PRIVATE_KEY && key_type != PAL_CHECK_PUBLIC_KEY), KCM_STATUS_INVALID_PARAMETER, "Invalid key type");

    //Create new key handler
    pal_status = pal_ECKeyNew(key_handle);
    SA_PV_ERR_RECOVERABLE_RETURN_IF((PAL_SUCCESS != pal_status), cs_error_handler(pal_status), "pal_ECKeyNew failed ");

    if (key_type == PAL_CHECK_PRIVATE_KEY)
    {
        //Parse der private key
        pal_status = pal_parseECPrivateKeyFromDER(key_data, key_data_size, *key_handle);
        SA_PV_ERR_RECOVERABLE_GOTO_IF((PAL_SUCCESS != pal_status), kcm_status = cs_error_handler(pal_status), exit, "pal_parseECPrivateKeyFromDER failed ");
    } else {
        //Parse der public key
        pal_status = pal_parseECPublicKeyFromDER(key_data, key_data_size, *key_handle);
        SA_PV_ERR_RECOVERABLE_GOTO_IF((PAL_SUCCESS != pal_status), kcm_status = cs_error_handler(pal_status), exit, "pal_parseECPublicKeyFromDER failed ");
    }

    //retrieve key curve from key handle
    pal_status = pal_ECKeyGetCurve(*key_handle, &pal_grp_idx);
    SA_PV_ERR_RECOVERABLE_GOTO_IF((PAL_SUCCESS != pal_status), kcm_status = cs_error_handler(pal_status), exit, "pal_ECKeyGetCurve failed ");

    //Load the key curve
    pal_status = pal_ECGroupInitAndLoad(grp, pal_grp_idx);
    SA_PV_ERR_RECOVERABLE_GOTO_IF((PAL_SUCCESS != pal_status), kcm_status = cs_error_handler(pal_status), exit, "pal_ECGroupInitAndLoad failed ");

    return kcm_status;

exit:
    //Free curve handler and key handle
    kcm_free_status = cs_free_pal_key_handle(grp, key_handle);
    SA_PV_ERR_RECOVERABLE_RETURN_IF((kcm_free_status != KCM_STATUS_SUCCESS), kcm_free_status, "failed in cs_free_pal_key_handle");

    return kcm_status;
}

//For now only EC keys supported!!!
static kcm_status_e der_key_verify(const uint8_t *der_key, size_t der_key_length, palKeyToCheck_t key_type)
{
    kcm_status_e kcm_status = KCM_STATUS_SUCCESS;
    kcm_status_e kcm_free_status = KCM_STATUS_SUCCESS;
    palStatus_t pal_status = PAL_SUCCESS;
    palECKeyHandle_t key_handle = NULLPTR;
    palCurveHandle_t grp = NULLPTR;
    bool verified = false;


    SA_PV_ERR_RECOVERABLE_RETURN_IF((der_key == NULL), KCM_STATUS_INVALID_PARAMETER, "Invalid der_key pointer");
    SA_PV_ERR_RECOVERABLE_RETURN_IF((der_key_length <= 0), KCM_STATUS_INVALID_PARAMETER, "Invalid der_key_length");

    //Create new key handler
    kcm_status = cs_init_and_set_pal_key_handle(der_key, der_key_length, key_type, &grp, &key_handle);
    SA_PV_ERR_RECOVERABLE_RETURN_IF((KCM_STATUS_SUCCESS != kcm_status), kcm_status = kcm_status, "cs_init_and_set_pal_key_handle failed ");

    //Perform key verification
    pal_status = pal_ECCheckKey(grp, key_handle, key_type, &verified);
    SA_PV_ERR_RECOVERABLE_GOTO_IF(((PAL_SUCCESS != pal_status) || (verified != true)), kcm_status = cs_error_handler(pal_status), exit, "pal_ECCheckKey failed ");


exit:
    //Free curve handler and key handle
    kcm_free_status = cs_free_pal_key_handle(&grp, &key_handle);
    SA_PV_ERR_RECOVERABLE_RETURN_IF((KCM_STATUS_SUCCESS != kcm_free_status), kcm_free_status, "failed in cs_free_pal_key_handle ");

    return kcm_status;
}

static kcm_status_e cs_key_pair_generate_int(palECKeyHandle_t key_handle, kcm_crypto_key_scheme_e curve_name, uint8_t *priv_key_out, size_t priv_key_max_size,
    size_t *priv_key_act_size_out, uint8_t *pub_key_out, size_t pub_key_max_size, size_t *pub_key_act_size_out)
{
    palStatus_t pal_status = PAL_SUCCESS;
    palGroupIndex_t pal_group_id;

    SA_PV_ERR_RECOVERABLE_RETURN_IF((priv_key_out == NULL), KCM_STATUS_INVALID_PARAMETER, "Invalid out private key buffer");
    SA_PV_ERR_RECOVERABLE_RETURN_IF((priv_key_max_size == 0), KCM_STATUS_INVALID_PARAMETER, "Invalid max private key size");
    SA_PV_ERR_RECOVERABLE_RETURN_IF((priv_key_act_size_out == NULL), KCM_STATUS_INVALID_PARAMETER, "Invalid out private key size");
    if (pub_key_out != NULL) {
        SA_PV_ERR_RECOVERABLE_RETURN_IF((pub_key_max_size == 0), KCM_STATUS_INVALID_PARAMETER, "Invalid max public key size");
        SA_PV_ERR_RECOVERABLE_RETURN_IF((pub_key_act_size_out == NULL), KCM_STATUS_INVALID_PARAMETER, "Invalid out public key size");
    }

    // convert curve_name to pal_group_id
    switch (curve_name) {
    case KCM_SCHEME_EC_SECP256R1:
        pal_group_id = PAL_ECP_DP_SECP256R1;
        break;
    default:
        SA_PV_ERR_RECOVERABLE_RETURN_IF(true, KCM_CRYPTO_STATUS_UNSUPPORTED_CURVE, "unsupported curve name");
    }

    // Generate keys
    pal_status = pal_ECKeyGenerateKey(pal_group_id, key_handle);
    SA_PV_ERR_RECOVERABLE_RETURN_IF((PAL_SUCCESS != pal_status), cs_error_handler(pal_status), "Failed to generate keys");

    // Save private key to out buffer
    pal_status = pal_writePrivateKeyToDer(key_handle, priv_key_out, priv_key_max_size, priv_key_act_size_out);
    SA_PV_ERR_RECOVERABLE_RETURN_IF((PAL_SUCCESS != pal_status), cs_error_handler(pal_status), "Failed to write private key to out buffer");

    if (pub_key_out != NULL) {
        // Save public key to out buffer
        pal_status = pal_writePublicKeyToDer(key_handle, pub_key_out, pub_key_max_size, pub_key_act_size_out);
        SA_PV_ERR_RECOVERABLE_RETURN_IF((PAL_SUCCESS != pal_status), cs_error_handler(pal_status), "Failed to write public key to out buffer");
    }

    return KCM_STATUS_SUCCESS;
}
//For now only EC SECP256R keys supported!!!
kcm_status_e cs_get_pub_raw_key_from_der(const uint8_t *der_key, size_t der_key_length, uint8_t *raw_key_data_out, size_t raw_key_data_max_size, size_t *raw_key_data_act_size_out)
{
    kcm_status_e kcm_status = KCM_STATUS_SUCCESS;
    palStatus_t pal_status = PAL_SUCCESS;
    int mbdtls_result = 0;
    palECKeyHandle_t key_handle = NULLPTR;
    mbedtls_pk_context* localECKey;
    mbedtls_ecp_keypair *ecp_key_pair;

    SA_PV_ERR_RECOVERABLE_RETURN_IF((der_key == NULL), KCM_STATUS_INVALID_PARAMETER, "Invalid der_key pointer");
    SA_PV_ERR_RECOVERABLE_RETURN_IF((der_key_length != KCM_EC_SECP256R1_MAX_PUB_KEY_DER_SIZE), KCM_STATUS_INVALID_PARAMETER, "Invalid der_key_length");
    SA_PV_ERR_RECOVERABLE_RETURN_IF((raw_key_data_out == NULL), KCM_STATUS_INVALID_PARAMETER, "Invalid raw_key_data_out");
    SA_PV_ERR_RECOVERABLE_RETURN_IF((raw_key_data_act_size_out == NULL), KCM_STATUS_INVALID_PARAMETER, "Invalid raw_key_data_act_size_out pointer");
    SA_PV_ERR_RECOVERABLE_RETURN_IF((raw_key_data_max_size < KCM_EC_SECP256R1_MAX_PUB_KEY_RAW_SIZE), KCM_STATUS_INVALID_PARAMETER, "Invalid raw_key_size_out value");

    //Create new key handler
    pal_status = pal_ECKeyNew(&key_handle);
    SA_PV_ERR_RECOVERABLE_RETURN_IF((PAL_SUCCESS != pal_status), cs_error_handler(pal_status), "pal_ECKeyNew failed ");

    pal_status = pal_parseECPublicKeyFromDER(der_key, der_key_length, key_handle);
    SA_PV_ERR_RECOVERABLE_GOTO_IF((PAL_SUCCESS != pal_status), kcm_status = cs_error_handler(pal_status), exit, "pal_parseECPublicKeyFromDER failed ");

    localECKey = (mbedtls_pk_context*)key_handle;
    ecp_key_pair = (mbedtls_ecp_keypair*)localECKey->pk_ctx;

    //Get raw public key data
    mbdtls_result = mbedtls_ecp_point_write_binary(&ecp_key_pair->grp, &ecp_key_pair->Q, MBEDTLS_ECP_PF_UNCOMPRESSED, raw_key_data_act_size_out, raw_key_data_out, raw_key_data_max_size);
    SA_PV_ERR_RECOVERABLE_GOTO_IF((mbdtls_result != 0), kcm_status = KCM_CRYPTO_STATUS_INVALID_PK_PUBKEY, exit, "mbedtls_ecp_point_write_binary failed ");
    SA_PV_ERR_RECOVERABLE_GOTO_IF((raw_key_data_max_size != KCM_EC_SECP256R1_MAX_PUB_KEY_RAW_SIZE), kcm_status = KCM_CRYPTO_STATUS_INVALID_PK_PUBKEY, exit, "Wrong raw_key_data_max_size ");

exit:
    //Free key handler
    (void)pal_ECKeyFree(&key_handle);
    return kcm_status;
}


kcm_status_e cs_der_priv_key_verify(const uint8_t *key, size_t key_length)
{
    kcm_status_e kcm_status = KCM_STATUS_SUCCESS;

    kcm_status = der_key_verify(key, key_length, PAL_CHECK_PRIVATE_KEY);
    SA_PV_ERR_RECOVERABLE_RETURN_IF((kcm_status != KCM_STATUS_SUCCESS), kcm_status, "Private key verification failed");

    return kcm_status;
}

kcm_status_e cs_der_public_key_verify(const uint8_t *der_key, size_t der_key_length)
{
    kcm_status_e kcm_status = KCM_STATUS_SUCCESS;

    kcm_status = der_key_verify(der_key, der_key_length, PAL_CHECK_PUBLIC_KEY);
    SA_PV_ERR_RECOVERABLE_RETURN_IF((kcm_status != KCM_STATUS_SUCCESS), kcm_status, "Public key verification failed");

    return kcm_status;
}


kcm_status_e cs_ecdsa_verify(const uint8_t *der_pub_key, size_t der_pub_key_len, const uint8_t *hash_dgst, size_t hash_dgst_len,const uint8_t *sign, size_t  signature_size)
{
    kcm_status_e kcm_status = KCM_STATUS_SUCCESS;
    kcm_status_e kcm_free_status = KCM_STATUS_SUCCESS;
    palStatus_t pal_status = PAL_SUCCESS;
    palECKeyHandle_t key_handle = NULLPTR;
    palCurveHandle_t grp = NULLPTR;
    bool is_sign_verified = false;

    SA_PV_ERR_RECOVERABLE_RETURN_IF((der_pub_key == NULL), KCM_STATUS_INVALID_PARAMETER, "Invalid public key pointer");
    SA_PV_ERR_RECOVERABLE_RETURN_IF((der_pub_key_len <= 0), KCM_STATUS_INVALID_PARAMETER, "Invalid public key length");
    SA_PV_ERR_RECOVERABLE_RETURN_IF((hash_dgst == NULL), KCM_STATUS_INVALID_PARAMETER, "Invalid hash digest pointer");
    SA_PV_ERR_RECOVERABLE_RETURN_IF((hash_dgst_len != CS_SHA256_SIZE), KCM_STATUS_INVALID_PARAMETER, "Invalid hash digest size");
    SA_PV_ERR_RECOVERABLE_RETURN_IF((sign == NULL), KCM_STATUS_INVALID_PARAMETER, "Invalid signature pointer");
    SA_PV_ERR_RECOVERABLE_RETURN_IF((signature_size == 0), KCM_STATUS_INVALID_PARAMETER, "Invalid signature size");

    //Create public key pal handle
    kcm_status = cs_init_and_set_pal_key_handle(der_pub_key, der_pub_key_len, PAL_CHECK_PUBLIC_KEY, &grp, &key_handle);
    SA_PV_ERR_RECOVERABLE_RETURN_IF((KCM_STATUS_SUCCESS != kcm_status), kcm_status = kcm_status, "cs_init_and_set_pal_key_handle failed ");

    //Verify the signature
    pal_status = pal_ECDSAVerify(key_handle, (unsigned char*)hash_dgst, (uint32_t)hash_dgst_len, (unsigned char*)sign, signature_size, &is_sign_verified);
    SA_PV_ERR_RECOVERABLE_GOTO_IF((PAL_SUCCESS != pal_status), kcm_status = cs_error_handler(pal_status), exit, "pal_ECDSAVerify failed ");
    SA_PV_ERR_RECOVERABLE_GOTO_IF((is_sign_verified != true), kcm_status = KCM_CRYPTO_STATUS_VERIFY_SIGNATURE_FAILED, exit, "pal_ECDSAVerify failed to verify signature ");

exit:
    //Free curve handler and key handle
    kcm_free_status = cs_free_pal_key_handle(&grp, &key_handle);
    SA_PV_ERR_RECOVERABLE_RETURN_IF((kcm_free_status!= KCM_STATUS_SUCCESS), kcm_free_status, "failed in cs_free_pal_key_handle");

    return kcm_status;

}

kcm_status_e cs_ecdsa_sign(const uint8_t *der_priv_key, size_t der_priv_key_length, const uint8_t *hash_dgst, size_t hash_dgst_len, uint8_t *out_sign, size_t  signature_data_max_size, size_t * signature_data_act_size_out)
{
    kcm_status_e kcm_status = KCM_STATUS_SUCCESS;
    kcm_status_e kcm_free_status = KCM_STATUS_SUCCESS;
    palStatus_t pal_status = PAL_SUCCESS;
    palECKeyHandle_t key_handle = NULLPTR;
    palCurveHandle_t grp = NULLPTR;
    palMDType_t md_type = PAL_SHA256;

    SA_PV_ERR_RECOVERABLE_RETURN_IF((der_priv_key == NULL), KCM_STATUS_INVALID_PARAMETER, "Invalid private key pointer");
    SA_PV_ERR_RECOVERABLE_RETURN_IF((der_priv_key_length <= 0), KCM_STATUS_INVALID_PARAMETER, "Invalid private key length");
    SA_PV_ERR_RECOVERABLE_RETURN_IF((hash_dgst == NULL), KCM_STATUS_INVALID_PARAMETER, "Invalid hash digest pointer");
    SA_PV_ERR_RECOVERABLE_RETURN_IF((hash_dgst_len != CS_SHA256_SIZE), KCM_STATUS_INVALID_PARAMETER, "Invalid hash digest size");
    SA_PV_ERR_RECOVERABLE_RETURN_IF((out_sign == NULL), KCM_STATUS_INVALID_PARAMETER, "Invalid out signature pointer");
    SA_PV_ERR_RECOVERABLE_RETURN_IF((signature_data_act_size_out == NULL), KCM_STATUS_INVALID_PARAMETER, "Invalid signature_data_act_size_out pointer");
    SA_PV_ERR_RECOVERABLE_RETURN_IF((signature_data_max_size < KCM_ECDSA_SECP256R1_MAX_SIGNATURE_SIZE_IN_BYTES), KCM_STATUS_INVALID_PARAMETER, "Invalid size of signature buffer");

    //Create new key handler
    kcm_status = cs_init_and_set_pal_key_handle(der_priv_key, der_priv_key_length, PAL_CHECK_PRIVATE_KEY, &grp, &key_handle);
    SA_PV_ERR_RECOVERABLE_RETURN_IF((KCM_STATUS_SUCCESS != kcm_status), kcm_status , "cs_init_and_set_pal_key_handle failed ");

    *signature_data_act_size_out = signature_data_max_size;
    //Sign on hash digest
    pal_status = pal_ECDSASign(grp, md_type, key_handle, (unsigned char*)hash_dgst, (uint32_t)hash_dgst_len, out_sign, signature_data_act_size_out);
    SA_PV_ERR_RECOVERABLE_GOTO_IF((PAL_SUCCESS != pal_status), kcm_status = cs_error_handler(pal_status), exit, "pal_ECDSASign failed ");

exit:
    //Free curve handler and key handle
    kcm_free_status = cs_free_pal_key_handle(&grp, &key_handle);
    SA_PV_ERR_RECOVERABLE_RETURN_IF((kcm_free_status != KCM_STATUS_SUCCESS), kcm_free_status, "failed in cs_free_pal_key_handle");

    return kcm_status;
}

kcm_status_e cs_verify_key_pair(const uint8_t *priv_key_data, size_t priv_key_data_size, const uint8_t *pub_key_data, size_t pub_key_data_size)
{
    kcm_status_e kcm_status = KCM_STATUS_SUCCESS;
    uint8_t out_sign[KCM_ECDSA_SECP256R1_MAX_SIGNATURE_SIZE_IN_BYTES] = { 0 };
    size_t size_of_sign = sizeof(out_sign);
    size_t act_size_of_sign = 0;
    const uint8_t hash_digest[] =
    { 0x34, 0x70, 0xCD, 0x54, 0x7B, 0x0A, 0x11, 0x5F, 0xE0, 0x5C, 0xEB, 0xBC, 0x07, 0xBA, 0x91, 0x88,
        0x27, 0x20, 0x25, 0x6B, 0xB2, 0x7A, 0x66, 0x89, 0x1A, 0x4B, 0xB7, 0x17, 0x11, 0x04, 0x86, 0x6F };

    SA_PV_ERR_RECOVERABLE_RETURN_IF((priv_key_data == NULL), KCM_STATUS_INVALID_PARAMETER, "Invalid priv_key_data pointer");
    SA_PV_ERR_RECOVERABLE_RETURN_IF((priv_key_data_size <= 0), KCM_STATUS_INVALID_PARAMETER, "Invalid private key length");
    SA_PV_ERR_RECOVERABLE_RETURN_IF((pub_key_data == NULL), KCM_STATUS_INVALID_PARAMETER, "Invalid pub_key_data pointer");
    SA_PV_ERR_RECOVERABLE_RETURN_IF((pub_key_data_size <= 0), KCM_STATUS_INVALID_PARAMETER, "Invalid pub_key length");

    //Sign on hash using private key
    kcm_status = cs_ecdsa_sign(priv_key_data, priv_key_data_size, hash_digest, sizeof(hash_digest), out_sign, size_of_sign, &act_size_of_sign);
    SA_PV_ERR_RECOVERABLE_RETURN_IF((kcm_status != KCM_STATUS_SUCCESS), kcm_status, "cs_ecdsa_sign failed");

    //Verify the signature with public key
    kcm_status = cs_ecdsa_verify(pub_key_data, pub_key_data_size, hash_digest, sizeof(hash_digest),(const uint8_t*)out_sign, act_size_of_sign);
    SA_PV_ERR_RECOVERABLE_RETURN_IF((kcm_status != KCM_STATUS_SUCCESS), kcm_status, "cs_ecdsa_sign failed");

    return kcm_status;

}

kcm_status_e cs_key_pair_generate(kcm_crypto_key_scheme_e curve_name, uint8_t *priv_key_out, size_t priv_key_max_size, size_t *priv_key_act_size_out, uint8_t *pub_key_out,
                                  size_t pub_key_max_size, size_t *pub_key_act_size_out)
{
    kcm_status_e kcm_status = KCM_STATUS_SUCCESS;
    palStatus_t pal_status = PAL_SUCCESS;
    palECKeyHandle_t key_handle = NULLPTR;

    // Create new key handler
    pal_status = pal_ECKeyNew(&key_handle);
    SA_PV_ERR_RECOVERABLE_RETURN_IF((PAL_SUCCESS != pal_status), cs_error_handler(pal_status), "pal_ECKeyNew failed");

    // Call to internal key_pair_generate
    kcm_status = cs_key_pair_generate_int(key_handle, curve_name, priv_key_out, priv_key_max_size, priv_key_act_size_out,
                                          pub_key_out, pub_key_max_size, pub_key_act_size_out);
    SA_PV_ERR_RECOVERABLE_GOTO_IF((kcm_status != KCM_STATUS_SUCCESS), kcm_status = kcm_status, exit, "Failed to generate keys");

exit:
    //Free key handler
    if (key_handle != NULLPTR) {
        pal_ECKeyFree(&key_handle);
        SA_PV_ERR_RECOVERABLE_RETURN_IF((key_handle != NULLPTR && kcm_status == KCM_STATUS_SUCCESS), KCM_STATUS_ERROR, "Free key handle failed ");
    }
    return kcm_status;
}

static kcm_status_e cs_csr_generate_int(palECKeyHandle_t key_handle, const kcm_csr_params_s *csr_params,
                                        uint8_t *csr_buff_out, size_t csr_buff_max_size, size_t *csr_buff_act_size_out)
{
    kcm_status_e kcm_status = KCM_STATUS_SUCCESS;
    palStatus_t pal_status = PAL_SUCCESS;
    palx509CSRHandle_t x509CSR_handle = NULLPTR;
    palMDType_t pal_md_type;
    uint32_t pal_key_usage = 0;
    uint32_t pal_ext_key_usage = 0;
    uint32_t eku_all_bits = KCM_CSR_EXT_KU_ANY | KCM_CSR_EXT_KU_SERVER_AUTH | KCM_CSR_EXT_KU_CLIENT_AUTH |
        KCM_CSR_EXT_KU_CODE_SIGNING | KCM_CSR_EXT_KU_EMAIL_PROTECTION | KCM_CSR_EXT_KU_TIME_STAMPING | KCM_CSR_EXT_KU_OCSP_SIGNING;

    SA_PV_ERR_RECOVERABLE_RETURN_IF((csr_params == NULL), KCM_STATUS_INVALID_PARAMETER, "Invalid csr_params pointer");
    SA_PV_ERR_RECOVERABLE_RETURN_IF((csr_params->subject == NULL), KCM_STATUS_INVALID_PARAMETER, "Invalid subject pointer in csr_params");
    SA_PV_ERR_RECOVERABLE_RETURN_IF((csr_buff_out == NULL), KCM_STATUS_INVALID_PARAMETER, "Invalid out csr buffer");
    SA_PV_ERR_RECOVERABLE_RETURN_IF((csr_buff_max_size == 0), KCM_STATUS_INVALID_PARAMETER, "Invalid max csr buffer size");
    SA_PV_ERR_RECOVERABLE_RETURN_IF((csr_buff_act_size_out == NULL), KCM_STATUS_INVALID_PARAMETER, "Invalid out csr buffer size");
    SA_PV_ERR_RECOVERABLE_RETURN_IF((csr_params->ext_key_usage & (~eku_all_bits)), KCM_STATUS_INVALID_PARAMETER, "Invalid extended key usage options");

    // Initialize x509 CSR handle 
    pal_status = pal_x509CSRInit(&x509CSR_handle);
    SA_PV_ERR_RECOVERABLE_RETURN_IF((PAL_SUCCESS != pal_status), cs_error_handler(pal_status), "Failed to initialize x509 CSR handle");

    // Set CSR Subject
    pal_status = pal_x509CSRSetSubject(x509CSR_handle, csr_params->subject);
    SA_PV_ERR_RECOVERABLE_GOTO_IF((PAL_SUCCESS != pal_status), kcm_status = cs_error_handler(pal_status), exit, "Failed to set CSR Subject");

    // Set MD algorithm to SHA256 for the signature
    switch (csr_params->md_type) {
        case KCM_MD_SHA256:
            pal_md_type = PAL_SHA256;
            break;
        default:
            SA_PV_ERR_RECOVERABLE_GOTO_IF(true, kcm_status = KCM_CRYPTO_STATUS_INVALID_MD_TYPE, exit, "MD type not supported");
    }
    pal_status = pal_x509CSRSetMD(x509CSR_handle, pal_md_type);
    SA_PV_ERR_RECOVERABLE_GOTO_IF((PAL_SUCCESS != pal_status), kcm_status = cs_error_handler(pal_status), exit, "Failed to set MD algorithm");

    // Set keys into CSR
    pal_status = pal_x509CSRSetKey(x509CSR_handle, key_handle, NULLPTR);
    SA_PV_ERR_RECOVERABLE_GOTO_IF((PAL_SUCCESS != pal_status), kcm_status = cs_error_handler(pal_status), exit, "Failed to Set keys into CSR");

    // Set CSR key usage
    if (csr_params->key_usage != KCM_CSR_KU_NONE) {
        if (csr_params->key_usage & KCM_CSR_KU_DIGITAL_SIGNATURE) {
            pal_key_usage |= PAL_X509_KU_DIGITAL_SIGNATURE;
        }
        if (csr_params->key_usage & KCM_CSR_KU_NON_REPUDIATION) {
            pal_key_usage |= PAL_X509_KU_NON_REPUDIATION;
        }
        if (csr_params->key_usage & KCM_CSR_KU_KEY_CERT_SIGN) {
            pal_key_usage |= PAL_X509_KU_KEY_CERT_SIGN;
        }
        if (csr_params->key_usage & KCM_CSR_KU_KEY_AGREEMENT) {
            pal_key_usage |= PAL_X509_KU_KEY_AGREEMENT;
        }
        pal_status = pal_x509CSRSetKeyUsage(x509CSR_handle, pal_key_usage);
        SA_PV_ERR_RECOVERABLE_GOTO_IF((PAL_SUCCESS != pal_status), kcm_status = cs_error_handler(pal_status), exit, "Failed to set CSR key usage");
    }

    // Set CSR extended key usage
    if (csr_params->ext_key_usage != KCM_CSR_EXT_KU_NONE) {
        if (csr_params->ext_key_usage & KCM_CSR_EXT_KU_ANY) {
            pal_ext_key_usage |= PAL_X509_EXT_KU_ANY;
        }
        if (csr_params->ext_key_usage & KCM_CSR_EXT_KU_SERVER_AUTH) {
            pal_ext_key_usage |= PAL_X509_EXT_KU_SERVER_AUTH;
        }
        if (csr_params->ext_key_usage & KCM_CSR_EXT_KU_CLIENT_AUTH) {
            pal_ext_key_usage |= PAL_X509_EXT_KU_CLIENT_AUTH;
        }
        if (csr_params->ext_key_usage & KCM_CSR_EXT_KU_CODE_SIGNING) {
            pal_ext_key_usage |= PAL_X509_EXT_KU_CODE_SIGNING;
        }
        if (csr_params->ext_key_usage & KCM_CSR_EXT_KU_EMAIL_PROTECTION) {
            pal_ext_key_usage |= PAL_X509_EXT_KU_EMAIL_PROTECTION;
        }
        if (csr_params->ext_key_usage & KCM_CSR_EXT_KU_TIME_STAMPING) {
            pal_ext_key_usage |= PAL_X509_EXT_KU_TIME_STAMPING;
        }
        if (csr_params->ext_key_usage & KCM_CSR_EXT_KU_OCSP_SIGNING) {
            pal_ext_key_usage |= PAL_X509_EXT_KU_OCSP_SIGNING;
        }
        pal_status = pal_x509CSRSetExtendedKeyUsage(x509CSR_handle, pal_ext_key_usage);
        SA_PV_ERR_RECOVERABLE_GOTO_IF((PAL_SUCCESS != pal_status), kcm_status = cs_error_handler(pal_status), exit, "Failed to set CSR extended key usage");
    }

    // Write the CSR to out buffer in DER format
    pal_status = pal_x509CSRWriteDER(x509CSR_handle, csr_buff_out, csr_buff_max_size, csr_buff_act_size_out);
    SA_PV_ERR_RECOVERABLE_GOTO_IF((PAL_SUCCESS != pal_status), kcm_status = cs_error_handler(pal_status), exit, "Failed to write the CSR to out buffer");

exit:
    //Free CSR handler
    if (x509CSR_handle != NULLPTR) {
        pal_x509CSRFree(&x509CSR_handle);
        SA_PV_ERR_RECOVERABLE_RETURN_IF((x509CSR_handle != NULLPTR && kcm_status == KCM_STATUS_SUCCESS), KCM_STATUS_ERROR, "Free CSR handle failed ");
    }

    return kcm_status;
}

kcm_status_e cs_csr_generate(const uint8_t *priv_key, size_t priv_key_size, const kcm_csr_params_s *csr_params, uint8_t *csr_buff_out,
                             size_t csr_buff_max_size, size_t *csr_buff_act_size_out)
{
    kcm_status_e kcm_status = KCM_STATUS_SUCCESS;
    palStatus_t pal_status = PAL_SUCCESS;
    palECKeyHandle_t key_handle = NULLPTR;

    SA_PV_ERR_RECOVERABLE_RETURN_IF((priv_key == NULL), KCM_STATUS_INVALID_PARAMETER, "Invalid private key pointer");
    SA_PV_ERR_RECOVERABLE_RETURN_IF((priv_key_size == 0), KCM_STATUS_INVALID_PARAMETER, "Invalid private key size");

    // Create new key handler
    pal_status = pal_ECKeyNew(&key_handle);
    SA_PV_ERR_RECOVERABLE_RETURN_IF((PAL_SUCCESS != pal_status), cs_error_handler(pal_status), "pal_ECKeyNew failed");

    // Parse private key from DER format
    pal_status = pal_parseECPrivateKeyFromDER(priv_key, priv_key_size, key_handle);
    SA_PV_ERR_RECOVERABLE_GOTO_IF((PAL_SUCCESS != pal_status), kcm_status = cs_error_handler(pal_status), exit, "Failed to parse private key from DER format");

    // Call to internal csr_generate
    kcm_status = cs_csr_generate_int(key_handle, csr_params, csr_buff_out, csr_buff_max_size, csr_buff_act_size_out);
    SA_PV_ERR_RECOVERABLE_GOTO_IF((kcm_status != KCM_STATUS_SUCCESS), kcm_status = kcm_status, exit, "Failed to generate csr");

exit:
    //Free key handler
    if (key_handle != NULLPTR) {
        pal_ECKeyFree(&key_handle);
        SA_PV_ERR_RECOVERABLE_RETURN_IF((key_handle != NULLPTR && kcm_status == KCM_STATUS_SUCCESS), KCM_STATUS_ERROR, "Free key handle failed ");
    }
    return kcm_status;
}
kcm_status_e cs_generate_keys_and_csr(kcm_crypto_key_scheme_e curve_name, const kcm_csr_params_s *csr_params, uint8_t *priv_key_out,
                                      size_t priv_key_max_size, size_t *priv_key_act_size_out, uint8_t *pub_key_out,
                                      size_t pub_key_max_size, size_t *pub_key_act_size_out, uint8_t *csr_buff_out,
                                      const size_t csr_buff_max_size, size_t *csr_buff_act_size_out)
{
    kcm_status_e kcm_status = KCM_STATUS_SUCCESS;
    palStatus_t pal_status = PAL_SUCCESS;
    palECKeyHandle_t key_handle = NULLPTR;

    // Create new key handler
    pal_status = pal_ECKeyNew(&key_handle);
    SA_PV_ERR_RECOVERABLE_RETURN_IF((PAL_SUCCESS != pal_status), cs_error_handler(pal_status), "pal_ECKeyNew failed");

    // Call to internal key_pair_generate
    kcm_status = cs_key_pair_generate_int(key_handle, curve_name, priv_key_out, priv_key_max_size, priv_key_act_size_out, pub_key_out, pub_key_max_size, pub_key_act_size_out);
    SA_PV_ERR_RECOVERABLE_GOTO_IF((kcm_status != KCM_STATUS_SUCCESS), kcm_status = kcm_status, exit, "Failed to generate keys");

    // Call to internal csr_generate
    kcm_status = cs_csr_generate_int(key_handle, csr_params, csr_buff_out, csr_buff_max_size, csr_buff_act_size_out);
    SA_PV_ERR_RECOVERABLE_GOTO_IF((kcm_status != KCM_STATUS_SUCCESS), kcm_status = kcm_status, exit, "Failed to generate csr");

exit:
    //Free key handler
    if (key_handle != NULLPTR) {
        pal_ECKeyFree(&key_handle);
        SA_PV_ERR_RECOVERABLE_RETURN_IF((key_handle != NULLPTR && kcm_status == KCM_STATUS_SUCCESS), KCM_STATUS_ERROR, "Free key handle failed ");
    }
    return kcm_status;
}


kcm_status_e cs_verify_items_correlation(cs_key_handle_t crypto_handle, const uint8_t *certificate_data, size_t certificate_data_len)
{
    kcm_status_e kcm_status = KCM_STATUS_SUCCESS;
    kcm_status_e kcm_free_status = KCM_STATUS_SUCCESS;
    cs_ec_key_context_s *cs_ec_key_handle = NULL;
    palX509Handle_t x509_cert = NULLPTR;


    //Check parameters
    SA_PV_ERR_RECOVERABLE_RETURN_IF(((cs_ec_key_context_s *)crypto_handle == NULL), KCM_STATUS_INVALID_PARAMETER, "Invalid crypto_handle");
    SA_PV_ERR_RECOVERABLE_RETURN_IF((certificate_data == NULL), KCM_STATUS_INVALID_PARAMETER, "Invalid certificate_data");
    SA_PV_ERR_RECOVERABLE_RETURN_IF((certificate_data_len == 0), KCM_STATUS_INVALID_PARAMETER, "Invalid certificate_data_len");
    cs_ec_key_handle = (cs_ec_key_context_s*)crypto_handle;


    //Create certificate handle
    kcm_status = cs_create_handle_from_der_x509_cert(certificate_data, certificate_data_len, &x509_cert);
    SA_PV_ERR_RECOVERABLE_RETURN_IF((KCM_STATUS_SUCCESS != kcm_status), kcm_status, "cs_create_handle_from_der_x509_cert failed");


    //Check certificate and private key correlation
    kcm_status = cs_check_certifcate_public_key(x509_cert, cs_ec_key_handle->priv_key, cs_ec_key_handle->priv_key_size);
    SA_PV_ERR_RECOVERABLE_GOTO_IF((kcm_status != KCM_STATUS_SUCCESS), kcm_status = kcm_status, exit, "cs_check_certifcate_public_key failed");

exit:

    kcm_free_status = cs_close_handle_x509_cert(&x509_cert);
    SA_PV_ERR_RECOVERABLE_RETURN_IF((KCM_STATUS_SUCCESS != kcm_free_status), kcm_free_status, "cs_close_handle_x509_cert failed");

    return kcm_status;
}

kcm_status_e cs_generate_keys_and_create_csr_from_certificate(const uint8_t *certificate,
                                                              size_t certificate_size,
                                                              cs_key_handle_t csr_key_h,
                                                              uint8_t *csr_buff_out,
                                                              const size_t csr_buff_max_size,
                                                              size_t *csr_buff_act_size_out)
{
    kcm_status_e kcm_status = KCM_STATUS_SUCCESS;
    palStatus_t pal_status = PAL_SUCCESS;

    palECKeyHandle_t pal_ec_key_handle = NULLPTR;
    palx509CSRHandle_t pal_csr_handle = NULLPTR;
    palX509Handle_t pal_crt_handle = NULLPTR;
    cs_ec_key_context_s *ec_key_ctx = NULL;

    // Get the key context from the handle
    ec_key_ctx = (cs_ec_key_context_s *)(csr_key_h);

    // Create new key handle
    pal_status = pal_ECKeyNew(&pal_ec_key_handle);
    SA_PV_ERR_RECOVERABLE_GOTO_IF((PAL_SUCCESS != pal_status), kcm_status = cs_error_handler(pal_status), exit, "pal_ECKeyNew failed");

    // Call to internal key_pair_generate
    kcm_status = cs_key_pair_generate_int(pal_ec_key_handle, KCM_SCHEME_EC_SECP256R1,
                                          ec_key_ctx->priv_key, sizeof(ec_key_ctx->priv_key), &ec_key_ctx->priv_key_size,
                                          ec_key_ctx->pub_key, sizeof(ec_key_ctx->pub_key), &ec_key_ctx->pub_key_size);
    SA_PV_ERR_RECOVERABLE_GOTO_IF((KCM_STATUS_SUCCESS != kcm_status), (kcm_status = kcm_status), exit, "Failed to generate keys");

    // Create CRT handle
    kcm_status = cs_create_handle_from_der_x509_cert(certificate, certificate_size, &pal_crt_handle);
    SA_PV_ERR_RECOVERABLE_GOTO_IF((KCM_STATUS_SUCCESS != kcm_status), (kcm_status = kcm_status), exit, "Failed getting handle from certificate");

    // Create CSR handle
    pal_status = pal_x509CSRInit(&pal_csr_handle);
    SA_PV_ERR_RECOVERABLE_GOTO_IF((PAL_SUCCESS != pal_status), kcm_status = cs_error_handler(pal_status), exit, "Failed intializing X509 CSR object");

    // Set keys into CSR
    pal_status = pal_x509CSRSetKey(pal_csr_handle, pal_ec_key_handle, NULLPTR);
    SA_PV_ERR_RECOVERABLE_GOTO_IF((PAL_SUCCESS != pal_status), kcm_status = cs_error_handler(pal_status), exit, "Failed to Set keys into CSR");
    
    // Create CSR from the given CRT
    pal_status = pal_x509CSRFromCertWriteDER(pal_crt_handle, pal_csr_handle, csr_buff_out, csr_buff_max_size, csr_buff_act_size_out);
    SA_PV_ERR_RECOVERABLE_GOTO_IF((PAL_SUCCESS != pal_status), kcm_status = cs_error_handler(pal_status), exit, "Failed generating CSR from Certificate");

exit:
    //Free key handle
    if (pal_ec_key_handle != NULLPTR) {
        pal_status = pal_ECKeyFree(&pal_ec_key_handle);
        SA_PV_ERR_RECOVERABLE_RETURN_IF((PAL_SUCCESS != pal_status) && (kcm_status == KCM_STATUS_SUCCESS), KCM_STATUS_ERROR, "Free key handle failed");
    }
    //Free x509 CSR handle
    if (pal_csr_handle != NULLPTR) {
        pal_status = pal_x509CSRFree(&pal_csr_handle);
        SA_PV_ERR_RECOVERABLE_RETURN_IF((PAL_SUCCESS != pal_status) && (kcm_status == KCM_STATUS_SUCCESS), KCM_STATUS_ERROR, "Free x509 CSR handle failed");
    }
    //Free x509 CRT handle
    if (pal_crt_handle != NULLPTR) {
        pal_status = pal_x509Free(&pal_crt_handle);
        SA_PV_ERR_RECOVERABLE_RETURN_IF((PAL_SUCCESS != pal_status) && (kcm_status == KCM_STATUS_SUCCESS), KCM_STATUS_ERROR, "Free x509 CRT handle failed");
    }

    return kcm_status;
}

kcm_status_e cs_ec_key_new(cs_key_handle_t *key_h)
{
    cs_ec_key_context_s *ec_key_ctx = NULL;

    ec_key_ctx = (cs_ec_key_context_s*)fcc_malloc(sizeof(cs_ec_key_context_s));
    SA_PV_ERR_RECOVERABLE_RETURN_IF((ec_key_ctx == NULL), KCM_STATUS_OUT_OF_MEMORY, "Failed to allocate EC key context");

    *key_h = (cs_key_handle_t)ec_key_ctx;

    return KCM_STATUS_SUCCESS;
}

kcm_status_e cs_ec_key_free(cs_key_handle_t *key_h)
{
    cs_ec_key_context_s *ec_key_ctx = (cs_ec_key_context_s *)(*key_h);
    fcc_free(ec_key_ctx);
    *key_h = 0;
    
    return KCM_STATUS_SUCCESS;
}


