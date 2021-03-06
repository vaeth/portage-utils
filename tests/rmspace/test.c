/*
 * Copyright 2005-2018 Gentoo Foundation
 * Distributed under the terms of the GNU General Public License v2
 *
 * Copyright 2005-2008 Ned Ludd        - <solar@gentoo.org>
 * Copyright 2005-2014 Mike Frysinger  - <vapier@gentoo.org>
 */

#include "tests/tests.h"

#include "libq/rmspace.c"

int main(int argc, char *argv[])
{
	int i;
	char *s;
	size_t len;

	if (argc <= 1)
		return 1;

	for (i = 1; i < argc; ++i) {
		s = rmspace(argv[i]);
		len = strlen(s);
		if (isspace(s[0]) || isspace(s[len - 1])) {
			fprintf(stderr, "FAIL {%s}\n", s);
			return 1;
		}
	}

	return 0;
}
