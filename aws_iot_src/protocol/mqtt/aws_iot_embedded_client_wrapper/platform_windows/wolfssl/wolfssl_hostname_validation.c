/*Copyright (C) 2012, iSEC Partners.
 *
 *Permission is hereby granted, free of charge, to any person obtaining a copy of
 *this software and associated documentation files (the "Software"), to deal in
 *the Software without restriction, including without limitation the rights to
 *use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 *of the Software, and to permit persons to whom the Software is furnished to do
 *so, subject to the following conditions:
 *
 *The above copyright notice and this permission notice shall be included in all
 *copies or substantial portions of the Software.
 *
 *THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *SOFTWARE.
 *
 */

#include <stdint.h>
#include <user_settings.h>
#include <openssl/ssl.h>
#include <wolfcrypt/asn.h>

#include "wolfssl_hostname_validation.h"
#include "hostname_compare.h"

//#include <string.h>
int wolfSSL_X509_NAME_get_text_by_NID( WOLFSSL_X509_NAME* name,
									   int nid, char* buf, int len );
/**
 * Tries to find a match for hostname in the certificate's Common Name field.
 *
 * Returns MatchFound if a match was found.
 * Returns MatchNotFound if no matches were found.
 * Returns MalformedCertificate if the Common Name had a NUL character embedded in it.
 * Returns Error if the Common Name could not be extracted.
 */
static HostnameValidationResult matches_common_name(const char *hostname, const X509 *server_cert) 
{	
	X509_NAME_ENTRY *common_name_entry = NULL;

	char common_name_str[ASN_NAME_MAX];
	uint16_t common_name_len = ASN_NAME_MAX; // Init to max val, this will throw comp warning if its possible to overflow

	// Find the CN field text in the Subject field of the certificate
	common_name_len = (uint8_t)wolfSSL_X509_NAME_get_text_by_NID(
			X509_get_subject_name( (X509 *)server_cert ), ASN_COMMON_NAME, common_name_str, ASN_NAME_MAX );

	//// Find the position of the CN field in the Subject field of the certificate
	//common_name_loc = X509_NAME_get_index_by_NID(
	//		X509_get_subject_name((X509 *) server_cert), NID_commonName, -1);
	//if (common_name_loc < 0) {
	//	return Error;
	//}

	//// Extract the CN field
	//common_name_entry = X509_NAME_get_entry(
	//		X509_get_subject_name((X509 *) server_cert), common_name_loc);
	if (common_name_entry == NULL) {
		return Error;
	}

	//// Convert the CN field to a C string
	//common_name_asn1 = X509_NAME_ENTRY_get_data(common_name_entry);
	//if (common_name_asn1 == NULL) {
	//	return Error;
	//}
	//common_name_str = (char *) ASN1_STRING_data(common_name_asn1);

	// Make sure there isn't an embedded NUL character in the CN
	if (common_name_len != strlen(common_name_str)) {
		return MalformedCertificate;
	}

	// Compare expected hostname with the CN
	if (hostmatch(hostname, common_name_str) == CURL_HOST_MATCH) {
		return MatchFound;
	} else {
		return MatchNotFound;
	}
}

/**
 * Tries to find a match for hostname in the certificate's Subject Alternative Name extension.
 *
 * Returns MatchFound if a match was found.
 * Returns MatchNotFound if no matches were found.
 * Returns MalformedCertificate if any of the hostnames had a NUL character embedded in it.
 * Returns NoSANPresent if the SAN extension was not present in the certificate.
 */
static HostnameValidationResult matches_subject_alternative_name(
		const char *hostname, const X509 *server_cert) 
{

	// TODO: Verify 100% that we need this.
	// Including AltNames in the cert appears to require
	// an extra 16kb of mem
	HostnameValidationResult result = MatchNotFound;
	return result; 
	//int i;
	//int san_names_nb = -1;
	//STACK_OF(GENERAL_NAME) *san_names = NULL;

	//// Try to extract the names within the SAN extension from the certificate
	//san_names = X509_get_ext_d2i((X509 *) server_cert, NID_subject_alt_name,
	//		NULL, NULL);
	//if (san_names == NULL) {
	//	return NoSANPresent;
	//}

	//san_names_nb = sk_GENERAL_NAME_num(san_names);
	//// Check each name within the extension
	//for (i = 0; i < san_names_nb; i++) {
	//	const GENERAL_NAME *current_name = sk_GENERAL_NAME_value(san_names, i);

	//	if (current_name->type == GEN_DNS) {
	//		// Current name is a DNS name, let's check it
	//		char *dns_name = (char *) ASN1_STRING_data(current_name->d.dNSName);

	//		// Make sure there isn't an embedded NUL character in the DNS name
	//		if (ASN1_STRING_length(current_name->d.dNSName)
	//				!= strlen(dns_name)) {
	//			result = MalformedCertificate;
	//			break;
	//		} else { // Compare expected hostname with the DNS name
	//			if (hostmatch(hostname, dns_name) == CURL_HOST_MATCH) {
	//				result = MatchFound;
	//				break;
	//			}
	//		}
	//	}
	//}
	//sk_GENERAL_NAME_pop_free(san_names, GENERAL_NAME_free);

	//return result;
}

/**
 * Validates the server's identity by looking for the expected hostname in the
 * server's certificate. As described in RFC 6125, it first tries to find a match
 * in the Subject Alternative Name extension. If the extension is not present in
 * the certificate, it checks the Common Name instead.
 *
 * Returns MatchFound if a match was found.
 * Returns MatchNotFound if no matches were found.
 * Returns MalformedCertificate if any of the hostnames had a NUL character embedded in it.
 * Returns Error if there was an error.
 */
HostnameValidationResult validate_hostname(const char *hostname, const X509 *server_cert) {
	HostnameValidationResult result;

	if ((hostname == NULL) || (server_cert == NULL))
		return Error;
	// First try the Subject Alternative Names extension
	result = matches_subject_alternative_name(hostname, server_cert);
	if (result == NoSANPresent) {
		// Extension was not found: try the Common Name
		result = matches_common_name(hostname, server_cert);
	}

	return result;
}
