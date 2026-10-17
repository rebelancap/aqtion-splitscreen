/*
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

// split_view.c -- splitscreen viewport cells
//
// Every context in use gets a cell (window pixels) from a fixed layout
// table, assigned in slot (playerNum) order. Slots are never renumbered;
// the cells follow the players in use, so after player 2 leaves player 3
// draws in the second cell. The layout is recomputed whenever the number
// of players, the window size, cl_splitVertical, cl_splitFill or
// cl_splitWidePlayer changes (checked once per drawn frame, and forced on
// join / leave). Fill layouts have rows of different cell sizes; the
// post-process FBO is sized to the largest (refresh GL_SizeFramebuffers).

#include "../client.h"
#include <time.h>

static cvar_t   *cl_splitVertical;
static cvar_t   *cl_splitBorder;
static cvar_t   *cl_splitFill;
static cvar_t   *cl_splitWidePlayer;

static struct {
    int         count;              // players with a cell
    unsigned    mask;               // their slots (players, notices << 8)
    int         width, height;      // window size the cells were made for
    int         vertical, fill, wide;
    int         numrows;
    int         rows[3];            // cells per row
    int         numcells;           // all cells, the empty ones too (borders)
    vrect_t     cells[MAX_SPLITVIEW + 1];
    int         numempty;
    vrect_t     empty[MAX_SPLITVIEW];
    vrect_t     notice[MAX_SPLITVIEW];  // cells of players that just left (below)
    bool        had[MAX_SPLITVIEW];     // slot had a cell last time (join order)
    unsigned    joinseq[MAX_SPLITVIEW];
    unsigned    seq;
} split_layout;

/*
A player the server refused, kicked or timed out keeps its cell for
SPLIT_NOTICE_TIME with the reason in it (Q3 R11: "in its cell for 10 s"),
unless the slot is taken again sooner. Only while player 1 is in a game.
*/
#define SPLIT_NOTICE_TIME   10000

static struct {
    char        text[MAX_QPATH * 2];
    unsigned    until;
} split_notices[MAX_SPLITVIEW];

void CL_SplitSetNotice(int n, const char *text)
{
    if (n < 1 || n >= MAX_SPLITVIEW)
        return;
    if (!text) {
        split_notices[n].until = 0;
        return;
    }
    Q_strlcpy(split_notices[n].text, text, sizeof(split_notices[n].text));
    split_notices[n].until = Sys_Milliseconds() + SPLIT_NOTICE_TIME;
}

void CL_SplitClearNotices(void)
{
    int i;

    for (i = 0; i < MAX_SPLITVIEW; i++)
        split_notices[i].until = 0;
}

static bool CL_SplitNoticeUp(int i)
{
    const client_context_t *p1 = clx[0];

    if (i < 1 || clx[i] || !split_notices[i].until)
        return false;
    if ((int)(split_notices[i].until - Sys_Milliseconds()) <= 0 ||
        (p1 == cla ? cls.state : p1->conn.state) < ca_connected) {
        split_notices[i].until = 0;
        return false;
    }
    return true;
}

// the text and cell of slot i's notice, NULL if none is up
const char *CL_SplitCellNotice(int i, vrect_t *rc)
{
    if (!CL_SplitNoticeUp(i) || split_layout.count < 2)
        return NULL;
    *rc = split_layout.notice[i];
    if (rc->width <= 0 || rc->height <= 0)
        return NULL;
    return split_notices[i].text;
}

// cells drawn: contexts in use + the notices that are up
int CL_SplitViewCount(void)
{
    int i, count = 0;

    for (i = 0; i < MAX_SPLITVIEW; i++)
        if ((clx[i] && clx[i]->inUse) || CL_SplitNoticeUp(i))
            count++;
    return count;
}

/*
Layouts (Q3 fork design 3.2, the same table):

  N  cl_splitFill 1 (default)          cl_splitFill 0
  1  full window
  2  top / bottom (cl_splitVertical 1: side by side)
  3  2 + 1 (wide bottom cell)          2x2, one black
  4  2x2
  5  3 + 2 (wider bottom row)          3x2, one black
  6  3x2
  7  4 + 3 (wider bottom row)          3x3 (cells keep the screen aspect), two black
  8  4x2                               3x3, one black

The wide row is the bottom row. cl_splitWidePlayer N puts player N in its
first cell (0 = the player who joined last); the others follow in slot order.
*/
static int CL_SplitRows(int count, bool fill, bool vertical, int *rows)
{
    switch (count) {
    case 0:
    case 1:
        rows[0] = 1;
        return 1;
    case 2:
        if (vertical) {
            rows[0] = 2;
            return 1;
        }
        rows[0] = rows[1] = 1;
        return 2;
    case 3:
        rows[0] = 2;
        rows[1] = fill ? 1 : 2;
        return 2;
    case 4:
        rows[0] = rows[1] = 2;
        return 2;
    case 5:
        rows[0] = 3;
        rows[1] = fill ? 2 : 3;
        return 2;
    case 6:
        rows[0] = rows[1] = 3;
        return 2;
    default:
        if (fill) {
            rows[0] = 4;
            rows[1] = count == 7 ? 3 : 4;
            return 2;
        }
        rows[0] = rows[1] = rows[2] = 3;
        return 3;
    }
}

/*
====================
CL_SplitLayout

Recomputes every context's cell if the players, the window size or a
layout cvar changed (force: always).
====================
*/
void CL_SplitLayout(bool force)
{
    int slots[MAX_SPLITVIEW], order[MAX_SPLITVIEW];
    int i, j, k, n, r, c, pos, wide, count = CL_SplitViewCount();
    int vertical = cl_splitVertical ? !!cl_splitVertical->integer : 0;
    int fill = cl_splitFill ? !!cl_splitFill->integer : 1;
    int widePlayer = cl_splitWidePlayer ? cl_splitWidePlayer->integer : 0;
    unsigned last, mask = 0;
    vrect_t *rc;

    // join order: a slot that got a cell since the last layout joined last
    for (i = 0; i < MAX_SPLITVIEW; i++) {
        bool in = clx[i] && clx[i]->inUse;
        if (in && !split_layout.had[i])
            split_layout.joinseq[i] = ++split_layout.seq;
        split_layout.had[i] = in;
        if (in)
            mask |= 1u << i;
        else if (CL_SplitNoticeUp(i))
            mask |= 1u << (i + 8);
    }

    // the set of slots, not only their number: a notice that expires in the
    // frame a player joins must still re-run the layout
    if (!force && count == split_layout.count && mask == split_layout.mask && vertical == split_layout.vertical &&
        fill == split_layout.fill && widePlayer == split_layout.wide &&
        r_config.width == split_layout.width && r_config.height == split_layout.height)
        return;

    split_layout.count = count;
    split_layout.mask = mask;
    split_layout.vertical = vertical;
    split_layout.fill = fill;
    split_layout.wide = widePlayer;
    split_layout.width = r_config.width;
    split_layout.height = r_config.height;
    split_layout.numrows = CL_SplitRows(count, fill, vertical, split_layout.rows);
    split_layout.numcells = split_layout.numempty = 0;

    if (count < 2) {
        for (i = 0; i < MAX_SPLITVIEW; i++) {
            if (clx[i] && clx[i]->inUse) {
                clx[i]->cell.x = clx[i]->cell.y = 0;
                clx[i]->cell.width = r_config.width;
                clx[i]->cell.height = r_config.height;
            }
        }
        return;
    }

    // the slots with a cell: players in use first (slot order), then the
    // notices; a notice never takes a cell a real player needs, and one
    // without a cell left (should not happen) is simply not drawn
    for (i = n = 0; i < MAX_SPLITVIEW; i++)
        if (clx[i] && clx[i]->inUse)
            slots[n++] = i;
    for (i = 0; i < MAX_SPLITVIEW; i++) {
        split_layout.notice[i].width = split_layout.notice[i].height = 0;
        if (CL_SplitNoticeUp(i) && n < count)
            slots[n++] = i;
    }

    // who gets the first wide cell: cl_splitWidePlayer, else the last joined
    wide = -1;
    if (widePlayer >= 1 && widePlayer <= MAX_SPLITVIEW && clx[widePlayer - 1] && clx[widePlayer - 1]->inUse) {
        wide = widePlayer - 1;
    } else {
        for (i = 1, last = 0; i < MAX_SPLITVIEW; i++) {
            if (clx[i] && clx[i]->inUse && split_layout.joinseq[i] > last) {
                last = split_layout.joinseq[i];
                wide = i;
            }
        }
    }

    for (i = 0; i < n; i++)
        order[i] = slots[i];
    r = split_layout.numrows;
    if (r > 1 && split_layout.rows[r - 1] < split_layout.rows[0] && wide >= 0) {
        for (j = pos = 0; j < r - 1; j++)
            pos += split_layout.rows[j];        // first cell of the wide row
        pos = min(pos, n - 1);
        for (i = 0; i < n && order[i] != wide; i++)
            ;
        if (i < n) {
            if (i < pos) {
                for (j = i; j < pos; j++)
                    order[j] = order[j + 1];
            } else {
                for (j = i; j > pos; j--)
                    order[j] = order[j - 1];
            }
            order[pos] = wide;
        }
    }

    for (r = k = 0; r < split_layout.numrows; r++) {
        for (c = 0; c < split_layout.rows[r]; c++, k++) {
            if (k < n) {
                i = order[k];
                rc = (clx[i] && clx[i]->inUse) ? &clx[i]->cell : &split_layout.notice[i];
            } else {
                rc = &split_layout.empty[split_layout.numempty++];
            }
            rc->x = c * r_config.width / split_layout.rows[r];
            rc->width = (c + 1) * r_config.width / split_layout.rows[r] - rc->x;
            rc->y = r * r_config.height / split_layout.numrows;
            rc->height = (r + 1) * r_config.height / split_layout.numrows - rc->y;
            split_layout.cells[split_layout.numcells++] = *rc;
        }
    }

    Com_Printf("splitscreen: %d players, rows %d", count, split_layout.rows[0]);
    for (r = 1; r < split_layout.numrows; r++)
        Com_Printf("+%d", split_layout.rows[r]);
    Com_Printf(" (%s), smallest cell %dx%d in %dx%d\n", fill ? "fill" : "grid",
               r_config.width / split_layout.rows[0], r_config.height / split_layout.numrows,
               r_config.width, r_config.height);
}

// unused cells of the layout (grid mode), drawn black
void CL_SplitDrawEmptyCells(void)
{
    int i;

    for (i = 0; i < split_layout.numempty; i++)
        R_DrawFill8(split_layout.empty[i].x, split_layout.empty[i].y,
                    split_layout.empty[i].width, split_layout.empty[i].height, 0);
}

/*
====================
CL_SplitDrawBorders

A line between the cells (cl_splitBorder 1, default): 2 px from a 720-line
window up, else 1, black like the empty cells (the Quake3e fork draws no
border; its cells meet edge to edge). Drawn on each cell's right and bottom
edge inside the window, so rows of different widths get their own lines.
====================
*/
void CL_SplitDrawBorders(void)
{
    const vrect_t *rc;
    int i, w;

    if (!cl_splitBorder->integer || split_layout.count < 2)
        return;
    if (split_layout.width != r_config.width || split_layout.height != r_config.height)
        return;

    w = r_config.height >= 720 ? 2 : 1;
    for (i = 0; i < split_layout.numcells; i++) {
        rc = &split_layout.cells[i];
        if (rc->x + rc->width < r_config.width)
            R_DrawFill8(rc->x + rc->width - w / 2, rc->y, w, rc->height, 0);
        if (rc->y + rc->height < r_config.height)
            R_DrawFill8(rc->x, rc->y + rc->height - w / 2, rc->width, w, 0);
    }
}

/*
====================
CL_SplitViewReady

With context i active in the screen loop: may its view be drawn? Player 1
always (SCR_DrawActive handles its states). An extra only while player 1
is in the game on the same level: its model / image handles are copies of
player 1's (CL_SplitCopyRegistration), stale while player 1 reloads.
====================
*/
bool CL_SplitViewReady(void)
{
    client_context_t *p1 = clx[0];

    if (!cla->playerNum)
        return true;
    if (cls.state != ca_active)
        return true;    // SCR_DrawActive draws it black
    if ((p1 == cla ? cls.state : p1->conn.state) != ca_active)
        return false;
    return CL_SplitServerCount(p1) == cl.servercount;
}

/*
====================
cl_splitperf

Developer aid for the M4 perf pass: CPU time per phase of the frame
(splitPerf_t), per view and in total, and the frame rate, printed every
2 s. R_RenderFrame is the time to submit the scene; GPU work that the
driver queues shows up in R_EndFrame (swap) or in a later phase.
====================
*/
static cvar_t   *cl_splitperf;

static struct {
    double      last, start;
    double      sum[SPLIT_PERF_MAX];
    int         cur;
    int         frames, views;
} split_perf;

static double CL_SplitPerfNow(void)
{
    struct timespec ts;

    timespec_get(&ts, TIME_UTC);
    return ts.tv_sec * 1e6 + ts.tv_nsec * 1e-3;     // microseconds
}

void CL_SplitPerfMark(splitPerf_t phase)
{
    double now;

    if (!cl_splitperf || !cl_splitperf->integer)
        return;
    now = CL_SplitPerfNow();
    if (split_perf.last > 0)
        split_perf.sum[split_perf.cur] += now - split_perf.last;
    else
        split_perf.start = now;
    split_perf.last = now;
    split_perf.cur = phase;
    if (phase == SPLIT_PERF_SCENE)
        split_perf.views++;
}

void CL_SplitPerfFrame(void)
{
    double now, total, f, v;
    int i;

    if (!cl_splitperf || !cl_splitperf->integer) {
        split_perf.last = 0;
        return;
    }
    if (split_perf.last <= 0)
        return;
    split_perf.frames++;
    now = CL_SplitPerfNow();
    total = now - split_perf.start;
    if (total < 2e6)
        return;

    CL_SplitPerfMark(SPLIT_PERF_NONE);
    f = 1000.0 * split_perf.frames;         // usec -> ms per frame
    v = 1000.0 * max(split_perf.views, 1);
    Com_Printf("splitperf: %d views %dx%d fps %.0f frame %.3f ms | per frame: scene %.3f render %.3f "
               "2d %.3f swap %.3f other %.3f | per view: scene %.3f render %.3f ms\n",
               (split_perf.views + split_perf.frames / 2) / split_perf.frames,
               r_config.width, r_config.height,
               split_perf.frames * 1e6 / total, total / f,
               split_perf.sum[SPLIT_PERF_SCENE] / f, split_perf.sum[SPLIT_PERF_RENDER] / f,
               split_perf.sum[SPLIT_PERF_2D] / f, split_perf.sum[SPLIT_PERF_SWAP] / f,
               split_perf.sum[SPLIT_PERF_NONE] / f,
               split_perf.sum[SPLIT_PERF_SCENE] / v, split_perf.sum[SPLIT_PERF_RENDER] / v);

    for (i = 0; i < SPLIT_PERF_MAX; i++)
        split_perf.sum[i] = 0;
    split_perf.frames = split_perf.views = 0;
    split_perf.start = split_perf.last;
}

void CL_SplitInitView(void)
{
    cl_splitperf = Cvar_Get("cl_splitperf", "0", 0);
    cl_splitVertical = Cvar_Get("cl_splitVertical", "0", CVAR_ARCHIVE);
    cl_splitBorder = Cvar_Get("cl_splitBorder", "1", CVAR_ARCHIVE);
    cl_splitFill = Cvar_Get("cl_splitFill", "1", CVAR_ARCHIVE);
    cl_splitWidePlayer = Cvar_Get("cl_splitWidePlayer", "0", CVAR_ARCHIVE);
}
