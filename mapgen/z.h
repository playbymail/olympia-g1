#include <stdlib.h>
#include <string.h>
#include <unistd.h>	/* BUGFIX (modernization): getpid, isatty */
#include <time.h>	/* BUGFIX (modernization): time */

#define	TRUE	1
#define	FALSE	0

#define bzero(a,n)		memset(a, '\0', n)
#define bcopy(a,b,n)		memcpy(b, a, n)

#define	abs(n)		((n) < 0 ? ((n) * -1) : (n))

#define	isalpha(c)	(((c)>='a' && (c)<='z') || ((c)>='A' && (c)<='Z'))
#define	isdigit(c)	((c) >= '0' && (c) <= '9')
#define	iswhite(c)	((c) == ' ' || (c) == '\t')

#define	tolower(c)	(((c) >= 'A' && (c) <= 'Z') ? ((c) - 'A' + 'a') : (c))
#define	toupper(c)	(((c) >= 'a' && (c) <= 'z') ? ((c) - 'a' + 'A') : (c))

extern void *my_malloc(unsigned size);
extern void *my_realloc(void *ptr, unsigned size);
extern char *str_save(char *);

extern char *getlin(FILE *);
extern char *getlin_ew(FILE *);
extern int i_strncmp(char *s, char *t, int n);
extern int i_strcmp(char *s, char *t);
extern int rnd(int low, int high);

/*
 *  Assertion verifier
 */

#ifdef __STDC__
#define	assert(p)	if(! (p)) asfail(__FILE__, __LINE__, #p);
#else
#define	assert(p)	if(! (p)) asfail(__FILE__, __LINE__, "p");
#endif

int read_file(char *name);
char *read_getlin(void);
char *read_getlin_ew(void);

extern void asfail(char *file, int line, char *cond);
extern void lcase(char *s);
extern void copy_fp(FILE *a, FILE *b);
extern void init_random(void);
extern void test_random(void);
