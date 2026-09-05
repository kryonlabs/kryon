/*
 * kry_xml_test.c - parser/emitter unit tests (hermetic, no network).
 */
#include "kry_xml.h"

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(cond) do { \
    if(!(cond)) { \
        failures++; \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
    } \
} while(0)

static void
test_basic_document(void)
{
    const char *src = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                      "<gnm:Workbook xmlns:gnm=\"http://www.gnumeric.org/v10.dtd\">"
                      "<gnm:Version Epoch=\"1\" Major=\"12\"/>"
                      "</gnm:Workbook>";
    KryXml *root = kry_xml_parse(src);
    KryXml *version;

    CHECK(root != NULL);
    CHECK(strcmp(kry_xml_name(root), "gnm:Workbook") == 0);
    CHECK(kry_xml_child(root, 1) == NULL);
    version = kry_xml_child(root, 0);
    CHECK(version != NULL);
    CHECK(strcmp(kry_xml_name(version), "gnm:Version") == 0);
    CHECK(kry_xml_attr(version, "Epoch") != NULL &&
          strcmp(kry_xml_attr(version, "Epoch"), "1") == 0);
    CHECK(kry_xml_attr(version, "Major") != NULL &&
          strcmp(kry_xml_attr(version, "Major"), "12") == 0);
    CHECK(kry_xml_attr(version, "Minor") == NULL);
    CHECK(kry_xml_attr_long(version, "Major", -1) == 12);
    CHECK(kry_xml_attr_long(version, "Missing", 7) == 7);
    kry_xml_free(root);
}

static void
test_gnumeric_cells(void)
{
    const char *src =
        "<gnm:Sheet><gnm:Name>Data</gnm:Name><gnm:Cells>"
        "<gnm:Cell Row=\"0\" Col=\"1\" ValueType=\"60\">hello &amp; bye</gnm:Cell>"
        "<gnm:Cell Row=\"1\" Col=\"0\" ValueType=\"40\">2.5</gnm:Cell>"
        "<gnm:Cell Row=\"1\" Col=\"1\">=IF(A1&lt;B1,&quot;yes&quot;,&#39;no&#39;)</gnm:Cell>"
        "</gnm:Cells></gnm:Sheet>";
    KryXml *root = kry_xml_parse(src);
    KryXml *cells;
    KryXml *cell;

    CHECK(root != NULL);
    CHECK(strcmp(kry_xml_text(kry_xml_find_local(root, "Name")), "Data") == 0);
    cells = kry_xml_find_local(root, "Cells");
    CHECK(cells != NULL && cells->count == 3);

    cell = kry_xml_child(cells, 0);
    CHECK(kry_xml_attr_long(cell, "Row", -1) == 0);
    CHECK(kry_xml_attr_long(cell, "Col", -1) == 1);
    CHECK(strcmp(kry_xml_attr(cell, "ValueType"), "60") == 0);
    CHECK(strcmp(kry_xml_text(cell), "hello & bye") == 0);

    cell = kry_xml_child(cells, 2);
    CHECK(strcmp(kry_xml_text(cell),
                 "=IF(A1<B1,\"yes\",'no')") == 0);

    CHECK(kry_xml_find(cells, "gnm:Cell") == kry_xml_child(cells, 0));
    CHECK(kry_xml_find(cells, "Cell") == NULL);
    CHECK(kry_xml_find_deep(root, "Name") != NULL);
    kry_xml_free(root);
}

static void
test_text_forms(void)
{
    KryXml *root;
    KryXml *child;

    /* nested elements with interleaved text */
    root = kry_xml_parse("<a>one<b>two</b>three<c/></a>");
    CHECK(root != NULL);
    CHECK(strcmp(kry_xml_text(root), "onethree") == 0);
    CHECK(root->count == 2);
    child = kry_xml_find(root, "b");
    CHECK(child != NULL && strcmp(kry_xml_text(child), "two") == 0);
    kry_xml_free(root);

    /* CDATA keeps raw markup */
    root = kry_xml_parse("<a><![CDATA[<not markup & raw>]]>x</a>");
    CHECK(root != NULL);
    CHECK(strcmp(kry_xml_text(root), "<not markup & raw>x") == 0);
    kry_xml_free(root);

    /* numeric and hex entities */
    root = kry_xml_parse("<a>&#65;&#x42;&#x263A;</a>");
    CHECK(root != NULL);
    CHECK(strcmp(kry_xml_text(root), "AB\xE2\x98\xBA") == 0);
    kry_xml_free(root);

    /* comments and processing instructions are skipped anywhere */
    root = kry_xml_parse("<a><?pi data?><!-- c --><b/><!-- t --></a>");
    CHECK(root != NULL && root->count == 1);
    kry_xml_free(root);

    /* empty text and self-closing with attributes */
    root = kry_xml_parse("<a x='1' y=\"2\"/>");
    CHECK(root != NULL);
    CHECK(strcmp(kry_xml_attr(root, "x"), "1") == 0);
    CHECK(strcmp(kry_xml_attr(root, "y"), "2") == 0);
    CHECK(strcmp(kry_xml_text(root), "") == 0);
    kry_xml_free(root);

    /* deep nesting beyond the cap is rejected, shallower accepted */
    {
        char deep[512];
        int i;
        int n;

        for(n = 40; n <= 70; n += 30) {
            size_t at = 0;

            for(i = 0; i < n; i++) {
                deep[at++] = '<';
                deep[at++] = 'a';
                deep[at++] = '>';
            }
            deep[at++] = 'x';
            for(i = 0; i < n; i++) {
                deep[at++] = '<';
                deep[at++] = '/';
                deep[at++] = 'a';
                deep[at++] = '>';
            }
            deep[at] = '\0';
            root = kry_xml_parse(deep);
            if(n == 40)
                CHECK(root != NULL);
            else
                CHECK(root == NULL);
            kry_xml_free(root);
        }
    }
}

static void
test_malformed(void)
{
    static const char *bad[] = {
        "",
        "plain text",
        "<a><b></a>",
        "<a></b>",
        "<a href=x'>1</a>",
        "<a>unterminated",
        "<a>",
        "<!-- no end",
    };
    size_t i;

    for(i = 0; i < sizeof(bad) / sizeof(bad[0]); i++)
        CHECK(kry_xml_parse(bad[i]) == NULL);
    CHECK(kry_xml_parse(NULL) == NULL);
}

static void
test_emitter(void)
{
    KryXmlBuf b = {0};

    kry_xml_buf_raw(&b, "<cell a=\"");
    kry_xml_buf_esc(&b, "1 < 2 & 3 > \"q\"");
    kry_xml_buf_raw(&b, "\">");
    kry_xml_buf_esc(&b, "=IF(A1<B1,\"x\",\"y\") & tail");
    kry_xml_buf_raw(&b, "</cell>");
    CHECK(strcmp(kry_xml_buf_finish(&b),
                 "<cell a=\"1 &lt; 2 &amp; 3 &gt; &quot;q&quot;\">"
                 "=IF(A1&lt;B1,&quot;x&quot;,&quot;y&quot;) &amp; tail"
                 "</cell>") == 0);
    kry_xml_buf_free(&b);

    /* zeroed buffer is safe */
    kry_xml_buf_free(&b);
    CHECK(strcmp(kry_xml_buf_finish(&b), "") == 0);
}

static void
test_round_trip(void)
{
    /* parse -> re-emit with escaping -> parse again */
    KryXmlBuf b = {0};
    KryXml *first = kry_xml_parse(
        "<sheet><cell v=\"a&amp;b\">x&lt;y</cell><cell>plain</cell></sheet>");
    KryXml *second;
    KryXml *cell;
    int i;

    CHECK(first != NULL);
    kry_xml_buf_raw(&b, "<sheet>");
    for(i = 0; i < first->count; i++) {
        cell = kry_xml_child(first, i);
        kry_xml_buf_raw(&b, "<cell v=\"");
        kry_xml_buf_esc(&b, kry_xml_attr(cell, "v"));
        kry_xml_buf_raw(&b, "\">");
        kry_xml_buf_esc(&b, kry_xml_text(cell));
        kry_xml_buf_raw(&b, "</cell>");
    }
    kry_xml_buf_raw(&b, "</sheet>");
    second = kry_xml_parse(kry_xml_buf_finish(&b));
    CHECK(second != NULL);
    CHECK(strcmp(kry_xml_attr(kry_xml_child(second, 0), "v"), "a&b") == 0);
    CHECK(strcmp(kry_xml_text(kry_xml_child(second, 0)), "x<y") == 0);
    CHECK(strcmp(kry_xml_text(kry_xml_child(second, 1)), "plain") == 0);
    kry_xml_free(second);
    kry_xml_free(first);
    kry_xml_buf_free(&b);
}

int
main(void)
{
    test_basic_document();
    test_gnumeric_cells();
    test_text_forms();
    test_malformed();
    test_emitter();
    test_round_trip();
    if(failures != 0) {
        fprintf(stderr, "kry_xml: %d failure(s)\n", failures);
        return 1;
    }
    printf("kry_xml ok\n");
    return 0;
}
