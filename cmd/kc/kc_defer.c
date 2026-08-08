/*
 * defer handling.
 *
 * `defer STMT` registers STMT to run when its enclosing block exits — by
 * falling off the end (the closing `}`), or via return/break/continue inside
 * the block. Multiple defers in one block run in reverse (LIFO) order. goto
 * is intentionally left alone: a goto out of a deferred scope will skip it.
 *
 * Body lines are stored as pre-translated C fragments. This pass walks them,
 * tracks the scope stack (matching write_body_line's brace accounting), and
 * rebuilds the body[] array with the deferred statements spliced in at every
 * exit point and removed from their declaration site.
 */
#include "kc_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* A pending defer: the scope index (into the scope stack) that owns it and the
 * statement text (a Kry fragment, retranslated at output like other lines).
 * `spent` marks a defer already emitted by an early exit (break/continue/
 * return) so a later scope boundary doesn't re-emit it. */
typedef struct KryDefer {
    int scope_index;
    int spent;
    char stmt[KC_BODY_LINE_MAX];
} KryDefer;

/* Scope kinds, to decide what break/continue unwind. */
enum {
    KRY_SCOPE_BLOCK,   /* plain { } or function body                  */
    KRY_SCOPE_BRANCH,  /* if / else if / else / switch                 */
    KRY_SCOPE_LOOP,    /* for / while — break/continue unwind to here  */
    KRY_SCOPE_CASE     /* a switch case body (implicit scope)          */
};

/* Classify a body line into a scope kind for the scope it OPENS, or -1 if it
 * does not open a scope. Looks at the leading keyword of the line. */
static int
scope_kind_for_open(const char *text)
{
    if(strncmp(text, "for", 3) == 0 &&
       (text[3] == '(' || text[3] == ' ' || text[3] == '\t'))
        return KRY_SCOPE_LOOP;
    if(strncmp(text, "while", 5) == 0 &&
       (text[5] == '(' || text[5] == ' ' || text[5] == '\t'))
        return KRY_SCOPE_LOOP;
    if(strncmp(text, "if", 2) == 0 &&
       (text[2] == '(' || text[2] == ' '))
        return KRY_SCOPE_BRANCH;
    if(strncmp(text, "else", 4) == 0 &&
       (text[4] == ' ' || text[4] == '{' || text[4] == '\0'))
        return KRY_SCOPE_BRANCH;
    if(strncmp(text, "switch", 6) == 0 &&
       (text[6] == '(' || text[6] == ' '))
        return KRY_SCOPE_BRANCH;
    if(strncmp(text, "case", 4) == 0 &&
       (text[4] == ' ' || text[4] == '\t'))
        return KRY_SCOPE_BRANCH;
    if(strcmp(text, "default:") == 0)
        return KRY_SCOPE_BRANCH;
    return KRY_SCOPE_BLOCK;   /* anonymous `{` block or anything else */
}

void
apply_defers(KryFunction *fn)
{
    if(fn == NULL || fn->body_count == 0)
        return;

    /* Quick check: any defer at all? Avoids allocation work otherwise. */
    int has_defer = 0;
    for(int i = 0; i < fn->body_count; i++) {
        const char *t = skip_indent(fn->body[i]);
        if(strncmp(t, "defer ", 6) == 0 || strcmp(t, "defer") == 0) {
            has_defer = 1;
            break;
        }
    }
    if(!has_defer)
        return;

    /* Working buffers, heap-allocated and sized to the body. There is no fixed
     * cap: scope_count and defer_count are each bounded by body_count, and the
     * rebuilt body holds at most each original line plus each defer emitted
     * once at a scope exit, so 2*body_count is a safe upper bound. */
    int cap = fn->body_count * 2 + 16;
    int *scope_kind = calloc((size_t)cap, sizeof(*scope_kind));
    KryDefer *defers = calloc((size_t)cap, sizeof(*defers));
    char **out_body = calloc((size_t)cap, sizeof(*out_body));
    if(scope_kind == NULL || defers == NULL || out_body == NULL)
        die("out of memory in apply_defers");
    for(int i = 0; i < cap; i++) {
        out_body[i] = calloc(KC_BODY_LINE_MAX, 1);
        if(out_body[i] == NULL)
            die("out of memory in apply_defers");
    }

    /* Scope stack: index 0 is the function body. */
    int scope_count = 0;
    scope_kind[scope_count++] = KRY_SCOPE_BLOCK;

    /* Pending defers, in registration order. */
    int defer_count = 0;

    int out_count = 0;

#define EMIT_BODY(text)                                            \
    do {                                                           \
        if(out_count >= cap) goto done;                            \
        snprintf(out_body[out_count], KC_BODY_LINE_MAX,            \
                 "%s", (text));                                    \
        out_count++;                                               \
    } while(0)

    /* Emit every pending defer owned by scope indices in (after, top], i.e.
     * unwind from the innermost scope down to (not including) `after`. Within
     * a scope, defers run in reverse registration order. Skips defers already
     * spent (emitted by an earlier break/continue/return). */
#define EMIT_UNWIND(after)                                                  \
    do {                                                                    \
        for(int s = scope_count - 1; s > (after); s--) {                    \
            for(int d = defer_count - 1; d >= 0; d--) {                     \
                if(defers[d].scope_index == s && !defers[d].spent) {        \
                    EMIT_BODY(defers[d].stmt);                              \
                }                                                           \
            }                                                               \
        }                                                                   \
    } while(0)

    /* Like EMIT_UNWIND, but marks the emitted defers as spent so a later
     * scope boundary (case label, close brace) doesn't re-emit them. Used by
     * early exits (break/continue/return). */
#define EMIT_UNWIND_AND_SPEND(after)                                        \
    do {                                                                    \
        for(int s = scope_count - 1; s > (after); s--) {                    \
            for(int d = defer_count - 1; d >= 0; d--) {                     \
                if(defers[d].scope_index == s && !defers[d].spent) {        \
                    EMIT_BODY(defers[d].stmt);                              \
                    defers[d].spent = 1;                                    \
                }                                                           \
            }                                                               \
        }                                                                   \
    } while(0)

    for(int i = 0; i < fn->body_count; i++) {
        const char *raw = fn->body[i];
        const char *text = skip_indent(raw);

        /* `defer STMT` — register and drop the line. */
        if(strncmp(text, "defer ", 6) == 0 || strcmp(text, "defer") == 0) {
            const char *stmt = text + 6;
            while(*stmt == ' ' || *stmt == '\t')
                stmt++;
            if(*stmt == '\0')
                continue;   /* malformed; parse_statement already validated */
            if(defer_count < cap) {
                defers[defer_count].scope_index = scope_count - 1;
                defers[defer_count].spent = 0;
                snprintf(defers[defer_count].stmt,
                         sizeof(defers[defer_count].stmt), "    %s;", stmt);
                defer_count++;
            }
            continue;
        }

        /* Closing brace: unwind THIS scope's defers, then pop the scope.
         * Lines like `} else {` / `} else if(...) {` also begin with `}` and
         * close the if-body scope before reopening. A loop body's `}` closes
         * the BLOCK scope and also the LOOP scope it sits inside, since the
         * loop header opened both with a single brace. */
        if(text[0] == '}') {
            EMIT_UNWIND(scope_count - 2);
            if(scope_count > 1)
                scope_count--;
            /* If the scope just closed was a loop body, also pop the loop. */
            if(scope_count >= 2 &&
               scope_kind[scope_count] == KRY_SCOPE_BLOCK &&
               scope_kind[scope_count - 1] == KRY_SCOPE_LOOP)
                scope_count--;
            /* If the scope just closed was a switch case, also pop the
             * switch (its `}` closes both the case and the switch). */
            if(scope_count >= 2 &&
               scope_kind[scope_count] == KRY_SCOPE_CASE &&
               scope_kind[scope_count - 1] == KRY_SCOPE_BRANCH)
                scope_count--;
            /* Drop defers that belonged to the closed scope(s). */
            {
                int kept = 0;
                for(int d = 0; d < defer_count; d++)
                    if(defers[d].scope_index < scope_count)
                        defers[kept++] = defers[d];
                defer_count = kept;
            }
            EMIT_BODY(raw);
            /* If the line reopens a scope (`} else {`, `} else if (...) {`),
             * push a fresh branch scope for the new block. */
            if(strchr(text, '{') != NULL && scope_count < cap)
                scope_kind[scope_count++] = KRY_SCOPE_BRANCH;
            continue;
        }

        /* case / default: a switch case is an implicit scope boundary.
         * The previous case's defers fire here (the fall-through path) and
         * are then dropped whether or not they were spent by an earlier
         * break/return. Then a fresh case scope is pushed for the new case
         * so its defers attach to it, not the switch. */
        if(strncmp(text, "case ", 5) == 0 || strcmp(text, "default:") == 0) {
            int case_scope = -1;
            if(scope_count > 0 &&
               scope_kind[scope_count - 1] == KRY_SCOPE_CASE)
                case_scope = scope_count - 1;
            if(case_scope >= 0) {
                /* Fire this case's unspent defers (fall-through), then drop
                 * all of this case's defers and pop the case scope. */
                for(int d = defer_count - 1; d >= 0; d--) {
                    if(defers[d].scope_index == case_scope && !defers[d].spent)
                        EMIT_BODY(defers[d].stmt);
                }
                {
                    int kept = 0;
                    for(int d = 0; d < defer_count; d++)
                        if(defers[d].scope_index != case_scope)
                            defers[kept++] = defers[d];
                    defer_count = kept;
                }
                scope_count--;
            }
            EMIT_BODY(raw);
            if(scope_count < cap)
                scope_kind[scope_count++] = KRY_SCOPE_CASE;
            continue;
        }

        /* return: unwind everything back to the function body. Returns must
         * fire all defers on every path (the function is exiting), so they
         * use the non-spending unwind — a defer can't be "used up" by one
         * return when a later return also needs it. EXCEPTION: defers in a
         * switch-case scope ARE spent, because the case boundary after the
         * return would otherwise re-emit them (a case is one-shot). */
        if(strncmp(text, "return", 6) == 0 &&
           (text[6] == ';' || text[6] == ' ' || text[6] == '\t')) {
            EMIT_UNWIND(-1);
            /* Spend case-scope defers so the following case boundary doesn't
             * re-emit them. */
            for(int s = scope_count - 1; s > 0; s--) {
                if(scope_kind[s] == KRY_SCOPE_CASE) {
                    for(int d = 0; d < defer_count; d++)
                        if(defers[d].scope_index == s)
                            defers[d].spent = 1;
                }
            }
            EMIT_BODY(raw);
            continue;
        }

        /* break / continue: unwind to the enclosing loop or switch case.
         * In a loop, the defer must fire every iteration (at the loop-body
         * close), so the non-spending unwind is correct. In a switch case,
         * the case boundary is a one-shot boundary, so break spends the
         * case's defers to prevent the case-label handler re-emitting them. */
        if(strcmp(text, "break;") == 0 || strcmp(text, "continue;") == 0) {
            int target = 0;
            int in_case = 0;
            for(int s = scope_count - 1; s > 0; s--) {
                if(scope_kind[s] == KRY_SCOPE_LOOP) {
                    target = s;
                    break;
                }
                if(scope_kind[s] == KRY_SCOPE_CASE) {
                    target = s - 1;
                    in_case = 1;
                    break;
                }
            }
            if(in_case)
                EMIT_UNWIND_AND_SPEND(target);
            else
                EMIT_UNWIND(target);
            EMIT_BODY(raw);
            continue;
        }

        /* Normal line. Emit it, then update the scope stack for any `{`. A
         * loop header (`for(...) {` / `while(...) {`) opens a LOOP scope for
         * the construct and a separate BLOCK scope for the body, so that
         * break/continue unwind only the body's defers, not the loop itself. */
        EMIT_BODY(raw);
        if(text[0] != '#' && brace_delta(text) > 0) {
            int kind = scope_kind_for_open(text);
            if(kind == KRY_SCOPE_LOOP) {
                if(scope_count < cap)
                    scope_kind[scope_count++] = KRY_SCOPE_LOOP;
                if(scope_count < cap)
                    scope_kind[scope_count++] = KRY_SCOPE_BLOCK;
            } else if(scope_count < cap) {
                scope_kind[scope_count++] = kind;
            }
        }
    }

done:
    /* Splice the rebuilt body back into the function. Defers registered at the
     * function-body scope (scope_index 0) are never matched by a `}` line in
     * body[] (the function's own close is synthetic), so flush them here for
     * the fall-through path. Skip this if the last emitted line already
     * returns: that path has already unwound, so a trailing emit would be
     * unreachable dead code. */
    if(out_count > 0) {
        const char *last = skip_indent(out_body[out_count - 1]);
        if(strncmp(last, "return", 6) != 0) {
            for(int d = 0; d < defer_count; d++)
                if(defers[d].scope_index < 1)   /* function-body defers */
                    EMIT_BODY(defers[d].stmt);
        }
    }

#undef EMIT_BODY
#undef EMIT_UNWIND

    /* Grow fn->body/body_line to hold the rebuilt body (defers can expand it
     * beyond the original line count), then splice the rebuilt lines back in. */
    if(out_count > fn->body_cap) {
        char **nb = realloc(fn->body, (size_t)out_count * sizeof(*nb));
        int *nl = realloc(fn->body_line,
                          (size_t)out_count * sizeof(*nl));
        if(nb == NULL || nl == NULL)
            die("out of memory splicing deferred body");
        for(int i = fn->body_cap; i < out_count; i++) {
            nb[i] = calloc(KC_BODY_LINE_MAX, 1);
            if(nb[i] == NULL)
                die("out of memory splicing deferred body");
        }
        fn->body = nb;
        fn->body_line = nl;
        fn->body_cap = out_count;
    }
    for(int i = 0; i < out_count; i++) {
        snprintf(fn->body[i], KC_BODY_LINE_MAX, "%s", out_body[i]);
        fn->body_line[i] = 0;
    }
    fn->body_count = out_count;

    for(int i = 0; i < cap; i++)
        free(out_body[i]);
    free(out_body);
    free(scope_kind);
    free(defers);
}
