
#include "legacy.h"
#include	<stdio.h>
#include	"z.h"
#include	"oly.h"


/*
 *  Explanation of trade->cloak field:
 *
 *	0	normal -- open buy or sell, list in market report
 *	1	cloak trader, but list in market report
 *	2	invisible -- don't list in market report, cloak trader
 */

/*
 *  How it works
 *
 *  Each unit has a list of possible trades.
 *  Each trade is either a buy or a sell.
 *  When a trade is entered with the BUY or SELL command, the
 *	list of possible trades from other units in the city
 *	is consulted for a possible match.
 *  If no match is found, the trade is added to the unit's list
 *	of pending trades.
 *  When a unit enters the city, their trades are scanned to see if
 *	any match the pending trades of other units in the city.
 *  When an item is added to a unit, a check is made to see if the
 *	addition might validate a pending trade.  If so, we will try
 *	to match pending trades for the unit when the command is
 *	finished running.
 *	(perhaps this should happen at the end of the day?)
 */

static int
market_here(int who)
{

	while (who > 0 && subkind(who) != sub_city)
	{
		if (is_ship(who))
			return 0;

		who = loc(who);
	}

	return who;
}


void
clear_all_trades(int who)
{
	struct trade *t;

	loop_trade(who, t)
	{
		my_free(t);
	}
	next_trade;

	trades_clear(&bx[who]->trades);
}


static int
seller_comp(const void *av, const void *bv)
{
	trades_list a = (trades_list) av;
	trades_list b = (trades_list) bv;

	if ((*a)->cost == (*b)->cost)
		return (*a)->sort - (*b)->sort;

	return (*a)->cost - (*b)->cost;
}


static trades_list
seller_list(int where, int except)
{
	static trades_list l = NULL;
	int i;
	struct trade *t;
	int count = 0;

	trades_clear(&l);

	loop_char_here(where, i)
	{
/*
 *  Don't trade with ourselves, moving characters, or prisoners
 */
		if (i == except || is_prisoner(i) || char_moving(i))
			continue;

		loop_trade(i, t)
		{
			if (t->kind == SELL)
			{
				t->sort = count++;
				trades_append(&l, t);
			}
		}
		next_trade;
	}
	next_char_here;

	if (trades_len(l) > 0)
		qsort(l, (unsigned) trades_len(l), sizeof(struct trade *), seller_comp);

	loop_trade(where, t)
	{
		if (t->kind == SELL)
			trades_append(&l, t);
	}
	next_trade;

	return l;
}


static trades_list
buyer_list(int where, int except)
{
	static trades_list l = NULL;
	int i;
	struct trade *t;

	trades_clear(&l);

	loop_char_here(where, i)
	{
/*
 *  Don't trade with ourselves, moving characters, or prisoners
 */
		if (i == except || is_prisoner(i) || char_moving(i))
			continue;

		loop_trade(i, t)
		{
			if (t->kind == BUY)
				trades_append(&l, t);
		}
		next_trade;
	}
	next_char_here;

	loop_trade(where, t)
	{
		if (t->kind == BUY)
			trades_append(&l, t);
	}
	next_trade;

	return l;
}


/*
 *  If we're a buyer, reduce qty to how many we can afford, given cost
 *  If we're a seller, reduce qty to how many we actually have to sell
 *  If we're a city, leave qty alone, since we just gen the gold or item
 *
 *  Also reduce such that we don't exceed our own have_left requirement
 */

static int
reduce_qty(struct trade *t, int cost)
{
	int has;

	if (kind(t->who) == T_loc)
		return t->qty;

	if (t->kind == BUY)
	{
		has = has_item(t->who, item_gold) - t->have_left;
		if (has < 0)
			has = 0;

		return min(t->qty, has / cost);
	}

	if (t->kind == SELL)
	{
		has = has_item(t->who, t->item) - t->have_left;
		if (has < 0)
			has = 0;

		return min(t->qty, has);
	}

	assert(FALSE);
}


static void
attempt_trade(struct trade *buyer, struct trade *seller)
{
	int item = buyer->item;
	int qty;
	int cost;
	char *seller_s;
	char *buyer_s;
	extern int gold_pot_basket;
	extern int gold_trade;
	extern int gold_opium;

	assert(buyer != NULL);
	assert(seller != NULL);
	assert(buyer->item == seller->item);

	if (buyer->cost < seller->cost)
		return;

	{
		int buyer_qty;
		int seller_qty;

		buyer_qty = reduce_qty(buyer, seller->cost);
		seller_qty = reduce_qty(seller, seller->cost);

		qty = min(buyer_qty, seller_qty);
	}

	if (qty <= 0)
		return;

	cost = seller->cost * qty;

	buyer->qty -= qty;
	seller->qty -= qty;

	assert(buyer->qty >= 0);
	assert(seller->qty >= 0);

	if (kind(buyer->who) == T_loc)
	{
		gen_item(seller->who, item_gold, cost);
		consume_item(seller->who, item, qty);

		if (item == item_pot || item == item_basket)
			gold_pot_basket += cost;
		else if (item == item_opium)
		{
			log(LOG_SPECIAL, "%s earned %s selling opium.",
					box_name(seller->who), gold_s(cost));
			gold_opium += cost;
		}
		else
			gold_trade += cost;
	}
	else if (kind(seller->who) == T_loc)
	{
		consume_item(buyer->who, item_gold, cost);

		if (item_unique(item))
			move_item(seller->who, buyer->who, item, qty);
		else
			gen_item(buyer->who, item, qty);
	}
	else
	{
		move_item(buyer->who, seller->who, item_gold, cost);
		move_item(seller->who, buyer->who, item, qty);
	}

	if (seller->cloak)
		seller_s = "";
	else
		seller_s = sout(" from %s", box_name(seller->who));

	if (buyer->cloak)
		buyer_s = "";
	else
		buyer_s = sout(" to %s", box_name(buyer->who));

	if (kind(buyer->who) != T_loc)
		wout(buyer->who, "Bought %s%s for %s.",
			box_name_qty(item, qty),
			seller_s,
			gold_s(cost));

	if (kind(seller->who) != T_loc)
		wout(seller->who, "Sold %s%s for %s.",
			box_name_qty(item, qty),
			buyer_s,
			gold_s(cost));

	{
		int where;

		if (kind(buyer->who) == T_loc)
			where = buyer->who;
		else if (kind(seller->who) == T_loc)
			where = seller->who;
		else
			where = subloc(buyer->who);

		if (!seller->cloak && !buyer->cloak)
		{
			wout(where, "%s bought %s from %s for %s.",
				box_name(buyer->who),
				box_name_qty(item, qty),
				box_name(seller->who),
				gold_s(cost));
		}
		else if (seller->cloak)
		{
			wout(where, "%s bought %s for %s.",
				box_name(buyer->who),
				box_name_qty(item, qty),
				gold_s(cost));
		}
		else if (buyer->cloak)
		{
			wout(where, "%s sold %s for %s.",
				box_name(seller->who),
				box_name_qty(item, qty),
				gold_s(cost));
		}
	}
}


static void
scan_trades(struct trade *t, trades_list l)
{
	int i;

	for (i = 0; i < trades_len(l) && t->qty > 0; i++)
	{
		if (l[i]->item != t->item)
			continue;

		if (l[i]->who == t->who)
			continue;	/* don't trade with ourself */

		if (t->kind == BUY)
			attempt_trade(t, l[i]);
		else if (t->kind == SELL)
			attempt_trade(l[i], t);
	}
}


void
match_trades(int who)
{
	struct trade *t;
	int where = subloc(who);
	int first_buy = TRUE;
	int first_sell = TRUE;
	trades_list sellers;
	trades_list buyers;

	if (!market_here(who))
		return;

	loop_trade(who, t)
	{
		assert(t->who == who);

		if (t->kind == BUY)
		{
			if (first_buy)
			{
				first_buy = FALSE;
				sellers = seller_list(where, who);
			}

			scan_trades(t, sellers);
		}
		else if (t->kind == SELL)
		{
			if (first_sell)
			{
				first_sell = FALSE;
				buyers = buyer_list(where, who);
			}

			scan_trades(t, buyers);
		}
	}
	next_trade;
}


void
match_all_trades()
{
	int where;
	trades_list sellers;
	trades_list buyers;
	int i;

	loop_loc(where)
	{
		if (!market_here(where))
			continue;

		sellers = seller_list(where, 0);
		buyers = buyer_list(where, 0);

		if (trades_len(buyers) <= 0 || trades_len(sellers) <= 0)
			continue;

		for (i = 0; i < trades_len(buyers); i++)
			scan_trades(buyers[i], sellers);
	}
	next_loc;
}


ilist trades_to_check = NULL;


void
check_validated_trades()
{
	int i;

	for (i = 0; i < ilist_len(trades_to_check); i++)
	{
		match_trades(trades_to_check[i]);
	}

	ilist_clear(&trades_to_check);
}


/*
 *  Who has been given some item.  See if this validates
 *  any pending trades.
 *
 *  Our strategy is to see if any pending trades weren't already
 *  active at the old quantity of the item.  If so, we'll attempt
 *  a match soon.
 *
 *  We don't fire the trade inside of add_item, since it's too
 *  dangerous.  We want the command to complete, and be able to
 *  assert that a unit actually has an item after add_item has
 *  been called.
 */

void
investigate_possible_trade(int who, int item, int old_has)
{
	struct trade *t;
	int check = FALSE;

	if (item == item_gold)
	{
		loop_trade(who, t)
		{
			if (t->kind != BUY)
				continue;

			if ((old_has - t->have_left) / t->cost < t->qty)
			{
				check = TRUE;
				break;
			}
		}
		next_trade;
	}
	else
	{
		loop_trade(who, t)
		{
#if 0
			if (t->kind != SELL)
#else
			if (t->kind != SELL || t->item != item)
#endif
				continue;

			if (old_has - t->have_left < t->qty)
			{
				check = TRUE;
				break;
			}
		}
		next_trade;
	}

	if (check)
		ilist_append(&trades_to_check, who);
}


static struct trade *
find_trade(int who, int kind, int item)
{
	struct trade *t;
	struct trade *ret = NULL;

	loop_trade(who, t)
	{
		if (t->kind == kind && t->item == item)
		{
			ret = t;
			break;
		}
	}
	next_trade;

	return ret;
}


static struct trade *
new_trade(int who, int kind, int item)
{
	struct trade *ret;

	ret = find_trade(who, kind, item);

	if (ret == NULL)
	{
		ret = my_malloc(sizeof(*ret));

		ret->who = who;
		ret->kind = kind;
		ret->item = item;

		trades_append(&bx[who]->trades, ret);
	}

	return ret;
}


static char *
gold_each(int cost, int qty)
{

	if (qty == 1)
		return gold_s(cost);

	return sout("%s each", gold_s(cost));
}


int
v_buy(struct command *c)
{
	int where = subloc(c->who);
	int item = c->a;
	int qty = c->b;
	int cost = c->c;
	int have_left = c->d;
	int hide_me = c->e;
	struct trade *t;

	if (kind(item) != T_item)
	{
		wout(c->who, "%s is not an item.", box_code(item));
		return FALSE;
	}

	if (item == item_gold)
	{
		wout(c->who, "Can't buy or sell gold.");
		return FALSE;
	}

	if (hide_me)
	{
		if (has_skill(c->who, sk_cloak_trade))
			hide_me = 1;
		else
		{
			wout(c->who, "Must have %s to conceal trades.",
					box_code_less(sk_cloak_trade));
			return FALSE;
		}
	}

	if (cost < 1) cost = 1;

	t = new_trade(c->who, BUY, item);
	assert(t->who == c->who);

	if (qty <= 0)
	{
	    if (t->qty <= 0)
		wout(c->who, "No pending buy for %s.", box_name(item));
	    else
		wout(c->who, "Cleared pending buy for %s.", box_name(item));
	}

	t->qty = qty;
	t->cost = cost;
	t->cloak = hide_me;
	t->have_left = have_left;

	if (qty > 0)
	{
		wout(c->who, "Try to buy %s for %s.",
					box_name_qty(item, qty),
					gold_each(cost, qty));

		if (market_here(c->who))
			scan_trades(t, seller_list(where, c->who));
	}

	return TRUE;
}


int
v_sell(struct command *c)
{
	int where = subloc(c->who);
	int item = c->a;
	int qty = c->b;
	int cost = c->c;
	int have_left = c->d;
	int hide_me = c->e;
	struct trade *t;

	if (kind(item) != T_item)
	{
		wout(c->who, "%s is not an item.", box_code(item));
		return FALSE;
	}

	if (item == item_gold)
	{
		wout(c->who, "Can't buy or sell gold.");
		return FALSE;
	}

	if (hide_me)
	{
		if (has_skill(c->who, sk_cloak_trade))
			hide_me = 1;
		else
		{
			wout(c->who, "Must have %s to conceal trades.",
					box_code_less(sk_cloak_trade));
			return FALSE;
		}
	}

	if (cost < 1) cost = 1;

	t = new_trade(c->who, SELL, item);
	assert(t->who == c->who);

	if (qty <= 0)
	{
	    if (t->qty <= 0)
		wout(c->who, "No pending sell for %s.", box_name(item));
	    else
		wout(c->who, "Cleared pending sell for %s.", box_name(item));
	}

	t->qty = qty;
	t->cost = cost;
	t->cloak = hide_me;
	t->have_left = have_left;

	if (qty > 0)
	{
		wout(c->who, "Try to sell %s for %s.",
					box_name_qty(item, qty),
					gold_each(cost, qty));

		if (market_here(c->who))
			scan_trades(t, buyer_list(where, c->who));
	}

	return TRUE;
}


static int
list_market_items(int who, trades_list l, int first)
{
	int i;
	int qty;

	for (i = 0; i < trades_len(l); i++)
	{
		if (l[i]->cloak >= 2)
			continue;

		qty = reduce_qty(l[i], l[i]->cost);

		if (qty <= 0)
			continue;

		if (first)
		{
			out(who, "");
			out(who, "%5s %*s %7s %6s %9s   %-25s",
				"trade", CHAR_FIELD, "who", "price",
				"qty", "wt/ea", "item");
			out(who, "%5s %*s %7s %6s %9s   %-25s",
				"-----", CHAR_FIELD, "---", "-----", "---",
				"-----", "----");

			first = FALSE;
		}

		out(who, "%5s %*s %7s %6s %9s   %-25s",
			l[i]->kind == BUY ? "buy" : "sell",
			CHAR_FIELD,
			l[i]->cloak ? "?" : box_code_less(l[i]->who),
			comma_num(l[i]->cost),
			comma_num(qty),
			comma_num(item_weight(l[i]->item)),
			plural_item_box(l[i]->item, qty));
	}

	return first;
}


void
market_report(int who, int where)
{
	trades_list l;
	int first = TRUE;

	out(who, "");
	out(who, "Market report:");
	indent += 3;

	{
		struct trade *t;
		int flag = TRUE;

		loop_trade(where, t)
		{
			if (t->kind == PRODUCE && t->month_prod)
			{
				if (flag)
				{
					out(who, "");
					flag = FALSE;
				}

				wout(who, "%s produces %s on month %d.",
					just_name(where),
					plural_item_name(t->item, 2),
					t->month_prod);
			}
		}
		next_trade;
	}

	l = buyer_list(where, 0);

	if (trades_len(l) > 0)
	{
		first = list_market_items(who, l, first);
	}

	l = seller_list(where, 0);

	if (trades_len(l) > 0)
	{
		first = list_market_items(who, l, first);
	}

	if (first)
		out(who, "No goods offered for trade.");

	indent -= 3;
}


void
list_pending_trades(int who, int num)
{
	int first = TRUE;
	struct trade *t;

	loop_trade(num, t)
	{
		if (t->kind != BUY && t->kind != SELL)
			continue;

		if (first)
		{
			out(who, "");
			out(who, "Pending trades:");
			out(who, "");
			indent += 3;
			first = FALSE;

			out(who, "%5s  %7s  %5s   %s",
					"trade", "price", "qty", "item");
			out(who, "%5s  %7s  %5s   %s",
					"-----", "-----", "---", "----");
		}

		out(who, "%5s  %7s  %5s   %s",
				t->kind == BUY ? "buy" : "sell",
				comma_num(t->cost),
				comma_num(t->qty),
				box_name(t->item));
	}
	next_trade;

	if (!first)
		indent -= 3;
}


void
add_city_trade(int where, int kind, int item, int qty, int cost, int month)
{
	struct trade *t;

	t = new_trade(where, kind, item);
	t->qty = qty;
	t->cost = cost;
	t->month_prod = month;
}


/*
 *  Opium model
 *
 *	Every city has a status indicating its level of opium economic
 *	development, i.e. how addicted is the local populace.
 *	This status is 1-8.  Any sale maintains, saturation causes
 *	rise, no sale decay.
 *
 *	level	profit	qty	price
 *	-----	------	---	-----
 *	  8	 800	 80	 10
 *	  7	 700	 70	 10
 *	  6	 600	 66	  9
 *	  5	 500	 55	  9
 *	  4	 400	 50	  8
 *	  3	 300	 37	  8
 *	  2	 200	 28	  7
 *	  1	 100	 15	  7
 */

struct
{
	int qty;
	int cost;
}
opium_data[] =
{
	{15, 17},
	{28, 17},
	{37, 18},
	{50, 18},
	{55, 19},
	{66, 19},
	{70, 20},
	{80, 20},
};

#define		MAX_OPIUM_ECON		7


static void
opium_market_delta(int where)
{
	struct entity_subloc *p;
	struct trade *t;

	assert(subkind(where) == sub_city);

	t = find_trade(where, BUY, item_opium);
	p = p_subloc(where);

	if (t)
	{
		if (t->qty < 1)			/* sold everything */
		{
			p->opium_econ++;
		}
		else if (t->qty == opium_data[p->opium_econ].qty)
		{
			p->opium_econ--;	/* sold none */
		}
	}

	if (p->opium_econ > MAX_OPIUM_ECON)
		p->opium_econ = MAX_OPIUM_ECON;
	if (p->opium_econ < 0)
		p->opium_econ = 0;

	t = new_trade(where, CONSUME, item_opium);

	t->qty = opium_data[p->opium_econ].qty;
	t->cost = opium_data[p->opium_econ].cost;

	if (p->opium_econ > 0)
		t->cloak = 1;
	else
		t->cloak = 2;
}


/*
 *  Override causes cities which only produce a good once per year
 *  to produce it now anyway.  This is useful for epoch city trade
 *  seeding.
 */

void
loc_trade_sup(int where, int override)
{
	struct trade *t;
	struct trade *new;

	loop_trade(where, t)
	{
		int okay = TRUE;
		int next_month;

		if (t->month_prod && !override)
		{
			int this_month = oly_month(sysclock) - 1;
			int next_month = (this_month + 1) % NUM_MONTHS;
			int prod_month = t->month_prod - 1;

			if (next_month != prod_month)
				okay = FALSE;
		}

		if (t->kind == PRODUCE && okay)
		{
			new = new_trade(where, SELL, t->item);

			if (new->qty < t->qty)
				new->qty = t->qty;

			new->cost = t->cost;
			new->cloak = t->cloak;
		}
		else if (t->kind == CONSUME)
		{
			new = new_trade(where, BUY, t->item);
			if (new->qty < t->qty)
				new->qty = t->qty;

			new->cost = t->cost;
			new->cloak = t->cloak;
		}
	}
	next_trade;
}


void
trade_suffuse_ring(int where)
{
	struct trade *t;
	struct trade *new;
	int found = FALSE;
	int item;

	loop_trade(where, t)
	{
		if (subkind(t->item) == sub_suffuse_ring &&
		    t->kind == SELL && t->qty > 0)
			found = TRUE;
	}
	next_trade;

	if (found || rnd(1,3) < 3)
		return;

	item = new_suffuse_ring(where);

	new = new_trade(where, SELL, item);

	new->qty = 1;
	new->cost = 450 + rnd(0,12) * 50;
	new->cloak = FALSE;
}


void
location_trades()
{
	int where;

	stage("location_trades()");

	loop_city(where)
	{
		opium_market_delta(where);
		loc_trade_sup(where, FALSE);

		if (in_faery(where) || in_clouds(where))
			trade_suffuse_ring(where);
	}
	next_city;
}

