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
#ifndef USE_TINY_CBOR
#include "fcc_bundle_handler.h"
#include "cn-cbor.h"
#include "pv_error_handling.h"
#include "fcc_bundle_utils.h"
#include "key_config_manager.h"
#include "fcc_output_info_handler.h"
#include "fcc_malloc.h"
#include "fcc_time_profiling.h"
#include "fcc_utils.h"


/** Processes  certificate chain list.
* The function extracts data parameters for each certificate chain and stores it.
*
* @param cert_chains_list_cb[in]   The cbor structure with certificate chain list.
*
* @return
*     true for success, false otherwise.
*/
fcc_status_e fcc_bundle_process_certificate_chains(const cn_cbor *cert_chains_list_cb)
{
    bool status = false;
    fcc_status_e fcc_status = FCC_STATUS_SUCCESS;
    fcc_status_e output_info_fcc_status = FCC_STATUS_SUCCESS;
    kcm_status_e kcm_result = KCM_STATUS_SUCCESS;
    uint32_t cert_chain_index = 0;
    uint32_t cert_index = 0;
    cn_cbor *cert_chain_cb = NULL;
    cn_cbor *cert_cb = NULL;
    fcc_bundle_data_param_s certificate_chain;
    uint8_t *certificate_data;
    size_t certificate_data_size = 0;
    kcm_cert_chain_handle cert_chain_handle = NULL;

    SA_PV_LOG_TRACE_FUNC_ENTER_NO_ARGS();
    SA_PV_ERR_RECOVERABLE_RETURN_IF((cert_chains_list_cb == NULL), fcc_status = FCC_STATUS_INVALID_PARAMETER, "Invalid cert_chains_list_cb pointer");

    //Initialize data struct
    memset(&certificate_chain, 0, sizeof(fcc_bundle_data_param_s));

    for (cert_chain_index = 0; cert_chain_index < (uint32_t)cert_chains_list_cb->length; cert_chain_index++) {

        FCC_SET_START_TIMER(fcc_certificate_chain_timer);

        certificate_data = NULL;


        //Get certificate chain CBOR struct at index cert_chain_index
        cert_chain_cb = cn_cbor_index(cert_chains_list_cb, cert_chain_index);
        SA_PV_ERR_RECOVERABLE_RETURN_IF((cert_chain_cb == NULL), fcc_status = FCC_STATUS_BUNDLE_ERROR, "Failed to get certificate chain at index (%" PRIu32 ") ", cert_chain_index);
        SA_PV_ERR_RECOVERABLE_RETURN_IF((cert_chain_cb->type != CN_CBOR_MAP), fcc_status = FCC_STATUS_BUNDLE_ERROR, "Wrong type of certificate chain CBOR struct at index (%" PRIu32 ") ", cert_chain_index);

        status = fcc_bundle_get_data_param(cert_chain_cb, &certificate_chain);
        SA_PV_ERR_RECOVERABLE_GOTO_IF((status != true), fcc_status = FCC_STATUS_BUNDLE_ERROR, exit, "Failed to get certificate chain data at index (%" PRIu32 ") ", cert_chain_index);

        // create chain
        kcm_result = kcm_cert_chain_create(&cert_chain_handle, certificate_chain.name, certificate_chain.name_len, (size_t)certificate_chain.array_cn->length, true);
        SA_PV_ERR_RECOVERABLE_GOTO_IF((kcm_result != KCM_STATUS_SUCCESS), fcc_status = FCC_STATUS_KCM_ERROR, exit, "Failed to create certificate chain");
        
        //Get all certificates in certificate chain and store it
        for (cert_index = 0; cert_index < (uint32_t)certificate_chain.array_cn->length; cert_index++) {

            cert_cb = cn_cbor_index(certificate_chain.array_cn, (unsigned int)cert_index);
            SA_PV_ERR_RECOVERABLE_GOTO_IF((cert_cb == NULL), fcc_status = FCC_STATUS_BUNDLE_ERROR, exit, "Failed to get cert cbor at index (%" PRIu32 ") ", cert_index);

            status = get_data_buffer_from_cbor(cert_cb, &certificate_data, &certificate_data_size);
            SA_PV_ERR_RECOVERABLE_GOTO_IF((status == false || certificate_data == NULL || certificate_data_size == 0), fcc_status = FCC_STATUS_BUNDLE_ERROR, exit, "Failed to get cert data at index (%" PRIu32 ") ", cert_index);

            //If private key name was passed - current leaf certificate is self generated and we need to perform verification against given private key
            if (cert_index == 0 && certificate_chain.private_key_name != NULL) {
                //Try to retrieve the private key from the device and verify the leaf certificate against private key data
                kcm_result = kcm_certificate_verify_with_private_key(
                    certificate_data,
                    certificate_data_size,
                    certificate_chain.private_key_name,
                    certificate_chain.private_key_name_len);
                    SA_PV_ERR_RECOVERABLE_GOTO_IF((kcm_result != KCM_STATUS_SUCCESS), fcc_status = FCC_STATUS_CERTIFICATE_PUBLIC_KEY_CORRELATION_ERROR, exit, "Failed to verify leaf certificate against given private key (%" PRIu32 ") ", cert_chain_index);
            }
            kcm_result = kcm_cert_chain_add_next(cert_chain_handle, certificate_data, certificate_data_size);            
            SA_PV_ERR_RECOVERABLE_GOTO_IF((kcm_result != KCM_STATUS_SUCCESS), fcc_status = fcc_convert_kcm_to_fcc_status(kcm_result), exit, "Failed to add certificate chain at index (%" PRIu32 ") ", cert_chain_index);
        }
        // close chain
        kcm_result = kcm_cert_chain_close(cert_chain_handle);
        cert_chain_handle = NULL;
        SA_PV_ERR_RECOVERABLE_GOTO_IF((kcm_result != KCM_STATUS_SUCCESS), fcc_status = FCC_STATUS_KCM_ERROR, exit, "Failed to close certificate chain");
        FCC_END_TIMER((char*)certificate_chain.name, certificate_chain.name_len, fcc_certificate_chain_timer);
    }

exit:
    if (kcm_result != KCM_STATUS_SUCCESS) {
        if (cert_chain_handle != NULL) {
            kcm_cert_chain_close(cert_chain_handle);
        }
        //KCM_STATUS_ITEM_NOT_FOUND returned only if private key of self-generate certificate is missing.In this case we need to return name of themissing item -the private key.
        if (kcm_result == KCM_STATUS_ITEM_NOT_FOUND) {
            output_info_fcc_status = fcc_bundle_store_error_info(certificate_chain.private_key_name, certificate_chain.private_key_name_len, kcm_result);
        } else {
            output_info_fcc_status = fcc_bundle_store_error_info(certificate_chain.name, certificate_chain.name_len, kcm_result);
        }

    }
    fcc_bundle_clean_and_free_data_param(&certificate_chain);
    SA_PV_ERR_RECOVERABLE_RETURN_IF((output_info_fcc_status != FCC_STATUS_SUCCESS),
                                    fcc_status = FCC_STATUS_OUTPUT_INFO_ERROR,
                                    "Failed to create output kcm_status error %d", kcm_result);

    SA_PV_LOG_TRACE_FUNC_EXIT_NO_ARGS();

    return fcc_status;
}
#endif
