implement app;

include "sys.m";
	sys: Sys;
include "draw.m";
	draw: Draw;
include "tk.m";
	tk: Tk;
include "tkclient.m";
	tkclient: Tkclient;

app: module {
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
	(t, titlech) := tkclient->toplevel(ctxt, "", "Center Widget Test", Tkclient->Appl);

	# Create widgets

	# Create Container widget
	tk->cmd(t, "frame .w1 -bg #191919");
		# Create Center widget
		tk->cmd(t, "frame .w1.w2");
			# Create Button widget
			tk->cmd(t, "button .w1.w2.w3 -text {Centered Button} -width 150 -height 50 -bg #404080 -fg white");
			tk->cmd(t, "pack .w1.w2.w3 -expand 1");
		tk->cmd(t, "pack .w1.w2 -fill both -expand 1");
	tk->cmd(t, "pack .w1 -fill both -expand 1");

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
