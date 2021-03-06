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

#include <fcntl.h>
#include <errno.h>
#include <string.h>

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <stdio.h>

#pragma comment(lib, "Ws2_32.lib")

#include <wolfssl/ssl.h>
#include "aws_iot_error.h"
#include "aws_iot_log.h"
#include "network_interface.h"
#include "wolfssl_hostname_validation.h"

static WOLFSSL_CTX* s_SSLContext;
static WOLFSSL *pSSLHandle;
static SOCKET server_TCPSocket = INVALID_SOCKET;
static struct addrinfo *server_info = NULL;
static char* pDestinationURL = NULL;
WSADATA wsaData;

struct TLSConnectParams;

static int Create_TCPSocket( const TLSConnectParams* params );
static IoT_Error_t Connect_TCPSocket();
static IoT_Error_t setSocketToNonBlocking( );
static IoT_Error_t ConnectOrTimeoutOrExitOnError( WOLFSSL *pSSL, int timeout_ms );
static IoT_Error_t ReadOrTimeoutOrExitOnError( WOLFSSL *pSSL, unsigned char *msg, int totalLen, int timeout_ms );
static IoT_Error_t WriteOrTimeoutOrExitOnError( WOLFSSL *pSSL, unsigned char *msg, int totalLen, int timeout_ms );


int iot_tls_init(Network *pNetwork)
{
	if (wolfSSL_Init() != SSL_SUCCESS)	{		return SSL_INIT_ERROR;
	}
	WOLFSSL_METHOD* method = wolfTLSv1_2_client_method();
	if ((s_SSLContext = wolfSSL_CTX_new( method )) == NULL)
	{
		ERROR( " WOLFSSL INIT Failed - Unable to create WOLFSSL Context" );
		return SSL_INIT_ERROR;
	}
	pNetwork->my_socket = 0;
	pNetwork->connect = iot_tls_connect;
	pNetwork->mqttread = iot_tls_read;
	pNetwork->mqttwrite = iot_tls_write;
	pNetwork->disconnect = iot_tls_disconnect;
	pNetwork->isConnected = iot_tls_is_connected;
	pNetwork->destroy = iot_tls_destroy;

	// Initialize Winsock
	int iResult = WSAStartup( MAKEWORD( 2, 2 ), &wsaData );
	if (iResult != 0) {
		ERROR( "WSAStartup failed: %d\n", iResult );
		return SSL_INIT_ERROR;
	}

	return NONE_ERROR;
}

int iot_tls_is_connected( Network *pNetwork ) {
	/* Use this to add implementation which can check for physical layer disconnect */
	return server_TCPSocket != INVALID_SOCKET;
}





int tls_server_certificate_verify(int preverify_ok, WOLFSSL_X509_STORE_CTX *pX509CTX){
	// preverify_ok
	// 0 ==> Fail
	// 1 ==> Pass
	int verification_return = 0;

	//last certificate(depth = 0) is the one provided by the Server
	if((wolfSSL_X509_STORE_CTX_get_error_depth(pX509CTX) == 0) && (preverify_ok == 1)){
		WOLFSSL_X509 *pX509Cert;
		HostnameValidationResult result;
		pX509Cert = wolfSSL_X509_STORE_CTX_get_current_cert(pX509CTX);
		result = validate_hostname(pDestinationURL, pX509Cert);
		if(MatchFound == result){
			verification_return = 1;
		}
	}
	else{
		verification_return = preverify_ok;
	}

	return verification_return;
}

int iot_tls_connect(Network *pNetwork, TLSConnectParams params) {

	IoT_Error_t ret_val = NONE_ERROR;

	if (Create_TCPSocket( &params ) != 0) {
		ret_val = TCP_SETUP_ERROR;
		return ret_val;
	}

	if (!wolfSSL_CTX_load_verify_locations(s_SSLContext, params.pRootCALocation, NULL)) {
		ERROR(" Root CA Loading error");
		ret_val = SSL_CERT_ERROR;
	}

	if (!wolfSSL_CTX_use_certificate_file(s_SSLContext, params.pDeviceCertLocation, SSL_FILETYPE_PEM)) {
		ERROR(" Device Certificate Loading error");
		ret_val = SSL_CERT_ERROR;
	}

	if(1 != wolfSSL_CTX_use_PrivateKey_file(s_SSLContext, params.pDevicePrivateKeyLocation, SSL_FILETYPE_PEM)){
		ERROR(" Device Private Key Loading error");
		ret_val = SSL_CERT_ERROR;
	}
	if(params.ServerVerificationFlag){
		wolfSSL_CTX_set_verify(s_SSLContext, SSL_VERIFY_PEER, tls_server_certificate_verify);
	}
	else{
		wolfSSL_CTX_set_verify(s_SSLContext, SSL_VERIFY_PEER, NULL);
	}

	pSSLHandle = wolfSSL_new(s_SSLContext);

	pDestinationURL = params.pDestinationURL;
	ret_val = Connect_TCPSocket();
	if(NONE_ERROR != ret_val){
		ERROR(" TCP Connection error");
		return ret_val;
	}

	wolfSSL_set_fd(pSSLHandle, server_TCPSocket);

	if(ret_val == NONE_ERROR){
		ret_val = setSocketToNonBlocking(server_TCPSocket);
		if(ret_val != NONE_ERROR){
			ERROR(" Unable to set the socket to Non-Blocking");
		}
	}

	if(NONE_ERROR == ret_val){
		ret_val = ConnectOrTimeoutOrExitOnError(pSSLHandle, params.timeout_ms);
		//if(X509_V_OK != SSL_get_verify_result(pSSLHandle)){
		if (ret_val != NONE_ERROR) {
			ERROR(" Server Certificate Verification failed");
			ret_val = SSL_CONNECT_ERROR;
		}
		else{
			// ensure you have a valid certificate returned, otherwise no certificate exchange happened
			if(NULL == wolfSSL_get_peer_certificate(pSSLHandle)){
				ERROR(" No certificate exchange happened");
				ret_val = SSL_CONNECT_ERROR;
			}
		}
	}
	return ret_val;
}

int iot_tls_write(Network *pNetwork, unsigned char *pMsg, int len, int timeout_ms){

	return WriteOrTimeoutOrExitOnError(pSSLHandle, pMsg, len, timeout_ms);
}

int iot_tls_read(Network *pNetwork, unsigned char *pMsg, int len, int timeout_ms) {
	return ReadOrTimeoutOrExitOnError(pSSLHandle, pMsg, len, timeout_ms);
}

void iot_tls_disconnect(Network *pNetwork){
	wolfSSL_shutdown(pSSLHandle);
	closesocket( server_TCPSocket );
	server_TCPSocket = INVALID_SOCKET;
}

int iot_tls_destroy(Network *pNetwork) {
	wolfSSL_free(pSSLHandle);
	wolfSSL_CTX_free(s_SSLContext);

	WSACleanup();

	return 0;
}

int Create_TCPSocket( const TLSConnectParams* params) {
	int iResult = -1;
	struct addrinfo hints;

	ZeroMemory( &hints, sizeof( hints ) );
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	// Resolve the server address and port
	char port_buff[256];
	_itoa( params->DestinationPort, port_buff, 10 );
	iResult = getaddrinfo( params->pDestinationURL, port_buff, &hints, &server_info );
	if (iResult != 0) {
		ERROR( "getaddrinfo failed: %d\n", iResult );
		WSACleanup();
		return iResult;
	}

	// Create a SOCKET for connecting to server
	server_TCPSocket = socket( server_info->ai_family, server_info->ai_socktype,
							   server_info->ai_protocol );

	if (server_TCPSocket == INVALID_SOCKET) {
		ERROR( "Error at socket(): %ld\n", WSAGetLastError() );
		freeaddrinfo( server_info );
		WSACleanup();
		return -1;
	}

	return 0; // return 0 on success
}

IoT_Error_t Connect_TCPSocket() {
	IoT_Error_t ret_val = TCP_CONNECT_ERROR;

	// Connect to server.
	int iResult = connect( server_TCPSocket, server_info->ai_addr, (int)server_info->ai_addrlen );
	if (iResult == SOCKET_ERROR) {
		closesocket( server_TCPSocket );
		server_TCPSocket = INVALID_SOCKET;
	}

	// Should really try the next address returned by getaddrinfo
	// if the connect call failed
	// But for this simple example we just free the resources
	// returned by getaddrinfo and print an error message

	freeaddrinfo( server_info );

	if (server_TCPSocket == INVALID_SOCKET) {
		ERROR( "Unable to connect to server!\n" );
		WSACleanup();
	}
	else ret_val = NONE_ERROR;

	return ret_val;
}

IoT_Error_t setSocketToNonBlocking( ) {

	u_long iMode = 1;
	int retval = ioctlsocket( server_TCPSocket, FIONBIO, &iMode );

	if (retval != 0)
	{
		retval = WSAGetLastError();
		// TODO: translate to a more accurate error-code
		return TCP_CONNECT_ERROR;
	}
	return NONE_ERROR;
}

IoT_Error_t ConnectOrTimeoutOrExitOnError(WOLFSSL *pSSL, int timeout_ms){

	enum{
		SSL_CONNECTED = 1,
		SELECT_TIMEOUT = 0,
		SELECT_ERROR = -1
	};

	IoT_Error_t ret_val = NONE_ERROR;
	int rc = 0;
	fd_set readFds;
	fd_set writeFds;
	struct timeval timeout;
	timeout.tv_sec = timeout_ms / 1000;
	timeout.tv_usec = (timeout_ms % 1000) * 1000;

	int errorCode = 0;
	int select_retCode = SELECT_TIMEOUT;

	do{
		rc = wolfSSL_connect(pSSL);

		if(SSL_CONNECTED == rc){
			ret_val = NONE_ERROR;
			break;
		}

		errorCode = wolfSSL_get_error(pSSL, rc);

		if(errorCode == SSL_ERROR_WANT_READ){
			FD_ZERO(&readFds);
			FD_SET(server_TCPSocket, &readFds);
			select_retCode = select(server_TCPSocket + 1, (void *) &readFds, NULL, NULL, &timeout);
			if (SELECT_TIMEOUT == select_retCode) {
				ERROR(" WOLFSSL Connect time out while waiting for read");
				ret_val = SSL_CONNECT_TIMEOUT_ERROR;
			} else if (SELECT_ERROR == select_retCode) {
				ERROR(" WOLFSSL Connect Select error for read %d", select_retCode);
				ret_val = SSL_CONNECT_ERROR;
			}
		}

		else if(errorCode == SSL_ERROR_WANT_WRITE){
			FD_ZERO(&writeFds);
			FD_SET(server_TCPSocket, &writeFds);
			select_retCode = select(server_TCPSocket + 1, NULL, (void *) &writeFds, NULL, &timeout);
			if (SELECT_TIMEOUT == select_retCode) {
				ERROR(" WOLFSSL Connect time out while waiting for write");
				ret_val = SSL_CONNECT_TIMEOUT_ERROR;
			} else if (SELECT_ERROR == select_retCode) {
				ERROR(" WOLFSSL Connect Select error for write %d", select_retCode);
				ret_val = SSL_CONNECT_ERROR;
			}
		}

		else{
			ret_val = SSL_CONNECT_ERROR;
		}

	}while(SSL_CONNECT_ERROR != ret_val && SSL_CONNECT_TIMEOUT_ERROR != ret_val);

	return ret_val;
}

IoT_Error_t WriteOrTimeoutOrExitOnError(WOLFSSL *pSSL, unsigned char *msg, int totalLen, int timeout_ms){


	IoT_Error_t errorStatus = NONE_ERROR;

	fd_set writeFds;
	enum{
		SELECT_TIMEOUT = 0,
		SELECT_ERROR = -1
	};
	int errorCode = 0;
	int select_retCode;
	int writtenLength = 0;
	int rc = 0;
	int returnCode = 0;
	struct timeval timeout;
	timeout.tv_sec = timeout_ms / 1000;
	timeout.tv_usec = (timeout_ms % 1000) * 1000;

	do{
		rc = wolfSSL_write(pSSL, msg, totalLen);

		errorCode = wolfSSL_get_error(pSSL, rc);

		if(0 < rc){
			writtenLength += rc;
		}

		else if (errorCode == SSL_ERROR_WANT_WRITE) {
			FD_ZERO(&writeFds);
			FD_SET(server_TCPSocket, &writeFds);
			select_retCode = select(server_TCPSocket + 1, NULL, (void *) &writeFds, NULL, &timeout);
			if (SELECT_TIMEOUT == select_retCode) {
				errorStatus = SSL_WRITE_TIMEOUT_ERROR;
			} else if (SELECT_ERROR == select_retCode) {
				errorStatus = SSL_WRITE_ERROR;
			}
		}

		else{
			errorStatus = SSL_WRITE_ERROR;
		}

	}while(SSL_WRITE_ERROR != errorStatus && SSL_WRITE_TIMEOUT_ERROR != errorStatus && writtenLength < totalLen);

	if(NONE_ERROR == errorStatus){
		returnCode = writtenLength;
	}
	else{
		returnCode = errorStatus;
	}

	return returnCode;
}

IoT_Error_t ReadOrTimeoutOrExitOnError(WOLFSSL *pSSL, unsigned char *msg, int totalLen, int timeout_ms){


	IoT_Error_t errorStatus = NONE_ERROR;

	fd_set readFds;
	enum{
		SELECT_TIMEOUT = 0,
		SELECT_ERROR = -1
	};
	int errorCode = 0;
	int select_retCode;
	int readLength = 0;
	int rc = 0;
	int returnCode = 0;
	struct timeval timeout;
	timeout.tv_sec = timeout_ms / 1000;
	timeout.tv_usec = (timeout_ms % 1000) * 1000;

	do{
		rc = wolfSSL_read(pSSL, msg, totalLen);
		errorCode = wolfSSL_get_error(pSSL, rc);

		if(0 < rc){
			readLength += rc;
		}

		else if (errorCode == SSL_ERROR_WANT_READ) {
			FD_ZERO(&readFds);
			FD_SET(server_TCPSocket, &readFds);
			select_retCode = select(server_TCPSocket + 1, (void *) &readFds, NULL, NULL, &timeout);
			if (SELECT_TIMEOUT == select_retCode) {
				errorStatus = SSL_READ_TIMEOUT_ERROR;
			} else if (SELECT_ERROR == select_retCode) {
				errorStatus = SSL_READ_ERROR;
			}
		}

		else{
			errorStatus = SSL_READ_ERROR;
		}

	}while(SSL_READ_ERROR != errorStatus && SSL_READ_TIMEOUT_ERROR != errorStatus && readLength < totalLen);

	if(NONE_ERROR == errorStatus){
		returnCode = readLength;
	}
	else{
		returnCode = errorStatus;
	}

	return returnCode;
}
