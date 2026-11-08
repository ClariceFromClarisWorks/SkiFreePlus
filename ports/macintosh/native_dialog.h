/* Modal System 6 Dialog Manager text viewer. Layout and allocation occur
 * outside gameplay. Long content scrolls once the available screen is used.
 * Shared dialog state makes this deliberately non-reentrant; callers pause
 * simulation and reset its deadline after the modal loop returns.
 */
/* System 6 Dialog Manager: native static text, button and scroll control.
   All allocation/layout is outside gameplay. */
#include <Dialogs.h>
#define DIALOG_LINES 64
#define DIALOG_CHARS 96
typedef struct {
  int count;
  char line[DIALOG_LINES][DIALOG_CHARS];
} DialogLines;
static DialogLines *dialog_lines_new(void) {
  return (DialogLines *)NewPtrClear(sizeof(DialogLines));
}
static void dialog_line(DialogLines *t, const char *s) {
  if (t && t->count < DIALOG_LINES) {
    strncpy(t->line[t->count], s, DIALOG_CHARS - 1);
    t->line[t->count][DIALOG_CHARS - 1] = 0;
    t->count++;
  }
}
static DialogLines *activeLines;
static DialogPtr activeDialog;
static ControlHandle dialogScroll;
static int dialogRows, dialogTop;
static void dialog_fill(void) {
  int i;
  for (i = 0; i < dialogRows; i++) {
    short type;
    Handle h;
    Rect r;
    Str255 p;
    const char *s = dialogTop + i < activeLines->count
                        ? activeLines->line[dialogTop + i]
                        : "";
    int n = strlen(s);
    p[0] = n;
    memcpy(p + 1, s, n);
    GetDialogItem(activeDialog, 2 + i, &type, &h, &r);
    SetDialogItemText(h, p);
  }
}
static void dialog_scroll_to(int n) {
  int max = activeLines->count - dialogRows;
  if (n < 0)
    n = 0;
  if (n > max)
    n = max;
  if (n != dialogTop) {
    dialogTop = n;
    if (dialogScroll)
      SetControlValue(dialogScroll, n);
    dialog_fill();
    DrawDialog(activeDialog);
  }
}
static pascal void dialog_scroll_action(ControlHandle c, short part) {
  int delta = part == inUpButton     ? -1
              : part == inDownButton ? 1
              : part == inPageUp     ? -dialogRows
              : part == inPageDown   ? dialogRows
                                     : 0;
  if (delta)
    dialog_scroll_to(dialogTop + delta);
}
static pascal Boolean dialog_filter(DialogPtr d, EventRecord *e, short *hit) {
  if (e->what == keyDown || e->what == autoKey) {
    unsigned char ch = e->message & 255;
    unsigned code = (e->message >> 8) & 127;
    if (ch == 13 || ch == 3 || ch == 27) {
      *hit = 1;
      return true;
    }
    if (dialogScroll &&
        (code == 125 || code == 126 || code == 116 || code == 121)) {
      *hit = 0;
      dialog_scroll_to(dialogTop + (code == 125   ? 1
                                    : code == 126 ? -1
                                    : code == 116 ? -dialogRows
                                                  : dialogRows));
      return true;
    }
  }
  if (e->what == mouseDown && dialogScroll) {
    Point p = e->where;
    ControlHandle c;
    short part;
    SetPort((GrafPtr)d);
    GlobalToLocal(&p);
    part = FindControl(p, (WindowPtr)d, &c);
    if (part && c == dialogScroll) {
      *hit = 0;
      if (part == inThumb) {
        TrackControl(c, p, NULL);
        dialog_scroll_to(GetControlValue(c));
      } else
        TrackControl(c, p, NewControlActionUPP(dialog_scroll_action));
      return true;
    }
  }
  return false;
}
static unsigned char *dialog_item(unsigned char *p, Rect *r, int type,
                                  const char *s) {
  int n = strlen(s);
  memset(p, 0, 4);
  p += 4;
  memcpy(p, r, 8);
  p += 8;
  *p++ = type;
  *p++ = n;
  memcpy(p, s, n);
  p += n;
  if (n & 1)
    *p++ = 0;
  return p;
}
static void native_text_dialog_width(const unsigned char *title,
                                     DialogLines *text, const char *buttonText,
                                     int minimumWidth) {
  GrafPtr old;
  GrafPort measuring;
  FontInfo fi;
  DialogLines *wrapped;
  Handle items;
  Rect r, bounds;
  unsigned char *p;
  int i, j, width = minimumWidth, maxWidth = 488, maxHeight = 298, rowHeight,
            rows, height, scroll, top;
  short hit;
  DialogPtr d;
  if (!text)
    return;
  wrapped = dialog_lines_new();
  if (!wrapped)
    return;
  GetPort(&old);
  OpenPort(&measuring);
  SetPort(&measuring);
  TextFont(0);
  TextSize(0);
  GetFontInfo(&fi);
  rowHeight = fi.ascent + fi.descent + fi.leading + 2;
  for (i = 0; i < text->count; i++) {
    int w = TextWidth(text->line[i], 0, strlen(text->line[i])) + 24;
    if (w > width)
      width = w;
  }
  if (width > maxWidth)
    width = maxWidth;
  /* Reserve scrollbar width before wrapping; consistent width in either case.
   */
  for (i = 0; i < text->count; i++) {
    const char *s = text->line[i];
    if (!*s)
      dialog_line(wrapped, "");
    while (*s && wrapped->count < DIALOG_LINES) {
      int n = strlen(s), cut;
      char line[DIALOG_CHARS];
      while (n > 1 && TextWidth((Ptr)s, 0, n) > width - 42)
        n--;
      cut = n;
      if (s[n]) {
        int k = n;
        while (k > 0 && s[k] != ' ')
          k--;
        if (k > 0)
          cut = k;
      }
      memcpy(line, s, cut);
      line[cut] = 0;
      dialog_line(wrapped, line);
      s += cut;
      while (*s == ' ')
        s++;
    }
  }
  ClosePort(&measuring);
  SetPort(old);
  rows = (maxHeight - 52) / rowHeight;
  if (rows < 1)
    rows = 1;
  if (rows > wrapped->count)
    rows = wrapped->count;
  if (rows < 1)
    rows = 1;
  scroll = wrapped->count > rows;
  height = rows * rowHeight + 52;
  top = 20 + (322 - height) / 2;
  SetRect(&bounds, (512 - width) / 2, top, (512 + width) / 2, top + height);
  items = NewHandleClear(2 + (rows + 1) * 32);
  if (!items) {
    DisposePtr((Ptr)wrapped);
    return;
  }
  HLock(items);
  p = (unsigned char *)*items;
  *(short *)p = rows;
  p += 2;
  SetRect(&r, width - 104, height - 32, width - 12, height - 12);
  p = dialog_item(p, &r, 4, buttonText);
  for (j = 0; j < rows; j++) {
    SetRect(&r, 12, 12 + j * rowHeight, width - (scroll ? 30 : 12),
            12 + (j + 1) * rowHeight);
    p = dialog_item(p, &r, 8 | 128, "");
  }
  HUnlock(items);
  d = NewDialog(NULL, &bounds, title, true, dBoxProc, (WindowPtr)-1, false, 0,
                items);
  if (!d) {
    DisposeHandle(items);
    DisposePtr((Ptr)wrapped);
    return;
  }
  SetPort((GrafPtr)d);
  activeDialog = d;
  activeLines = wrapped;
  dialogRows = rows;
  dialogTop = 0;
  dialogScroll = NULL;
  if (scroll) {
    SetRect(&r, width - 28, 12, width - 12, 12 + rows * rowHeight);
    dialogScroll = NewControl((WindowPtr)d, &r, "\p", true, 0, 0,
                              wrapped->count - rows, scrollBarProc, 0);
  }
  dialog_fill();
  DrawDialog(d);
  do {
    ModalDialog(NewModalFilterUPP(dialog_filter), &hit);
  } while (hit != 1);
  DisposeDialog(d);
  activeDialog = NULL;
  dialogScroll = NULL;
  activeLines = NULL;
  DisposePtr((Ptr)wrapped);
  SetPort(old);
}

static void native_text_dialog(const unsigned char *title, DialogLines *text,
                               const char *buttonText) {
  native_text_dialog_width(title, text, buttonText, 160);
}
