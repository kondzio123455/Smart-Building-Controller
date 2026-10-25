#ifndef HELP_H
#define HELP_H

#include <stdbool.h>

/* The how-to-play screens, and the little hint boxes that appear when you point at things. */
void help_show(bool show);
void help_page(int page);               /* jump to a page (0-2) */
bool help_visible(void);
void help_update(void);                  /* every frame: H, the arrow keys and clicks */
void help_draw(void);                    /* draws only when open */
void help_tooltip(const char *text);     /* a hint box next to the mouse; '\n' starts a new line */

#endif
