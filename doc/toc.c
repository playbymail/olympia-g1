
#include	<stdio.h>

#define		LEN	1024


char *
dots(s)
char *s;
{
	char *t;
	char *u;

	for (t = s; *t; t++)
		if (strncmp(t, "time:", 5) == 0)
			break;

	if (!*t)
		return s;

	t -= 2;

	if (t <= s)
		return s;

	for (u = t; u > &s[10]; u--)
		if (*u != ' ')
			break;

	if (u <= s)
		return s;

	u += 2;

	if (u >= t)
		return s;

	for (; u <= t; u++)
		*u = '.';

	return s;
}

main()
{
	char last[LEN] = "?oops";
	char buf[LEN];
	char *p;

	while (fgets(buf, LEN, stdin) != NULL)
	{
		for (p = buf; *p && *p != '\n'; p++)
			;

		*p = '\0';

		if (strncmp(buf, "--", 2) == 0)
		{
			if (*last)
				printf("\t%s\n", dots(last));
		}
		else if (strncmp(buf, "==", 2) == 0)
			printf("%s\n", last);

		strcpy(last, buf);
	}
}


