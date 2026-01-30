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
	(t, titlech) := tkclient->toplevel(ctxt, "", "Dropdown Demo", Tkclient->Appl);

	# Create widgets

	# Create Container widget
	tk->cmd(t, "frame .w1 -width 400 -height 300 -bg #f8f9fa");
		# Create Column widget
		tk->cmd(t, "frame .w1.w2 -width 100 -height 100");
			# Create Text widget
			tk->cmd(t, "label .w1.w2.w3 -text {Select your favorite color:} -fg #6b7280 -font /fonts/lucidasans/latin1.7.font -padx 5 -pady 5");
			tk->cmd(t, "pack .w1.w2.w3 -expand 1");
			# Create Dropdown widget
			tk->cmd(t, "frame .w1.w2.w4 -width 200 -height 40 -bg white -fg #2d3748 -borderwidth 1 -relief raised");
			tk->cmd(t, "pack .w1.w2.w4 -padx 20 -pady 20");
			# Create Text widget
			tk->cmd(t, "label .w1.w2.w5 -text {Select your country:} -fg #6b7280 -font /fonts/lucidasans/latin1.7.font -padx 5 -pady 5");
			tk->cmd(t, "pack .w1.w2.w5 -expand 1");
			# Create Dropdown widget
			tk->cmd(t, "frame .w1.w2.w6 -width 200 -height 40 -bg white -fg #2d3748 -borderwidth 1 -relief raised");
			tk->cmd(t, "pack .w1.w2.w6 -padx 20 -pady 20");
		tk->cmd(t, "pack .w1.w2 -padx 20 -pady 20");
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
