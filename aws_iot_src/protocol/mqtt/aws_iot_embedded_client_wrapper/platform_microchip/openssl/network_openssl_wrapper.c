/*
 * Copyright 2010-2015 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 * A copy of the License is located at
 *
 *  http://aws.amazon.com/apache2.0
 *
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

// THIS FILE IS COMPLETELY NON-FUNCTIONAL
// Connection is dependent on the modem, and these
// functions will need to be moved out of this
// API into the units firmware
// (CHECK Linux/Windows for sample implementation though)

struct Network Network;
struct X509_STORE_CTX;
struct TLSConnectParams TLSConnectParams;

int iot_tls_init(Network *pNetwork) {

	IoT_Error_t ret_val = NONE_ERROR;
    // TODO
	return ret_val;
}

int iot_tls_is_connected(Network *pNetwork) {
	/* Use this to add implementation which can check for physical layer disconnect */
	return 1;
}

int tls_server_certificate_verify(int preverify_ok, X509_STORE_CTX *pX509CTX){
    // TODO
	return 1;
}

int iot_tls_connect(Network *pNetwork, TLSConnectParams params) {
    // TODO
	return NONE_ERROR;
}

int iot_tls_write(Network *pNetwork, unsigned char *pMsg, int len, int timeout_ms){
    // TODO
	return NONE_ERROR;
}

int iot_tls_read(Network *pNetwork, unsigned char *pMsg, int len, int timeout_ms) {
	// TODO
    return NONE_ERROR;
}

void iot_tls_disconnect(Network *pNetwork){
	// TODO
}

int iot_tls_destroy(Network *pNetwork) {
    // TODO
	return 0;
}
