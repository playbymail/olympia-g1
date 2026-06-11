#include "legacy.h"
#include	<stdio.h>
#include	"z.h"
#include	"oly.h"



static int
keep_undead_check(struct command *c, int check_bond)
{
	int target = c->a;

	if (kind(target) != T_char || subkind(target) != sub_undead)
	{
	    wout(c->who, "%s is not a demon lord.", box_code(target));
	    return FALSE;
	}

	if (subloc(target) != subloc(c->who))
	{
		wout(c->who, "%s is not here.", box_code(target));
		return FALSE;
	}

	if (check_bond && loyal_kind(target) != LOY_summon)
	{
		wout(c->who, "%s is no longer bonded.", box_code(target));
		return FALSE;
	}

	return TRUE;
}


int
v_keep_undead(struct command *c)
{
	int target = c->a;

	if (!keep_undead_check(c, TRUE))
		return FALSE;

	if (!check_aura(c->who, 3))
		return FALSE;

	return TRUE;
}


int
d_keep_undead(struct command *c)
{
	int target = c->a;
	int loy;

	if (!keep_undead_check(c, TRUE))
		return FALSE;

	if (!charge_aura(c->who, 3))
		return FALSE;

	set_loyal(target, LOY_summon, max(loyal_rate(target) + 4, 8));

	wout(c->who, "%s will remain for %d months.",
				box_code(target),
				loyal_rate(target));
	return TRUE;
}


int
v_undead_lord(struct command *c)
{
	int where = subloc(c->who);
	int aura = c->a;

	if (aura < 3)
		c->a = aura = 3;
	if (aura > 8)
		c->a = aura = 8;

	if (!may_cookie_npc(c->who, where, item_undead_cookie))
		return FALSE;

	if (!check_aura(c->who, aura))
		return FALSE;

	return TRUE;
}


int
d_undead_lord(struct command *c)
{
	int where = subloc(c->who);
	int aura = c->a;
	int undead;
	int ret;
	int rating;

	if (!may_cookie_npc(c->who, where, item_undead_cookie))
		return FALSE;

	if (!charge_aura(c->who, aura))
		return FALSE;

	undead = do_cookie_npc(c->who, where, item_undead_cookie, c->who);

	if (undead == 0)
	{
		log(LOG_CODE, "d_undead_lord: why not?");
		wout(c->who, "Unable to summon a demon lord.");
		return FALSE;
	}

	switch (aura)
	{
	case 3:		rating = 100;	break;
	case 4:		rating = 150;	break;
	case 5:		rating = 190;	break;
	case 6:		rating = 220;	break;
	case 7:		rating = 240;	break;
	case 8:		rating = 250;	break;

	default:
		assert(FALSE);
	}

	p_char(undead)->attack = rating;
	p_char(undead)->defense = rating;

	set_loyal(undead, LOY_summon, 5);

	wout(c->who, "Summoned %s.", box_name(undead));
	wout(where, "%s has summoned %s.",
				box_name(c->who),
				liner_desc(undead));

	return TRUE;
}


int
v_banish_undead(struct command *c)
{

	if (!keep_undead_check(c, FALSE))
		return FALSE;

	if (!check_aura(c->who, 6))
		return FALSE;

	return TRUE;
}


int
d_banish_undead(struct command *c)
{
	int target = c->a;
	int where = cast_where(c->who);
	int head;

	if (!keep_undead_check(c, FALSE))
		return FALSE;

	if (!charge_aura(c->who, 6))
		return FALSE;

	head = stack_leader(target);

	wout(head, "%s banishes %s!", box_name(c->who), box_name(target));
	wout(where, "%s banishes %s!", box_name(c->who), box_name(target));

	extract_stacked_unit(target);
	kill_char(target, 0);

	reset_cast_where(c->who);

	return TRUE;
}


int
v_eat_dead(struct command *c)
{
	int body = c->a;

	if (kind(body) != T_item || subkind(body) != sub_dead_body)
	{
		wout(c->who, "%s is not the dead body of a noble.",
						box_code(body));
		return FALSE;
	}

	if (has_item(c->who, body) == 0)
	{
		wout(c->who, "Don't have %s.", box_code(body));
		return FALSE;
	}

	if (has_item(c->who, item_ratspider_venom) == 0)
	{
		wout(c->who, "Requires %s.",
				box_name_qty(item_ratspider_venom, 1));
		return FALSE;
	}

	if (!check_aura(c->who, 5))
		return FALSE;

	return TRUE;
}


static void
get_some_skills(int who, int body, int chance)
{
	struct skill_ent *e;
	int parent;

/*
 *  First do category skills
 */

	loop_char_skill_known(body, e)
	{
		parent = skill_school(e->skill);
		if (parent != e->skill)
			continue;

		if (has_skill(who, e->skill))
			continue;

		if (e->skill == sk_adv_sorcery)
			continue;

		if (rnd(1,100) > chance)
			continue;

		learn_skill(who, e->skill);
	}
	next_char_skill_known;

/*
 *  Now do subskills
 *  Must know parent in order to pick up a subskill
 */

	loop_char_skill_known(body, e)
	{
		parent = skill_school(e->skill);
		if (parent == e->skill)
			continue;

		if (has_skill(who, e->skill) || has_skill(who, parent) == 0)
			continue;

		if (rnd(1,100) > chance)
			continue;

		learn_skill(who, e->skill);
	}
	next_char_skill_known;
}


int
d_eat_dead(struct command *c)
{
	int body = c->a;
	extern int dead_body_np;

	if (kind(body) != T_item || subkind(body) != sub_dead_body)
	{
		wout(c->who, "%s is not the dead body of a noble.",
						box_code(body));
		return FALSE;
	}

	if (has_item(c->who, body) == 0)
	{
		wout(c->who, "Don't have %s.", box_code(body));
		return FALSE;
	}

	if (subkind(body) != sub_dead_body)
	{
		wout(c->who, "%s is not the dead body of a noble.",
						box_code(body));
		return FALSE;
	}

	if (consume_item(c->who, item_ratspider_venom, 1) == 0)
	{
		wout(c->who, "Requires %s.",
				box_name_qty(item_ratspider_venom, 1));
		return FALSE;
	}

	if (!charge_aura(c->who, 5))
		return FALSE;

	wout(c->who, "Consumed %s.", box_name(body));
	get_some_skills(c->who, body, 100);

	{
		int pl = body_old_lord(body);

		if (valid_box(pl))
		{
			out(pl, "The spirit of %s~%s has been defiled "
				"by Necromancy.",
				rp_misc(body)->save_name, box_code(body));

			rp_misc(body)->old_lord = 0;  /* inhibit NP return */
		}
	}

	dead_body_np = FALSE;
	destroy_unique_item(c->who, body);
	dead_body_np = TRUE;

	if (rnd(1,100) <= 25 && !char_sick(c->who))
	{
		p_char(c->who)->sick = TRUE;
		wout(c->who, "%s has fallen ill.", box_name(c->who));
	}

	return TRUE;
}


static int
random_body_here(int where)
{
	struct item_ent *e;
	static ilist l = NULL;

	ilist_clear(&l);

	loop_inv(where, e)
	{
		if (subkind(e->item) == sub_dead_body &&
		    sysclock.turn > p_char(e->item)->death_time.turn)
		{
			ilist_append(&l, e->item);
		}
	}
	next_inv;

	if (ilist_len(l) == 0)
		return 0;

	ilist_scramble(l);

	return l[0];
}


int
v_exhume(struct command *c)
{
	int where = subloc(c->who);
	int targ = c->a;
	int n;

	if (subkind(where) != sub_graveyard)
	{
		wout(c->who, "Bodies may only be exhumed in graveyards.");
		return FALSE;
	}

	if (targ &&
		(!valid_box(targ) ||
		subkind(targ) != sub_dead_body ||
		has_item(where, targ) == 0))
	{
		wout(c->who, "No body %s is buried here.", box_code(targ));
		return FALSE;
	}

	if (!targ && (n = random_body_here(where)) == 0)
	{
		wout(c->who, "There are no fresh graves here to dig up.");
		return FALSE;
	}

	if (!targ)
		targ = n;

	if (sysclock.turn == p_char(targ)->death_time.turn)
	{
		wout(c->who, "%s may not be exhumed until next month.",
						cap(box_name(targ)));
		return FALSE;
	}

	return TRUE;
}


int
d_exhume(struct command *c)
{
	int where = subloc(c->who);
	int targ = c->a;
	int n;

	if (subkind(where) != sub_graveyard)
	{
		wout(c->who, "Bodies may only be exhumed in graveyards.");
		return FALSE;
	}

	if (targ &&
		(!valid_box(targ) ||
		subkind(targ) != sub_dead_body ||
		has_item(where, targ) == 0))
	{
		wout(c->who, "No body %s is buried here.", box_code(targ));
		return FALSE;
	}

	if (!targ && (n = random_body_here(where)) == 0)
	{
		wout(c->who, "There are no fresh graves here to dig up.");
		return FALSE;
	}

	if (!targ)
		targ = n;

	if (sysclock.turn == p_char(targ)->death_time.turn)
	{
		wout(c->who, "%s may not be exhumed until next month.",
						cap(box_name(targ)));
		return FALSE;
	}

	move_item(where, c->who, targ, 1);

	wout(c->who, "Exhumed %s.", box_name(targ));
	wout(where, "%s exhumed %s.", box_name(c->who), box_name(targ));

	return TRUE;
}


void
auto_undead(int who)
{
	int master;
	int where = subloc(who);

	master = npc_summoner(who);

	if (master && subloc(who) == subloc(master))
	{
		queue(who, "attack %s", box_code_less(master));
		p_misc(who)->summoned_by = 0;
		return;
	}

	if (loc_depth(where) != LOC_province || rnd(1,2) == 1)
	{
		npc_move(who);
		return;
	}

	queue(who, "pillage 1");
}


int
v_aura_blast(struct command *c)
{
	int target = c->a;
	int aura = c->b;
	int have_left = c->c;
	int where = subloc(c->who);

	if (in_safe_now(where))
	{
		wout(c->who, "Not allowed in a safe haven.");
		return FALSE;
	}

	return TRUE;
}


int
d_aura_blast(struct command *c)
{
	int target = c->a;
	int aura = c->b;
	int have_left = c->c;
	int where = subloc(c->who);

	if (!cast_check_char_here(c->who, target))
		return FALSE;

	if (in_safe_now(where) || in_safe_now(target))
	{
		wout(c->who, "Not allowed in a safe haven.");
		return FALSE;
	}

	if (aura < 1)
		aura = char_cur_aura(c->who);
	
	if (have_left)
	{
		int m = max(char_cur_aura(c->who) - have_left, 0);

		if (aura > m)
			aura = m;
	}

	if (aura == 0)
	{
		wout(c->who, "No aura available for blast.");
		return FALSE;
	}

	if (!charge_aura(c->who, aura))
		return FALSE;

	vector_clear();
	vector_add(c->who);
	vector_add(target);
	vector_add(where);

	wout(VECT, "%s blasts %s with a burst of aura!",
			box_name(c->who), box_name(target));

	log(LOG_SPECIAL, "%s blasts %s with a burst of aura!",
			box_name(c->who), box_name(target));

	if (has_skill(target, sk_absorb_blast))
	{
		if (reflect_blast(target))
		{
			wout(VECT, "%s reflected the blast back to %s!",
					just_name(target), just_name(c->who));

			add_char_damage(c->who, aura * 2, MATES);
		}
		else
		{
			wout(VECT, "%s absorbed the blast!", just_name(target));

			p_magic(target)->cur_aura += aura;
			wout(target, "Current aura is now %d.",
					rp_magic(target)->cur_aura);
		}
	}
	else
		add_char_damage(target, aura * 2, MATES);

	return TRUE;
}


int
v_aura_reflect(struct command *c)
{
	int flag = c->a;

	p_magic(c->who)->aura_reflect = flag;

	if (flag)
		wout(c->who, "Will reflect aura blasts back at the attacker.");
	else
		wout(c->who, "Will absorb aura blasts.");

	return TRUE;
}

