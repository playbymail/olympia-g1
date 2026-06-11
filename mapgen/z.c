
#include	<stdio.h>
#include	"z.h"


void *
my_malloc(unsigned size)
{
	char *p;

	p = malloc(size);

	if (p == NULL) {
		fprintf(stderr, "my_malloc: out of memory (can't malloc "
				"%d bytes)\n", size);
		exit(1);
	}

	bzero(p, size);

	return p;
}


void *
my_realloc(void *ptr, unsigned size)
{
	char *p;

	if (ptr == NULL)
		p = malloc(size);
	else
		p = realloc(ptr, size);

	if (p == NULL) {
		fprintf(stderr, "my_realloc: out of memory (can't realloc "
				"%d bytes)\n", size);
		exit(1);
	}

	return p;
}


char *
str_save(char *s)
{
	char *p;

	p = my_malloc(strlen(s) + 1);
	strcpy(p, s);

	return p;
}


void
asfail(char *file, int line, char *cond)
{
	fprintf(stderr, "assertion failure: %s (%d): %s\n",
						file, line, cond);
	abort();
	exit(1);
}


void
lcase(char *s)
{

	while (*s)
	{
		*s = tolower(*s);
		s++;
	}
}


/*
 *  Line reader with no size limits
 *  strips newline off end of line
 */

#define	GETLIN_ALLOC	256

char *
getlin(FILE *fp)
{
	static char *buf = NULL;
	static unsigned int size = 0;
	int len;
	int c;

	len = 0;

	while ((c = fgetc(fp)) != EOF)
	{
		if (len + 1 >= size)
		{
			size += GETLIN_ALLOC;
			buf = my_realloc(buf, size);
		}

		if (c == '\n')
		{
			buf[len] = '\0';
			return buf;
		}

		buf[len++] = (char) c;
	}

	if (len == 0)
		return NULL;

	buf[len] = '\0';

	return buf;
}


/*
 *  Get line, remove leading and trailing whitespace
 */

char *
getlin_ew(FILE *fp)
{
	char *line;
	char *p;

	line = getlin(fp);

	if (line)
	{
		while (*line && iswhite(*line))
			line++;			/* eat leading whitespace */

		for (p = line; *p; p++)
			if (*p < 32 || *p == '\t')	/* remove ctrl chars */
				*p = ' ';
		p--;
		while (p >= line && iswhite(*p))
		{				/* eat trailing whitespace */
			*p = '\0';
			p--;
		}
	}

	return line;
}


#define	COPY_LEN	1024

void
copy_fp(FILE *a, FILE *b)
{
	char buf[COPY_LEN];

	while (fgets(buf, COPY_LEN, a) != NULL)
		fputs(buf, b);
}


int
i_strcmp(char *s, char *t)
{
	char a, b;

	do {
		a = tolower(*s);
		b = tolower(*t);
		s++;
		t++;
		if (a != b)
			return a - b;
	} while (a);

	return 0;
}


int
i_strncmp(char *s, char *t, int n)
{
	char a, b;

	do {
		a = tolower(*s);
		b = tolower(*t);
		if (a != b)
			return a - b;
		s++;
		t++;
		n--;
	} while (a && n > 0);

	return 0;
}


#ifdef USE_OUR_RND

unsigned short seed[3];

void
init_random(void)
{
	long l;

	for (l = 0; l < 3; l++)
	{
		char variable[128];
		sprintf(variable, "G1_MAPGEN_SEED_%ld", l + 1);
		char *value = getenv(variable);
		if (value == NULL)
		{
			seed[l] = 0;
		} else
		{
			seed[l] = (unsigned short)atoi(value);
		}
	}

	if (seed[0] == 0 && seed[1] == 0 && seed[2] == 0)
	{
		l = time(NULL);
		seed[0] = l & 0xFFFF;
		seed[1] = getpid();
		seed[2] = l >> 16;
	}
}


int
rnd(int low, int high)
{
	extern double erand48();

	return (int) (erand48(seed) * (high - low + 1) + low);
}

#else	/* ifdef SYSV */

init_random() {
	long l;

	srandom(l);
}


rnd(low, high)
int low;
int high;
{

	return random() % (high - low + 1) + low;
}
#endif	/* ifdef SYSV */


void
test_random(void)
{
	int i;

	if (isatty(1))
	    for (i = 0; i < 10; i++)
		printf("%3d  %3d  %3d  %3d  %3d  %3d  %3d  %3d  %3d  %3d\n",
			rnd(1, 10), rnd(1, 10), rnd(1, 10), rnd(1, 10),
			rnd(1, 10), rnd(1, 10), rnd(1, 10), rnd(1, 10),
			rnd(1, 10), rnd(1, 10));
	else
	    for (i = 0; i < 100; i++)
		printf("%d\n", rnd(1, 10));

	for (i = -10; i >= -16; i--)
		printf("rnd(%d, %d) == %d\n", -3, i, rnd(-3, i));
}






#if 0
static char *read_buffer = NULL;
static int read_size = 0;
static int read_len = 0;
static int read_pos = 0;

#define	READ_CHUNK	4096


int
read_file(char *name)
{
	int fd;
	int i;
	int n;

	fd = open(name, 0);
	if (fd < 0)
	{
		fprintf(stderr, "can't read %s: ", name);
		perror("");
		return FALSE;
	}

	read_len = 0;

	do {
	
		if (read_size - read_len < READ_CHUNK)
		{
			read_buffer = my_realloc(read_buffer,
						read_size + READ_CHUNK);
			read_size += READ_CHUNK;
		}

		n = read(fd, &read_buffer[read_len], READ_CHUNK);

		if (n < 0)
		{
			fprintf(stderr, "error reading %s: ", name);
			perror("");
			close(fd);
			return FALSE;
		}

		read_len += n;

	} while (n > 0);

	close(fd);
	read_pos = 0;

	for (i = 0; i < read_len; i++)
		if (read_buffer[i] == '\n')
			read_buffer[i] = '\0';

	return TRUE;
}


char *
read_getlin()
{
	char *p;

	if (read_pos >= read_len)
		return NULL;

	p = &read_buffer[read_pos];

	while (read_pos < read_len && read_buffer[read_pos] != '\0')
		read_pos++;
	read_pos++;

	return p;
}


char *
read_getlin_ew()
{
	char *line;
	char *p;

	line = read_getlin();

	if (line)
	{
		while (*line && iswhite(*line))
			line++;			/* eat leading whitespace */

		for (p = line; *p; p++)
			if (*p < 32 || *p == '\t')	/* remove ctrl chars */
				*p = ' ';
		p--;
		while (p >= line && iswhite(*p))
		{				/* eat trailing whitespace */
			*p = '\0';
			p--;
		}
	}

	return line;
}
#endif
