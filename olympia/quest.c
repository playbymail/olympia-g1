
#include	<stdio.h>
#include	"z.h"
#include	"oly.h"


int subloc_player = 0;


static char *art_att_s[] = {"sword", "dagger", "longsword"};
static char *art_def_s[] = {"helm", "shield", "armor"};
static char *art_mis_s[] = {"spear", "bow", "javelin", "dart"};
static char *art_mag_s[] = {"ring", "staff", "amulet"};

static char *pref[] = {"magic", "golden", "crystal", "enchanted", "elven"};

static char *of_names[] = {
	"Achilles", "Darkness", "Justice", "Truth", "Norbus", "Dirbrand",
	"Pyrellica", "Halhere", "Eadak", "Faelgrar", "Napich", "Renfast",
	"Ociera", "Shavnor", "Dezarne", "Roshun", "Areth Lorbin", "Anarth",
	"Vernolt", "Pentara", "Gravecarn", "Sardis", "Lethrys", "Habyn",
	"Obraed", "Beebas", "Bayarth", "Haim", "Balatea", "Bobbiek", "Moldarth",
	"Grindor", "Sallen", "Ferenth", "Rhonius", "Ragnar", "Pallia", "Kior",
	"Baraxes", "Coinbalth", "Raskold", "Lassan", "Haemfrith", "Earnberict",
	"Sorale", "Lorbin", "Osgea", "Fornil", "Kuneack", "Davchar", "Urvil",
	"Pantarastar", "Cyllenedos", "Echaliatic", "Iniera", "Norgar", "Broen",
	"Estbeorn", "Claunecar", "Salamus", "Rhovanth", "Illinod", "Pictar",
	"Elakain", "Antresk", "Kichea", "Raigor", "Pactra", "Aethelarn",
	"Descarq", "Plagcath", "Nuncarth", "Petelinus", "Cospera", "Sarindor",
	"Albrand", "Evinob", "Dafarik", "Haemin", "Resh", "Tarvik", "Odasgunn",
	"Areth Pirn", "Miranth", "Dorenth", "Arkaune", "Kircarth", "Perendor",
	"Syssale", "Aelbarik", "Drassa", "Pirn", "Maire", "Lebrus", "Birdan",
	"Fistrock", "Shotluth", "Aldain", "Nantasarn", "Carim", "Ollayos",
	"Hamish", "Sudabuk", "Belgarth", "Woodhead",
	NULL
};


/*
 *  Create an Old book which offers instruction in a rare skill
 */

static void
make_teach_book(int who)
{
	int new;
	char *s;
	struct item_magic *p;
	int skill;

	new = create_unique_item(who, 0);

	switch (rnd(1,5))
	{
	case 1:		skill = sk_necromancy;		break;
	case 2:		skill = sk_scry;		break;
	case 3:		skill = sk_gate;		break;
	case 4:		skill = sk_weather;		break;
	case 5:		skill = sk_artifact;		break;

	default:
		assert(FALSE);
	}

	switch (rnd(1,2))
	{
	case 1:		s = "old book";			break;
	case 2:		s = "rare book";		break;

	default:
		assert(FALSE);
	}

	set_name(new, s);
	p_item(new)->weight = 5;
	p = p_item_magic(new);
	ilist_append(&p->may_study, skill);
}


/*
 *  Find an artifact in our region held by a subloc monster
 *  which is not only-defeatable by another artifact.
 */

static int
free_artifact(int where)
{
	int reg = region(where);
	int i;
	int owner;
	ilist l = NULL;
	int ret;

	loop_item(i)
	{
		if (subkind(i) != sub_artifact)
			continue;

		owner = item_unique(i);
		assert(owner);

		if (region(owner) != reg)
			continue;

		if (!is_npc(owner) ||
		    npc_program(owner) != PROG_subloc_monster)
			continue;

		if (only_defeatable(owner))
			continue;

		ilist_append(&l, i);
	}
	next_item;

	if (ilist_len(l) == 0)
		return 0;

	ret = l[rnd(0,ilist_len(l)-1)];

	ilist_reclaim(&l);

	return ret;
}


static int
new_artifact(int who)
{
	int new;
	char *s;

	new = create_unique_item(who, sub_artifact);

	switch (rnd(1,4))
	{
	case 1:
		s = art_att_s[rnd(0,2)];
		p_item_magic(new)->attack_bonus = rnd(1,10) * 5;
		break;

	case 2:
		s = art_def_s[rnd(0,2)];
		p_item_magic(new)->defense_bonus = rnd(1,10) * 5;
		break;

	case 3:
		s = art_mis_s[rnd(0,3)];
		p_item_magic(new)->missile_bonus = rnd(1,10) * 5;
		break;

	case 4:
		s = art_mag_s[rnd(0,2)];
		p_item_magic(new)->aura_bonus = rnd(1,3);
		break;

	default:
		assert(FALSE);
	}

	if (rnd(1,3) < 3)
	{
		s = sout("%s %s", pref[rnd(0,4)], s);
	}
	else
	{
		int i;

		for (i = 0; of_names[i]; i++)
			;
		i = rnd(0, i-1);

		s = sout("%s of %s", cap(s), of_names[i]);
	}

	p_item(new)->weight = 10;
	set_name(new, s);

	return new;
}


static int
new_monster(int where)
{
	int new;
	int item = 0;

	switch (subkind(where))
	{
	case sub_island:
		switch (rnd(1,3))
		{
		case 1:		item = item_pirate;		break;
		case 2:		item = item_spider;		break;
		case 3:		item = item_cyclops;		break;
		default:	assert(FALSE);
		}
		break;

	case sub_graveyard:
	case sub_battlefield:
		switch (rnd(1,3))
		{
		case 1:		item = item_corpse;		break;
		case 2:		item = item_skeleton;		break;
		case 3:		item = item_spirit;		break;
		default:	assert(FALSE);
		}
		break;

	case sub_ench_forest:
		switch (rnd(1,2))
		{
		case 1:		item = item_elf;		break;
		case 2:		item = item_faery;		break;
		default:	assert(FALSE);
		}
		break;

	default:
		switch (rnd(1,10))
		{
		case 1:		item = item_ratspider;		break;
		case 2:		item = item_centaur;		break;
		case 3:		item = item_minotaur;		break;
		case 4:		item = item_spider;		break;
		case 5:		item = item_rat;		break;
		case 6:		item = item_lion;		break;
		case 7:		item = item_bird;		break;
		case 8:		item = item_lizard;		break;
		case 9:		item = item_chimera;		break;
		case 10:	item = item_harpie;		break;
		case 11:	item = item_gorgon;		break;
		case 12:	item = item_giant;		break;
		case 13:	item = item_wolf;		break;
		case 14:	item = item_dragon;		break;
		default:	assert(FALSE);
		}
	}

	new = new_char(sub_ni, item, where, -1, npc_pl, LOY_npc, 0, NULL);

	switch (item)
	{
	case item_dragon:	/* always alone */
	case item_centaur:
	case item_minotaur:
	case item_giant:
	case item_chimera:
		break;

	default:
		gen_item(new, item, rnd(3, 15));
	}

	p_char(new)->npc_prog = PROG_subloc_monster;

	return new;
}


int
seed_subloc_with_monster(int where)
{
	int monster;

	monster = new_monster(where);

/*
 *  Give monster a treasure
 */

	switch (rnd(1,11))
	{
	case 1:
	case 2:
	case 3:
		gen_item(monster, item_gold, rnd(100, 3000));
		break;

	case 4:
	case 5:
	{
		int art;

		art = new_artifact(monster);
		break;
	}

	case 6:
	{
		int pris;
		char *name;

		switch (rnd(1,8))
		{
		case 1:	name = "Old man";		break;
		case 2:	name = "Old man";		break;
		case 3: name = "Knight";		break;
		case 4: name = "Princess";		break;
		case 5: name = "King's daughter";	break;
		case 6: name = "Nobleman";		break;
		case 7: name = "Merchant";		break;
		case 8: name = "Distressed Lady";	break;

		default:
			assert(FALSE);
		}

		pris = new_char(0, 0, monster, 100, indep_player,
						LOY_unsworn, 0, name);

		p_magic(pris)->swear_on_release = TRUE;
		p_char(pris)->prisoner = TRUE;
		break;
	}

	case 7:
	{
		extern int rare_trade_items[];
		int i;
		int item;

		for (i = 0; rare_trade_items[i]; i++)
			;

		i = rnd(0, i-1);
		item = rare_trade_items[i];

		gen_item(monster, item, rnd(50, 500));

		break;
	}

	case 8:
	{
		int item;

		if (rnd(1,2) == 1)
			item = item_nazgul;
		else
			item = item_pegasus;

		gen_item(monster, item, rnd(3, 10));
		break;
	}

	case 9:
	{
		gen_item(monster, item_elfstone, 1);
		break;
	}

	case 10:
	{
		int item;

		item = create_npc_token(monster);
		break;
	}

	case 11:
	{
		make_teach_book(monster);
		break;
	}

	case 12:
	{
		int item;

		item = new_orb(monster);
		break;
	}

	default:
		assert(FALSE);
	}

	if (rnd(1,6) == 1)
	{
		int item;

/*
 *  Temporarily set only_vulnerable for ourselves so we don't
 *  have a circular problem.  free_artifact() will take care of
 *  skipping over other only_vulnerable's.
 */
		p_misc(monster)->only_vulnerable = 1;
		item = free_artifact(monster);

		if (item)
			rp_misc(monster)->only_vulnerable = item;
		else
			rp_misc(monster)->only_vulnerable = 0;
	}

	return monster;
}


void
seed_monster_sublocs()
{
	int where;

	stage("seed_monster_sublocs()");

	loop_loc(where)
	{
		if (loc_depth(where) != LOC_subloc)
			continue;

		if (in_faery(where) || in_hades(where))
			continue;

		if (subkind(where) == sub_city)
			continue;

		if (controlled_humans_here(province(where)))
			continue;

		seed_subloc_with_monster(where);
	}
	next_loc;
}

