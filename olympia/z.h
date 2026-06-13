
/*
 * BUGFIX (modernization): pull in the libc headers BEFORE the bzero/bcopy/abs
 * macros below shadow those names.  z.h is included (ahead of oly.h) by every
 * engine translation unit, so this is the single chokepoint that supplies real
 * prototypes for the string/stdlib/io functions the engine calls.  The macros
 * must come after these includes or they mangle the system declarations.
 */
#include	<stdlib.h>	/* malloc, realloc, free, exit, abort, atoi, system */
#include	<string.h>	/* strlen, strcpy, strcat, strchr, strcmp, memset */
#include	<unistd.h>	/* sleep, unlink, getpid, isatty, close, read */
#include	<fcntl.h>	/* open */
#include	<time.h>	/* time */
#include	<sys/types.h>
#include	<sys/stat.h>	/* mkdir, chmod */

#define	TRUE	1
#define	FALSE	0

#define		LEN		2048	/* generic string max length */

#define bzero(a,n)		memset(a, '\0', n)
#define bcopy(a,b,n)		memcpy(b, a, n)

#define	abs(n)		((n) < 0 ? ((n) * -1) : (n))

#define	isalpha(c)	(((c)>='a' && (c)<='z') || ((c)>='A' && (c)<='Z'))
#define	isdigit(c)	((c) >= '0' && (c) <= '9')
#define	iswhite(c)	((c) == ' ' || (c) == '\t')

#if 1
#define	tolower(c)	(lower_array[c])
extern char lower_array[];
#else
#define	tolower(c)	(((c) >= 'A' && (c) <= 'Z') ? ((c) - 'A' + 'a') : (c))
#endif

#define	toupper(c)	(((c) >= 'a' && (c) <= 'z') ? ((c) - 'a' + 'A') : (c))

#if 0
#define	max(a,b)	((a) > (b) ? (a) : (b))
#define	min(a,b)	((a) < (b) ? (a) : (b))
#endif

extern void *my_malloc(unsigned size);
extern void *my_realloc(void *ptr, unsigned size);
extern void my_free(void *ptr);
extern char *str_save(char *);

extern char *getlin(FILE *);
extern char *getlin_ew(FILE *);
extern int i_strncmp(char *s, char *t, int n);
extern int i_strcmp(char *s, char *t);
extern int fuzzy_strcmp(char *, char *);
extern int rnd(int low, int high);

/*
 *  Assertion verifier
 */

extern void asfail(char *file, int line, char *cond);

#ifdef __STDC__
#define	assert(p)	if(!(p)) asfail(__FILE__, __LINE__, #p);
#else
#define	assert(p)	if(!(p)) asfail(__FILE__, __LINE__, "p");
#endif

extern int readfile(char *path);
extern char *readlin(void);
extern char *readlin_ew(void);
extern char *eat_leading_trailing_whitespace(char *s);

extern void copy_fp(FILE *a, FILE *b);
extern void lcase(char *s);
extern void init_lower(void);
extern void init_random(void);
extern void test_random(void);

/* drand48-family pseudo-random number generator (z.c) */
extern double drand48(void);
extern double erand48(unsigned short *xsubi);
extern long lrand48(void);
extern long nrand48(unsigned short *xsubi);
extern long mrand48(void);
extern long jrand48(unsigned short *xsubi);
extern void srand48(long seedval);
extern unsigned short *seed48(unsigned short seed16v[3]);
extern void lcong48(unsigned short param[7]);
