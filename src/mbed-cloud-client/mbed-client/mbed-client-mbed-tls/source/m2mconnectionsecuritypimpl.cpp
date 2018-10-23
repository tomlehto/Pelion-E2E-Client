/*
 * Copyright (c) 2015 - 2017 ARM Limited. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 * Licensed under the Apache License, Version 2.0 (the License); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an AS IS BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Needed for PRIu64 on FreeRTOS
#include <stdio.h>
// Note: this macro is needed on armcc to get the the limit macros like UINT16_MAX
#ifndef __STDC_LIMIT_MACROS
#define __STDC_LIMIT_MACROS
#endif

// Note: this macro is needed on armcc to get the the PRI*32 macros
// from inttypes.h in a C++ code.
#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif

#include "mbed-client/m2mconnectionhandler.h"
#include "mbed-client-mbedtls/m2mconnectionsecuritypimpl.h"
#include "mbed-client/m2msecurity.h"
#include "mbed-trace/mbed_trace.h"
#include "mbed-client/m2mconstants.h"
#include "pal.h"
#include "m2mdevice.h"
#include "m2minterfacefactory.h"
#include <string.h>

#define TRACE_GROUP "mClt"

M2MConnectionSecurityPimpl::M2MConnectionSecurityPimpl(M2MConnectionSecurity::SecurityMode mode)
    :_init_done(M2MConnectionSecurityPimpl::INIT_NOT_STARTED),
     _conf(0),
     _ssl(0),
     _sec_mode(mode)
{
    memset(&_entropy, 0, sizeof(entropy_cb));
    memset(&_tls_socket, 0, sizeof(palTLSSocket_t));
}

M2MConnectionSecurityPimpl::~M2MConnectionSecurityPimpl()
{
    if(_ssl) {
        pal_freeTLS(&_ssl);
    }
    if(_conf) {
        pal_tlsConfigurationFree(&_conf);
    }
}

void M2MConnectionSecurityPimpl::reset()
{
    if(_ssl) {
        pal_freeTLS(&_ssl);
    }
    if(_conf) {
        pal_tlsConfigurationFree(&_conf);
    }
    _init_done = M2MConnectionSecurityPimpl::INIT_NOT_STARTED;
}

int M2MConnectionSecurityPimpl::init(const M2MSecurity *security, uint16_t security_instance_id)
{
    tr_debug("M2MConnectionSecurityPimpl::init");

    if(!security){
        tr_error("M2MConnectionSecurityPimpl Security NULL.");
        return -1;
    }

    if(_entropy.entropy_source_ptr) {
        if(PAL_SUCCESS != pal_addEntropySource(_entropy.entropy_source_ptr)){
            return -1;
        }
    }

    palTLSTransportMode_t mode = PAL_DTLS_MODE;
    if(_sec_mode == M2MConnectionSecurity::TLS){
        mode = PAL_TLS_MODE;
    }

    if(PAL_SUCCESS != pal_initTLSConfiguration(&_conf, mode)){
        tr_error("pal_initTLSConfiguration failed");
        return -1;
    }

    _init_done = M2MConnectionSecurityPimpl::INIT_CONFIGURING;


    if(_sec_mode == M2MConnectionSecurity::DTLS){
        // PAL divides the defined MAX_TIMEOUT by 2
        pal_setHandShakeTimeOut(_conf, MBED_CLIENT_DTLS_PEER_MAX_TIMEOUT*2);
    }

    M2MSecurity::SecurityModeType cert_mode =
        (M2MSecurity::SecurityModeType)security->resource_value_int(M2MSecurity::SecurityMode, security_instance_id);

    if( cert_mode == M2MSecurity::Certificate || cert_mode == M2MSecurity::EST ){

        palX509_t owncert;
        palPrivateKey_t privateKey;
        palX509_t caChain;

        uint8_t certificate[MAX_CERTIFICATE_SIZE];
        uint8_t *certificate_ptr = (uint8_t *)&certificate;

        caChain.size = MAX_CERTIFICATE_SIZE;
        int ret_code = security->resource_value_buffer(M2MSecurity::ServerPublicKey, certificate_ptr, security_instance_id, (size_t*)&caChain.size);
        caChain.buffer = certificate_ptr;

        if (ret_code < 0 || PAL_SUCCESS != pal_setCAChain(_conf, &caChain, NULL)){
            tr_error("pal_setCAChain failed");
            return -1;
        }

        privateKey.size = MAX_CERTIFICATE_SIZE;
        ret_code = security->resource_value_buffer(M2MSecurity::Secretkey, certificate_ptr, security_instance_id, (size_t*)&privateKey.size);
        privateKey.buffer = certificate_ptr;
        if (ret_code < 0 || PAL_SUCCESS != pal_setOwnPrivateKey(_conf, &privateKey)) {
            tr_error("pal_setOwnPrivateKey failed");
            return -1;
        }

        void *dummy;

        // Open certificate chain, size parameter contains the depth of certificate chain
        size_t cert_chain_size = 0;
        security->resource_value_buffer(M2MSecurity::OpenCertificateChain, (uint8_t *&)dummy, security_instance_id, &cert_chain_size);
        tr_info("M2MConnectionSecurityPimpl::init - cert chain length: %lu", (unsigned long)cert_chain_size);

        int index = 0;
        while (index < cert_chain_size) {
            owncert.size = MAX_CERTIFICATE_SIZE;
            ret_code = security->resource_value_buffer(M2MSecurity::ReadDeviceCertificateChain, certificate_ptr, security_instance_id, (size_t*)&owncert.size);
            owncert.buffer = certificate_ptr;

            if (ret_code < 0 || PAL_SUCCESS != pal_setOwnCertChain(_conf, &owncert)){
                tr_error("pal_setOwnCertChain failed");
                security->resource_value_buffer(M2MSecurity::CloseCertificateChain, (uint8_t *&)dummy, security_instance_id, &cert_chain_size);
                return -1;
            }

            index++;
        }
        security->resource_value_buffer(M2MSecurity::CloseCertificateChain, (uint8_t *&)dummy, security_instance_id, &cert_chain_size);

    } else if (cert_mode == M2MSecurity::Psk){

        uint8_t identity[MAX_CERTIFICATE_SIZE];
        uint8_t *identity_ptr = (uint8_t *)&identity;
        uint32_t identity_len;
        uint8_t psk[MAX_CERTIFICATE_SIZE];
        uint8_t *psk_ptr = (uint8_t *)&psk;
        uint32_t psk_len;

        security->resource_value_buffer(M2MSecurity::PublicKey, identity_ptr, security_instance_id, (size_t*)&identity_len);
        security->resource_value_buffer(M2MSecurity::Secretkey, psk_ptr, security_instance_id, (size_t*)&psk_len);
        palStatus_t ret = pal_setPSK(_conf, identity_ptr, identity_len, psk_ptr, psk_len);


        if(PAL_SUCCESS != ret){
            tr_error("pal_setPSK failed");
            return -1;
        }

    }else{
        tr_error("Security mode not set");
        return -1;

    }

    if(PAL_SUCCESS != pal_initTLS(_conf, &_ssl)){
        tr_error("pal_initTLS failed");
        return -1;
    }

    if(PAL_SUCCESS != pal_tlsSetSocket(_conf, &_tls_socket)){
        tr_error("pal_tlsSetSocket failed");
        return -1;
    }

    _init_done = M2MConnectionSecurityPimpl::INIT_DONE;

#if MBED_CONF_MBED_TRACE_ENABLE
    // Note: This call is not enough, one also needs the MBEDTLS_DEBUG_C to be defined globally
    // on build and if using default mbedtls configuration file, the
    // "#undef MBEDTLS_DEBUG_C" -line needs to be removed from mbedtls_mbed_client_config.h
    pal_sslSetDebugging(_conf, 1);
#endif

    tr_debug("M2MConnectionSecurityPimpl::init - out");
    return 0;
}

int M2MConnectionSecurityPimpl::start_handshake()
{
    tr_debug("M2MConnectionSecurityPimpl::start_handshake");

    palStatus_t ret;

    ret = pal_handShake(_ssl, _conf);

    if(ret == PAL_ERR_TLS_WANT_READ || ret == PAL_ERR_TLS_WANT_WRITE || ret == PAL_ERR_TIMEOUT_EXPIRED){
        return M2MConnectionHandler::CONNECTION_ERROR_WANTS_READ;
    }
    else if(ret == PAL_ERR_TLS_PEER_CLOSE_NOTIFY) {
        return M2MConnectionHandler::SSL_PEER_CLOSE_NOTIFY;
    }
    else if(ret == PAL_ERR_NO_MEMORY) {
        return M2MConnectionHandler::MEMORY_ALLOCATION_FAILED;
    }

    if(ret != PAL_SUCCESS){ //We loose the original error here!
        tr_debug("M2MConnectionScurityPimpl::start_handshake pal_handShake() error %" PRIx32, ret);
        return -1;
    }

    ret = pal_sslGetVerifyResult(_ssl);
    if(PAL_SUCCESS != ret){
        tr_debug("M2MConnectionSecurityPimpl::start_handshake pal_sslGetVerifyResult() error %" PRIx32, ret);
        return -1;
    }

    return ret;
}

int M2MConnectionSecurityPimpl::connect(M2MConnectionHandler* connHandler)
{
    tr_debug("M2MConnectionSecurityPimpl::connect");
    int ret = -1;

    if(M2MConnectionSecurityPimpl::INIT_DONE != _init_done){
        return ret;
    }

    ret = start_handshake();
    tr_debug("M2MConnectionSecurityPimpl::connect - handshake ret: %d", ret);
    return ret;
}


int M2MConnectionSecurityPimpl::send_message(unsigned char *message, int len)
{
    tr_debug("M2MConnectionSecurityPimpl::send_message");
    int ret = -1;
    palStatus_t return_value;
    uint32_t len_write;

    if(M2MConnectionSecurityPimpl::INIT_DONE != _init_done){
        return ret;
    }


    if(PAL_SUCCESS == (return_value = pal_sslWrite(_ssl, message, len, &len_write))){
        ret = (int)len_write;
    }
    else if(return_value == PAL_ERR_TLS_WANT_READ || return_value == PAL_ERR_TIMEOUT_EXPIRED){
        ret = M2MConnectionHandler::CONNECTION_ERROR_WANTS_READ;
    }
    else if(return_value == PAL_ERR_TLS_WANT_WRITE) {
        ret = M2MConnectionHandler::CONNECTION_ERROR_WANTS_WRITE;
    }
    else if(return_value == PAL_ERR_TLS_PEER_CLOSE_NOTIFY) {
        ret = M2MConnectionHandler::SSL_PEER_CLOSE_NOTIFY;
    }
    else if(return_value == PAL_ERR_NO_MEMORY) {
        ret = M2MConnectionHandler::MEMORY_ALLOCATION_FAILED;
    }

    tr_debug("M2MConnectionSecurityPimpl::send_message - ret: %d", ret);
    return ret; //bytes written
}

int M2MConnectionSecurityPimpl::read(unsigned char* buffer, uint16_t len)
{
    int ret = -1;
    palStatus_t return_value;
    uint32_t len_read;

    if(M2MConnectionSecurityPimpl::INIT_DONE != _init_done){
        tr_error("M2MConnectionSecurityPimpl::read - init not done!");
        return ret;
    }

    if(PAL_SUCCESS == (return_value = pal_sslRead(_ssl, buffer, len, &len_read))){
        ret = (int)len_read;
    }

    else if(return_value == PAL_ERR_TLS_WANT_READ || return_value == PAL_ERR_TLS_WANT_WRITE || return_value == PAL_ERR_TIMEOUT_EXPIRED){
        ret = M2MConnectionHandler::CONNECTION_ERROR_WANTS_READ;
    }

    else if(return_value == PAL_ERR_TLS_PEER_CLOSE_NOTIFY) {
        ret = M2MConnectionHandler::SSL_PEER_CLOSE_NOTIFY;
    }

    else if(return_value == PAL_ERR_NO_MEMORY) {
        ret = M2MConnectionHandler::MEMORY_ALLOCATION_FAILED;
    }

    return ret;
}

void M2MConnectionSecurityPimpl::set_random_number_callback(random_number_cb callback)
{
    (void)callback;
}

void M2MConnectionSecurityPimpl::set_entropy_callback(entropy_cb callback)
{

    _entropy = callback;

}

void M2MConnectionSecurityPimpl::set_socket(palSocket_t socket, palSocketAddress_t *address)
{
    _tls_socket.socket = socket;
    _tls_socket.socketAddress = address;
    _tls_socket.addressLength = sizeof(palSocketAddress_t);

    if(_sec_mode == M2MConnectionSecurity::TLS){
        _tls_socket.transportationMode = PAL_TLS_MODE;
    }
    else{
        _tls_socket.transportationMode = PAL_DTLS_MODE;
    }
}
