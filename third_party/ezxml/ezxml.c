/* ezxml.c
 *
 * Copyright 2004-2006 Aaron Voisine <aaron@voisine.org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include "ezxml.h"

#define EZXML_WS   "\t\r\n "  // whitespace
#define EZXML_ERRL 128        // maximum error string length

typedef struct ezxml_root *ezxml_root_t;
struct ezxml_root {       // additional data for the root tag
    struct ezxml xml;     // is a super-struct built on top of ezxml struct
    ezxml_t cur;          // current xml tree insertion point
    char *m;              // original xml string
    char *u;              // UTF-8 conversion of string if original was UTF-16
    char *s;              // start of work area
    char *e;              // end of work area
    char **ent;           // general entities (ampersand sequences)
    char ***attr;         // default attributes
    char ***pi;           // processing instructions
    short standalone;     // non-zero if <?xml standalone="yes"?>
    char err[EZXML_ERRL]; // error string
};

char *EZXML_NIL[] = { NULL }; // empty, null terminated array of strings

// returns the first child tag with the given name or NULL if not found
ezxml_t ezxml_child(ezxml_t xml, const char *name)
{
    xml = (xml) ? xml->child : NULL;
    while (xml && strcmp(name, xml->name)) xml = xml->sibling;
    return xml;
}

// returns the value of the requested tag attribute or NULL if not found
const char *ezxml_attr(ezxml_t xml, const char *attr)
{
    int i = 0, j = 1;
    ezxml_root_t root = (ezxml_root_t)xml;

    if (! xml || ! xml->attr) return NULL;
    while (xml->attr[i] && strcmp(attr, xml->attr[i])) i += 2;
    if (xml->attr[i]) return xml->attr[i + 1]; // found attribute

    while (root->xml.parent) root = (ezxml_root_t)root->xml.parent; // root tag
    for (i = 0; root->attr[i] && strcmp(xml->name, root->attr[i][0]); i++);
    if (! root->attr[i]) return NULL; // no matching default attributes
    while (root->attr[i][j] && strcmp(attr, root->attr[i][j])) j += 3;
    return (root->attr[i][j]) ? root->attr[i][j + 1] : NULL; // found default
}

// set an error string and return root
ezxml_t ezxml_err(ezxml_root_t root, char *s, const char *err, ...)
{
    va_list ap;
    int line = 1;
    char *t, fmt[EZXML_ERRL];
    
    for (t = root->s; t < s; t++) if (*t == '\n') line++;
    snprintf(fmt, EZXML_ERRL, "[error near line %d]: %s", line, err);

    va_start(ap, err);
    vsnprintf(root->err, EZXML_ERRL, fmt, ap);
    va_end(ap);

    return &root->xml;
}

// Recursively decodes entity and character references and normalizes new lines
// ent is a null terminated array of alternating entity names and values. set t
// to '&' for general entity decoding, '%' for parameter entity decoding, 'c'
// for cdata sections, ' ' for attribute normalization, or '*' for non-cdata
// attribute normalization. Returns s, or if the decoded string is longer than
// s, returns a malloced string that must be freed.
char *ezxml_decode(char *s, char **ent, char t)
{
    char *e, *r = s, *m = s;
    long b, c, d, l;

    for (; *s; s++) { // normalize line endings
        while (*s == '\r') {
            *(s++) = '\n';
            if (*s == '\n') memmove(s, (s + 1), strlen(s));
        }
    }
    
    for (s = r; ; ) {
        while (*s && *s != '&' && (*s != '%' || t != '%') && !isspace(*s)) s++;

        if (! *s) break;
        else if (t != 'c' && ! strncmp(s, "&#", 2)) { // character reference
            if (s[2] == 'x') c = strtol(s + 3, &e, 16); // base 16
            else c = strtol(s + 2, &e, 10); // base 10
            /* Ursatz: a reference outside 0 to 0x10FFFF (strtol also reads a sign, and a value past LONG_MAX) stays as written text; the UTF-8 encoder below shifted by a negative or oversized amount. */
            if (! c || c < 0 || c > 0x10FFFF || *e != ';') { s++; continue; } // not a character ref

            if (c < 0x80) *(s++) = c; // US-ASCII subset
            else { // multi-byte UTF-8 sequence
                for (b = 0, d = c; d; d /= 2) b++; // number of bits in c
                b = (b - 2) / 5; // number of bytes in payload
                *(s++) = (0xFF << (7 - b)) | (c >> (6 * b)); // head
                while (b) *(s++) = 0x80 | ((c >> (6 * --b)) & 0x3F); // payload
            }

            memmove(s, strchr(s, ';') + 1, strlen(strchr(s, ';')));
        }
        else if ((*s == '&' && (t == '&' || t == ' ' || t == '*')) ||
                 (*s == '%' && t == '%')) { // entity reference
            for (b = 0; ent[b] && strncmp(s + 1, ent[b], strlen(ent[b]));
                 b += 2); // find entity in entity list

            if (ent[b++]) { // found a match
                if ((c = strlen(ent[b])) - 1 > (e = strchr(s, ';')) - s) {
                    l = (d = (s - r)) + c + strlen(e); // new length
                    r = (r == m) ? strcpy(malloc(l), r) : realloc(r, l);
                    e = strchr((s = r + d), ';'); // fix up pointers
                }

                memmove(s + c, e + 1, strlen(e)); // shift rest of string
                strncpy(s, ent[b], c); // copy in replacement text
            }
            else s++; // not a known entity
        }
        else if ((t == ' ' || t == '*') && isspace(*s)) *(s++) = ' ';
        else s++; // no decoding needed
    }

    if (t == '*') { // normalize spaces for non-cdata attributes
        for (s = r; *s; s++) {
            if ((l = strspn(s, " "))) memmove(s, s + l, strlen(s + l) + 1);
            while (*s && *s != ' ') s++;
        }
        if (--s >= r && *s == ' ') *s = '\0'; // trim any trailing space
    }
    return r;
}

// called when parser finds start of new tag
void ezxml_open_tag(ezxml_root_t root, char *name, char **attr)
{
    ezxml_t xml = root->cur;
    
    if (xml->name) xml = ezxml_add_child(xml, name, strlen(xml->txt));
    else xml->name = name; // first open tag

    xml->attr = attr;
    root->cur = xml; // update tag insertion point
}

// called when parser finds character content between open and closing tag
void ezxml_char_content(ezxml_root_t root, char *s, size_t len, char t)
{
    ezxml_t xml = root->cur;
    char *m = s;
    size_t l;

    if (! xml || ! xml->name || ! len) return; // sanity check

    s[len] = '\0'; // null terminate text (calling functions anticipate this)
    len = strlen(s = ezxml_decode(s, root->ent, t)) + 1;

    if (! *(xml->txt)) xml->txt = s; // initial character content
    else { // allocate our own memory and make a copy
        xml->txt = (xml->flags & EZXML_TXTM) // allocate some space
                   ? realloc(xml->txt, (l = strlen(xml->txt)) + len)
                   : strcpy(malloc((l = strlen(xml->txt)) + len), xml->txt);
        strcpy(xml->txt + l, s); // add new char content
        if (s != m) free(s); // free s if it was malloced by ezxml_decode()
    }

    if (xml->txt != m) ezxml_set_flag(xml, EZXML_TXTM);
}

// called when parser finds closing tag
ezxml_t ezxml_close_tag(ezxml_root_t root, char *name, char *s)
{
    if (! root->cur || ! root->cur->name || strcmp(name, root->cur->name))
        return ezxml_err(root, s, "unexpected closing tag </%s>", name);

    root->cur = root->cur->parent;
    return NULL;
}

// called when the parser finds a processing instruction
void ezxml_proc_inst(ezxml_root_t root, char *s, size_t len)
{
    int i = 0, j = 1;
    char *target = s;

    s[len] = '\0'; // null terminate instruction
    if (*(s += strcspn(s, EZXML_WS))) {
        *s = '\0'; // null terminate target
        s += strspn(s + 1, EZXML_WS) + 1; // skip whitespace after target
    }

    if (! strcmp(target, "xml")) { // <?xml ... ?>
        if ((s = strstr(s, "standalone")) && ! strncmp(s + strspn(s + 10,
            EZXML_WS "='\"") + 10, "yes", 3)) root->standalone = 1;
        return;
    }

    if (! root->pi[0]) *(root->pi = malloc(sizeof(char **))) = NULL; //first pi

    while (root->pi[i] && strcmp(target, root->pi[i][0])) i++; // find target
    if (! root->pi[i]) { // new target
        root->pi = realloc(root->pi, sizeof(char **) * (i + 2));
        root->pi[i] = malloc(sizeof(char *) * 3);
        root->pi[i][0] = target;
        root->pi[i][1] = (char *)(root->pi[i + 1] = NULL); // terminate pi list
        root->pi[i][2] = strdup(""); // empty document position list
    }

    while (root->pi[i][j]) j++; // find end of instruction list for this target
    root->pi[i] = realloc(root->pi[i], sizeof(char *) * (j + 3));
    root->pi[i][j + 2] = realloc(root->pi[i][j + 1], j + 1);
    strcpy(root->pi[i][j + 2] + j - 1, (root->xml.name) ? ">" : "<");
    root->pi[i][j + 1] = NULL; // null terminate pi list for this target
    root->pi[i][j] = s; // set instruction
}

// Converts a UTF-16 string to UTF-8. Returns a new string that must be freed
// or NULL if no conversion was needed.
char *ezxml_str2utf8(char **s, size_t *len)
{
    char *u;
    size_t l = 0, sl, max = *len;
    long c, d;
    int b, be = (**s == '\xFE') ? 1 : (**s == '\xFF') ? 0 : -1;

    if (be == -1) return NULL; // not UTF-16

    u = malloc(max);
    for (sl = 2; sl < *len - 1; sl += 2) {
        c = (be) ? (((*s)[sl] & 0xFF) << 8) | ((*s)[sl + 1] & 0xFF)  //UTF-16BE
                 : (((*s)[sl + 1] & 0xFF) << 8) | ((*s)[sl] & 0xFF); //UTF-16LE
        if (c >= 0xD800 && c <= 0xDFFF && (sl += 2) < *len - 1) { // high-half
            d = (be) ? (((*s)[sl] & 0xFF) << 8) | ((*s)[sl + 1] & 0xFF)
                     : (((*s)[sl + 1] & 0xFF) << 8) | ((*s)[sl] & 0xFF);
            c = (((c & 0x3FF) << 10) | (d & 0x3FF)) + 0x10000;
        }

        while (l + 6 > max) u = realloc(u, max += EZXML_BUFSIZE);
        if (c < 0x80) u[l++] = c; // US-ASCII subset
        else { // multi-byte UTF-8 sequence
            for (b = 0, d = c; d; d /= 2) b++; // bits in c
            b = (b - 2) / 5; // bytes in payload
            u[l++] = (0xFF << (7 - b)) | (c >> (6 * b)); // head
            while (b) u[l++] = 0x80 | ((c >> (6 * --b)) & 0x3F); // payload
        }
    }
    /* Ursatz: no characters after the byte order mark: free the buffer here (a realloc to 0 is a free that valgrind reports) and tell the caller there is no text. */
    if (! l) { free(u); *s = NULL; *len = 0; return NULL; }
    return *s = realloc(u, *len = l);
}

// frees a tag attribute list
void ezxml_free_attr(char **attr) {
    int i = 0;
    char *m;
    
    if (! attr || attr == EZXML_NIL) return; // nothing to free
    while (attr[i]) i += 2; // find end of attribute list
    m = attr[i + 1]; // list of which names and values are malloced
    for (i = 0; m[i]; i++) {
        if (m[i] & EZXML_NAMEM) free(attr[i * 2]);
        if (m[i] & EZXML_TXTM) free(attr[(i * 2) + 1]);
    }
    free(m);
    free(attr);
}

// parse the given xml string and return an ezxml structure
ezxml_t ezxml_parse_str(char *s, size_t len)
{
    ezxml_root_t root = (ezxml_root_t)ezxml_new(NULL);
    if (! root) return NULL; /* Ursatz: out of memory (the importer reports it) */
    char q, e, *d, **attr, **a = NULL; // initialize a to avoid compile warning
    int l, i, j;

    root->m = s;
    if (! len) return ezxml_err(root, NULL, "root tag missing");
    root->u = ezxml_str2utf8(&s, &len); // convert utf-16 to utf-8
    /* Ursatz: a UTF-16 byte order mark with fewer than two bytes of text leaves nothing to parse (the realloc to 0 made s NULL, then s[len - 1] crashed). */
    if (! len || ! s) return ezxml_err(root, NULL, "no characters after the UTF-16 byte order mark");
    root->e = (root->s = s) + len; // record start and end of work area
    
    e = s[len - 1]; // save end char
    s[len - 1] = '\0'; // turn end char into null terminator

    while (*s && *s != '<') s++; // find first tag
    if (! *s) return ezxml_err(root, s, "root tag missing");

    for (; ; ) {
        attr = (char **)EZXML_NIL;
        d = ++s;
        
        if (isalpha(*s) || *s == '_' || *s == ':' || *s < '\0') { // new tag
            if (! root->cur)
                return ezxml_err(root, d, "markup outside of root element");

            s += strcspn(s, EZXML_WS "/>");
            while (isspace(*s)) *(s++) = '\0'; // null terminate tag name
  
            if (*s && *s != '/' && *s != '>') // find tag in default attr list
                for (i = 0; (a = root->attr[i]) && strcmp(a[0], d); i++);

            for (l = 0; *s && *s != '/' && *s != '>'; l += 2) { // new attrib
                attr = (l) ? realloc(attr, (l + 4) * sizeof(char *))
                           : malloc(4 * sizeof(char *)); // allocate space
                attr[l + 3] = (l) ? realloc(attr[l + 1], (l / 2) + 2)
                                  : malloc(2); // mem for list of maloced vals
                strcpy(attr[l + 3] + (l / 2), " "); // value is not malloced
                attr[l + 2] = NULL; // null terminate list
                attr[l + 1] = ""; // temporary attribute value
                attr[l] = s; // set attribute name

                s += strcspn(s, EZXML_WS "=/>");
                if (*s == '=' || isspace(*s)) { 
                    *(s++) = '\0'; // null terminate tag attribute name
                    q = *(s += strspn(s, EZXML_WS "="));
                    if (q == '"' || q == '\'') { // attribute value
                        attr[l + 1] = ++s;
                        while (*s && *s != q) s++;
                        if (*s) *(s++) = '\0'; // null terminate attribute val
                        else {
                            ezxml_free_attr(attr);
                            return ezxml_err(root, d, "missing %c", q);
                        }

                        for (j = 1; a && a[j] && strcmp(a[j], attr[l]); j +=3);
                        attr[l + 1] = ezxml_decode(attr[l + 1], root->ent, (a
                                                   && a[j]) ? *a[j + 2] : ' ');
                        if (attr[l + 1] < d || attr[l + 1] > s)
                            attr[l + 3][l / 2] = EZXML_TXTM; // value malloced
                    }
                }
                while (isspace(*s)) s++;
            }

            if (*s == '/') { // self closing tag
                *(s++) = '\0';
                if ((*s && *s != '>') || (! *s && e != '>')) {
                    if (l) ezxml_free_attr(attr);
                    return ezxml_err(root, d, "missing >");
                }
                ezxml_open_tag(root, d, attr);
                ezxml_close_tag(root, d, s);
            }
            else if ((q = *s) == '>' || (! *s && e == '>')) { // open tag
                *s = '\0'; // temporarily null terminate tag name
                ezxml_open_tag(root, d, attr);
                *s = q;
            }
            else {
                if (l) ezxml_free_attr(attr);
                return ezxml_err(root, d, "missing >"); 
            }
        }
        else if (*s == '/') { // close tag
            s += strcspn(d = s + 1, EZXML_WS ">") + 1;
            if (! (q = *s) && e != '>') return ezxml_err(root, d, "missing >");
            *s = '\0'; // temporarily null terminate tag name
            if (ezxml_close_tag(root, d, s)) return &root->xml;
            if (isspace(*s = q)) s += strspn(s, EZXML_WS);
        }
        else if (! strncmp(s, "!--", 3)) { // xml comment
            if (! (s = strstr(s + 3, "--")) || (*(s += 2) != '>' && *s) ||
                (! *s && e != '>')) return ezxml_err(root, d, "unclosed <!--");
        }
        else if (! strncmp(s, "![CDATA[", 8)) { // cdata
            if ((s = strstr(s, "]]>")))
                ezxml_char_content(root, d + 8, (s += 2) - d - 10, 'c');
            else return ezxml_err(root, d, "unclosed <![CDATA[");
        }
        else if (! strncmp(s, "!DOCTYPE", 8)) { // dtd
            for (l = 0; *s && ((! l && *s != '>') || (l && (*s != ']' || 
                 *(s + strspn(s + 1, EZXML_WS) + 1) != '>')));
                 l = (*s == '[') ? 1 : l) s += strcspn(s + 1, "[]>") + 1;
            if (! *s && e != '>')
                return ezxml_err(root, d, "unclosed <!DOCTYPE");
            /* Ursatz: an internal DTD subset is refused. It held the entity expansion (a 436-byte nested entity document grew to 100 MB) and the <!ATTLIST and <!ENTITY parser bugs; MusicXML files name only an external DTD. ezxml_internal_dtd and ezxml_ent_ok are deleted. */
            if (l) return ezxml_err(root, d, "internal DTD subset not supported");
        }
        else if (*s == '?') { // <?...?> processing instructions
            do { s = strchr(s, '?'); } while (s && *(++s) && *s != '>');
            if (! s || (! *s && e != '>')) 
                return ezxml_err(root, d, "unclosed <?");
            else ezxml_proc_inst(root, d + 1, s - d - 2);
        }
        else return ezxml_err(root, d, "unexpected <");
        
        if (! s || ! *s) break;
        *s = '\0';
        d = ++s;
        if (*s && *s != '<') { // tag character content
            while (*s && *s != '<') s++;
            if (*s) ezxml_char_content(root, d, s - d, '&');
            else break;
        }
        else if (! *s) break;
    }

    if (! root->cur) return &root->xml;
    else if (! root->cur->name) return ezxml_err(root, d, "root tag missing");
    else return ezxml_err(root, d, "unclosed tag <%s>", root->cur->name);
}

// free the memory allocated for the ezxml structure
void ezxml_free(ezxml_t xml)
{
    ezxml_root_t root = (ezxml_root_t)xml;
    int i, j;
    char **a, *s;

    if (! xml) return;
    ezxml_free(xml->child);
    ezxml_free(xml->ordered);

    if (! xml->parent) { // free root tag allocations
        for (i = 10; root->ent[i]; i += 2) // 0 - 9 are default entites (<>&"')
            if ((s = root->ent[i + 1]) < root->s || s > root->e) free(s);
        free(root->ent); // free list of general entities

        for (i = 0; (a = root->attr[i]); i++) {
            for (j = 1; a[j++]; j += 2) // free malloced attribute values
                if (a[j] && (a[j] < root->s || a[j] > root->e)) free(a[j]);
            free(a);
        }
        if (root->attr[0]) free(root->attr); // free default attribute list

        for (i = 0; root->pi[i]; i++) {
            for (j = 1; root->pi[i][j]; j++);
            free(root->pi[i][j + 1]);
            free(root->pi[i]);
        }            
        if (root->pi[0]) free(root->pi); // free processing instructions

        if (root->u) free(root->u); // utf8 conversion
    }

    ezxml_free_attr(xml->attr); // tag attributes
    if ((xml->flags & EZXML_TXTM)) free(xml->txt); // character content
    if ((xml->flags & EZXML_NAMEM)) free(xml->name); // tag name
    free(xml);
}

// return parser error message or empty string if none
const char *ezxml_error(ezxml_t xml)
{
    while (xml && xml->parent) xml = xml->parent; // find root tag
    return (xml) ? ((ezxml_root_t)xml)->err : "";
}

// returns a new empty ezxml structure with the given root tag name
ezxml_t ezxml_new(const char *name)
{
    static char *ent[] = { "lt;", "&#60;", "gt;", "&#62;", "quot;", "&#34;",
                           "apos;", "&#39;", "amp;", "&#38;", NULL };
    /* Ursatz: both allocations are checked (CVE-2021-26221, CVE-2021-26222); a failure returns NULL. */
    ezxml_root_t root = (ezxml_root_t)malloc(sizeof(struct ezxml_root));
    if (! root) return NULL;
    memset(root, '\0', sizeof(struct ezxml_root));
    root->ent = malloc(sizeof(ent));
    if (! root->ent) { free(root); return NULL; }
    memcpy(root->ent, ent, sizeof(ent));
    root->xml.name = (char *)name;
    root->cur = &root->xml;
    strcpy(root->err, root->xml.txt = "");
    root->attr = root->pi = (char ***)(root->xml.attr = EZXML_NIL);
    return &root->xml;
}

// inserts an existing tag into an ezxml structure
ezxml_t ezxml_insert(ezxml_t xml, ezxml_t dest, size_t off)
{
    ezxml_t cur, prev, head;

    xml->next = xml->sibling = xml->ordered = NULL;
    xml->off = off;
    xml->parent = dest;

    if ((head = dest->child)) { // already have sub tags
        if (head->off <= off) { // not first subtag
            for (cur = head; cur->ordered && cur->ordered->off <= off;
                 cur = cur->ordered);
            xml->ordered = cur->ordered;
            cur->ordered = xml;
        }
        else { // first subtag
            xml->ordered = head;
            dest->child = xml;
        }

        for (cur = head, prev = NULL; cur && strcmp(cur->name, xml->name);
             prev = cur, cur = cur->sibling); // find tag type
        if (cur && cur->off <= off) { // not first of type
            while (cur->next && cur->next->off <= off) cur = cur->next;
            xml->next = cur->next;
            cur->next = xml;
        }
        else { // first tag of this type
            if (prev && cur) prev->sibling = cur->sibling; // remove old first
            xml->next = cur; // old first tag is now next
            for (cur = head, prev = NULL; cur && cur->off <= off;
                 prev = cur, cur = cur->sibling); // new sibling insert point
            xml->sibling = cur;
            if (prev) prev->sibling = xml;
        }
    }
    else dest->child = xml; // only sub tag

    return xml;
}

// Adds a child tag. off is the offset of the child tag relative to the start
// of the parent tag's character content. Returns the child tag.
ezxml_t ezxml_add_child(ezxml_t xml, const char *name, size_t off)
{
    ezxml_t child;

    if (! xml) return NULL;
    child = (ezxml_t)memset(malloc(sizeof(struct ezxml)), '\0',
                            sizeof(struct ezxml));
    child->name = (char *)name;
    child->attr = EZXML_NIL;
    child->txt = "";

    return ezxml_insert(child, xml, off);
}

// sets a flag for the given tag and returns the tag
ezxml_t ezxml_set_flag(ezxml_t xml, short flag)
{
    if (xml) xml->flags |= flag;
    return xml;
}
