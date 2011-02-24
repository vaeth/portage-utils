/*
 * Copyright 2005-2010 Gentoo Foundation
 * Distributed under the terms of the GNU General Public License v2
 * $Header: /var/cvsroot/gentoo-projects/portage-utils/libq/virtuals.c,v 1.25 2011/02/24 01:29:27 vapier Exp $
 *
 * Copyright 2005-2010 Ned Ludd        - <solar@gentoo.org>
 * Copyright 2005-2010 Mike Frysinger  - <vapier@gentoo.org>
 *
 * $Header: /var/cvsroot/gentoo-projects/portage-utils/libq/virtuals.c,v 1.25 2011/02/24 01:29:27 vapier Exp $
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

/* used to queue a lot of things */
struct queue_t {
	char *name;
	char *item;
	struct queue_t *next;
};

typedef struct queue_t queue;

/* global */
queue *virtuals = NULL;

queue *del_set(char *s, queue *q, int *ok);
queue *add_set(const char *vv, const char *ss, queue *q);

/* void free_virtuals(queue *list); */

static queue *append_set(queue *q, queue *ll)
{
	queue *z;

	if (!q)
		return ll;

	z = q;
	while (z->next)
		z = z->next;
	z->next = ll;

	return q;
}

/* add a set to a cache */
queue *add_set(const char *vv, const char *ss, queue *q)
{
	queue *ll;
	char *s, *ptr;
	char *v, *vptr;

	s = xstrdup(ss);
	v = xstrdup(vv);
	ptr = xmalloc(strlen(ss));
	vptr = xmalloc(strlen(vv));

	do {
		*ptr = 0;
		*vptr = 0;
		rmspace(ptr);
		rmspace(s);
		rmspace(vptr);
		rmspace(v);

		ll = xmalloc(sizeof(queue));
		ll->next = NULL;
		ll->name = xmalloc(strlen(v) + 1);
		ll->item = xmalloc(strlen(s) + 1);
		strcpy(ll->item, s);
		strcpy(ll->name, v);

		q = append_set(q, ll);

		*v = 0;
		strcpy(v, vptr);
		*s = 0;
		strcpy(s, ptr);

	} while (v[0]);
	free(s);
	free(ptr);
	free(v);
	free(vptr);
	return q;
}

/* remove a set from a cache. matches ->name and frees name,item */
queue *del_set(char *s, queue *q, int *ok)
{
	queue *ll, *list, *old;
	ll = q;
	list = q;
	old = q;
	*ok = 0;

	while (ll != NULL) {
		if (strcmp(ll->name, s) == 0) {
			if (ll == list) {
				list = (ll->next);
				free(ll->name);
				free(ll->item);
				free(ll);
				ll = list;

			} else {
				old->next = ll->next;
				free(ll->name);
				free(ll->item);
				free(ll);
				ll = old->next;
			}
			*ok = 1;
		} else {
			old = ll;
			ll = ll->next;
		}
	}
	return list;
}

/* clear out a list */
void free_sets(queue *list);
void free_sets(queue *list)
{
	queue *ll, *q;
	ll = list;
	while (ll != NULL) {
		q = ll->next;
		free(ll->name);
		free(ll->item);
		free(ll);
		ll = q;
	}
}

char *virtual(char *name, queue *list);
char *virtual(char *name, queue *list)
{
	queue *ll;
	for (ll = list; ll != NULL; ll = ll->next)
		if ((strcmp(ll->name, name)) == 0)
			return ll->item;
	return NULL;
}

void print_sets(queue *list);
void print_sets(queue *list)
{
	queue *ll;
	for (ll = list; ll != NULL; ll = ll->next)
		printf("%s -> %s\n", ll->name, ll->item);
}

queue *resolve_vdb_virtuals(char *vdb);
queue *resolve_vdb_virtuals(char *vdb)
{
	DIR *dir, *dirp;
	struct dirent *dentry_cat, *dentry_pkg;
	char buf[_Q_PATH_MAX];
	depend_atom *atom;

	xchdir("/");

	/* now try to run through vdb and locate matches for user inputs */
	if ((dir = opendir(vdb)) == NULL)
		return virtuals;

	/* scan all the categories */
	while ((dentry_cat = q_vdb_get_next_dir(dir)) != NULL) {
		snprintf(buf, sizeof(buf), "%s/%s", vdb, dentry_cat->d_name);
		if ((dirp = opendir(buf)) == NULL)
			continue;

		/* scan all the packages in this category */
		while ((dentry_pkg = q_vdb_get_next_dir(dirp)) != NULL) {
			char *p;
			/* see if user wants any of these packages */
			snprintf(buf, sizeof(buf), "%s/%s/%s/PROVIDE", vdb, dentry_cat->d_name, dentry_pkg->d_name);

			if (!eat_file(buf, buf, sizeof(buf)))
				continue;

			if ((p = strrchr(buf, '\n')) != NULL)
				*p = 0;

			rmspace(buf);

			if (*buf) {
				int ok = 0;
				char *v, *tmp = xstrdup(buf);
				snprintf(buf, sizeof(buf), "%s/%s", dentry_cat->d_name, dentry_pkg->d_name);

				atom = atom_explode(buf);
				if (!atom) {
					warn("could not explode '%s'", buf);
					continue;
				}
				sprintf(buf, "%s/%s", atom->CATEGORY, atom->PN);
				if ((v = virtual(tmp, virtuals)) != NULL) {
					/* IF_DEBUG(fprintf(stderr, "%s provided by %s (removing)\n", tmp, v)); */
					virtuals = del_set(tmp,  virtuals, &ok);
				}
				virtuals = add_set(tmp, buf, virtuals);
				/* IF_DEBUG(fprintf(stderr, "%s provided by %s/%s (adding)\n", tmp, atom->CATEGORY, dentry_pkg->d_name)); */
				free(tmp);
				atom_implode(atom);
			}
		}
	}

	return virtuals;
}

static queue *resolve_local_profile_virtuals(void)
{
	static size_t buflen;
	static char *buf = NULL;
	FILE *fp;
	char *p;
	static const char * const paths[] = { "/etc/portage/profile/virtuals", "/etc/portage/virtuals" };
	size_t i;

	for (i = 0; i < ARRAY_SIZE(paths); ++i) {
		fp = fopen(paths[i], "r");
		if (!fp)
			continue;

		while (getline(&buf, &buflen, fp) != -1) {
			if (*buf != 'v')
				continue;
			rmspace(buf);
			if ((p = strchr(buf, ' ')) != NULL) {
				int ok = 0;
				*p = 0;
				virtuals = del_set(buf, virtuals, &ok);
				virtuals = add_set(buf, rmspace(++p), virtuals);
				ok = 0;
			}
		}

		fclose(fp);
	}

	return virtuals;
}

static queue *resolve_virtuals(void)
{
	static char buf[_Q_PATH_MAX];
	static char savecwd[_Q_PATH_MAX];
	static char *p;
	FILE *fp;

	xgetcwd(savecwd, sizeof(savecwd));

	free_sets(virtuals);
	virtuals = resolve_local_profile_virtuals();
	virtuals = resolve_vdb_virtuals(portvdb);

	if (chdir("/etc/") == -1)
		return virtuals;

	if (readlink("make.profile", buf, sizeof(buf)) != -1) {
		xchdir(buf);
		xgetcwd(buf, sizeof(buf));
		if (access(buf, R_OK) != 0)
			return virtuals;
	vstart:
		if ((fp = fopen("virtuals", "r")) != NULL) {
			while ((fgets(buf, sizeof(buf), fp)) != NULL) {
				if (*buf != 'v') continue;
				rmspace(buf);
				if ((p = strchr(buf, ' ')) != NULL) {
					*p = 0;
					if (virtual(buf, virtuals) == NULL)
						virtuals = add_set(buf, rmspace(++p), virtuals);
				}
			}
			fclose(fp);
		}
		if ((fp = fopen("parent", "r")) != NULL) {
			while ((fgets(buf, sizeof(buf), fp)) != NULL) {
				rmspace(buf);
				if (!*buf) continue;
				if (*buf == '#') continue;
				if (isspace(*buf)) continue;
				fclose(fp);
				if (chdir(buf) == -1) {
					fclose(fp);
					goto done;
				}
				goto vstart;
			}
			fclose(fp);
		}
	}
 done:
	xchdir(savecwd);
	return virtuals;
}
