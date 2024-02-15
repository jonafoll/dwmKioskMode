        /* See LICENSE file for copyright and license details.
         *
         * dynamic window manager is designed like any other X client as well. It is
         * driven through handling X events. In contrast to other X clients, a window
         * manager selects for SubstructureRedirectMask on the root window, to receive
         * events about window (dis-)appearance. Only one X connection at a time is
         * allowed to select for this event mask.
         *
         * The event handlers of dwm are organized in an array which is accessed
         * whenever a new event has been fetched. This allows event dispatching
         * in O(1) time.
         *
         * Each child of the root window is called a client, except windows which have
         * set the override_redirect flag. Clients are organized in a linked client
         * list on each monitor, the focus history is remembered through a stack list
         * on each monitor. Each client contains a bit array to indicate the tags of a
         * client.
         *
         * Keys and tagging rules are organized as arrays and defined in config.h.
         *
         * To understand everything else, start reading main().
         */
        #include <errno.h>
        #include <locale.h>
        #include <signal.h>
        #include <stdarg.h>
        #include <stdio.h>
        #include <stdlib.h>
        #include <string.h>
        #include <unistd.h>
        #include <sys/types.h>
        #include <sys/wait.h>
        #include <X11/cursorfont.h>
        #include <X11/keysym.h>
        #include <X11/Xatom.h>
        #include <X11/Xlib.h>
        #include <X11/Xproto.h>
        #include <X11/Xutil.h>
        #ifdef XINERAMA
        #include <X11/extensions/Xinerama.h>
        #endif /* XINERAMA */
        #include <X11/Xft/Xft.h>

        #include "drw.h"
        #include "util.h"

        /* macros */
        #define BUTTONMASK              (ButtonPressMask|ButtonReleaseMask)
        #define CLEANMASK(mask)         (mask & ~(numlockmask|LockMask) & (ShiftMask|ControlMask|Mod1Mask|Mod2Mask|Mod3Mask|Mod4Mask|Mod5Mask))
        #define INTERSECT(x,y,w,h,m)    (MAX(0, MIN((x)+(w),(m)->wx+(m)->ww) - MAX((x),(m)->wx)) \
                                        * MAX(0, MIN((y)+(h),(m)->wy+(m)->wh) - MAX((y),(m)->wy)))
        #define ISVISIBLE(C)            ((C->tags & C->mon->tagset[C->mon->seltags]))
        #define LENGTH(X)               (sizeof X / sizeof X[0])
        #define MOUSEMASK               (BUTTONMASK|PointerMotionMask)
        #define WIDTH(X)                ((X)->w + 2 * (X)->bw)
        #define HEIGHT(X)               ((X)->h + 2 * (X)->bw)
        #define TAGMASK                 ((1 << LENGTH(tags)) - 1)
        #define TEXTW(X)                (drw_fontset_getwidth(drw, (X)) + lrpad)

        /* enums */
        enum { CurNormal, CurResize, CurMove, CurLast }; /* cursor */
        enum { SchemeNorm, SchemeSel }; /* color schemes */
        enum { NetSupported, NetWMName, NetWMState, NetWMCheck,
               NetWMFullscreen, NetActiveWindow, NetWMWindowType,
               NetWMWindowTypeDialog, NetClientList, NetLast }; /* EWMH atoms */
        enum { WMProtocols, WMDelete, WMState, WMTakeFocus, WMLast }; /* default atoms */
        enum { ClkTagBar, ClkLtSymbol, ClkStatusText, ClkWinTitle,
               ClkClientWin, ClkRootWin, ClkLast }; /* clicks */

    typedef union {
        int i;
        unsigned int ui;
        float f;
        const void *v;
    } Arg;

    typedef struct {
        unsigned int click;
        unsigned int mask;
        unsigned int button;
        void (*func)(const Arg *arg);
        const Arg arg;
    } Button;

    typedef struct Monitor Monitor;
    typedef struct Client Client;
    struct Client {
        char name[256];
        float mina, maxa;
        int x, y, w, h;
        int oldx, oldy, oldw, oldh;
        int basew, baseh, incw, inch, maxw, maxh, minw, minh, hintsvalid;
        int bw, oldbw;
        unsigned int tags;
        int isfixed, isfloating, isurgent, neverfocus, oldstate, isfullscreen;
        Client *next;
        Client *snext;
        Monitor *mon;
        Window win;
    };

    typedef struct {
        unsigned int mod;
        KeySym keysym;
        void (*func)(const Arg *);
        const Arg arg;
    } Key;

    typedef struct {
        const char *symbol;
        void (*arrange)(Monitor *);
    } Layout;

    struct Monitor {
        char ltsymbol[16];
        float mfact;
        int nmaster;
        int num;
        int by;               /* bar geometry */
        int mx, my, mw, mh;   /* screen size */
        int wx, wy, ww, wh;   /* window area  */
        unsigned int seltags;
        unsigned int sellt;
        unsigned int tagset[2];
        int showbar;
        int topbar;
        Client *clients;
        Client *sel;
        Client *stack;
        Monitor *next;
        Window barwin;
        const Layout *lt[2];
    };

    typedef struct {
        const char *class;
        const char *instance;
        const char *title;
        unsigned int tags;
        int isfloating;
        int monitor;
    } Rule;

    /* function declarations */
    static void applyrules(Client *c);
    static int applysizehints(Client *c, int *x, int *y, int *w, int *h, int interact);
    static void arrange(Monitor *m);
    static void arrangemon(Monitor *m);
    static void attach(Client *c);
    static void attachstack(Client *c);
    static void buttonpress(XEvent *e);
    static void checkotherwm(void);
    static void cleanup(void);
    static void cleanupmon(Monitor *mon);
    static void clientmessage(XEvent *e);
    static void configure(Client *c);
    static void configurenotify(XEvent *e);
    static void configurerequest(XEvent *e);
    static Monitor *createmon(void);
    static void destroynotify(XEvent *e);
    static void detach(Client *c);
    static void detachstack(Client *c);
    static Monitor *dirtomon(int dir);
    static void drawbar(Monitor *m);
    static void drawbars(void);
    static void enternotify(XEvent *e);
    static void expose(XEvent *e);
    static void focus(Client *c);
    static void focusin(XEvent *e);
    static void focusmon(const Arg *arg);
    static void focusstack(const Arg *arg);
    static Atom getatomprop(Client *c, Atom prop);
    static int getrootptr(int *x, int *y);
    static long getstate(Window w);
    static int gettextprop(Window w, Atom atom, char *text, unsigned int size);
    static void grabbuttons(Client *c, int focused);
    static void grabkeys(void);
    static void incnmaster(const Arg *arg);
    static void keypress(XEvent *e);
    static void killclient(const Arg *arg);
    static void manage(Window w, XWindowAttributes *wa);
    static void mappingnotify(XEvent *e);
    static void maprequest(XEvent *e);
    static void monocle(Monitor *m);
    static void motionnotify(XEvent *e);
    static void movemouse(const Arg *arg);
    static Client *nexttiled(Client *c);
    static void pop(Client *c);
    static void propertynotify(XEvent *e);
    static void quit(const Arg *arg);
    static Monitor *recttomon(int x, int y, int w, int h);
    static void resize(Client *c, int x, int y, int w, int h, int interact);
    static void resizeclient(Client *c, int x, int y, int w, int h);
    static void resizemouse(const Arg *arg);
    static void restack(Monitor *m);
    static void run(void);
    static void scan(void);
    static int sendevent(Client *c, Atom proto);
    static void sendmon(Client *c, Monitor *m);
    static void setclientstate(Client *c, long state);
    static void setfocus(Client *c);
    static void setfullscreen(Client *c, int fullscreen);
    static void setlayout(const Arg *arg);
    static void setmfact(const Arg *arg);
    static void setup(void);
    static void seturgent(Client *c, int urg);
    static void showhide(Client *c);
    static void spawn(const Arg *arg);
    static void tag(const Arg *arg);
    static void tagmon(const Arg *arg);
    static void tile(Monitor *m);
    static void togglebar(const Arg *arg);
    static void togglefloating(const Arg *arg);
    static void toggletag(const Arg *arg);
    static void toggleview(const Arg *arg);
    static void unfocus(Client *c, int setfocus);
    static void unmanage(Client *c, int destroyed);
    static void unmapnotify(XEvent *e);
    static void updatebarpos(Monitor *m);
    static void updatebars(void);
    static void updateclientlist(void);
    static int updategeom(void);
    static void updatenumlockmask(void);
    static void updatesizehints(Client *c);
    static void updatestatus(void);
    static void updatetitle(Client *c);
    static void updatewindowtype(Client *c);
    static void updatewmhints(Client *c);
    static void view(const Arg *arg);
    static Client *wintoclient(Window w);
    static Monitor *wintomon(Window w);
    static int xerror(Display *dpy, XErrorEvent *ee);
    static int xerrordummy(Display *dpy, XErrorEvent *ee);
    static int xerrorstart(Display *dpy, XErrorEvent *ee);
    static void zoom(const Arg *arg);

    /* variables */
    static const char broken[] = "broken";
    static char stext[256];
    static int screen;
    static int sw, sh;           /* X display screen geometry width, height */
    static int bh;               /* bar height */
    static int lrpad;            /* sum of left and right padding for text */
    static int (*xerrorxlib)(Display *, XErrorEvent *);
    static unsigned int numlockmask = 0;
    static void (*handler[LASTEvent]) (XEvent *) = {
        [ButtonPress] = buttonpress,
        [ClientMessage] = clientmessage,
        [ConfigureRequest] = configurerequest,
        [ConfigureNotify] = configurenotify,
        [DestroyNotify] = destroynotify,
        [EnterNotify] = enternotify,
        [Expose] = expose,
        [FocusIn] = focusin,
        [KeyPress] = keypress,
        [MappingNotify] = mappingnotify,
        [MapRequest] = maprequest,
        [MotionNotify] = motionnotify,
        [PropertyNotify] = propertynotify,
        [UnmapNotify] = unmapnotify
    };
    static Atom wmatom[WMLast], netatom[NetLast];
    static int running = 1;
    static Cur *cursor[CurLast];
    static Clr **scheme;
    static Display *dpy;
    static Drw *drw;
    static Monitor *mons, *selmon;
    static Window root, wmcheckwin;

    /* configuration, allows nested code to access above variables */
    #include "config.h"

    /* compile-time check if all tags fit into an unsigned int bit array. */
    struct NumTags { char limitexceeded[LENGTH(tags) > 31 ? -1 : 1]; };

    /* function implementations */
    void applyrules(Client *c)
    {
        const char *class, *instance;
        unsigned int i;
        const Rule *r;
        Monitor *m;
        XClassHint ch = { NULL, NULL };

        /* rule matching */
        c->isfloating = 0;
        c->tags = 0;
        XGetClassHint(dpy, c->win, &ch);
        class    = ch.res_class ? ch.res_class : broken;
        instance = ch.res_name  ? ch.res_name  : broken;

        for (i = 0; i < LENGTH(rules); i++) {
            r = &rules[i];
            if ((!r->title || strstr(c->name, r->title))
            && (!r->class || strstr(class, r->class))
            && (!r->instance || strstr(instance, r->instance)))
            {
                c->isfloating = r->isfloating;
                c->tags |= r->tags;
                for (m = mons; m && m->num != r->monitor; m = m->next);
                if (m)
                    c->mon = m;
            }
        }
        if (ch.res_class)
            XFree(ch.res_class);
        if (ch.res_name)
            XFree(ch.res_name);
        c->tags = c->tags & TAGMASK ? c->tags & TAGMASK : c->mon->tagset[c->mon->seltags];
    }

    int applysizehints(Client *c, int *x, int *y, int *w, int *h, int interact)
    {
        int baseismin;
        Monitor *m = c->mon;

        /* set minimum possible */
        *w = MAX(1, *w);
        *h = MAX(1, *h);
        if (interact) {
            if (*x > sw)
                *x = sw - WIDTH(c);
            if (*y > sh)
                *y = sh - HEIGHT(c);
            if (*x + *w + 2 * c->bw < 0)
                *x = 0;
            if (*y + *h + 2 * c->bw < 0)
                *y = 0;
        } else {
            if (*x >= m->wx + m->ww)
                *x = m->wx + m->ww - WIDTH(c);
            if (*y >= m->wy + m->wh)
                *y = m->wy + m->wh - HEIGHT(c);
            if (*x + *w + 2 * c->bw <= m->wx)
                *x = m->wx;
            if (*y + *h + 2 * c->bw <= m->wy)
                *y = m->wy;
        }
        if (*h < bh)
            *h = bh;
        if (*w < bh)
            *w = bh;
        if (resizehints || c->isfloating || !c->mon->lt[c->mon->sellt]->arrange) {
            if (!c->hintsvalid)
                updatesizehints(c);
            /* see last two sentences in ICCCM 4.1.2.3 */
            baseismin = c->basew == c->minw && c->baseh == c->minh;

    if (!c)
        return;
    if (cme->message_type == netatom[NetWMState]) {
        if (cme->data.l[1] == netatom[NetWMFullscreen]
        || cme->data.l[2] == netatom[NetWMFullscreen])
            setfullscreen(c, (cme->data.l[0] == 1 /* _NET_WM_STATE_ADD    */
                || (cme->data.l[0] == 2 /* _NET_WM_STATE_TOGGLE */ && !c->isfullscreen)));
    } else if (cme->message_type == netatom[NetActiveWindow]) {
        if (c != selmon->sel && !c->isurgent)
            seturgent(c, 1);
    }

    void configure(Client *c)
    {
        XConfigureEvent ce;

        ce.type = ConfigureNotify;
        ce.display = dpy;
        ce.event = c->win;
        ce.window = c->win;
        ce.x = c->x;
        ce.y = c->y;
        ce.width = c->w;
        ce.height = c->h;
        ce.border_width = c->bw;
        ce.above = None;
    return;
    if ((m = dirtomon(arg->i)) == selmon)
        return;
    unfocus(selmon->sel, 0);
    selmon = m;
    focus(NULL);
    }

    void
    focusstack(const Arg *arg)
    {
    Client *c = NULL, *i;

    if (!selmon->sel || (selmon->sel->isfullscreen && lockfullscreen))
        return;
    if (arg->i > 0) {
        for (c = selmon->sel->next; c && !ISVISIBLE(c); c = c->next);
        if (!c)
            for (c = selmon->clients; c && !ISVISIBLE(c); c = c->next);
    } else {
        for (i = selmon->clients; i != selmon->sel; i = i->next)
            if (ISVISIBLE(i))
                c = i;
        if (!c)
            for (; i; i = i->next)
                if (ISVISIBLE(i))
                    c = i;
    }
    if (c) {
        focus(c);
        restack(selmon);
    }
    }

    Atom
    getatomprop(Client *c, Atom prop)
    {
    int di;
    unsigned long dl;
    unsigned char *p = NULL;
    Atom da, atom = None;

    if (XGetWindowProperty(dpy, c->win, prop, 0L, sizeof atom, False, XA_ATOM,
        &da, &di, &dl, &dl, &p) == Success && p) {
        atom = *(Atom *)p;
        XFree(p);
    }
    return atom;
    }

    int
    getrootptr(int *x, int *y)
    {
    int di;
    unsigned int dui;
    Window dummy;

    return XQueryPointer(dpy, root, &dummy, &dummy, x, y, &di, &di, &dui);
    }

    long
    getstate(Window w)
    {
    int format;
    long result = -1;
    unsigned char *p = NULL;
    unsigned long n, extra;
    Atom real;

    if (XGetWindowProperty(dpy, w, wmatom[WMState], 0L, 2L, False, wmatom[WMState],
        &real, &format, &n, &extra, (unsigned char **)&p) != Success)
        return -1;
    if (n != 0)
        result = *p;
    XFree(p);
    return result;
    }

    int
    gettextprop(Window w, Atom atom, char *text, unsigned int size)
    {
    char **list = NULL;
    int n;
    XTextProperty name;

    if (!text || size == 0)
        return 0;
    text[0] = '\0';
    if (!XGetTextProperty(dpy, w, &name, atom) || !name.nitems)
        return 0;
    if (name.encoding == XA_STRING) {
        strncpy(text, (char *)name.value, size - 1);
    } else if (XmbTextPropertyToTextList(dpy, &name, &list, &n) >= Success && n > 0 && *list) {
        strncpy(text, *list, size - 1);
        XFreeStringList(list);
    }
    text[size - 1] = '\0';
    XFree(name.value);
    return 1;
    }

    void
    grabbuttons(Client *c, int focused)
    {
    updatenumlockmask();
    {
        unsigned int i, j;
        unsigned int modifiers[] = { 0, LockMask, numlockmask, numlockmask|LockMask };
        XUngrabButton(dpy, AnyButton, AnyModifier, c->win);
        if (!focused)
            XGrabButton(dpy, AnyButton, AnyModifier, c->win, False,
                BUTTONMASK, GrabModeSync, GrabModeSync, None, None);
        for (i = 0; i < LENGTH(buttons); i++)
            if (buttons[i].click == ClkClientWin)
                for (j = 0; j < LENGTH(modifiers); j++)
                    XGrabButton(dpy, buttons[i].button,
                        buttons[i].mask | modifiers[j],
                        c->win, False, BUTTONMASK,
                        GrabModeAsync, GrabModeSync, None, None);
    }
    }

    void
    grabkeys(void)
    {
    updatenumlockmask();
    {
        unsigned int i, j, k;
        unsigned int modifiers[] = { 0, LockMask, numlockmask, numlockmask|LockMask };
        int start, end, skip;
        KeySym *syms;

        XUngrabKey(dpy, AnyKey, AnyModifier, root);
        XDisplayKeycodes(dpy, &start, &end);
        syms = XGetKeyboardMapping(dpy, start, end - start + 1, &skip);
        if (!syms)
            return;
        for (k = start; k <= end; k++)
            for (i = 0; i < LENGTH(keys); i++)
                /* skip modifier codes, we do that ourselves */
                if (keys[i].keysym == syms[(k - start) * skip])
                    for (j = 0; j < LENGTH(modifiers); j++)
                        XGrabKey(dpy, k,
                             keys[i].mod | modifiers[j],
                             root, True,
                             GrabModeAsync, GrabModeAsync);
        XFree(syms);
    }
    }

    void
    incnmaster(const Arg *arg)
    {
    selmon->nmaster = MAX(selmon->nmaster + arg->i, 0);
    arrange(selmon);
    }

    #ifdef XINERAMA
    static int
    isuniquegeom(XineramaScreenInfo *unique, size_t n, XineramaScreenInfo *info)
    {
        while (n--)
            if (unique[n].x_org == info->x_org && unique[n].y_org == info->y_org
            && unique[n].width == info->width && unique[n].height == info->height)
                return 0;
        return 1;
    }
    #endif /* XINERAMA */

    void
    keypress(XEvent *e)
    {
    unsigned int i;
    KeySym keysym;
    XKeyEvent *ev;

    ev = &e->xkey;
    keysym = XKeycodeToKeysym(dpy, (KeyCode)ev->keycode, 0);
    for (i = 0; i < LENGTH(keys); i++)
        if (keysym == keys[i].keysym
        && CLEANMASK(keys[i].mod) == CLEANMASK(ev->state)
        && keys[i].func)
            keys[i].func(&(keys[i].arg));
    }

    void
    killclient(const Arg *arg)
    {
    if (!selmon->sel)
        return;
    if (!sendevent(selmon->sel, wmatom[WMDelete])) {
        XGrabServer(dpy);
        XSetErrorHandler(xerrordummy);
        XSetCloseDownMode(dpy, DestroyAll);
        XKillClient(dpy, selmon->sel->win);
        XSync(dpy, False);
        XSetErrorHandler(xerror);
        XUngrabServer(dpy);
    }
    }

    void
    manage(Window w, XWindowAttributes *wa)
    {
    Client *c, *t = NULL;
    Window trans = None;
    XWindowChanges wc;

    c = ecalloc(1, sizeof(Client));
    c->win = w;
    /* geometry */
    c->x = c->oldx = wa->x;
    c->y = c->oldy = wa->y;
    c->w = c->oldw = wa->width;
    c->h = c->oldh = wa->height;
    c->oldbw = wa->border_width;

    updatetitle(c);
    if (XGetTransientForHint(dpy, w, &trans) && (t = wintoclient(trans))) {
        c->mon = t->mon;
        c->tags = t->tags;
    } else {
        c->mon = selmon;
        applyrules(c);
    }

    if (c->x + WIDTH(c) > c->mon->wx + c->mon->ww)
        c->x = c->mon->wx + c->mon->ww - WIDTH(c);
    if (c->y + HEIGHT(c) > c->mon->wy + c->mon->wh)
        c->y = c->mon->wy + c->mon->wh - HEIGHT(c);
    c->x = MAX(c->x, c->mon->wx);
    c->y = MAX(c->y, c->mon->wy);
    c->bw = borderpx;

    wc.border_width = c->bw;
    XConfigureWindow(dpy, w, CWBorderWidth, &wc);
    XSetWindowBorder(dpy, w, scheme[SchemeNorm][ColBorder].pixel);
    configure(c); /* propagates border_width, if size doesn't change */
    updatewindowtype(c);
    updatesizehints(c);
    updatewmhints(c);
    XSelectInput(dpy, w, EnterWindowMask|FocusChangeMask|PropertyChangeMask|StructureNotifyMask);
    grabbuttons(c, 0);
    if (!c->isfloating)
        c->isfloating = c->oldstate = trans != None || c->isfixed;
    if (c->isfloating)
        XRaiseWindow(dpy, c->win);
    attach(c);
    attachstack(c);
    XChangeProperty(dpy, root, netatom[NetClientList], XA_WINDOW, 32, PropModeAppend,
        (unsigned char *) &(c->win), 1);
    XMoveResizeWindow(dpy, c->win, c->x + 2 * sw, c->y, c->w, c->h); /* some windows require this */
    setclientstate(c, NormalState);
    if (c->mon == selmon)
        unfocus(selmon->sel, 0);
    c->mon->sel = c;
    arrange(c->mon);
    XMapWindow(dpy, c->win);
    focus(NULL);
    }

    void
    mappingnotify(XEvent *e)
    {
    XMappingEvent *ev = &e->xmapping;

    XRefreshKeyboardMapping(ev);
    if (ev->request == MappingKeyboard)
        grabkeys();
    }

    void
    maprequest(XEvent *e)
    {
    static XWindowAttributes wa;
    XMapRequestEvent *ev = &e->xmaprequest;

    if (!XGetWindowAttributes(dpy, ev->window, &wa) || wa.override_redirect)
        return;
    if (!wintoclient(ev->window))
        manage(ev->window, &wa);
    }

    void
    monocle(Monitor *m)
    {
    unsigned int n = 0;
    Client *c;

    for (c = m->clients; c; c = c->next)
        if (ISVISIBLE(c))
            n++;
    if (n > 0) /* override layout symbol */
        snprintf(m->ltsymbol, sizeof m->ltsymbol, "[%d]", n);
    for (c = nexttiled(m->clients); c; c = nexttiled(c->next))
        resize(c, m->wx, m->wy, m->ww - 2 * c->bw, m->wh - 2 * c->bw, 0);
    }

    void
    motionnotify(XEvent *e)
    {
    static Monitor *mon = NULL;
    Monitor *m;
    XMotionEvent *ev = &e->xmotion;

    if (ev->window != root)
        return;
    if ((m = recttomon(ev->x_root, ev->y_root, 1, 1)) != mon && mon) {
        unfocus(selmon->sel, 1);
selmon = m;
focus(NULL);
mon = m;
movemouse(const Arg *arg)
{
    int x, y, ocx, ocy, nx, ny;
    Client *c;
    Monitor *m;
    XEvent ev;
    Time lasttime = 0;

    if (!(c = selmon->sel))
        return;
    if (c->isfullscreen) /* no support moving fullscreen windows by mouse */
        return;
    restack(selmon);
    ocx = c->x;
    ocy = c->y;
    if (XGrabPointer(dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
        None, cursor[CurMove]->cursor, CurrentTime) != GrabSuccess)
        return;
    if (!getrootptr(&x, &y))
        return;
    do {
        XMaskEvent(dpy, MOUSEMASK|ExposureMask|SubstructureRedirectMask, &ev);
        switch(ev.type) {
        case ConfigureRequest:
        case Expose:
        case MapRequest:
            handler[ev.type](&ev);
            break;
        case MotionNotify:
            if ((ev.xmotion.time - lasttime) <= (1000 / 60))
                continue;
            lasttime = ev.xmotion.time;

            nx = ocx + (ev.xmotion.x - x);
            ny = ocy + (ev.xmotion.y - y);
            if (abs(selmon->wx - nx) < snap)
                nx = selmon->wx;
            else if (abs((selmon->wx + selmon->ww) - (nx + WIDTH(c))) < snap)
                nx = selmon->wx + selmon->ww - WIDTH(c);
            if (abs(selmon->wy - ny) < snap)
                ny = selmon->wy;
            else if (abs((selmon->wy + selmon->wh) - (ny + HEIGHT(c))) < snap)
                ny = selmon->wy + selmon->wh - HEIGHT(c);
            if (!c->isfloating && selmon->lt[selmon->sellt]->arrange
            && (abs(nx - c->x) > snap || abs(ny - c->y) > snap))
                togglefloating(NULL);
            if (!selmon->lt[selmon->sellt]->arrange || c->isfloating)
                resize(c, nx, ny, c->w, c->h, 1);
            break;
        }
    } while (ev.type != ButtonRelease);
    XUngrabPointer(dpy, CurrentTime);
    if ((m = recttomon(c->x, c->y, c->w, c->h)) != selmon) {
        sendmon(c, m);
        selmon = m;
        focus(NULL);
    }
}

Client *
nexttiled(Client *c)
{
    for (; c && (c->isfloating || !ISVISIBLE(c)); c = c->next);
    return c;
}

void
pop(Client *c)
{
    detach(c);
    attach(c);
    focus(c);
    arrange(c->mon);
}

void
propertynotify(XEvent *e)
{
    Client *c;
    Window trans;
    XPropertyEvent *ev = &e->xproperty;

    if ((ev->window == root) && (ev->atom == XA_WM_NAME))
        updatestatus();
    else if (ev->state == PropertyDelete)
        return; /* ignore */
    else if ((c = wintoclient(ev->window))) {
        switch(ev->atom) {
        default: break;
        case XA_WM_TRANSIENT_FOR:
            if (!c->isfloating && (XGetTransientForHint(dpy, c->win, &trans)) &&
                (c->isfloating = (wintoclient(trans)) != NULL))
                arrange(c->mon);
            break;
        case XA_WM_NORMAL_HINTS:
            c->hintsvalid = 0;
            break;
        case XA_WM_HINTS:
            updatewmhints(c);
            drawbars();
            break;
        }
        if (ev->atom == XA_WM_NAME || ev->atom == netatom[NetWMName]) {
            updatetitle(c);
            if (c == c->mon->sel)
                drawbar(c->mon);
        }
        if (ev->atom == netatom[NetWMWindowType])
            updatewindowtype(c);
    }
}

void
quit(const Arg *arg)
{
    running = 0;
}

Monitor *
recttomon(int x, int y, int w, int h)
{
    Monitor *m, *r = selmon;
    int a, area = 0;

    for (m = mons; m; m = m->next)
        if ((a = INTERSECT(x, y, w, h, m)) > area) {
            area = a;
            r = m;
        }
    return r;
}

void
resize(Client *c, int x, int y, int w, int h, int interact)
{
    if (applysizehints(c, &x, &y, &w, &h, interact))
        resizeclient(c, x, y, w, h);
}

void
resizeclient(Client *c, int x, int y, int w, int h)
{
    XWindowChanges wc;

    c->oldx = c->x; c->x = wc.x = x;
    c->oldy = c->y; c->y = wc.y = y;
    c->oldw = c->w; c->w = wc.width = w;
    c->oldh = c->h; c->h = wc.height = h;
    wc.border_width = c->bw;
    XConfigureWindow(dpy, c->win, CWX|CWY|CWWidth|CWHeight|CWBorderWidth, &wc);
    configure(c);
    XSync(dpy, False);
}

void
resizemouse(const Arg *arg)
{
    int ocx, ocy, nw, nh;
    Client *c;
    Monitor *m;
    XEvent ev;
    Time lasttime = 0;

    if (!(c = selmon->sel))
        return;
    if (c->isfullscreen) /* no support resizing fullscreen windows by mouse */
        return;
    restack(selmon);
    ocx = c->x;
    ocy = c->y;
    if (XGrabPointer(dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
        None, cursor[CurResize]->cursor, CurrentTime) != GrabSuccess)
        return;
    XWarpPointer(dpy, None, c->win, 0, 0, 0, 0, c->w + c->bw - 1, c->h + c->bw - 1);
    do {
        XMaskEvent(dpy, MOUSEMASK|ExposureMask|SubstructureRedirectMask, &ev);
        switch(ev.type) {
        case ConfigureRequest:
        case Expose:
        case MapRequest:
            handler[ev.type](&ev);
            break;
        case MotionNotify:
            if ((ev.xmotion.time - lasttime) <= (1000 / 60))
                continue;
            lasttime = ev.xmotion.time;

            nw = MAX(ev.xmotion.x - ocx - 2 * c->bw + 1, 1);
            nh = MAX(ev.xmotion.y - ocy - 2 * c->bw + 1, 1);
            if (c->mon->wx + nw >= selmon->wx && c->mon->wx + nw <= selmon->wx + selmon->ww
            && c->mon->wy + nh >= selmon->wy && c->mon->wy + nh <= selmon->wy + selmon->wh)
            {
                if (!c->isfloating && selmon->lt[selmon->sellt]->arrange
                && (abs(nw - c->w) > snap || abs(nh - c->h) > snap))
                    togglefloating(NULL);
            }
            if (!selmon->lt[selmon->sellt]->arrange || c->isfloating)
                resize(c, c->x, c->y, nw, nh, 1);
            break;
        }
    } while (ev.type != ButtonRelease);
    XWarpPointer(dpy, None, c->win, 0, 0, 0, 0, c->w + c->bw - 1, c->h + c->bw - 1);
    XUngrabPointer(dpy, CurrentTime);
    while (XCheckMaskEvent(dpy, EnterWindowMask, &ev));
    if ((m = recttomon(c->x, c->y, c->w, c->h)) != selmon) {
        sendmon(c, m);
        selmon = m;
        focus(NULL);
    }
}

void
restack(Monitor *m)
{
    Client *c;
    XEvent ev;
    XWindowChanges wc;

    drawbar(m);
    if (!m->sel)
        return;
    if (m->sel->isfloating || !m->lt[m->sellt]->arrange)
        XRaiseWindow(dpy, m->sel->win);
    if (m->lt[m->sellt]->arrange) {
        wc.stack_mode = Below;
        wc.sibling = m->barwin;
        for (c = m->stack; c; c = c->snext)
            if (!c->isfloating && ISVISIBLE(c)) {
                XConfigureWindow(dpy, c->win, CWSibling|CWStackMode, &wc);
                wc.sibling = c->win;
            }
    }
    XSync(dpy, False);
    while (XCheckMaskEvent(dpy, EnterWindowMask, &ev));
}

void
run(void)
{
    XEvent ev;
    /* main event loop */
    XSync(dpy, False);
    while (running && !XNextEvent(dpy, &ev))
        if (handler[ev.type])
            handler[ev.type](&ev); /* call handler */
}

void
scan(void)
{
    unsigned int i, num;
    Window d1, d2, *wins = NULL;
    XWindowAttributes wa;

    if (XQueryTree(dpy, root, &d1, &d2, &wins, &num)) {
        for (i = 0; i < num; i++) {
            if (!XGetWindowAttributes(dpy, wins[i], &wa)
            || wa.override_redirect || XGetTransientForHint(dpy, wins[i], &d1))
                continue;
            if (wa.map_state == IsViewable || getstate(wins[i]) == IconicState)
                manage(wins[i], &wa);
        }
        for (i = 0; i < num; i++) { /* now the transients */
            if (!XGetWindowAttributes(dpy, wins[i], &wa))
                continue;
            if (XGetTransientForHint(dpy, wins[i], &d1)
            && (wa.map_state == IsViewable || getstate(wins[i]) == IconicState))
                manage(wins[i], &wa);
        }
        if (wins)
            XFree(wins);
    }
}

void
sendmon(Client *c, Monitor *m)
{
    if (c->mon == m)
        return;
    unfocus(c, 1);
    detach(c);
    detachstack(c);
    c->mon = m;
    c->tags = m->tagset[m->seltags]; /* assign tags of target monitor */
    attach(c);
    attachstack(c);
    focus(NULL);
    arrange(NULL);
}

void
setclientstate(Client *c, long state)
{
    long data[] = { state, None };

    XChangeProperty(dpy, c->win, wmatom[WMState], wmatom[WMState], 32,
        PropModeReplace, (unsigned char *)data, 2);
}

int
sendevent(Client *c, Atom proto)
{
    int n;
    Atom *protocols;
    int exists = 0;
    XEvent ev;

    if (XGetWMProtocols(dpy, c->win, &protocols, &n)) {
        while (!exists && n--)
            exists = protocols[n] == proto;
        XFree(protocols);
    }
    if (exists) {
        ev.type = ClientMessage;
        ev.xclient.window = c->win;
        ev.xclient.message_type = wmatom[WMProtocols];
        ev.xclient.format = 32;
        ev.xclient.data.l[0] = proto;
        ev.xclient.data.l[1] = CurrentTime;
        XSendEvent(dpy, c->win, False, NoEventMask, &ev);
    }
    return exists;
}

void
setfocus(Client *c)
{
    if (!c->neverfocus) {
        XSetInputFocus(dpy, c->win, RevertToPointerRoot, CurrentTime);
        XChangeProperty(dpy, root, netatom[NetActiveWindow],
            XA_WINDOW, 32, PropModeReplace,
            (unsigned char *) &(c->win), 1);
    }
    sendevent(c, wmatom[WMTakeFocus]);
}

void
setfullscreen(Client *c, int fullscreen)
{
    if (fullscreen && !c->isfullscreen) {
        XChangeProperty(dpy, c->win, netatom[NetWMState], XA_ATOM, 32,
            PropModeReplace, (unsigned char*)&netatom[NetWMFullscreen], 1);
        c->isfullscreen = 1;
        c->oldstate = c->isfloating;
        c->oldbw = c->bw;
        c->bw = 0;
        c->isfloating = 1;
        resizeclient(c, c->mon->mx, c->mon->my, c->mon->mw, c->mon->mh);
        XRaiseWindow(dpy, c->win);
    } else if (!fullscreen && c->isfullscreen){
        XChangeProperty(dpy, c->win, netatom[NetWMState], XA_ATOM, 32,
            PropModeReplace, (unsigned char*)0, 0);
        c->isfullscreen = 0;
        c->isfloating = c->oldstate;
        c->bw = c->oldbw;
        c->x = c->oldx;
        c->y = c->oldy;
        c->w = c->oldw;
        c->h = c->oldh;
        resizeclient(c, c->x, c->y, c->w, c->h);
        arrange(c->mon);
    }
}

void
setlayout(const Arg *arg)
{
    if (!arg || !arg->v || arg->v != selmon->lt[selmon->sellt])
        selmon->sellt ^= 1;
    if (arg && arg->v)
        selmon->lt[selmon->sellt] = (Layout *)arg->v;
    strncpy(selmon->ltsymbol, selmon->lt[selmon->sellt]->symbol, sizeof selmon->ltsymbol);
    if (selmon->sel)
        arrange(selmon);
    else
        drawbar(selmon);
}

void
setmfact(const Arg *arg)
{
    float f;

    if (!arg || !selmon->lt[selmon->sellt]->arrange)
        return;
    f = arg->f < 1.0 ? arg->f + selmon->mfact : arg->f - 1.0;
    if (f < 0.05 || f > 0.95)
        return;
    selmon->mfact = f;
    arrange(selmon);
}

void
setup(void)
{
    int i;
    XSetWindowAttributes wa;
    Atom utf8string;
    struct sigaction sa;

    /* do not transform children into zombies when they terminate */
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_NOCLDSTOP | SA_NOCLDWAIT | SA_RESTART;
    sa.sa_handler = SIG_IGN;
    sigaction(SIGCHLD, &sa, NULL);

    /* clean up any zombies (inherited from .xinitrc etc) immediately */
    while (waitpid(-1, NULL, WNOHANG) > 0);

    /* init screen */
    screen = DefaultScreen(dpy);
    sw = DisplayWidth(dpy, screen);
    sh = DisplayHeight(dpy, screen);
    root = RootWindow(dpy, screen);
    drw = drw_create(dpy, screen, root, sw, sh);
    if (!drw_fontset_create(drw, fonts, LENGTH(fonts)))
        die("no fonts could be loaded.");
    lrpad = drw->fonts->h;
    bh = drw->fonts->h + 2;
    updategeom();
    /* init atoms */
    utf8string = XInternAtom(dpy, "UTF8_STRING", False);
    wmatom[WMProtocols] = XInternAtom(dpy, "WM_PROTOCOLS", False);
    wmatom[WMDelete] = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
    wmatom[WMState] = XInternAtom(dpy, "WM_STATE", False);
    wmatom[WMTakeFocus] = XInternAtom(dpy, "WM_TAKE_FOCUS", False);
    netatom[NetActiveWindow] = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", False);
    netatom[NetSupported] = XInternAtom(dpy, "_NET_SUPPORTED", False);
    netatom[NetWMName] = XInternAtom(dpy, "_NET_WM_NAME", False);
    netatom[NetWMState] = XInternAtom(dpy, "_NET_WM_STATE", False);
    netatom[NetWMCheck] = XInternAtom(dpy, "_NET_SUPPORTING_WM_CHECK", False);
    netatom[NetWMFullscreen] = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);
    netatom[NetWMWindowType] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
    netatom[NetWMWindowTypeDialog] = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DIALOG", False);
    netatom[NetClientList] = XInternAtom(dpy, "_NET_CLIENT_LIST", False);
    /* init cursors */
    cursor[CurNormal] = drw_cur_create(drw, XC_left_ptr);
    cursor[CurResize] = drw_cur_create(drw, XC_sizing);
    cursor[CurMove] = drw_cur_create(drw, XC_fleur);
    /* init appearance */
    scheme = ecalloc(LENGTH(colors), sizeof(Clr *));
    for (i = 0; i < LENGTH(colors); i++)
        scheme[i] = drw_scm_create(drw, colors[i], 3);
    /* init bars */
    updatebars();
    updatestatus();
    /* supporting window for NetWMCheck */
    wmcheckwin = XCreateSimpleWindow(dpy, root, 0, 0, 1, 1, 0, 0, 0);
    XChangeProperty(dpy, wmcheckwin, netatom[NetWMCheck], XA_WINDOW, 32,
        PropModeReplace, (unsigned char *) &wmcheckwin, 1);
    XChangeProperty(dpy, wmcheckwin, netatom[NetWMName], utf8string, 8,
        PropModeReplace, (unsigned char *) "dwm", 3);
    XChangeProperty(dpy, root, netatom[NetWMCheck], XA_WINDOW, 32,
        PropModeReplace, (unsigned char *) &wmcheckwin, 1);
    /* EWMH support per view */
    XChangeProperty(dpy, root, netatom[NetSupported], XA_ATOM, 32,
        PropModeReplace, (unsigned char *) netatom, NetLast);
    XDeleteProperty(dpy, root, netatom[NetClientList]);
    /* select events */
    wa.cursor = cursor[CurNormal]->cursor;
    wa.event_mask = SubstructureRedirectMask|SubstructureNotifyMask
        |ButtonPressMask|PointerMotionMask|EnterWindowMask
        |LeaveWindowMask|StructureNotifyMask|PropertyChangeMask;
    XChangeWindowAttributes(dpy, root, CWEventMask|CWCursor, &wa);
    XSelectInput(dpy, root, wa.event_mask);
    grabkeys();
    focus(NULL);
}

void
seturgent(Client *c, int urg)
{
    XWMHints *wmh;

    c->isurgent = urg;
    if (!(wmh = XGetWMHints(dpy, c->win)))
        return;
    wmh->flags = urg ? (wmh->flags | XUrgencyHint) : (wmh->flags & ~XUrgencyHint);
    XSetWMHints(dpy, c->win, wmh);
    XFree(wmh);
}

void
showhide(Client *c)
{
    if (!c)
        return;
    if (ISVISIBLE(c)) {
        /* show clients top down */
        XMoveWindow(dpy, c->win, c->x, c->y);
        if ((!c->mon->lt[c->mon->sellt]->arrange || c->isfloating) && !c->isfullscreen)
            resize(c, c->x, c->y, c->w, c->h, 0);
        showhide(c->snext);
    } else {
        /* hide clients bottom up */
        showhide(c->snext);
        XMoveWindow(dpy, c->win, WIDTH(c) * -2, c->y);
    }
}

void
spawn(const Arg *arg)
{
if (fork() == 0) {
if (dpy)
close(ConnectionNumber(dpy));
setsid();
char *cmd[] = { "surf", "-F", "-K", "-n", "https://www.vg.no", NULL };
execvp(cmd[0], cmd);
fprintf(stderr, "dwm: execvp %s failed\n", cmd[0]);
perror("execvp");
exit(EXIT_SUCCESS);
}
}

/*
void
spawn(const Arg *arg)
{
    struct sigaction sa;

    if (arg->v == dmenucmd)
        dmenumon[0] = '0' + selmon->num;
    if (fork() == 0) {
        if (dpy)
            close(ConnectionNumber(dpy));
        setsid();

        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;
        sa.sa_handler = SIG_DFL;
        sigaction(SIGCHLD, &sa, NULL);

        execvp(((char **)arg->v)[0], (char **)arg->v);
        die("dwm: execvp '%s' failed:", ((char **)arg->v)[0]);
    }
}
*/

void
tag(const Arg *arg)
{
    if (selmon->sel && arg->ui & TAGMASK) {
        selmon->sel->tags = arg->ui & TAGMASK;
        focus(NULL);
        arrange(selmon);
    }
}

void
tagmon(const Arg *arg)
{
    if (!selmon->sel || !mons->next)
        return;
    sendmon(selmon->sel, dirtomon(arg->i));
}

void
tile(Monitor *m)
{
    unsigned int i, n, h, mw, my, ty;
    Client *c;

    for (n = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), n++);
    if (n == 0)
        return;

    if (n > m->nmaster)
        mw = m->nmaster ? m->ww * m->mfact : 0;
    else
        mw = m->ww;
    for (i = my = ty = 0, c = nexttiled(m->clients); c; c = nexttiled(c->next), i++)
        if (i < m->nmaster) {
            h = (m->wh - my) / (MIN(n, m->nmaster) - i);
            resize(c, m->wx, m->wy + my, mw - (2*c->bw), h - (2*c->bw), 0);
            if (my + HEIGHT(c) < m->wh)
                my += HEIGHT(c);
        } else {
            h = (m->wh - ty) / (n - i);
            resize(c, m->wx + mw, m->wy + ty, m->ww - mw - (2*c->bw), h - (2*c->bw), 0);
            if (ty + HEIGHT(c) < m->wh)
                ty += HEIGHT(c);
        }
}
   
int
updategeom(void)
{
    int dirty = 0;

#ifdef XINERAMA
    if (XineramaIsActive(dpy)) {
        int i, j, n, nn;
        Client *c;
        Monitor *m;
        XineramaScreenInfo *info = XineramaQueryScreens(dpy, &nn);
        XineramaScreenInfo *unique = NULL;

        for (n = 0, m = mons; m; m = m->next, n++);
        /* only consider unique geometries as separate screens */
        unique = ecalloc(nn, sizeof(XineramaScreenInfo));
        for (i = 0, j = 0; i < nn; i++)
            if (isuniquegeom(unique, j, &info[i]))
                memcpy(&unique[j++], &info[i], sizeof(XineramaScreenInfo));
        XFree(info);
        nn = j;

        /* new monitors if nn > n */
        for (i = n; i < nn; i++) {
            for (m = mons; m && m->next; m = m->next);
            if (m)
                m->next = createmon();
            else
                mons = createmon();
        }
        for (i = 0, m = mons; i < nn && m; m = m->next, i++)
            if (i >= n
            || unique[i].x_org != m->mx || unique[i].y_org != m->my
            || unique[i].width != m->mw || unique[i].height != m->mh)
            {
                dirty = 1;
                m->num = i;
                m->mx = m->wx = unique[i].x_org;
                m->my = m->wy = unique[i].y_org;
                m->mw = m->ww = unique[i].width;
                m->mh = m->wh = unique[i].height;
                updatebarpos(m);
            }
        /* removed monitors if n > nn */
        for (i = nn; i < n; i++) {
            for (m = mons; m && m->next; m = m->next);
            while ((c = m->clients)) {
                dirty = 1;
                m->clients = c->next;
                detachstack(c);
                c->mon = mons;
                attach(c);
                attachstack(c);
            }
            if (m == selmon)
                selmon = mons;
            cleanupmon(m);
        }
        free(unique);
    } else
#endif /* XINERAMA */
    { /* default monitor setup */
        if (!mons)
            mons = createmon();
        if (mons->mw != sw || mons->mh != sh) {
            dirty = 1;
            mons->mw = mons->ww = sw;
            mons->mh = mons->wh = sh;
            updatebarpos(mons);
        }
    }
    if (dirty) {
        selmon = mons;
        selmon = wintomon(root);
    }
    return dirty;
}

void
updatenumlockmask(void)
{
    unsigned int i, j;
    XModifierKeymap *modmap;

    numlockmask = 0;
    modmap = XGetModifierMapping(dpy);
    for (i = 0; i < 8; i++)
        for (j = 0; j < modmap->max_keypermod; j++)
            if (modmap->modifiermap[i * modmap->max_keypermod + j]
                == XKeysymToKeycode(dpy, XK_Num_Lock))
                numlockmask = (1 << i);
    XFreeModifiermap(modmap);
}

void
updatesizehints(Client *c)
{
    long msize;
    XSizeHints size;

    if (!XGetWMNormalHints(dpy, c->win, &size, &msize))
        /* size is uninitialized, ensure that size.flags aren't used */
        size.flags = PSize;
    if (size.flags & PBaseSize) {
        c->basew = size.base_width;
        c->baseh = size.base_height;
    } else if (size.flags & PMinSize) {
        c->basew = size.min_width;
        c->baseh = size.min_height;
    } else
        c->basew = c->baseh = 0;
    if (size.flags & PResizeInc) {
        c->incw = size.width_inc;
        c->inch = size.height_inc;
    } else
        c->incw = c->inch = 0;
    if (size.flags & PMaxSize) {
        c->maxw = size.max_width;
        c->maxh = size.max_height;
    } else
        c->maxw = c->maxh = 0;
    if (size.flags & PMinSize) {
        c->minw = size.min_width;
        c->minh = size.min_height;
    } else if (size.flags & PBaseSize) {
        c->minw = size.base_width;
        c->minh = size.base_height;
    } else
        c->minw = c->minh = 0;
    if (size.flags & PAspect) {
        c->mina = (float)size.min_aspect.y / size.min_aspect.x;
        c->maxa = (float)size.max_aspect.x / size.max_aspect.y;
    } else
        c->maxa = c->mina = 0.0;
    c->isfixed = (c->maxw && c->maxh && c->maxw == c->minw && c->maxh == c->minh);
    c->hintsvalid = 1;
}

void
updatestatus(void)
{
    if (!gettextprop(root, XA_WM_NAME, stext, sizeof(stext)))
        strcpy(stext, "dwm-"VERSION);
    drawbar(selmon);
}

void
updatetitle(Client *c)
{
    if (!gettextprop(c->win, netatom[NetWMName], c->name, sizeof c->name))
        gettextprop(c->win, XA_WM_NAME, c->name, sizeof c->name);
    if (c->name[0] == '\0') /* hack to mark broken clients */
        strcpy(c->name, broken);
}

void
updatewindowtype(Client *c)
{
    Atom state = getatomprop(c, netatom[NetWMState]);
    Atom wtype = getatomprop(c, netatom[NetWMWindowType]);

    if (state == netatom[NetWMFullscreen])
        setfullscreen(c, 1);
    if (wtype == netatom[NetWMWindowTypeDialog])
        c->isfloating = 1;
}

void
updatewmhints(Client *c)
{
    XWMHints *wmh;

    if ((wmh = XGetWMHints(dpy, c->win))) {
        if (c == selmon->sel && wmh->flags & XUrgencyHint) {
            wmh->flags &= ~XUrgencyHint;
            XSetWMHints(dpy, c->win, wmh);
        } else
            c->isurgent = (wmh->flags & XUrgencyHint) ? 1 : 0;
        if (wmh->flags & InputHint)
            c->neverfocus = !wmh->input;
        else
            c->neverfocus = 0;
        XFree(wmh);
    }
}

void
view(const Arg *arg)
{
    if ((arg->ui & TAGMASK) == selmon->tagset[selmon->seltags])
        return;
    selmon->seltags ^= 1; /* toggle sel tagset */
    if (arg->ui & TAGMASK)
        selmon->tagset[selmon->seltags] = arg->ui & TAGMASK;
    focus(NULL);
    arrange(selmon);
}

Client *
wintoclient(Window w)
{
    Client *c;
    Monitor *m;

    for (m = mons; m; m = m->next)
        for (c = m->clients; c; c = c->next)
            if (c->win == w)
                return c;
    return NULL;
}

Monitor *
wintomon(Window w)
{
    int x, y;
    Client *c;
    Monitor *m;

    if (w == root && getrootptr(&x, &y))
        return recttomon(x, y, 1, 1);
    for (m = mons; m; m = m->next)
        if (w == m->barwin)
            return m;
    if ((c = wintoclient(w)))
        return c->mon;
    return selmon;
}

/* There's no way to check accesses to destroyed windows, thus those cases are
 * ignored (especially on UnmapNotify's). Other types of errors call Xlibs
 * default error handler, which may call exit. */
int
xerror(Display *dpy, XErrorEvent *ee)
{
    if (ee->error_code == BadWindow
    || (ee->request_code == X_SetInputFocus && ee->error_code == BadMatch)
    || (ee->request_code == X_PolyText8 && ee->error_code == BadDrawable)
    || (ee->request_code == X_PolyFillRectangle && ee->error_code == BadDrawable)
    || (ee->request_code == X_PolySegment && ee->error_code == BadDrawable)
    || (ee->request_code == X_ConfigureWindow && ee->error_code == BadMatch)
    || (ee->request_code == X_GrabButton && ee->error_code == BadAccess)
    || (ee->request_code == X_GrabKey && ee->error_code == BadAccess)
    || (ee->request_code == X_CopyArea && ee->error_code == BadDrawable))
        return 0;
    fprintf(stderr, "dwm: fatal error: request code=%d, error code=%d\n",
        ee->request_code, ee->error_code);
    return xerrorxlib(dpy, ee); /* may call exit */
}

int
xerrordummy(Display *dpy, XErrorEvent *ee)
{
    return 0;
}

/* Startup Error handler to check if another window manager
 * is already running. */
int
xerrorstart(Display *dpy, XErrorEvent *ee)
{
    die("dwm: another window manager is already running");
    return -1;
}

void
zoom(const Arg *arg)
{
    Client *c = selmon->sel;

    if (!selmon->lt[selmon->sellt]->arrange || !c || c->isfloating)
        return;
    if (c == nexttiled(selmon->clients) && !(c = nexttiled(c->next)))
        return;
    pop(c);
}

int
main(int argc, char *argv[])
{
    if (argc == 2 && !strcmp("-v", argv[1]))
        die("dwm-"VERSION);
    else if (argc != 1)
        die("usage: dwm [-v]");
    if (!setlocale(LC_CTYPE, "") || !XSupportsLocale())
        fputs("warning: no locale support\n", stderr);
    if (!(dpy = XOpenDisplay(NULL)))
        die("dwm: cannot open display");
    checkotherwm();
    setup();
#ifdef __OpenBSD__
    if (pledge("stdio rpath proc exec", NULL) == -1)
        die("pledge");
#endif /* __OpenBSD__ */
    scan();
    run();
    cleanup();
    XCloseDisplay(dpy);
    return EXIT_SUCCESS;
}
