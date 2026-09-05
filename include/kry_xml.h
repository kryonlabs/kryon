/*
 * kry_xml.h - Kry standard library: XML parsing and emission.
 *
 * A small DOM parser with no dependencies. Nodes are plain structs the
 * caller walks directly; element and attribute names keep their namespace
 * prefix exactly as written (gnm:Cell stays "gnm:Cell"). Character data is
 * decoded (entities, CDATA) and concatenated into one text buffer per
 * element. Build output with KryXmlBuf, read input with kry_xml_parse.
 */
#ifndef KRYON_KRY_XML_H
#define KRYON_KRY_XML_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct KryXml KryXml;

struct KryXml {
    char *name;          /* element name including any ns prefix */
    char **attrs;        /* attribute names (may be NULL) */
    char **attr_values;  /* parallel to attrs */
    int attr_count;
    char *text;          /* decoded character data, "" when none */
    KryXml **children;   /* child elements (may be NULL) */
    int count;
};

/* Parse a complete XML document and return its root element. Returns NULL
 * on malformed input. Nesting is capped (64) so hostile input cannot
 * exhaust the C stack. Comments, processing instructions, the XML
 * declaration and DOCTYPE are skipped. */
KryXml *kry_xml_parse(const char *text);

/* Free a node and everything under it. NULL is allowed. */
void kry_xml_free(KryXml *node);

/* Element name (NULL only for a NULL node). */
const char *kry_xml_name(const KryXml *node);

/* Attribute value by exact name, NULL when the node lacks it. */
const char *kry_xml_attr(const KryXml *node, const char *name);

/* Attribute value parsed as a long (decimal). Returns fallback when the
 * attribute is absent or not a number. */
long kry_xml_attr_long(const KryXml *node, const char *name, long fallback);

/* Element name without any namespace prefix ("Cell" for "gnm:Cell").
 * NULL only for a NULL node. */
const char *kry_xml_name_local(const KryXml *node);

/* Decoded character data ("" for a NULL node). */
const char *kry_xml_text(const KryXml *node);

/* Child element by index, NULL when out of range. */
KryXml *kry_xml_child(const KryXml *node, int index);

/* First direct child whose name matches exactly. NULL when absent. */
KryXml *kry_xml_find(const KryXml *node, const char *name);

/* First direct child whose name matches after any namespace prefix
 * ("Cell" matches "gnm:Cell"). NULL when absent. */
KryXml *kry_xml_find_local(const KryXml *node, const char *local_name);

/* First descendant (document order, depth first) matching the local name. */
KryXml *kry_xml_find_deep(const KryXml *node, const char *local_name);

/* --- emitter ------------------------------------------------------------ */

typedef struct {
    char *buf;
    unsigned long len;
    unsigned long cap;
} KryXmlBuf;

/* Append raw text (tags, attributes, already-valid fragments). */
void kry_xml_buf_raw(KryXmlBuf *b, const char *text);

/* Append text escaped for XML character data and double-quoted attribute
 * values (& < > " become entities). */
void kry_xml_buf_esc(KryXmlBuf *b, const char *text);

/* NUL-terminate and return the buffer (owned by the KryXmlBuf; valid until
 * the next append or free). */
const char *kry_xml_buf_finish(KryXmlBuf *b);

/* Release the buffer. Safe on a zeroed KryXmlBuf. */
void kry_xml_buf_free(KryXmlBuf *b);

#ifdef __cplusplus
}
#endif

#endif /* KRYON_KRY_XML_H */
