/*
 * Copyright (c) 2016, SRCH2
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *    * Neither the name of the SRCH2 nor the
 *      names of its contributors may be used to endorse or promote products
 *      derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL SRCH2 BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <ctime>
#include <cstdlib>
#include <fstream>
#include <exception>

using std::exception;

#include <openssl/sha.h>
#include <openssl/bio.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>

#include "license/LicenseVerifier.h"
#include "util/Logger.h"
#include "base64.h"

using srch2::util::Logger;
namespace srch2
{
namespace instantsearch
{

static const std::string base64_chars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";

class Base64Decoder {
public:
	static std::string decode(const std::string& encoded_string)
	{
		return base64_decode(encoded_string);
	}
};

static const char *PUB_KEY =
		"-----BEGIN PUBLIC KEY-----\n"
		"MIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQC4m+K1VswPvpeL/jCb0qc6TmF0\n"
		"aY+F0zuExIXDS1vWBYGCpPm8cttplienaBNfsGZ+ZL7MkEvB3519mGFWcPykV+vC\n"
		"gV53vp9uLDOLHDLZVFRrbXN+gOCVG3jaY3TkQH8RGLETLJRoTKRT+EeEqg4Kg81J\n"
		"f46avGyDoWwKKNUxCwIDAQAB\n"
		"-----END PUBLIC KEY-----";

class PublicKeyInitializer {
public:
	RSA *rsa;

	PublicKeyInitializer()
	{
		BIO *b = NULL;
		b = BIO_new_mem_buf(const_cast<char *>(PUB_KEY), strlen(PUB_KEY));
		EVP_PKEY *pkey = NULL;
		pkey = PEM_read_bio_PUBKEY(b, NULL, NULL, NULL);

		rsa = EVP_PKEY_get1_RSA(pkey);
	}
};

static RSA *getRSAPublicKey()
{
	static PublicKeyInitializer pki;
	return pki.rsa;
}

/// The input string must be strictly in format: YYYY-MM-DD
/// Example: 2012-04-27
bool isLicenseDateValid(std::string expiryDate)
{
	int year = static_cast<int>(strtol(expiryDate.substr(0, 4).c_str(), NULL, 10));
	int month = static_cast<int>(strtol(expiryDate.substr(5, 2).c_str(), NULL, 10));

	int day = static_cast<int>(strtol(expiryDate.substr(8, 2).c_str(), NULL, 10));

	struct std::tm endDate = {0, 0, 0, day, month - 1, year - 1900};

	time_t timer = time(NULL);
	struct std::tm* currentDate = gmtime(&timer);

	//on error, -1 is returned by mktime
	std::time_t x = std::mktime(currentDate);
	std::time_t y = std::mktime(&endDate);

	if ( x != (std::time_t)(-1) && y != (std::time_t)(-1) )
	{
		double difference = std::difftime(y, x);
		if(difference < 0) {
			return false;
		}
	}
	return true;
}

bool LicenseVerifier::test(const std::string& license)
{
	std::string::size_type splitIdx = license.find(',');
	std::string signature(license.substr(0, splitIdx));
	std::string unsignedLicense(license.substr(splitIdx + 1));

	SHA_CTX sha_ctx = { 0 };
	unsigned char digest[SHA_DIGEST_LENGTH];
	int rc;

	if ((rc = SHA1_Init(&sha_ctx)) != 1) {
		return false;
	}

	if ((rc = SHA1_Update(&sha_ctx, unsignedLicense.c_str(), unsignedLicense.length())) != 1) {
		return false;
	}

	if ((rc = SHA1_Final(digest, &sha_ctx)) != 1) {
		return false;
	}

	std::string base64Sign(signature.substr(signature.find('=') + 1));
	std::string decoded(Base64Decoder::decode(base64Sign));

	if ((rc = RSA_verify(NID_sha1, digest, sizeof digest, (unsigned char *)decoded.c_str(), decoded.length(), getRSAPublicKey())) != 1) {
		return false;
	}

	std::string::size_type splitDate = unsignedLicense.find("Expiry-Date=");
	std::string expiryDate(unsignedLicense.substr(splitDate + 12, splitDate + 10));

	if ( isLicenseDateValid(expiryDate) == false) {
		return false;
	}
	return true;
}

bool LicenseVerifier::testFile(const std::string& filenameWithPath)
{
	std::string license_dir = "";
	std::string license_file = "";
	std::string line;
	std::ifstream infile;
	if (filenameWithPath.compare("") == 0)
	{
		//return testWithEnvironmentVariable();
        Logger::error("Cannot read the license key file. Check \"<licenseFile>\" in the configuration file, which defines the location of the license key file.");
        Logger::error("Note: Since all the paths in the configuration file are appended to the \"<srch2Home>\" value before use, make sure the license file path is relative to srch2Home.");
        Logger::error("Note: It is recommended to use an absolute path value for <srch2Home>");
        exit(-1);     
	}
	else
	{
		try
		{
			//license_dir = std::string(getenv("srch2_license_dir")); /// getenv could return NULL and app will give seg fault if assigned to string
			//license_file = license_dir + "/srch2_license_key.txt";
			license_file = filenameWithPath;
			infile.open (license_file.c_str());
		}
		catch (exception& e)
		{
            Logger::error("Cannot read the license key file. Check \"<licenseFile>\" in the configuration file, which defines the location of the license key file.");
			exit(-1);
		}

		if (infile.good())
		{
			getline(infile,line); // Saves the line in STRING.
			if(line.length()>0 && line[line.length()-1] == '\r') line.resize(line.length()-1);
		}
		else
		{
            Logger::error("Cannot read the license key file. Check \"<licenseFile>\" in the configuration file, which defines the location of the license key file.");
            Logger::error("Note: Since all the paths in the configuration file are appended to the \"<srch2Home>\" value before use, make sure the license file path is relative to srch2Home.");
            Logger::error("Note: It is recommended to use an absolute path value for <srch2Home>");
			exit(-1);
		}

		infile.close();

		if (! test(line))
		{
            Logger::error("License key file invalid. Please provide a valid license key file. Feel free to contact contact@srch2.com");
			exit(-1);
		}
		return true;
	}
}

bool LicenseVerifier::testWithEnvironmentVariable()
{
	std::string license_dir = "";
	std::string license_file = "";
	std::string line;
	std::ifstream infile;
	try
	{
		license_dir = std::string(getenv("srch2_license_dir")); /// getenv could return NULL and app will give seg fault if assigned to string
		license_file = license_dir + "/srch2_license_key.txt";
		infile.open (license_file.c_str());
	}
	catch (exception& e)
	{
        Logger::error("Cannot read the license key file. Check the environment variable \"srch2_license_dir\", which defines the folder that includes the license key file.");
        Logger::error("Note: Since all the paths in the configuration file are appended to the \"<srch2Home>\" value before use, make sure the license file path is relative to srch2Home.");
        Logger::error("Note: It is recommended to use an absolute path value for <srch2Home>");
		exit(-1);  // should be changed to throw an ExitException
	}

	if (infile.good())
	{
		getline(infile,line); // Saves the line in STRING.
	}
	else
	{
        Logger::error("Cannot read the license key file. Check the environment variable \"srch2_license_dir\", which defines the folder that includes the license key file.");
        Logger::error("Note: Since all the paths in the configuration file are appended to the \"<srch2Home>\" value before use, make sure the license file path is relative to srch2Home.");
        Logger::error("Note: It is recommended to use an absolute path value for <srch2Home>");
		exit(-1);
	}

	infile.close();

	if (! test(line))
	{
        Logger::error("License key file invalid. Please provide a valid license key file. Feel free to contact contact@srch2.com");
		exit(-1);
	}

	return true;
}


}}


