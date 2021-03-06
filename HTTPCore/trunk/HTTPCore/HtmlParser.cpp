#ifdef _SPIDER_
#include "Build.h"
#include "HtmlParser.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <assert.h>

#include "HTTP.h"
#include "HTTPRequest.h"
#include "HTTPResponse.h"

/*******************************************************************************************************/
VALIDTAGS tags[]= {
	{_T("action"),_T("") },
	{_T("background"),_T("td")},
	{_T("href"),_T("")},
	{_T("mce_href"),_T("a")},
	{_T("parameter"),_T("")},
	{_T("pluginspace"),_T("embeded")},
	{_T("src"),_T("")},
	{_T("template"),_T("url")},
	{_T("xmlns"),_T("html")},
	{_T("xmlns"),_T("meta")},
};
/*******************************************************************************************************/
/*******************************************************************************************************/
int IsValidHTMLTag(HTTPCHAR *tagattribute, HTTPCHAR *tagname)
{
	for (unsigned int i=0; i<sizeof(tags)/sizeof(VALIDTAGS);i++)
	{
		int ret = _tcsicmp(tagattribute,tags[i].tagattribute);
		if (ret==0)
		{
			if (tags[i].tagname[0]!=_T('\0'))
			{
				if (_tcsicmp(tags[i].tagname,tagname)==0)
				{
					return(1);
				}
			} else
			{
				return(1);
			}
		} else {
			if (ret<0) return(0);
		}
	}
	return(0);
}
/******************************************************************************/
// Stristr : Case insensitive _tcsstr
/******************************************************************************/
HTTPCHAR *stristr(HTTPCSTR String, HTTPCSTR Pattern)
{
	HTTPCHAR *pptr, *sptr, *start;

	for (start = (HTTPCHAR *)String; *start; start++)
	{
		/* find start of pattern in string */
		for (; ((*start) && (toupper(*start) != toupper(*Pattern))); start++);
		if (!*start)
			return NULL;

		pptr = (HTTPCHAR*)Pattern;
		sptr = (HTTPCHAR*)start;

		while (toupper(*sptr) == toupper(*pptr)) {
			sptr++;
			pptr++;

			/* if end of pattern then pattern was found */

			if (!*pptr)
				return (start);
		}
	} return NULL;
}

/*******************************************************************************************************/

/*
Fix directory transversals por HTTP paths
test cases:
1)  ./test.html -> test.html
2)  /./test.html -> /test.html
3)  .../test.html -> test.html
4)  /.../test.html -> /test.html
5)  test/../test.html ->test.html
6)  /test/../test.html -> /test.html
7)  ../test.html       -> ../test.html
*/
HTTPCHAR *FixUrlTransversal(HTTPCHAR *path) {

	HTTPCHAR *p = path;
	HTTPCHAR *last = p;
	int fix = 0;

	while (*p) {
		if (*p == _T('/')) {
			if (p[1] == _T('/')) {
				size_t n = _tcslen(p + 1);
				memcpy(p, p + 1, n*sizeof(HTTPCHAR));
				p[n] = 0;
			}
			else if (p[1] == _T('.')) {
				if (memcmp(p + 1, _T("./"), 2) == 0) {
					size_t n = _tcslen(p + 2);
					memcpy(p, p + 2, n*sizeof(HTTPCHAR));
					p[n] = 0;
					fix = 1;
				}
				else if (memcmp(p + 1, _T(".../"), 4 * sizeof(HTTPCHAR)) == 0) {
					size_t n = _tcslen(p + 4);
					memcpy(p, p + 4, n*sizeof(HTTPCHAR));
					p[n] = 0;
				}
				else if (memcmp(p + 1, _T("../"), 3 * sizeof(HTTPCHAR)) == 0) {
					size_t n = _tcslen(p + 3);
					if (*last == _T('/')) {
						memcpy(last, p + 3, n*sizeof(HTTPCHAR));
					}
					else {
						memcpy(last, p + 4, n*sizeof(HTTPCHAR));
						n++;
					}
					last[n] = 0;
					p = last + 1;
					fix = 1;
				}
				else {
					last = p;
					p++;
				}
			}
			else {
				last = p;
				p++;
			}
		}
		else if ((last == p) && (p[0] == _T('.'))) {
			if (memcmp(p, _T("./"), 2) == 0) {
				size_t n = _tcslen(p + 2);
				memcpy(p, p + 2, n*sizeof(HTTPCHAR));
				p[n] = 0;
			}
			else if (memcmp(p, _T(".../"), 4) == 0) {
				size_t n = _tcslen(p + 4);
				memcpy(p, p + 4, n*sizeof(HTTPCHAR));
				p[n] = 0;
			}
			else
				p++;
		}
		else
			p++;
	}
	if (fix) {
		return (FixUrlTransversal(path));
	}
	return (path);
}

/*******************************************************************************************************/
HTTPCHAR *RemoveParameters(HTTPCHAR *data)
{
	HTTPCHAR *p=data;

	while (*p)
	{
		if ((*p==_T(';')) || (*p==_T('#')) || (*p==_T('?')) || (*p==_T('&')) )
		{
			*p=0;
			break;
		}
		p++;
	}
	/*
	char *p=strchr(data,';');
	if (p) *p=0;
	p=strchr(data,'#');
	if (p) *p=0;


	p=strchr(data,'?');
	if (p) {
	*p=0;
	return(p+1);
	}
	p=strchr(data,'&');
	if (p) {
	*p=0;
	return(p+1);
	}
	*/
	return (NULL);
}
/*******************************************************************************************************/
static void test_mapper (struct taginfo *taginfo, void *arg, HTTPCHAR *lpHostname, HTTPCHAR *lpBaseURL, struct httpdata *response)
{

	//printf ("[%s%s]\n", taginfo->end_tag_p ? "/" : "", taginfo->name);
	//	int valid = 0;
	for (int i = 0; i < taginfo->nattrs; i++)
	{
		if (i==taginfo->nattrs-1)
		{
			//struct attr_pair *attrs = &taginfo->attrs[i];
			//printf("aki\n");

		}

		if (IsValidHTMLTag(taginfo->attrs[i].name,taginfo->name))
		{
			//			int add = 0;
			//char num[6];
			//MessageBox( NULL, taginfo->name,taginfo->attrs[i].name, MB_OK|MB_ICONINFORMATION );
			//Delete GET Parameters
			// remove quotes \"value"\ or "\value\"
			int salir=0;
			HTTPCHAR *p =taginfo->attrs[i].value;
			int n=0;


			while ((*p) && (!salir))
			{
				if ( (*p==_T('\\')) || (*p==_T('\"')) ||  (*p==_T('+')) ||  (*p==_T('\''))  ||  (*p==_T('('))  ||  (*p==_T(')')) ||  (*p==_T(' ')) ||  (*p==_T('\r')) ||  (*p==_T('\n'))   )
				{
					p++;
					n++;
				} else salir=1;
			}
			salir=0;
			while ((*p) && (!salir) && (n>0))
			{
				size_t l =_tcslen(p)-1;
				if ( (p[l]==_T('\\')) || (p[l]==_T('\"')) ||  (p[l]==_T('+')) ||  (p[l]==_T('\''))  ||  (p[l]==_T('('))  ||  (p[l]==_T(')')) ||  (p[l]==_T('\r')) ||  (p[l]==_T('\n'))   )
				{
					p[l]=0;
					l--;
					n--;
				} else salir=1;
			}

			//char *parameters =
			RemoveParameters(p);

			HTTPCHAR tmp[2048];
			/*3
			Se trata de una URL completa...
			*/
			if ((_tcsnccmp(p, _T("http"), 4) == 0) && ((_tcsnccmp(p + 4, _T("://"),
				3) == 0) || (_tcsnccmp(p + 4, _T("s://"), 4) == 0))) {
					if (_tcschr(p, _T('\'')) || (_tcsstr(p, _T(" +")))) {
#ifdef _VERBOSE
						fwrite("\n----\n", 6, 1, filename);
						fwrite(taginfo->name, _tcslen(taginfo->name), 1, filename);
						fwrite("\n", 1, 1, filename);
						fwrite(taginfo->attrs[i].name, _tcslen(taginfo->attrs[i].name), 1, filename);
						fwrite("\n", 1, 1, filename);
						fwrite(taginfo->attrs[i].value, _tcslen(taginfo->attrs[i].value), 1, filename);
						fwrite("\n", 1, 1, filename);
#endif
					}
					else {

						HTTPCHAR *myhost = p + 7;
						if (p[4] == _T('s')) {
							myhost++;
						}
						HTTPCHAR *path = _tcschr(myhost, _T('/'));
						HTTPCHAR tags[256];
						_sntprintf(tags, sizeof(tags) / sizeof(HTTPCHAR) - 1, _T("%s %s"),
							taginfo->name, taginfo->attrs[i].name);
						tags[sizeof(tags) / sizeof(HTTPCHAR) - 1] = 0;
						if (path) {
							response->AddUrlCrawled(p, tags);
							// printf("%s %s-> %s\n",taginfo->name,taginfo->attrs[i].name,p);

						}
						else {
							size_t l = _tcslen(p);
							HTTPCHAR *x = (HTTPCHAR*)malloc(l + 2);
							if (x) {
								memcpy(x, p, l*sizeof(HTTPCHAR));
								x[l] = _T('/');
								x[l + 1] = 0;
								response->AddUrlCrawled(x, tags);
								free(x);
							}

							// printf("%s %s-> %s/\n",taginfo->name,taginfo->attrs[i].name,p);
						}
						// valid=1;
					}

			}
			else // se trata de un enlace relativo...
			{

				// MessageBox( NULL, tmp,"Nuevo path", MB_OK|MB_ICONINFORMATION );
				// MessageBox( NULL, urlptr,"pathreal ", MB_OK|MB_ICONINFORMATION );

				// HACK TODO:
				/*
				Se construyen dos urls pero despues no se usan correctamente!!

				*/
				if ((_tcsnccmp(p, _T("javascript"), 10) == 0)) {

					// MessageBox( NULL, p,"0", MB_OK|MB_ICONINFORMATION );
#ifdef _VERBOSE
					fwrite("\n----\n", 6, 1, javascript);
					fwrite(taginfo->name, strlen(taginfo->name), 1, javascript);
					fwrite("\n", 1, 1, javascript);
					fwrite(taginfo->attrs[i].name, strlen(taginfo->attrs[i].name), 1, javascript);
					fwrite("\n", 1, 1, javascript);
					fwrite(taginfo->attrs[i].value, strlen(taginfo->attrs[i].value), 1, javascript);
					fwrite("\n", 1, 1, javascript);
#endif
					// add = 0;
				}
				else if ((*p == _T(' ')) || (*p == _T('\'')) || (*p == _T('#')) ||
					(*p == _T('+')) || (_tcschr(p, _T('\''))) || (_tcschr(p, _T('\"'))) ||
					(_tcsstr(p, _T("mailto:"))) || (_tcsstr(p, _T("//:"))) ||
					(_tcsstr(p, _T(" +"))) || (_tcsstr(p, _T("<%")))) {

#ifdef _VERBOSE
						fwrite("\n----\n", 6, 1, invalid);
						fwrite(taginfo->name, strlen(taginfo->name), 1, invalid);
						fwrite("\n", 1, 1, invalid);
						fwrite(taginfo->attrs[i].name, strlen(taginfo->attrs[i].name), 1, invalid);
						fwrite("\n", 1, 1, invalid);
						fwrite(taginfo->attrs[i].value, strlen(taginfo->attrs[i].value), 1, invalid);
						fwrite("\n", 1, 1, invalid);
#endif
				}
				else {
					HTTPCHAR tags[256];
					_sntprintf(tags, sizeof(tags) - 1, _T("%s %s"), taginfo->name,
						taginfo->attrs[i].name);
					tags[sizeof(tags) - 1] = _T('\0');

					HTTPCHAR *urlptr = tmp + 7 + _tcslen(lpHostname);
					if (*p == _T('/')) {
						_sntprintf(tmp, sizeof(tmp) - 1, _T("http://%s%s"), lpHostname, p);
					}
					else {
						_stprintf(tmp, _T("http://%s%s%s"), lpHostname, lpBaseURL, p);
					}
					FixUrlTransversal(urlptr);
					response->AddUrlCrawled(tmp, tags);

					// printf("%s %s-> %s\n",taginfo->name,taginfo->attrs[i].name,tmp);
					// valid=1;
				}
			}

		}
		else // El parámetro encontrado no es uno seleccionado... vamos a verificar si debemos guardarlo..
		{
			// Extraer objetos que no machean. Ejemplo:
			// javascript:window.open('http://www.terra.es/deportes/futbol/directos/evento_champions_v2.cfm?id_evento=161736','blank','width=750,height=665,resizable=no,statusbar=no,scrollbars=no,top=0,left=0');window.location.href='http://www.terra.es/deportes/futbol/'

			HTTPCHAR *p = stristr(taginfo->attrs[i].value, _T("http"));
			// int ofst;
			if (p) {

				if ((_tcsnccmp(p + 4, _T("://"), 3) == 0) && (_tcsnccmp(p + 4, _T("s://"),
					4) == 0) && (_tcsnccmp(p + 4, _T("s%3A//"), 6) == 0) && (_tcsnccmp(p + 4,
					_T("%3A//"), 5) == 0)) {
						p = NULL;
				}
				else {

					// int add = 0;
					// char *parameters =
					RemoveParameters(p);
					HTTPCHAR *q = _tcschr(p, _T('\''));
					if (q)
						* q = 0;
					q = _tcschr(p, _T('\"'));
					if (q)
						* q = 0;

					HTTPCHAR tags[256];
					_sntprintf(tags, sizeof(tags) / sizeof(HTTPCHAR) - 1, _T("%s %s"),
						taginfo->name, taginfo->attrs[i].name);
					tags[sizeof(tags) / sizeof(HTTPCHAR) - 1] = 0;

					response->AddUrlCrawled(p, tags);
					// printf("*Gathered Link1: %s %s %s\n",taginfo->name,taginfo->attrs[i].name,p);
					// valid=1;

				}

			}
		}

	}

	// putchar ('\n');
	++*(int *)arg;
}
/*******************************************************************************************************/

/* Pool support.  A pool is a resizable chunk of memory.  It is first
allocated on the stack, and moved to the heap if it needs to be
larger than originally expected.  map_html_tags() uses it to store
the zero-terminated names and values of tags and attributes.

Thus taginfo->name, and attr->name and attr->value for each
attribute, do not point into separately allocated areas, but into
different parts of the pool, separated only by terminating zeros.
This ensures minimum amount of allocation and, for most tags, no
allocation because the entire pool is kept on the stack. */

struct pool {
	HTTPCHAR *contents; /* pointer to the contents. */
	int size; /* size of the pool. */
	int tail; /* next available position index. */
	bool resized; /* whether the pool has been resized
				  using malloc. */

	HTTPCHAR *orig_contents; /* original pool contents, usually
							 stack-allocated.  used by POOL_FREE
							 to restore the pool to the initial
							 state. */
	int orig_size;
};

/* Initialize the pool to hold INITIAL_SIZE bytes of storage. */

#define POOL_INIT(p, initial_storage, initial_size) do {        \
struct pool *P = (p);                                         \
	P->contents = (initial_storage);                              \
	P->size = (initial_size);                                     \
	P->tail = 0;                                                  \
	P->resized = false;                                           \
	P->orig_contents = P->contents;                               \
	P->orig_size = P->size;                                       \
} while (0)

/* Grow the pool to accomodate at least SIZE new bytes.  If the pool
already has room to accomodate SIZE bytes of data, this is a no-op. */

#define POOL_GROW(p, increase)                                  \
	GROW_ARRAY ((p)->contents, (p)->size, (p)->tail + (increase), \
	(p)->resized, HTTPCHAR)

/* Append text in the range [beg, end) to POOL.  No zero-termination
is done. */

#define POOL_APPEND(p, beg, end) do {                   \
	HTTPCSTR PA_beg = (beg);                           \
	int PA_size = (end) - PA_beg;                         \
	POOL_GROW (p, PA_size);                               \
	memcpy ((p)->contents + (p)->tail, PA_beg, PA_size);  \
	(p)->tail += PA_size;                                 \
} while (0)

/* Append one character to the pool.  Can be used to zero-terminate
pool strings. */

#define POOL_APPEND_CHR(p, ch) do {             \
	HTTPCHAR PAC_char = (ch);                         \
	POOL_GROW (p, 1);                             \
	(p)->contents[(p)->tail++] = PAC_char;        \
} while (0)

/* Forget old pool contents.  The allocated memory is not freed. */
#define POOL_REWIND(p) (p)->tail = 0

/* Free heap-allocated memory for contents of POOL.  This calls
xfree() if the memory was allocated through malloc.  It also
restores `contents' and `size' to their original, pre-malloc
values.  That way after POOL_FREE, the pool is fully usable, just
as if it were freshly initialized with POOL_INIT. */

#define POOL_FREE(p) do {                       \
struct pool *P = p;                           \
	if (P->resized)                               \
	xfree (P->contents);                        \
	P->contents = P->orig_contents;               \
	P->size = P->orig_size;                       \
	P->tail = 0;                                  \
	P->resized = false;                           \
} while (0)

/* Used for small stack-allocated memory chunks that might grow.  Like
DO_REALLOC, this macro grows BASEVAR as necessary to take
NEEDED_SIZE items of TYPE.

The difference is that on the first resize, it will use
malloc+memcpy rather than realloc.  That way you can stack-allocate
the initial chunk, and only resort to heap allocation if you
stumble upon large data.

After the first resize, subsequent ones are performed with realloc,
just like DO_REALLOC. */

#define GROW_ARRAY(basevar, sizevar, needed_size, resized, type) do {           \
	long ga_needed_size = (needed_size);                                          \
	long ga_newsize = (sizevar);                                                  \
	while (ga_newsize < ga_needed_size)                                           \
	ga_newsize <<= 1;                                                           \
	if (ga_newsize != (sizevar))                                                  \
{                                                                           \
	if (resized)                                                              \
	basevar =(type*) xrealloc (basevar, ga_newsize * sizeof (type));               \
	  else                                                                      \
{                                                                       \
	void *ga_new = xmalloc (ga_newsize * sizeof (type));                  \
	memcpy (ga_new, basevar, (sizevar) * sizeof (type));                  \
	(basevar) = (type *)ga_new;                                                   \
	resized = true;                                                       \
}                                                                       \
	(sizevar) = ga_newsize;                                                   \
}                                                                           \
} while (0)

/* Test whether n+1-sized entity name fits in P.  We don't support
IE-style non-terminated entities, e.g. "&ltfoo" -> "<foo".
However, "&lt;foo" will work, as will "&lt!foo", "&lt", etc.  In
other words an entity needs to be terminated by either a
non-alphanumeric or the end of string. */
#define FITS(p, n) (p + n == end || (p + n < end && !ISALNUM (p[n])))

/* Macros that test entity names by returning true if P is followed by
the specified characters. */
#define ENT1(p, c0) (FITS (p, 1) && p[0] == c0)
#define ENT2(p, c0, c1) (FITS (p, 2) && p[0] == c0 && p[1] == c1)
#define ENT3(p, c0, c1, c2) (FITS (p, 3) && p[0]==c0 && p[1]==c1 && p[2]==c2)

/* Increment P by INC chars.  If P lands at a semicolon, increment it
past the semicolon.  This ensures that e.g. "&lt;foo" is converted
to "<foo", but "&lt,foo" to "<,foo". */
#define SKIP_SEMI(p, inc) (p += inc, p < end && *p == _T(';') ? ++p : p)

/* Decode the HTML character entity at *PTR, considering END to be end
of buffer.  It is assumed that the "&" character that marks the
beginning of the entity has been seen at *PTR-1.  If a recognized
ASCII entity is seen, it is returned, and *PTR is moved to the end
of the entity.  Otherwise, -1 is returned and *PTR left unmodified.

The recognized entities are: &lt, &gt, &amp, &apos, and &quot. */

static int decode_entity(HTTPCSTR *ptr, HTTPCSTR end) {
	HTTPCSTR p = *ptr;
	int value = -1;

	if (++p == end)
		return -1;

	switch (*p++) {
	case _T('#'):
		/* Process numeric entities "&#DDD;" and "&#xHH;". */ {
			int digits = 0;
			value = 0;
			if (*p == _T('x'))
				for (++p; value < 256 && p < end && ISXDIGIT(*p); p++, digits++)
					value = (value << 4) + XDIGIT_TO_NUM(*p);
			else
				for (; value < 256 && p < end && ISDIGIT(*p); p++, digits++)
					value = (value * 10) + (*p - _T('0'));
			if (!digits)
				return -1;
			/* Don't interpret 128+ codes and NUL because we cannot
			portably reinserted them into HTML. */
			if (!value || (value&~0x7f))
				return -1;
			*ptr = SKIP_SEMI(p, 0);
			return value;
		}
		/* Process named ASCII entities. */
	case _T('g'):
		if (ENT1(p, _T('t')))
			value = _T('>'), *ptr = SKIP_SEMI(p, 1);
		break;
	case _T('l'):
		if (ENT1(p, _T('t')))
			value = _T('<'), *ptr = SKIP_SEMI(p, 1);
		break;
	case _T('a'):
		if (ENT2(p, _T('m'), _T('p')))
			value = _T('&'), *ptr = SKIP_SEMI(p, 2);
		else if (ENT3(p, _T('p'), _T('o'), _T('s')))
			/* handle &apos for the sake of the XML/XHTML crowd. */
			value = _T('\''), *ptr = SKIP_SEMI(p, 3);
		break;
	case _T('q'):
		if (ENT3(p, _T('u'), _T('o'), _T('t')))
			value = _T('\"'), *ptr = SKIP_SEMI(p, 3);
		break;
	}
	return value;
}
#undef ENT1
#undef ENT2
#undef ENT3
#undef FITS
#undef SKIP_SEMI

enum {
	AP_DOWNCASE = 1, AP_DECODE_ENTITIES = 2, AP_TRIM_BLANKS = 4
};

/* Copy the text in the range [BEG, END) to POOL, optionally
performing operations specified by FLAGS.  FLAGS may be any
combination of AP_DOWNCASE, AP_DECODE_ENTITIES and AP_TRIM_BLANKS
with the following meaning:

* AP_DOWNCASE -- downcase all the letters;

* AP_DECODE_ENTITIES -- decode the named and numeric entities in
the ASCII range when copying the string.

* AP_TRIM_BLANKS -- ignore blanks at the beginning and at the end
of text, as well as embedded newlines. */

static void convert_and_copy(struct pool *pool, HTTPCSTR beg, HTTPCSTR end,
	int flags) {
		int old_tail = pool->tail;

		/* Skip blanks if required.  We must do this before entities are
		processed, so that blanks can still be inserted as, for instance,
		`&#32;'. */
		if (flags & AP_TRIM_BLANKS) {
			while (beg < end && ISSPACE(*beg))
				++beg;
			while (end > beg && ISSPACE(end[-1]))
				--end;
		}

		if (flags & AP_DECODE_ENTITIES) {
			/* Grow the pool, then copy the text to the pool character by
			character, processing the encountered entities as we go
			along.

			It's safe (and necessary) to grow the pool in advance because
			processing the entities can only *shorten* the string, it can
			never lengthen it. */
			HTTPCSTR from = beg;
			HTTPCHAR *to;
			bool squash_newlines = !!(flags & AP_TRIM_BLANKS);

			POOL_GROW(pool, end - beg);
			to = pool->contents + pool->tail;

			while (from < end) {
				if (*from == _T('&')) {
					int entity = decode_entity(&from, end);
					if (entity != -1)
						* to++ = entity;
					else
						*to++ = *from++;
				}
				else if ((*from == _T('\n') || *from == _T('\r')) && squash_newlines)
					++from;
				else
					*to++ = *from++;
			}
			/* Verify that we haven't exceeded the original size.  (It
			shouldn't happen, hence the assert.) */
			assert(to - (pool->contents + pool->tail) <= end - beg);

			/* Make POOL's tail point to the position following the string
			we've written. */
			pool->tail = (int)(to - pool->contents);
			POOL_APPEND_CHR(pool, _T('\0'));
		}
		else {
			/* Just copy the text to the pool. */
			POOL_APPEND(pool, beg, end);
			POOL_APPEND_CHR(pool, _T('\0'));
		}

		if (flags & AP_DOWNCASE) {
			HTTPCHAR *p = pool->contents + old_tail;
			for (; *p; p++)
				* p = TOLOWER(*p);
		}
}

/* Originally we used to adhere to rfc 1866 here, and allowed only
letters, digits, periods, and hyphens as names (of tags or
attributes).  However, this broke too many pages which used
proprietary or strange attributes, e.g. <img src="a.gif"
v:shapes="whatever">.

So now we allow any character except:
* whitespace
* 8-bit and control chars
* characters that clearly cannot be part of name:
'=', '>', '/'.

This only affects attribute and tag names; attribute values allow
an even greater variety of characters. */

#define NAME_CHAR_P(x) ((x) > 32 && (x) < 127                           \
	&& (x) != _T('=') && (x) != _T('>') && (x) != _T('/'))

#ifdef STANDALONE
static int comment_backout_count;
#endif

/* Advance over an SGML declaration, such as <!DOCTYPE ...>.  In
strict comments mode, this is used for skipping over comments as
well.

To recap: any SGML declaration may have comments associated with
it, e.g.
<!MY-DECL -- isn't this fun? -- foo bar>

An HTML comment is merely an empty declaration (<!>) with a comment
attached, like this:
<!-- some stuff here -->

Several comments may be embedded in one comment declaration:
<!-- have -- -- fun -->

Whitespace is allowed between and after the comments, but not
before the first comment.  Additionally, this function attempts to
handle double quotes in SGML declarations correctly. */

static HTTPCSTR advance_declaration(HTTPCSTR beg, HTTPCSTR end) {
	HTTPCSTR p = beg;
	HTTPCHAR quote_char = _T('\0'); /* shut up, gcc! */
	HTTPCHAR ch;

	enum {
		AC_S_DONE, AC_S_BACKOUT, AC_S_BANG, AC_S_DEFAULT, AC_S_DCLNAME, AC_S_DASH1,
		AC_S_DASH2, AC_S_COMMENT, AC_S_DASH3, AC_S_DASH4, AC_S_QUOTE1, AC_S_IN_QUOTE,
		AC_S_QUOTE2
	} state = AC_S_BANG;

	if (beg == end)
		return beg;
	ch = *p++;

	/* It looked like a good idea to write this as a state machine, but
	now I wonder... */

	while (state != AC_S_DONE && state != AC_S_BACKOUT) {
		if (p == end)
			state = AC_S_BACKOUT;
		switch (state) {
		case AC_S_DONE:
		case AC_S_BACKOUT:
			break;
		case AC_S_BANG:
			if (ch == _T('!')) {
				ch = *p++;
				state = AC_S_DEFAULT;
			}
			else
				state = AC_S_BACKOUT;
			break;
		case AC_S_DEFAULT:
			switch (ch) {
			case _T('-'):
				state = AC_S_DASH1;
				break;
			case _T(' '):
			case _T('\t'):
			case _T('\r'):
			case _T('\n'):
				ch = *p++;
				break;
			case _T('>'):
				state = AC_S_DONE;
				break;
			case _T('\''):
			case _T('\"'):
				state = AC_S_QUOTE1;
				break;
			default:
				if (NAME_CHAR_P(ch))
					state = AC_S_DCLNAME;
				else
					state = AC_S_BACKOUT;
				break;
			}
			break;
		case AC_S_DCLNAME:
			if (ch == _T('-'))
				state = AC_S_DASH1;
			else if (NAME_CHAR_P(ch))
				ch = *p++;
			else
				state = AC_S_DEFAULT;
			break;
		case AC_S_QUOTE1:
			/* We must use 0x22 because broken assert macros choke on
			'"' and '\"'. */
			assert(ch == _T('\'') || ch == 0x22);
			quote_char = ch; /* cheating -- I really don't feel like
							 introducing more different states for
							 different quote characters. */
			ch = *p++;
			state = AC_S_IN_QUOTE;
			break;
		case AC_S_IN_QUOTE:
			if (ch == quote_char)
				state = AC_S_QUOTE2;
			else
				ch = *p++;
			break;
		case AC_S_QUOTE2:
			assert(ch == quote_char);
			ch = *p++;
			state = AC_S_DEFAULT;
			break;
		case AC_S_DASH1:
			assert(ch == _T('-'));
			ch = *p++;
			state = AC_S_DASH2;
			break;
		case AC_S_DASH2:
			switch (ch) {
			case _T('-'):
				ch = *p++;
				state = AC_S_COMMENT;
				break;
			default:
				state = AC_S_BACKOUT;
			}
			break;
		case AC_S_COMMENT:
			switch (ch) {
			case _T('-'):
				state = AC_S_DASH3;
				break;
			default:
				ch = *p++;
				break;
			}
			break;
		case AC_S_DASH3:
			assert(ch == _T('-'));
			ch = *p++;
			state = AC_S_DASH4;
			break;
		case AC_S_DASH4:
			switch (ch) {
			case _T('-'):
				ch = *p++;
				state = AC_S_DEFAULT;
				break;
			default:
				state = AC_S_COMMENT;
				break;
			}
			break;
		}
	}

	if (state == AC_S_BACKOUT) {
#ifdef STANDALONE
		++comment_backout_count;
#endif
		return beg + 1;
	}
	/*
	char *data = (char*)malloc(p - beg +1);
	memcpy(data,beg,p-beg);
	data[p-beg]='\0';
	printf("ignorado:\n%s\n",data);
	free(data);
	*/
	return p;
}

/* Find the first occurrence of the substring "-->" in [BEG, END) and
return the pointer to the character after the substring.  If the
substring is not found, return NULL. */

static HTTPCSTR find_comment_end(HTTPCSTR beg, HTTPCSTR end) {
	/* Open-coded Boyer-Moore search for "-->".  Examine the third char;
	if it's not '>' or '-', advance by three characters.  Otherwise,
	look at the preceding characters and try to find a match. */

	HTTPCSTR p = beg - 1;

	while ((p += 3) < end)
		switch (p[0]) {
		case _T('>'):
			if (p[-1] == _T('-') && p[-2] == _T('-'))
				return p + 1;
			break;
		case _T('-'):
at_dash :
			if (p[-1] == _T('-')) {
at_dash_dash:
				if (++p == end)
					return NULL;
				switch (p[0]) {
				case _T('>'):
					return p + 1;
				case _T('-'):
					goto at_dash_dash;
				}
			}
			else {
				if ((p += 2) >= end)
					return NULL;
				switch (p[0]) {
				case _T('>'):
					if (p[-1] == _T('-'))
						return p + 1;
					break;
				case _T('-'):
					goto at_dash;
				}
			}
	}
	return NULL;
}

/* Return true if the string containing of characters inside [b, e) is
present in hash table HT. */

static bool name_allowed(const struct hash_table *ht, HTTPCSTR b, HTTPCSTR e) {
	HTTPCHAR *copy = NULL;
	if (!ht)
		return true;
	(b, e, copy);
	return hash_table_get(ht, copy) != NULL;
}

/* Advance P (a char pointer), with the explicit intent of being able
to read the next character.  If this is not possible, go to finish. */

#define ADVANCE(p) do {                         \
	++p;                                          \
	if (p >= end)                                 \
	goto finish;                                \
} while (0)

/* Skip whitespace, if any. */

#define SKIP_WS(p) do {                         \
	while (ISSPACE (*p)) {                        \
	ADVANCE (p);                                \
	}                                             \
} while (0)

/* Skip non-whitespace, if any. */

#define SKIP_NON_WS(p) do {                     \
	while (!ISSPACE (*p)) {                       \
	ADVANCE (p);                                \
	}                                             \
} while (0)

#ifdef STANDALONE
static int tag_backout_count;
#endif

/* Map MAPFUN over HTML tags in TEXT, which is SIZE characters long.
MAPFUN will be called with two arguments: pointer to an initialized
struct taginfo, and MAPARG.

ALLOWED_TAGS and ALLOWED_ATTRIBUTES are hash tables the keys of
which are the tags and attribute names that this function should
use.  If ALLOWED_TAGS is NULL, all tags are processed; if
ALLOWED_ATTRIBUTES is NULL, all attributes are returned.

(Obviously, the caller can filter out unwanted tags and attributes
just as well, but this is just an optimization designed to avoid
unnecessary copying of tags/attributes which the caller doesn't
care about.) */

void map_html_tags(HTTPCSTR text, size_t size, void(*mapfun)(struct taginfo *,
	void *, HTTPCHAR*, HTTPCHAR*, httpdata*), void *maparg, int flags,
	const struct hash_table *allowed_tags,
	const struct hash_table *allowed_attributes, HTTPCHAR *lphostname,
	HTTPCHAR *lpURL, struct httpdata *response) {
		/* storage for strings passed to MAPFUN callback; if 256 bytes is
		too little, POOL_APPEND allocates more with malloc. */
		HTTPCHAR pool_initial_storage[256];
		struct pool pool;

		HTTPCSTR p = text;
		HTTPCSTR end = text + size;

		struct attr_pair attr_pair_initial_storage[8];
		int attr_pair_size = countof(attr_pair_initial_storage);
		bool attr_pair_resized = false;
		struct attr_pair *pairs = attr_pair_initial_storage;

		if (!size)
			return;

		POOL_INIT(&pool, pool_initial_storage, countof(pool_initial_storage));

		{
			int nattrs, end_tag;
			HTTPCSTR tag_name_begin, *tag_name_end;
			HTTPCSTR tag_start_position;
			bool uninteresting_tag;

look_for_tag:
			POOL_REWIND(&pool);

			nattrs = 0;
			end_tag = 0;

			/* Find beginning of tag.  We use memchr() instead of the usual
			looping with ADVANCE() for speed. */
			p = (HTTPCHAR*) memchr(p, _T('<'), end - p);
			if (!p)
				goto finish;

			tag_start_position = p;
			ADVANCE(p);

			/* Establish the type of the tag (start-tag, end-tag or
			declaration). */
			if (*p == _T('!')) {
				if (!(flags & MHT_STRICT_COMMENTS) && p < end + 3 && p[1] == _T('-')
					&& p[2] == _T('-')) {
						/* If strict comments are not enforced and if we know
						we're looking at a comment, simply look for the
						terminating "-->".  Non-strict is the default because
						it works in other browsers and most HTML writers can't
						be bothered with getting the comments right. */
						HTTPCSTR comment_end = find_comment_end(p + 3, end);
						if (comment_end) {
							/* int len =   comment_end -p ;
							printf("\n** COMENTARIO de len %i: |",len);
							for(int j=0;j<len;j++) printf("%c",p[j]);
							printf("| **\n");
							*/
							p = comment_end;

						}

				}
				else {
					/* Either in strict comment mode or looking at a non-empty
					declaration.  Real declarations are much less likely to
					be misused the way comments are, so advance over them
					properly regardless of strictness. */
					p = advance_declaration(p, end);
				}
				if (p == end)
					goto finish;
				goto look_for_tag;
			}
			else if (*p == _T('/')) {
				end_tag = 1;
				ADVANCE(p);
			}
			tag_name_begin = p;
			while (NAME_CHAR_P(*p))
				ADVANCE(p);
			if (p == tag_name_begin)
				goto look_for_tag;
			tag_name_end = p;
			SKIP_WS(p);
			if (end_tag && *p != _T('>'))
				goto backout_tag;

			if (!name_allowed(allowed_tags, tag_name_begin, tag_name_end))
				/* We can't just say "goto look_for_tag" here because we need
				the loop below to properly advance over the tag's attributes. */
				uninteresting_tag = true;
			else {
				uninteresting_tag = false;
				convert_and_copy(&pool, tag_name_begin, tag_name_end, AP_DOWNCASE);
			}

			/* Find the attributes. */
			while (1) {
				HTTPCSTR attr_name_begin, *attr_name_end;
				HTTPCSTR attr_value_begin, *attr_value_end;
				HTTPCSTR attr_raw_value_begin, *attr_raw_value_end;
				int operation = AP_DOWNCASE; /* stupid compiler. */

				SKIP_WS(p);

				if (*p == _T('/')) {
					/* A slash at this point means the tag is about to be
					closed.  This is legal in XML and has been popularized
					in HTML via XHTML. */
					/* <foo a=b c=d /> */
					/* ^ */
					ADVANCE(p);
					SKIP_WS(p);
					if (*p != _T('>'))
						goto backout_tag;
				}

				/* Check for end of tag definition. */
				if (*p == _T('>'))
					break;

				/* Establish bounds of attribute name. */
				attr_name_begin = p; /* <foo bar ...> */
				/* ^ */
				while (NAME_CHAR_P(*p))
					ADVANCE(p);
				attr_name_end = p; /* <foo bar ...> */
				/* ^ */
				if (attr_name_begin == attr_name_end)
					goto backout_tag;

				/* Establish bounds of attribute value. */
				SKIP_WS(p);
				if (NAME_CHAR_P(*p) || *p == _T('/') || *p == _T('>')) {
					/* Minimized attribute syntax allows `=' to be omitted.
					For example, <UL COMPACT> is a valid shorthand for <UL
					COMPACT="compact">.  Even if such attributes are not
					useful to Wget, we need to support them, so that the
					tags containing them can be parsed correctly. */
					attr_raw_value_begin = attr_value_begin = attr_name_begin;
					attr_raw_value_end = attr_value_end = attr_name_end;
				}
				else if (*p == _T('=')) {
					ADVANCE(p);
					SKIP_WS(p);
					if (*p == _T('\"') || *p == _T('\'')) {
						bool newline_seen = false;
						HTTPCHAR quote_char = *p;
						attr_raw_value_begin = p;
						ADVANCE(p);
						attr_value_begin = p; /* <foo bar="baz"> */
						/* ^ */
						while (*p != quote_char) {
							if (!newline_seen && *p == _T('\n')) {
								/* If a newline is seen within the quotes, it
								is most likely that someone forgot to close
								the quote.  In that case, we back out to
								the value beginning, and terminate the tag
								at either `>' or the delimiter, whichever
								comes first.  Such a tag terminated at `>'
								is discarded. */
								p = attr_value_begin;
								newline_seen = true;
								continue;
							}
							else if (newline_seen && *p == _T('>'))
								break;
							ADVANCE(p);
						}
						attr_value_end = p; /* <foo bar="baz"> */
						/* ^ */
						if (*p == quote_char)
							ADVANCE(p);
						else
							goto look_for_tag;
						attr_raw_value_end = p; /* <foo bar="baz"> */
						/* ^ */
						operation = AP_DECODE_ENTITIES;
						if (flags & MHT_TRIM_VALUES)
							operation |= AP_TRIM_BLANKS;
					}
					else {
						attr_value_begin = p; /* <foo bar=baz> */
						/* ^ */
						/* According to SGML, a name token should consist only
						of alphanumerics, . and -.  However, this is often
						violated by, for instance, `%' in `width=75%'.
						We'll be liberal and allow just about anything as
						an attribute value. */
						while (!ISSPACE(*p) && *p != _T('>'))
							ADVANCE(p);
						attr_value_end = p; /* <foo bar=baz qux=quix> */
						/* ^ */
						if (attr_value_begin == attr_value_end)
							/* <foo bar=> */
							/* ^ */
							goto backout_tag;
						attr_raw_value_begin = attr_value_begin;
						attr_raw_value_end = attr_value_end;
						operation = AP_DECODE_ENTITIES;
					}
				}
				else {
					/* We skipped the whitespace and found something that is
					neither `=' nor the beginning of the next attribute's
					name.  Back out. */
					goto backout_tag; /* <foo bar [... */
					/* ^ */
				}

				/* If we're not interested in the tag, don't bother with any
				of the attributes. */
				if (uninteresting_tag)
					continue;

				/* If we aren't interested in the attribute, skip it.  We
				cannot do this test any sooner, because our text pointer
				needs to correctly advance over the attribute. */
				if (!name_allowed(allowed_attributes, attr_name_begin, attr_name_end))
					continue;

				GROW_ARRAY(pairs, attr_pair_size, nattrs + 1, attr_pair_resized,
				struct attr_pair);

				pairs[nattrs].name_pool_index = pool.tail;
				convert_and_copy(&pool, attr_name_begin, attr_name_end, AP_DOWNCASE);

				pairs[nattrs].value_pool_index = pool.tail;
				convert_and_copy(&pool, attr_value_begin, attr_value_end, operation);
				pairs[nattrs].value_raw_beginning = attr_raw_value_begin;
				pairs[nattrs].value_raw_size = (int)(attr_raw_value_end - attr_raw_value_begin);
				++nattrs;
			}

			if (uninteresting_tag) {
				ADVANCE(p);
				goto look_for_tag;
			}

			/* By now, we have a valid tag with a name and zero or more
			attributes.  Fill in the data and call the mapper function. */ {
				int i;
				struct taginfo taginfo;

				taginfo.name = pool.contents;
				taginfo.end_tag_p = end_tag;
				taginfo.nattrs = nattrs;
				/* We fill in the char pointers only now, when pool can no
				longer get realloc'ed.  If we did that above, we could get
				hosed by reallocation.  Obviously, after this point, the pool
				may no longer be grown. */
				for (i = 0; i < nattrs; i++) {
					pairs[i].name = pool.contents + pairs[i].name_pool_index;
					pairs[i].value = pool.contents + pairs[i].value_pool_index;
				}
				taginfo.attrs = pairs;
				taginfo.start_position = tag_start_position;
				taginfo.end_position = p + 1;
				mapfun(&taginfo, maparg, lphostname, lpURL, response);
				ADVANCE(p);
			} goto look_for_tag;

backout_tag:
#ifdef STANDALONE
			++tag_backout_count;
#endif
			/* The tag wasn't really a tag.  Treat its contents as ordinary
			data characters. */
			p = tag_start_position + 1;
			goto look_for_tag;
		}

finish:
		POOL_FREE(&pool);
		if (attr_pair_resized)
			xfree(pairs);
}

#undef ADVANCE
#undef SKIP_WS
#undef SKIP_NON_WS

typedef struct _spiderItem {
	HTTPCHAR *path;
	HTTPCHAR *parameters;
	HTTPCHAR *urlcookie;
	HTTPCHAR *hostname;
	int SSL;
	int port;
} SPIDERITEM;

void HTTPAPI::doSpider(HTTPCHAR *host, HTTPCHAR *FullPath, httpdata* response) {
	if (!response->DataSize) {
		return;
	}
	int tag_counter = 0;
	HTTPCHAR *p = FullPath + 1;
	HTTPCHAR *l;
	do {
		l = _tcschr(p, _T('/'));
		if (l) {
			p = l + 1;
		}
	}
	while (l);
	*p = 0;
	map_html_tags(response->Data, response->DataSize, test_mapper, &tag_counter, 0,
		NULL, NULL, host, FullPath, response);

}

/*******************************************************************************************************/
#endif
