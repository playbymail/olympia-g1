
#include	<stdio.h>
#include	<unistd.h>
#include	"z.h"
#include	"oly.h"


/*
 *  pretty_data_files:  include parenthesisted names in the data files,
 *  to make them easier to read.
 */

int pretty_data_files = FALSE;

int immediate = TRUE;
int immed_after = FALSE;
int immed_see_all = FALSE;
int flush_always = FALSE;
int time_self = FALSE;		/* print timing info */
int save_flag = FALSE;

int main(int argc, char **argv)
{
	extern int optind, opterr;
	extern char *optarg;
	int errflag = 0;
	int c;
	int run_flag = FALSE;
	int add_flag = FALSE;
	int eat_flag = FALSE;
	int mail_now = FALSE;
	int acct_flag = FALSE;

	printf("\tsizeof(struct box) = %zu\n", sizeof(struct box));
	setbuf(stderr, NULL);

	call_init_routines();

	while ((c = getopt(argc, argv, "aefirl:pR?StMTA")) != EOF)
	{
		switch (c)
		{
		case 'a':
			add_flag = TRUE;
			immediate = FALSE;
			break;

		case 'A':
			acct_flag = TRUE;
			break;

		case 'f':
			flush_always = TRUE;
			setbuf(stdout, NULL);
			break;

		case 'e':
			eat_flag = TRUE;
			immediate = FALSE;
			break;

		case 'i':
			immed_after = TRUE;
			break;

		case 'l':		/* set libdir */
			libdir = str_save(optarg);
			break;

		case 'p':
			pretty_data_files = !pretty_data_files;
			break;

		case 'r':		/* run a turn */
			immediate = FALSE;
			run_flag = TRUE;
			break;

		case 'R':		/* test random number generator */
			test_random();
			exit(0);

		case 'S':		/* save database when done */
			save_flag = TRUE;
			break;

		case 't':
			ilist_test();
			exit_views_test();
			exit(0);

		case 'T':
			time_self = TRUE;
			break;

		case 'M':
			mail_now = TRUE;
			break;

		default:
			errflag++;
		}
	}

	if (errflag)
	{
	    fprintf(stderr, "usage: oly [options]\n");
	    fprintf(stderr, "  -a        Add new players mode\n");
	    fprintf(stderr, "  -e        Eat orders from libdir/spool\n");
	    fprintf(stderr, "  -f        Don't buffer files for debugging\n");
	    fprintf(stderr, "  -i        Immediate mode\n");
	    fprintf(stderr, "  -l dir    Specify libdir, default ./lib\n");
	    fprintf(stderr, "  -p        Don't make data files pretty\n");
	    fprintf(stderr, "  -r        Run a turn\n");
	    fprintf(stderr, "  -R        Test the random number generator\n");
	    fprintf(stderr, "  -S        Save the database at completion\n");
	    fprintf(stderr, "  -t        Test ilist code\n");
	    fprintf(stderr, "  -T        Print timing info\n");
	    fprintf(stderr, "  -M        Mail reports\n");
	    fprintf(stderr, "  -A        Charge player accounts\n");
	    exit(1);
	}

	/* BUGFIX (modernization): allow running without adding players */
	if (run_flag && add_flag)
	{
		immediate = FALSE;
	}

	load_db();

	if (eat_flag)
	{
		eat_loop();
		exit(0);
	}

	if (run_flag)
	{
		open_logfile();
		open_times();

		show_day = TRUE;
		process_orders();
		post_month();
		show_day = FALSE;

		determine_output_order();
		turn_end_loc_reports();
		list_order_templates();
		player_ent_info();
		character_report();

		player_banner();
		if (acct_flag)
		{
			charge_account();
		}
		else
		{
			printf("\tcharge_account: skipped (use -A)\n");
		}
		report_account();
		summary_report();
		player_report();

		scan_char_skill_lore();
		show_lore_sheets();
#if 0
		list_new_players();
#endif
		gm_report(gm_player);
		gm_show_all_skills(skill_player);
		if (add_flag)
		{
			add_new_players();
			add_flag = FALSE;
		}
		else
		{
			printf("\tadd_new_players: skipped (use -a)\n");
		}
		gen_include_section();		/* must be last */
		close_logfile();

		write_player_list();
		write_email();
		write_totimes();
		write_forwards();
		write_factions();
	}

	if (add_flag)
	{
		new_player_top(mail_now);
		mail_now = FALSE;
	}

	if (immediate || immed_after)
	{
		immediate = TRUE;

		open_logfile();
		immediate_commands();
		close_logfile();
	}

	check_db();			/* check database integrity */

	if (save_flag)
		save_db();

	if (save_flag && run_flag)
		save_logdir();

	if (mail_now)
		mail_reports();

	times_masthead();
	close_times();
	stage(NULL);

	{
	extern int malloc_size, realloc_size;

	printf("\tmalloc, realloc = %d, %d\n", malloc_size, realloc_size);
	}

	exit(0);
}


int call_init_routines(void)
{

	init_lower();
	dir_assert();
	glob_init();		/* initialize global tables */
	initialize_buffer();	/* used by sout() */
	init_spaces();
	init_random();		/* seed random number generator */
}


int write_totimes(void)
{
	FILE *fp;
	char *fnam;
	int pl;

	fnam = sout("%s/totimes", libdir);

	fp = fopen(fnam, "w");

	if (fp == NULL)
	{
		fprintf(stderr, "can't write %s", fnam);
		perror("");
		return;
	}

	loop_player(pl)
	{
		if (rp_player(pl) &&
		    rp_player(pl)->email &&
		    !player_compuserve(pl))
		{
			fprintf(fp, "%s\n", rp_player(pl)->email);
		}
	}
	next_player;

	fclose(fp);
}


int write_email(void)
{
	FILE *fp;
	char *fnam;
	int pl;

	fnam = sout("%s/email", libdir);

	fp = fopen(fnam, "w");

	if (fp == NULL)
	{
		fprintf(stderr, "can't write %s", fnam);
		perror("");
		return;
	}

	loop_player(pl)
	{
		if (rp_player(pl) && rp_player(pl)->email)
			fprintf(fp, "%s\n", rp_player(pl)->email);
	}
	next_player;

	fclose(fp);
}

static void
list_a_player(FILE *fp, int pl, int *flag)
{
	struct entity_player *p;
	char *s;
	int n;
	char c;
	char *email;

	p = p_player(pl);

	if (p->email || p->vis_email)
	{
		if (p->vis_email)
			email = p->vis_email;
		else
			email = p->email;

		if (p->full_name)
			s = sout("%s <%s>", p->full_name, email);
		else
			s = sout("<%s>", email);
	}
	else if (p->full_name)
		s = p->full_name;
	else
		s = "";

	if (ilist_lookup(new_players, pl) >= 0)
	{
		c = '*';
		*flag = TRUE;
	}
	else
		c = ' ';

	fprintf(fp, "%4s %c  %s\n",
			box_code_less(pl), c, just_name(pl));
	if (*s)
		fprintf(fp, "        %s\n", s);
	fprintf(fp, "\n");
}


int write_player_list(void)
{
	FILE *fp;
	char *fnam;
	int pl;
	int flag = FALSE;

	stage("write_player_list()");

	fnam = sout("%s/players", libdir);

	fp = fopen(fnam, "w");

	if (fp == NULL)
	{
		fprintf(stderr, "can't write %s", fnam);
		perror("");
		return;
	}

	fprintf(fp, "%4s   %s\n", "num", "faction");
	fprintf(fp, "%4s   %s\n", "---", "-------");

	loop_player(pl)
	{
		if (rp_player(pl) &&
		    rp_player(pl)->email &&
		    subkind(pl) == sub_pl_regular)
			list_a_player(fp, pl, &flag);
	}
	next_player;

	if (flag) {
		fprintf(fp, "\n                * -- New player this turn\n");
	}

	fclose(fp);
}


int write_forward_sup(int who_for, int target, FILE *fp)
{
	int pl;
	char *s, *u;

	pl = player(who_for);
	s = player_email(pl);

	if (s && *s)
	{
		fprintf(fp, "%s|%s\n", box_code_less(target), s);
	}
}


int write_forwards(void)
{
	FILE *fp;
	char *fnam;
	int i, j;
	ilist l;

	fnam = sout("%s/forward", libdir);

	fp = fopen(fnam, "w");

	if (fp == NULL)
	{
		fprintf(stderr, "can't write %s", fnam);
		perror("");
		return;
	}

	loop_player(i)
	{
		write_forward_sup(i, i, fp);
	}
	next_player;

	loop_char(i)
	{
		write_forward_sup(i, i, fp);
	}
	next_char;

	loop_garrison(i)
	{
		l = players_who_rule_here(i);

		for (j = 0; j < ilist_len(l); j++)
			if (l[j])
				write_forward_sup(l[j], i, fp);
	}
	next_garrison;

	fclose(fp);
}


int write_faction_sup(int who_for, int target, FILE *fp)
{
	int pl;
	char *s, *u;

	pl = player(who_for);
	s = player_email(pl);

	if (s && *s)
	{
		fprintf(fp, "%s|%s\n", box_code_less(target), s);
	}
}


int write_factions(void)
{
	FILE *fp;
	char *fnam;
	int i, j;
	ilist l;

	fnam = sout("%s/factions", libdir);

	fp = fopen(fnam, "w");

	if (fp == NULL)
	{
		fprintf(stderr, "can't write %s", fnam);
		perror("");
		return;
	}

	loop_player(i)
	{
		write_faction_sup(i, i, fp);
	}
	next_player;

	fclose(fp);
}


#if 1
int
send_rep(int pl, int turn)
{
	struct entity_player *p;
	char report[LEN];
	FILE *fp;
	int ret;
	char *zfnam = NULL;
	char *fnam;
	char *cmd;
	int split_lines = player_split_lines(pl);
	int split_bytes = player_split_bytes(pl);

	p = rp_player(pl);

	if (p == NULL || p->email == NULL || *p->email == '\0')
		return FALSE;

	sprintf(report, "/tmp/sendrep%d.%s", getpid(), box_code_less(pl));

	fp = fopen(report, "w");
	if (fp == NULL)
	{
		fprintf(stderr, "send_rep: can't write %s:", report);
		perror("");
		return FALSE;
	}

	fprintf(fp, "From: %s\n", from_host);
	if (reply_host && *reply_host)
		fprintf(fp, "Reply-To: %s\n", reply_host);
	fprintf(fp, "To: %s (%s)\n", p->email,
			p->full_name ? p->full_name : "???");
	fprintf(fp, "Subject: Olympia g1 turn %d report\n", turn);
	fprintf(fp, "\n");
	fclose(fp);

	fnam = sout("%s/save/%d/%d", libdir, turn, pl);

	if (access(fnam, R_OK) < 0)
	{
		zfnam = sout("%s/save/%d/%d.gz", libdir, turn, pl);

		if (access(zfnam, R_OK) < 0)
		{
			unlink(report);
			return FALSE;
		}

		fnam = sout("/tmp/zrep.%d", pl);

		ret = system(sout("gzcat %s > %s", zfnam, fnam));

		if (ret)
		{
			fprintf(stderr, "couldn't unpack %s\n", zfnam);
			unlink(fnam);
			unlink(report);
			return FALSE;
		}
	}

	if (player_notab(pl))
		cmd = sout("rep %s >> %s", fnam, report);
	else
		cmd = sout("rep %s | entab >> %s", fnam, report);

	ret = system(cmd);

	if (zfnam)
		unlink(fnam);

	if (ret)
	{
		fprintf(stderr, "send_rep: command failed: %s\n", cmd);
		unlink(report);
		return FALSE;
	}

	if (split_lines == 0 && split_bytes == 0)
		cmd = sout("sendmail -t -odq < %s", report);
	else
		cmd = sout("mailsplit -s %d -l %d -c 'sendmail -t -odq' < %s",
				split_bytes, split_lines, report);

	fprintf(stderr, "   %s\n", cmd);
	ret = system(cmd);

	if (ret)
		fprintf(stderr, "send_rep: mail to %s failed: %s\n",
						p->email, cmd);

	unlink(report);

#if 0
	sleep(5);	/* don't need to sleep if we are just queuing */
#endif

	return (ret == 0);
}
#else
int
send_rep(int pl, int turn)
{
	struct entity_player *p;
	char *fnam, *zfnam = NULL;
	char *cmd;
	int ret;

	p = rp_player(pl);

	if (p == NULL || p->email == NULL || *p->email == '\0')
		return FALSE;

	fnam = sout("%s/save/%d/%d", libdir, turn, pl);

	if (access(fnam, R_OK) < 0)
	{
		zfnam = sout("%s/save/%d/%d.gz", libdir, turn, pl);

		if (access(zfnam, R_OK) < 0)
			return FALSE;

		fnam = sout("/tmp/zrep.%d", pl);

		ret = system(sout("gzcat %s > %s", zfnam, fnam));

		if (ret)
		{
			fprintf(stderr, "couldn't unpack %s\n", zfnam);
			unlink(fnam);
			return FALSE;
		}
	}

#if 0
	cmd = sout("rep %s | %srmail %s", fnam, entab(pl), p->email);
#else
	cmd = sout("rep %s | %ssendmail -t -odq", fnam, entab(pl));
#endif

	fprintf(stderr, "   %s\n", cmd);

	ret = system(cmd);

	if (ret)
		fprintf(stderr, "couldn't mail to %s>\n", p->email);

#if 0
	sleep(5);	/* don't need to sleep if we are just queuing */
#endif

	if (zfnam)
		unlink(fnam);

	return (ret == 0);
}
#endif


int mail_reports(void)
{
	int pl;

	stage("mail_reports()");

	loop_player(pl)
	{
		send_rep(pl, sysclock.turn);
	}
	next_player;
}

int
v_remail(struct command *c)
{
	mail_reports();
	return TRUE;
}

