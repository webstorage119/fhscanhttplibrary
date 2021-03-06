/*
 Copyright (C) 2007 - 2012  fhscan project.
 Andres Tarasco - http://www.tarasco.org/security - http://www.tarlogic.com

 All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions
 are met:
 1. Redistributions of source code must retain the above copyright
 notice, this list of conditions and the following disclaimer.
 2. Redistributions in binary form must reproduce the above copyright
 notice, this list of conditions and the following disclaimer in the
 documentation and/or other materials provided with the distribution.
 3. All advertising materials mentioning features or use of this software
 must display the following acknowledgement:
 This product includes software developed by Andres Tarasco fhscan
 project and its contributors.
 4. Neither the name of the project nor the names of its contributors
 may be used to endorse or promote products derived from this software
 without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 SUCH DAMAGE.

 */

/** \file CookieHandling.h
 * Fast HTTP Auth Scanner - HTTP Engine v1.4.
 * ..
 * \author Andres Tarasco Acuna - http://www.tarasco.org
 */

#ifndef __COOKIEHANDLING_H
#define __COOKIEHANDLING_H

#include "Build.h"
#include "Tree.h"
#include "Threading.h"
/*
 #include <stdio.h>
 #include <string.h>
 //#include <time.h>
 #ifdef __WIN32__RELEASE__
 #include <windows.h>
 #else
 #include <unistd.h>
 #include <pthread.h>  //pthread
 #endif
 */
#define COOKIETIMEFORMAT  _T("%a, %d-%b-%Y %H:%M:%S GMT") /* time formats in cookie headers */
#define COOKIETIMEFORMAT2 _T("%a, %d-%b-%y %H:%M:%S GMT") /* some idiotic websites send the year without the century */
#define GetDataWithoutSpaces(data) \
	while (*data==' ') data++;     \
	while (data[_tcslen(data)-1]==_T(' ')) data[_tcslen(data)-1]=0

#define IS_PATH(a)		(_tcscmp(a,_T("path"))==0)
#define IS_EXPIRES(a)	(_tcscmp(a,_T("expires"))==0)
#define IS_DOMAIN(a)   	(_tcscmp(a,_T("domain"))==0)
#define IS_MAXAGE(a)    (_tcscmp(a,_T("max-age"))==0)
#define IS_VERSION(a)   (_tcscmp(a,_T("version"))==0)

#define IS_HTTPONLY(a)    (_tcscmp(a,_T("HttpOnly"))==0)
#define IS_HTTPONLY2(a)    (_tcscmp(a,_T("httponly"))==0)

#define IS_SECURE(a)    (_tcscmp(a,_T("secure"))==0)
/* Sanity checks.  These are important, otherwise it is possible for
 mailcious attackers to destroy important cookie information and/or
 violate your privacy. */

#define REQUIRE_DIGITS(p) do {                  \
	if (!ISDIGIT (*p))                            \
	return 0;                                   \
	for (++p; ISDIGIT (*p); p++)                  \
	;                                           \
} while (0)

#define REQUIRE_DOT(p) do {                     \
	if (*p++ != '.')                              \
	return 0;                                   \
} while (0)

/* Check whether ADDR matches <digits>.<digits>.<digits>.<digits>.

 We don't want to call network functions like inet_addr() because all
 we need is a check, preferrably one that is small, fast, and
 well-defined. */

#ifndef time_t
// #define time_t long
#endif

class Cookie {
	HTTPCHAR *lpCookieName;
	HTTPCHAR *lpCookieValue;
	time_t expire;
	HTTPCHAR *path;
	HTTPCHAR *domain;
	BOOL secure;
	BOOL httponly;

public:
	Cookie();
	Cookie(HTTPCHAR *cName, HTTPCHAR *cValue, time_t cExpire, HTTPCHAR *cPath,
		HTTPCHAR *cDomain, int cSecure, int cHttponly);
	~Cookie();
	int MatchCookie(HTTPCHAR *lppath, HTTPCHAR *lpdomain,
		HTTPCHAR *lpCookieName, int securevalue);
	void SetDate(time_t cExpire);

	time_t GetDate(void) {
		return expire;
	}
	size_t path_matches(HTTPCSTR RequestedPath);

	HTTPCHAR *GetCookieName(void) {
		return (lpCookieName);
	}

	HTTPCHAR *GetCookieValue(void) {
		return (lpCookieValue);
	}

	HTTPCHAR *GetCookiePath(void) {
		return (path);
	}

	HTTPCHAR *GetCookieDomain(void) {
		return domain;
	}

	BOOL IsSecure(void) {
		return secure;
	}
	void SetValue(HTTPCHAR *cValue);
};

/***********************************************************************/
 struct CookieList
 {
 //class Threading *lock;
	 int nCookies;
	 class Cookie **CookieElement;
 };

 #define MAX_COOKIES_PER_DOMAIN 1000

 class CookieStatus
 {
	 bTree *DomainList;
	 int nDomains;
	 class Threading lock;

	 int RemoveCookieFromList(struct CookieList *List,HTTPCHAR *path,HTTPCHAR *name, HTTPCHAR *lpDomain);
	 //void InsertCookieToList(struct CookieList *List,char *path, char *name, char *value, int secure, int HttpOnly);
	 int numeric_address_p (HTTPCSTR addr);
	 int check_domain_match (HTTPCSTR cookie_domain, HTTPCSTR host);
	 BOOL cookie_expired (time_t CookieExpireTime);

 public:
	 CookieStatus();
     ~CookieStatus();
	 time_t ExtractDate(HTTPCHAR *lpdate);
	 int ParseCookieData(HTTPCHAR *lpCookieData, HTTPCSTR lpPath, HTTPCSTR lpDomain);
	 HTTPCHAR *ReturnCookieHeaderFor(HTTPCSTR lpDomain,HTTPCSTR path,int CookieOverSSL);

	 int CookieAlreadyExist(struct CookieList *List,HTTPCHAR *path, HTTPCHAR *domain, HTTPCHAR *name, int secure );


 };


 #endif
