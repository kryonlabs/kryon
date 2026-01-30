implement KryonApp;

include "sys.m";
	sys: Sys;
include "draw.m";
	draw: Draw;
include "tk.m";
	tk: Tk;
include "tkclient.m";
	tkclient: Tkclient;

KryonApp: module {
	init: fn(ctxt: ref Draw->Context, argv: list of string);
};

init(ctxt: ref Draw->Context, argv: list of string) {
	# Load required modules
	sys = load Sys Sys->PATH;
	draw = load Draw Draw->PATH;
	tk = load Tk Tk->PATH;
	tkclient = load Tkclient Tkclient->PATH;
	tkclient->init();

	# Create toplevel window
	(t, titlech) := tkclient->toplevel(ctxt, "", "Hello World Example", Tkclient->Appl);

	# Create widgets

	# Create root Container widget with dark background
	tk->cmd(t, "frame .w1 -bg #1a1a2e");
	tk->cmd(t, "pack .w1 -fill both -expand 1");

	# Create nested Container widget with midnight blue background
	tk->cmd(t, "frame .w1.w2 -width 200 -height 100 -bg #191970 -borderwidth 2 -relief raised");
	tk->cmd(t, "pack .w1.w2 -padx 20 -pady 20");

	# Create Text widget centered inside container
	tk->cmd(t, "label .w1.w2.w3 -text {Hello World} -fg yellow -bg #191970 -font /fonts/lucidasans/latin1.7.font");
	tk->cmd(t, "place .w1.w2.w3 -relx 0.5 -rely 0.5 -anchor center");

	# Update display
	tk->cmd(t, "update");

	tkclient->onscreen(t, nil);
	tkclient->startinput(t, "kbd" :: "ptr" :: nil);

	# Event loop
	for(;;) alt {
		s := <-t.ctxt.kbd =>
			tk->keyboard(t, s);
		s := <-t.ctxt.ptr =>
			tk->pointer(t, *s);
		s := <-t.ctxt.ctl or s = <-t.wreq or s = <-titlech =>
			tkclient->wmctl(t, s);
	}
}
