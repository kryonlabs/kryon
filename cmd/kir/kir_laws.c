/*
 * kir_laws.c - named .kry laws enforced after semantic checking.
 *
 * A law is an invariant over the parsed program that gates strict builds.
 * Diagnostics carry the stable dotted law name so tests and callers can
 * match them; lenient builds count violations silently, like kir_check.c.
 */
#include "kir_laws.h"
#include "kir_diagnostic.h"

#include <stdio.h>
#include <string.h>

typedef struct LawCheck {
	const char *current;   /* active law name */
	int strict;
	int violations;
} LawCheck;

static void
violation(LawCheck *c, KirSourceSpan span, const char *detail)
{
	c->violations++;
	if(!c->strict) return;
	KirDiagnostic(span, c->current, "law %s: %s", c->current, detail);
}

/* Same blocked set as tools/check-kryon-laws-api.sh: low-level texture
 * draws and internal UI helpers bypass the public widget surface. */
static int
blocked_surface_call(const char *name)
{
	return strcmp(name, "Texture") == 0
	    || strcmp(name, "DrawTexture") == 0
	    || strcmp(name, "DrawTexturePro") == 0
	    || strcmp(name, "DrawTextureRec") == 0
	    || strcmp(name, "TextInputControl") == 0
	    || strncmp(name, "UIText", 6) == 0
	    || strncmp(name, "UIRender", 8) == 0;
}

static void
check_image_surface(LawCheck *c, KirProgram **programs, int count)
{
	for(int p = 0; p < count; p++) {
		for(int m = 0; m < programs[p]->module_count; m++) {
			KirModule *module = &programs[p]->modules[m];
			for(int f = 0; f < module->function_count; f++) {
				KirFunction *fn = &module->functions[f];
				for(int x = 0; x < fn->expr_count; x++) {
					KirExpr *e = &fn->exprs[x];
					char detail[KIR_TEXT_MAX];
					if(e->kind != KIR_EXPR_CALL)
						continue;
					if(!blocked_surface_call(e->name))
						continue;
					snprintf(detail, sizeof(detail),
					         "blocked UI surface call '%s': use Image(ImageProps)",
					         e->name);
					violation(c, e->span, detail);
				}
			}
		}
	}
}

typedef void (*LawFn)(LawCheck *c, KirProgram **programs, int count);

static const struct { const char *name; LawFn check; } laws[] = {
	{"image.surface.no_low_level_calls", check_image_surface},
};

int
KirCheckLaws(KirProgram **programs, int count, int strict)
{
	LawCheck c = {0};
	c.strict = strict;
	for(size_t i = 0; i < sizeof(laws) / sizeof(laws[0]); i++) {
		c.current = laws[i].name;
		laws[i].check(&c, programs, count);
	}
	return !(c.strict && c.violations);
}
