
#include	<stdio.h>
#include	"z.h"
#include	"oly.h"


/*
 *  10%  9
 *  40%  6
 *  40%  3
 *  10%  0
 */

static int
choose_city_prominence(int city)
{
	int n;

	if (safe_haven(city) || major_city(city))
		return 3;

	if (loc_hidden(city) || loc_hidden(province(city)))
		return 0;

	n = rnd(1,100);

	if (n <= 10)
		return 0;
	if (n <= 50)
		return 1;
	if (n <= 90)
		return 2;
	return 3;
}


static void
add_near_city(int where, int city)
{
	struct entity_subloc *p;

	p = p_subloc(where);

	ilist_append(&p->near_cities, city);
}


void
prop_city_near_list(int city)
{
	int prom;
	int m;
	int i;
	int n;
	int dest;
	int where;
	exit_views_list l;

	clear_temps(T_loc);

	bx[province(city)]->temp = 1;
	prom = choose_city_prominence(city);
	p_subloc(city)->prominence = prom;
	prom *= 3;

	for (m = 1; m < prom; m++)
	{
		loop_loc(where)
		{
			if (bx[where]->temp != m)
				continue;

			l = exits_from_loc_nsew(0, where);

			for (i = 0; i < exit_views_len(l); i++)
			{
				dest = l[i]->destination;

				if (loc_depth(dest) != LOC_province)
					continue;

				if (bx[dest]->temp == 0)
				{
					bx[dest]->temp = m + 1;
					if (n = city_here(dest))
						add_near_city(n, city);
				}
			}
		}
		next_loc;
	}
}


void
seed_city_near_lists()
{
	int city;

	stage("INIT: seed_city_near_lists()");

	loop_city(city)
	{
		ilist_clear(&p_subloc(city)->near_cities);
	}
	next_city;

	loop_city(city)
	{
		prop_city_near_list(city);
	}
	next_city;
}


void
seed_mob_cookies()
{
	int i;

	loop_loc(i)
	{
		if (subkind(i) != sub_city && loc_depth(i) != LOC_province)
			continue;

		if (subkind(i) == sub_ocean)
			continue;

		gen_item(i, item_mob_cookie, 1);
	}
	next_loc;
}


void
seed_undead_cookies()
{
	int i;

	loop_loc(i)
	{
		if (subkind(i) != sub_graveyard)
			continue;

		gen_item(i, item_undead_cookie, 1);
	}
	next_loc;
}


void
seed_weather_cookies()
{
	int i;

	loop_loc(i)
	{
		switch (subkind(i))
		{
		case sub_forest:
			gen_item(i, item_rain_cookie, 1);
			gen_item(i, item_fog_cookie, 1);
			break;

		case sub_plain:
		case sub_desert:
		case sub_mountain:
			gen_item(i, item_wind_cookie, 1);
			break;

		case sub_swamp:
			gen_item(i, item_fog_cookie, 1);
			break;

		case sub_ocean:
			gen_item(i, item_fog_cookie, 1);
			gen_item(i, item_wind_cookie, 1);
			gen_item(i, item_rain_cookie, 1);
			break;
		}
	}
	next_loc;
}


void
seed_cookies()
{

	stage("INIT: seed_cookies()");

	seed_mob_cookies();
	seed_undead_cookies();
	seed_weather_cookies();
}


static void
compute_dist_generic(int terr)
{
	int where;
	exit_views_list l;
	int set_one;
	int i;
	int dest;
	int m;

	clear_temps(T_loc);

	loop_province(where)
	{
		if (subkind(where) != terr)
			continue;

		l = exits_from_loc_nsew(0, where);

		for (i = 0; i < exit_views_len(l); i++)
		{
			if (loc_depth(l[i]->destination) != LOC_province)
				continue;

			if (subkind(l[i]->destination) != terr)
			{
				bx[l[i]->destination]->temp = 1;
			}
		}
	}
	next_province;

	m = 1;

	do
	{
	    set_one = FALSE;

	    loop_province(where)
	    {
		if (subkind(where) == terr || bx[where]->temp != m)
		    continue;

		l = exits_from_loc_nsew(0, where);

		for (i = 0; i < exit_views_len(l); i++)
		{
		    dest = l[i]->destination;

		    if (loc_depth(dest) != LOC_province)
			continue;

		    if (subkind(dest) != terr && bx[dest]->temp == 0)
		    {
			bx[dest]->temp = m + 1;
			set_one = TRUE;
		    }
		}
	    }
	    next_province;

	    m++;
	}
	while (set_one);

	loop_province(where)
	{
		if (subkind(where) != terr && bx[where]->temp < 1)
			fprintf(stderr, "error on %d\n", where);
	}
	next_province;
}


/*
 *  Could be speeded up by saving the return from province_gate_here()
 *  in some temp field.  But this routine is only run once, when a new
 *  database is first read in, so it probably doesn't matter.
 */

void
compute_dist_gate()
{
	int where;
	exit_views_list l;
	int set_one;
	int i;
	int dest;
	int m;

	clear_temps(T_loc);

	loop_province(where)
	{
		if (!province_gate_here(where))
			continue;

		l = exits_from_loc_nsew(0, where);

		for (i = 0; i < exit_views_len(l); i++)
		{
			if (loc_depth(l[i]->destination) != LOC_province)
				continue;

			if (!province_gate_here(l[i]->destination))
			{
				bx[l[i]->destination]->temp = 1;
			}
		}
	}
	next_province;

	m = 1;

	do
	{
	    set_one = FALSE;

	    loop_province(where)
	    {
		if (province_gate_here(where) || bx[where]->temp != m)
		    continue;

		l = exits_from_loc_nsew(0, where);

		for (i = 0; i < exit_views_len(l); i++)
		{
		    dest = l[i]->destination;

		    if (loc_depth(dest) != LOC_province)
			continue;

		    if (!province_gate_here(dest) && bx[dest]->temp == 0)
		    {
			bx[dest]->temp = m + 1;
			set_one = TRUE;
		    }
		}
	    }
	    next_province;

	    m++;
	}
	while (set_one);

	loop_province(where)
	{
		if (!province_gate_here(where) && bx[where]->temp < 1)
			fprintf(stderr, "error on %d\n", where);
	}
	next_province;
}


void
compute_nearby_graves()
{
	int where;
	exit_views_list l;
	int set_one = TRUE;
	int i;
	int dest;
	int sequence = 0;

	loop_province(i)
	{
		bx[i]->temp = 0;
		p_loc(i)->near_grave = 0;
	}
	next_province;

	loop_subkind(sub_graveyard, i)
	{
		where = province(i);

		p_loc(where)->near_grave = i;
		bx[where]->temp = 1;
	}
	next_subkind;

	while (set_one)
	{
	    set_one = FALSE;
	    sequence++;

	    loop_province(where)
	    {
		if (bx[where]->temp == sequence)
		{
		    l = exits_from_loc(0, where);

		    for (i = 0; i < exit_views_len(l); i++)
		    {
			dest = province(l[i]->destination);

			if (!l[i]->water && bx[dest]->temp == 0)
			{
			    bx[dest]->temp = bx[where]->temp + 1;
			    rp_loc(dest)->near_grave = rp_loc(where)->near_grave;
			    set_one = TRUE;
			}
		    }
		}
	    }
	    next_province;
	}

/*
 *  We skipped water on the first pass so that graveyards on the
 *  same continent wouldn't short-cut across a water route.  Now
 *  that all land graveyards should be set, do water ones.  These
 *  should come out a straight-lines from the water location to the
 *  nearest graveyard on nearby land.
 */

	loop_province(i)
	{
		if (bx[i]->temp)
			bx[i]->temp = 1;	/* reset sequence */
	}
	next_province;

	sequence = 0;
	set_one = TRUE;

	while (set_one)
	{
	    set_one = FALSE;
	    sequence++;

	    loop_province(where)
	    {
		if (bx[where]->temp == sequence)
		{
		    l = exits_from_loc(0, where);

		    for (i = 0; i < exit_views_len(l); i++)
		    {
			dest = province(l[i]->destination);

			if (bx[dest]->temp == 0)
			{
			    bx[dest]->temp = bx[where]->temp + 1;
			    rp_loc(dest)->near_grave = rp_loc(where)->near_grave;
			    set_one = TRUE;
			}
		    }
		}
	    }
	    next_province;
	}
}


void
compute_dist()
{
	int i;

	stage("INIT: compute_dist()");
	compute_dist_generic(sub_ocean);

	loop_province(i)
	{
		p_loc(i)->dist_from_sea = bx[i]->temp;
	}
	next_province;

	compute_dist_generic(sub_swamp);

	loop_province(i)
	{
		p_loc(i)->dist_from_swamp = bx[i]->temp;
	}
	next_province;

	compute_dist_gate();

	loop_province(i)
	{
		p_loc(i)->dist_from_gate = bx[i]->temp;
	}
	next_province;

	compute_nearby_graves();
}


int
int_comp(const void *av, const void *bv)
{
	int *a = (int *) av;
	int *b = (int *) bv;

	return *a - *b;
}


static void
seed_city_skill(int where)
{
	int prov = province(where);
	int prov_k = subkind(prov);
	struct entity_subloc *p;

	p = p_subloc(where);

	ilist_clear(&p->teaches);

	if (in_faery(where))		/* taught only in Faery city */
	{
		ilist_append(&p->teaches, sk_scry);
		ilist_append(&p->teaches, sk_artifact);
		return;
	}

	if (in_clouds(where))		/* taught only in the Cloudlands */
	{
		ilist_append(&p->teaches, sk_weather);
		ilist_append(&p->teaches, sk_artifact);
		return;
	}

	if (in_hades(where))		/* taught only in Hades */
	{
		ilist_append(&p->teaches, sk_necromancy);
		ilist_append(&p->teaches, sk_artifact);
		return;
	}

/*
 *  Skills taught everywhere
 */

	ilist_append(&p->teaches, sk_combat);
	ilist_append(&p->teaches, sk_construction);

	if (is_port_city(where))
		ilist_append(&p->teaches, sk_shipcraft);

	switch (prov_k)
	{
	case sub_plain:
		ilist_append(&p->teaches, sk_beast);
		break;

	case sub_mountain:
		ilist_append(&p->teaches, sk_mining);
		break;

	case sub_forest:
		ilist_append(&p->teaches, sk_forestry);
		break;
	}

	if (rnd(1,2) == 1)
		ilist_append(&p->teaches, sk_alchemy);
	else
		ilist_append(&p->teaches, sk_basic);

	if (rnd(1,2) == 1)
		ilist_append(&p->teaches, sk_stealth);
	else
		ilist_append(&p->teaches, sk_persuasion);

	if (rnd(1,3) == 1)
	{
		ilist_append(&p->teaches, sk_gate);
	}

	if (ilist_len(p->teaches) > 0)
	    qsort(p->teaches, ilist_len(p->teaches), sizeof(int), int_comp);
}


void
seed_city_trade(int where)
{
	int prov = province(where);
	int prov_kind = subkind(prov);
	struct entity_subloc *p = rp_subloc(where);

	clear_all_trades(where);

	if (in_hades(where))
	{
		return;
	}

	if (in_clouds(where))
	{
		return;
	}

	if (in_faery(where))		/* seed Faery city trade */
	{
	    add_city_trade(where, PRODUCE, item_pegasus, 1, 1000, 0);

	    if (rnd(1,2) == 1)
	 	add_city_trade(where, PRODUCE, item_lana_bark, 3, 50, 0);
	    else
	 	add_city_trade(where, PRODUCE, item_avinia_leaf, 10, 35, 0);

	    if (rnd(1,2) == 1)
		add_city_trade(where, PRODUCE, item_yew, 5, 100, 0);
	    else
	 	add_city_trade(where, PRODUCE, item_mallorn_wood, 5, 200, 0);

	    if (rnd(1,2) == 1)
		add_city_trade(where, PRODUCE, item_cardamom, 2, 30, 0);
	    else
		add_city_trade(where, PRODUCE, item_honey, 2, 15, 0);

	    add_city_trade(where, CONSUME, item_mithril, 10, 500, 0);

	    if (rnd(1,2) == 1)
		add_city_trade(where, CONSUME, item_gate_crystal, 2, 1000, 0);
	    else
		add_city_trade(where, CONSUME, item_jewel, 5, 100, 0);

	    loc_trade_sup(where, TRUE);
	    return;
	}

	if (is_port_city(where))
	{
		add_city_trade(where, CONSUME, item_fish, 100, 2, 0);
		if (rnd(1,2) == 1)
		    add_city_trade(where, PRODUCE, item_fish_oil, 25, 5, 0);
		else
		    add_city_trade(where, PRODUCE, item_dried_fish, 20, 6, 0);
		add_city_trade(where, PRODUCE, item_glue, 10, 50, 0);
	}
	else if (rnd(1,3) == 1)
	{
		if (rnd(1,2) == 1)
			add_city_trade(where, CONSUME, item_fish_oil, 15,
					9 + min(7, sea_dist(prov)) * 2, 0);
		else
			add_city_trade(where, CONSUME, item_dried_fish, 12,
					10 + min(7, sea_dist(prov)) * 2, 0);
	}

	if (rnd(1,2) == 1)
		add_city_trade(where, CONSUME, item_pot, 9, 7, 0);
	else
		add_city_trade(where, CONSUME, item_basket, 15, 4, 0);

	if (prov_kind == sub_plain)
	{
		add_city_trade(where, PRODUCE, item_ox, 5, 100, 0);
		add_city_trade(where, PRODUCE, item_riding_horse, rnd(2,3),
							rnd(20,30) * 5, 0);
	}
	else if (rnd(1,3) == 1)
		add_city_trade(where, CONSUME, item_hide, rnd(3,6),
						rnd(125,135), 0);

	if (prov_kind == sub_mountain)
	{
		add_city_trade(where, PRODUCE, item_iron, rnd(1,2),
						rnd(25,30), 0);
	}

	if (prov_kind == sub_forest)
	{
		add_city_trade(where, PRODUCE, item_lumber, 25,
						rnd(11,15), 0);
	}

	if (p && ilist_lookup(p->teaches, sk_alchemy) >= 0)
		add_city_trade(where, PRODUCE, item_lead, 50, 1, 0);

	loc_trade_sup(where, TRUE);
}


int
base_price(int n)
{
	struct entity_item *t;

	assert(kind(n) == T_item);

	t = p_item(n);

	if (t->base_price == 0)
		t->base_price = rnd(9,56);

	return t->base_price;
}

#if 0

#define	MAX_NEAR		3	/* provinces considered "near" */
#define	LONG_ROUTE_MIN		15	/* min length of a "long" route */
#define	LONG_ROUTE_MAX		50	/* max length of a "long" route */


static int
nearby_city(ilist cities, int a)
{
	int i;
	int m = 999999;
	int dist;
	int save = 0;
	int reg = region(a);

	for (i = 0; i < ilist_len(cities); i++)
	{
		if (cities[i] == a || reg != region(cities[i]))
			continue;

		dist = los_province_distance(cities[i], a);

		if (dist < m)
		{
			m = dist;
			save = cities[i];
		}
	}

	if (m > MAX_NEAR)
		return 0;

	return save;
}


static int
nearby_city_two(ilist cities, int a, int b)
{
	int i;
	int m = 999999;
	int dist;
	int save = 0;
	int reg = region(a);

	for (i = 0; i < ilist_len(cities); i++)
	{
		if (cities[i] == a ||
		    cities[i] == b ||
		    reg != region(cities[i]))
			continue;

		dist = max(los_province_distance(cities[i], a),
				los_province_distance(cities[i], b));

		if (dist < m)
		{
			m = dist;
			save = cities[i];
		}
	}

	if (m > MAX_NEAR)
		return 0;

	return save;
}


static void
long_route_sup(int item, ilist source, ilist consume, int distance)
{
	int qty;
	int profit;
	int premium;
	int m, n;
	int q, c;
	int i;

	assert(kind(item) == T_item);

	n = ilist_len(source);
	m = ilist_len(consume);

	assert(n > 0);
	assert(m > 0);

	qty = 100 + (n-1) * 25;

	profit = (135 + 2 * distance) * distance;
	premium = profit / qty;

	for (i = 0; i < n; i++)
	{
	    q = qty/n;
	    q += rnd(-q/10, q/10);

	    c = base_price(item);
	    c -= rnd(0, c/20);

	    add_city_trade(source[i], PRODUCE, item, q, c, rnd(1, NUM_MONTHS));
	}

	for (i = 0; i < m; i++)
	{
	    q = qty/m;
	    q += rnd(-q/10, q/10);

	    c = base_price(item) + premium;
	    c += rnd(0, c/20);

	    add_city_trade(consume[i], CONSUME, item, q, c, 0);
	}
}


static void
city_cluster(ilist cities, ilist *l, int where)
{
	int one, two;

	ilist_append(l, where);

	if (rnd(1,2) == 1)
		return;

	one = nearby_city(cities, where);

	if (one)
		ilist_append(l, one);

	if (rnd(1,2) == 1)
		return;

	two = nearby_city_two(cities, where, one);

	if (two)
		ilist_append(l, two);
}


static int
long_route(ilist cities, int item)
{
	static ilist source = NULL;
	static ilist consume = NULL;
	int count = 0;
	int dist, dist_r, dist_g;
	int one, two;		/* source, consume cities */

	ilist_clear(&source);
	ilist_clear(&consume);

	assert(ilist_len(cities) >= 2);

	while (1)
	{
		if (count++ > 50)
			return FALSE;

		ilist_scramble(cities);

		one = cities[0];
		two = cities[1];

		dist_g = distance(one, two, TRUE);

		if (dist_g < LONG_ROUTE_MIN)
			continue;

		dist_r = distance(one, two, FALSE);

		if (dist_r < LONG_ROUTE_MIN || dist_r > LONG_ROUTE_MAX)
			continue;

		if (dist_g > dist_r || dist_r - dist_g <= 10)
			break;
	}

	dist = min(dist_r, dist_g);

	city_cluster(cities, &source, one);
	city_cluster(cities, &consume, two);

	printf("%s from %s-%d <%s> to %s-%d <%s>, d=%d g=%d\n",
			just_name(item),
			just_name(region(one)),
			ilist_len(source),
			box_code_less(one),
			just_name(region(two)),
			ilist_len(consume),
			box_code_less(two),
			dist_r, dist_g);

	long_route_sup(item, source, consume, dist);

	return TRUE;
}

#endif


int
rare_trade_items[] =
{
	item_fine_cloak,
	item_chocolate,
	item_ivory,
	item_rug,
	item_honey,
	item_ink,
	item_licorice,
	item_soap,
	item_old_book,
	item_jade_idol,
	item_purple_cloth,
	item_rose_perfume,
	item_silk,
	item_incense,
	item_ochre,
	item_jeweled_egg,
	item_obsidian,
	item_pepper,
	item_cardamom,
	item_orange,
	item_cinnabar,
	item_myrhh,
	item_saffron,
	0
};


int
common_trade_items[] =
{
	item_pipeweed,
	item_ale,
	item_tallow,
	item_candles,
	item_wool,
	item_vinegar,
	item_wax,
	item_sugar,
	item_salt,
	item_linen,
	item_beans,
	item_walnuts,
	item_flax,
	item_flutes,
	item_cassava,
	item_plum_wine,
	item_tea,
	0
};


#if 0
void
seed_long_routes()
{
	int i;
	static ilist all_cities = NULL;
	static ilist cities = NULL;
	static ilist regions = NULL;
	int reg = 0;
	int reg_len;
	int one, two;
	int item;

	ilist_clear(&all_cities);
	ilist_clear(&regions);
	clear_temps(T_loc);

	loop_city(i)
	{
		ilist_append(&all_cities, i);
		bx[region(i)]->temp++;
	}
	next_city;

	loop_loc(i)
	{
		if (bx[i]->temp)
			ilist_append(&regions, i);
	}
	next_loc;

	ilist_scramble(regions);
	reg_len = ilist_len(regions);

	for (item = 0; rare_trade_items[item] && reg+1 < reg_len; item++)
	{
		one = regions[reg];
		two = regions[reg+1];
		ilist_clear(&cities);

		for (i = 0; i < ilist_len(all_cities); i++)
			if (region(all_cities[i]) == one ||
			    region(all_cities[i]) == two)
				ilist_append(&cities, all_cities[i]);

		if (!long_route(cities, rare_trade_items[item]))
		        if (!long_route(all_cities, rare_trade_items[item]))
		        {
			    fprintf(stderr, ">>> Couldn't route %s!\n",
				just_name(rare_trade_items[item]));
		        }

		reg += 2;
	}

	for (; rare_trade_items[item]; item++)
	{
		if (!long_route(all_cities, rare_trade_items[item]))
		{
			fprintf(stderr, ">>> Couldn't route %s!\n",
				just_name(rare_trade_items[item]));
		}
	}
}
#endif


void
seed_common_tradegoods()
{
	int reg;
	int i, j;
	static ilist cities = NULL;
	static ilist goods = NULL;
	int source, consume;
	int item;
	int count;
	int qty, premium;

	loop_subkind(sub_region, reg)
	{
		if (loc_depth(reg) != LOC_region)
			continue;

		ilist_clear(&cities);
		ilist_clear(&goods);

		loop_city(i)
		{
			if (region(i) == reg)
				ilist_append(&cities, i);
		}
		next_city;

		for (i = 0; common_trade_items[i]; i++)
			ilist_append(&goods, common_trade_items[i]);

		ilist_scramble(cities);
		ilist_scramble(goods);

		for (i = 0, j = 0; i < ilist_len(cities) &&
					j < ilist_len(goods); i++, j++)
		{
			source = cities[i];
			item = goods[j];

			qty = (base_price(item) * item_weight(item) % 40) +
						rnd(6,12);

			add_city_trade(source, PRODUCE, item, qty,
						base_price(item) - rnd(0,1), 0);

			qty += rnd(-qty/10, qty/10);
			premium = rnd(100,200) / qty;

			count = 0;
			do
			{
				consume = cities[rnd(0, ilist_len(cities)-1)];
			}
			while (consume == source && count++ < 5);

			if (consume != source)
			{
				add_city_trade(consume, CONSUME, item, qty,
						base_price(item) + premium, 0);
			}
		}
	}
	next_subkind;
}


void
seed_rare_tradegoods()
{
	int reg;
	int i, j;
	static ilist cities = NULL;
	static ilist goods = NULL;
	int source, consume;
	int item;
	int count;
	int qty, premium;

	loop_subkind(sub_region, reg)
	{
		if (loc_depth(reg) != LOC_region)
			continue;

		ilist_clear(&cities);
		ilist_clear(&goods);

		loop_city(i)
		{
			if (region(i) == reg)
				ilist_append(&cities, i);
		}
		next_city;

		for (i = 0; rare_trade_items[i]; i++)
			ilist_append(&goods, rare_trade_items[i]);

		ilist_scramble(cities);
		ilist_scramble(goods);

		for (i = 0, j = 0; i < ilist_len(cities) &&
					j < ilist_len(goods); i++)
		{
			if (rnd(0,1))
				continue;

			item = goods[j++];
			qty = 100 + (item + base_price(item)) % 250;
			qty += rnd(-qty/10, qty/10);

			premium = rnd(1500, 3000) / qty;

			if (rnd(0,1) == 0)
			{
				fprintf(stderr, "%s sold in %s for %s\n",
					box_name_qty(item, qty),
					box_name(cities[i]),
					comma_num(base_price(item)));

				add_city_trade(cities[i], PRODUCE, item, qty,
						base_price(item)-rnd(0,1),
						rnd(1, NUM_MONTHS));
			}
			else
			{
				if (rnd(0,1))
					qty /= 2;

				add_city_trade(cities[i], CONSUME, item, qty,
						base_price(item) + premium, 0);

				fprintf(stderr, "%s bought in %s for %s\n",
					box_name_qty(item, qty),
					box_name(cities[i]),
					comma_num(base_price(item) + premium));
			}
		}
	}
	next_subkind;
}


void
seed_city(int where)
{

	seed_city_skill(where);
	seed_city_trade(where);
}


void
seed_initial_locations()
{

	int i;

	loop_city(i)
	{
		seed_city(i);
	}
	next_city;

	seed_common_tradegoods();
	seed_rare_tradegoods();

	loop_city(i)
	{
		loc_trade_sup(i, TRUE);
	}
	next_city;

	seed_has_been_run = TRUE;
}


void
seed_taxes()
{
	int where;
	int base;
	int pil;

	loop_loc(where)
	{
		if (loc_depth(where) != LOC_province &&
		    subkind(where) != sub_city)
			continue;

		if (subkind(where) == sub_ocean)
			continue;

		if (subkind(where) == sub_city)
		{
			consume_item(where, item_petty_thief,
					has_item(where, item_petty_thief));

			gen_item(where, item_petty_thief, 1);
		}

/*
 *  Magician menial labor cookies
 */

		consume_item(where, item_mage_menial,
					has_item(where, item_mage_menial));

		consume_item(where, item_tax_cookie,
					has_item(where, item_tax_cookie));

		gen_item(where, item_mage_menial, 1);

		assert(has_item(where, item_tax_cookie) == 0);

/*
 *  Tax base of province is equal to civilization level there
 */

		switch (loc_civ(province(where)))
		{
		case 0:
			base = 50;
			break;

		default:
			base = 150 + loc_civ(province(where)) * 50;
		}

/*
 *  Each point of loc_opium reduces tax base by 10%
 */

		base -= base/10 * loc_opium(where);

		assert(base > 0);

		if (pil = loc_pillage(where))
			base /= (pil + 1);

		gen_item(where, item_tax_cookie, base);
	}
	next_loc;
}


#if 0
static void
init_gate_dests()
{
	int i;
	int where;
	int dest;
	exit_views_list l;
	int j;

	loop_gate(i)
	{
		where = subloc(i);
		dest = gate_dest(i);

		if (in_faery(where) || in_faery(dest))
			continue;

		ilist_append(&p_misc(where)->gate_dest, dest);
		ilist_append(&p_misc(dest)->gate_dest, where);
	}
	next_gate;

	loop_loc(i)
	{
		if (loc_depth(i) == LOC_region)
			continue;

		l = exits_from_loc(0, i);

		for (j = 0; j < ilist_len(l); j++)
			ilist_append(&p_misc(i)->prov_dest, l[j]->destination);
	}
	next_loc(i);
}


int
distance(int orig, int dest, int gate)
{
	int i, j;
	int where;
	int flag = TRUE;
	static int gate_init = FALSE;
	struct entity_misc *p;

	if (!gate_init)
	{
		init_gate_dests();
		gate_init = TRUE;
	}

	clear_temps(T_loc);

	bx[orig]->temp = 1;

	while (bx[dest]->temp == 0 && flag)
	{
	    flag = FALSE;

	    loop_loc(where)
	    {
		if (bx[where]->temp > 0)
		{
		    p = rp_misc(where);

		    for (i = 0; i < ilist_len(p->prov_dest); i++)
		    {
			j = p->prov_dest[i];
			if (bx[j]->temp == 0)
			{
			    bx[j]->temp = bx[where]->temp + 1;
			    flag = TRUE;
			}
		    }

		    if (gate)
		    {
			for (i = 0; i < ilist_len(p->gate_dest); i++)
			{
			    j = p->gate_dest[i];
			    if (bx[j]->temp == 0)
			    {
				bx[j]->temp = bx[where]->temp + 1;
				flag = TRUE;
			    }
			}
		    }

		    if (where != dest)
			bx[where]->temp = -1;
		}
	    }
	    next_loc;
	}

	return bx[dest]->temp;
}
#endif

