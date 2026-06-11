
#include	<stdio.h>
#include	<unistd.h>
#include	"z.h"
#include	"oly.h"


static int
check_vision_target(struct command *c, int target)
{

	switch (kind(target))
	{
	case T_char:
	case T_ship:
	case T_loc:
		break;

	case T_item:
		if (!item_unique(target))
		{
			wout(c->who, "%s is not a unique item.",
							box_code(target));
			return FALSE;
		}
		break;

	default:
		wout(c->who, "Cannot receive a vision for %s.",
						box_code(target));
		return FALSE;
	}

	return TRUE;
}


int
v_reveal_vision(struct command *c)
{
	int target = c->a;
	struct char_magic *p;

	if (!check_vision_target(c, target))
		return FALSE;

	p = rp_magic(c->who);
	if (p && test_bit(p->visions, target))
	{
		wout(c->who, "Already have received a vision of %s.",
					box_code(target));
		wout(c->who, "A vision may only be received once for "
				"a particular target.");
		return FALSE;
	}

	return TRUE;
}


int
d_reveal_vision(struct command *c)
{
	int target = c->a;
	struct char_magic *p;
	int chance = 50;

	if (!check_vision_target(c, target))
		return FALSE;

	if (char_pray(c->who))
	{
		p_magic(c->who)->pray = 0;
		chance = 100;
	}

	if (rnd(1,100) > chance)
	{
		wout(c->who, "Failed to receive a vision.");
		return FALSE;
	}

	p = p_magic(c->who);
	set_bit(&p->visions, target);

	wout(c->who, "%s receives a vision of %s:", box_name(c->who),
					box_name(target));

	out(c->who, "");

	switch (kind(target))
	{
	case T_loc:
	case T_ship:
		show_loc(c->who, viewloc(target));
		break;

	case T_char:
		char_rep_sup(c->who, target);
		break;

	case T_item:
		show_item_where(c->who, target);
		break;

	default:
		assert(FALSE);
	}

	return TRUE;
}


int
v_resurrect(struct command *c)
{
	int body = c->a;

	if (!valid_box(body) || has_item(c->who, body) < 1)
	{
		wout(c->who, "Don't have any body %s.", box_code(body));
		return FALSE;
	}

	if (kind(body) != T_item || subkind(body) != sub_dead_body)
	{
		wout(c->who, "%s is not the dead body of a noble.",
						box_code(body));
		return FALSE;
	}

	assert(item_unique(body));

	return TRUE;
}


int
d_resurrect(struct command *c)
{
	int body = c->a;
	int chance = 50;

	if (!valid_box(body) || has_item(c->who, body) < 1)
	{
		wout(c->who, "Don't have any body %s.", box_code(body));
		return FALSE;
	}

	if (kind(body) != T_item || subkind(body) != sub_dead_body)
	{
		wout(c->who, "%s is not the dead body of a noble.",
						box_code(body));
		return FALSE;
	}

	if (char_pray(c->who))
	{
		p_magic(c->who)->pray = 0;
		chance = 100;
	}

	if (rnd(1,100) > chance)
	{
		wout(c->who, "Resurrection failed.");
		return FALSE;
	}

	if (rp_misc(body))
		wout(c->who, "Brought %s back to life!",
					rp_misc(body)->save_name);

	restore_dead_body(c->who, body);

	return TRUE;
}


int
v_prep_ritual(struct command *c)
{

	if (!check_skill(c->who, sk_pray))
		return FALSE;

	if (char_pray(c->who))
	{
		wout(c->who, "Have already completed a preparatory ritual.");
		return FALSE;
	}

	return TRUE;
}


int
d_prep_ritual(struct command *c)
{

	wout(c->who, "The next religious act will surely succeed.");
	p_magic(c->who)->pray = 1;
	return TRUE;
}


int
v_last_rites(struct command *c)
{
	int body = c->a;
	int where = subloc(c->who);

	if (subkind(where) != sub_temple)
	{
		wout(c->who, "Must be performed in a temple.");
		return FALSE;
	}

	if (kind(body) != T_item || subkind(body) != sub_dead_body)
	{
		wout(c->who, "%s is not a dead noble.", box_code(body));
		return FALSE;
	}

	return TRUE;
}


int
d_last_rites(struct command *c)
{
	int body = c->a;
	int owner;
	char *old_name;
	int where = subloc(c->who);
	int chance = 50;

	if (subkind(where) != sub_temple)
	{
		wout(c->who, "Must be performed in a temple.");
		return FALSE;
	}

	if (kind(body) != T_item || subkind(body) != sub_dead_body)
	{
		wout(c->who, "%s is not a dead noble.", box_code(body));
		return FALSE;
	}

	if (char_pray(c->who))
	{
		p_magic(c->who)->pray = 0;
		chance = 100;
	}

	if (rnd(1,100) > chance)
	{
		wout(c->who, "Failed to administer last rites.");
		return FALSE;
	}

	owner = item_unique(body);
	assert(owner);

	if (kind(owner) == T_char)
		wout(owner, "%s vanished.", box_name(body));

	old_name = p_misc(body)->save_name;

	if (old_name == NULL || *old_name == '\0')
		old_name = box_code(body);

	wout(c->who, "Last rites peformed for %s.", old_name);

	destroy_unique_item(owner, body);

	return TRUE;
}


int
v_remove_bless(struct command *c)
{
	int target = c->a;

	if (target == 0)
		target = c->a = c->who;

	if (!check_char_here(c->who, target))
		return FALSE;

	return TRUE;
}


int
d_remove_bless(struct command *c)
{
	int has;
	int target = c->a;
	int chance = 50;

	if (!check_still_here(c->who, target))
		return FALSE;

	has = has_item(target, item_blessed_soldier);
	if (has < 1)
	{
		wout(c->who, "%s has no %s.", box_name(target),
				just_name_qty(item_blessed_soldier, 2));
		return FALSE;
	}

	if (char_pray(c->who))
	{
		p_magic(c->who)->pray = 0;
		chance = 100;
	}

	if (rnd(1,100) > chance)
	{
		wout(c->who, "Failed to remove blessing.");
		return FALSE;
	}

	consume_item(target, item_blessed_soldier, has);
	gen_item(target, item_soldier, has);

	wout(c->who, "Removed blessing from %s.",
				just_name_qty(item_soldier, 2));

	if (target != c->who)
		wout(target, "%s removed the blessing from %s of "
				"our soldiers!",
				box_name(c->who),
				comma_num(has));

	return TRUE;
}

