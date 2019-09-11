/*
 * Unloved program to convert a binary on stdin to a C include on stdout
 *
 * Jan 1999 Matt Mackall <mpm@selenic.com>
 *
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 */

#include <stdio.h>

int main(int argc, char *argv[])
{
	int ch, total = 0;

	do {
		printf("\t\"");
		while ((ch = getchar()) != EOF) {
			total++;
			printf("\\x%02x", ch);
			if (total % 16 == 0)
				break;
		}
		printf("\"\n");
	} while (ch != EOF);

	return 0;
}
