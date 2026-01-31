implement Test;

include "sys.m";
	sys: Sys;

Test: module {
	init: fn(ctxt: ref Draw->Context, argv: list of string);
};

init(ctxt: ref Draw->Context, argv: list of string) {
	sys = load Sys Sys->PATH;
	sys->print("Hello, world\!");
}
