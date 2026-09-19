#include "accessibility_internal.h"

#if defined(KRYON_ACCESSIBILITY_DBUS)
#include "accessibility_protocol.h"
#include "ui_tree.h"
#include "kryon_version.h"
#include <gio/gio.h>
#include <pango/pango.h>
#include <stdlib.h>
#include <string.h>

typedef struct AccessibleObject {
    gint references;
    int retired;
    AccessibilityNode node;
    char *key;
    char *path;
    char *label;
    char *value;
    char *role;
    char *parent;
    int index;
    int position;
    int child_count;
    int old_child_count;
    int cache_dirty;
    guint registrations[7];
} AccessibleObject;

typedef struct AccessibilityBridge {
    GMutex mutex;
    GMainContext *context;
    GThread *worker;
    GCancellable *cancel;
    GDBusConnection *connection;
    GDBusNodeInfo *protocol;
    char *title;
    char *address;
    GPtrArray *children;
    AccessibleObject *root;
    AccessibleObject *window;
    guint64 serial;
    int ready;
    int application_id;
    guint cache_registration;
    int active;
} AccessibilityBridge;

static AccessibilityBridge *accessibility_bridge;

static void
accessible_unref(gpointer data)
{
    AccessibleObject *object = data;
    if(!g_atomic_int_dec_and_test(&object->references))
        return;
    g_free(object->key);
    g_free(object->path);
    g_free(object->label);
    g_free(object->value);
    g_free(object->role);
    g_free(object->parent);
    g_free(object);
}

static const char *
accessible_text(const AccessibleObject *object)
{
    if(object->node.secure)
        return "";
    return strcmp(object->role, "text") == 0 ? object->label : object->value;
}

static int
accessible_scalar_offset(const char *text, int offset)
{
    int length = (int)strlen(text);
    if(offset < 0)
        offset = 0;
    if(offset > length)
        offset = length;
    return (int)g_utf8_strlen(text, offset);
}

static guint
accessible_role(const AccessibleObject *object)
{
    static const struct { const char *name; guint role; } roles[] = {
        {"application", 75}, {"frame", 23}, {"button", 43}, {"checkbox", 7},
        {"radio", 44}, {"textbox", 61}, {"text", 61}, {"group", 39},
        {"img", 27}, {"combobox", 11}, {"slider", 51}, {"progressbar", 42},
        {"menu", 33}, {"tablist", 38}, {"toolbar", 63}, {"listbox", 98},
        {"tree", 65}, {"table", 55}, {"dialog", 16}
    };
    if(object->node.secure)
        return 40;
    for(size_t i = 0; i < G_N_ELEMENTS(roles); i++)
        if(strcmp(object->role, roles[i].name) == 0)
            return roles[i].role;
    return 67;
}

static guint64
accessible_states(const AccessibleObject *object)
{
    AccessibilityNode node = object->node;
    guint64 bits = ((guint64)1 << 25) | ((guint64)1 << 30);
    if(!node.disabled)
        bits |= ((guint64)1 << 8) | ((guint64)1 << 24);
    if(node.actions & AccessibilityActionFocus)
        bits |= (guint64)1 << 11;
    if(node.focused)
        bits |= (guint64)1 << 12;
    if(node.checked)
        bits |= (guint64)1 << 4;
    if(strcmp(object->role, "checkbox") == 0 || strcmp(object->role, "radio") == 0)
        bits |= (guint64)1 << 41;
    if(node.read_only)
        bits |= (guint64)1 << 43;
    if(strcmp(object->role, "textbox") == 0) {
        if(!node.read_only && !node.disabled)
            bits |= (guint64)1 << 7;
        bits |= (guint64)1 << (node.multiline ? 17 : 26);
        if(!node.secure)
            bits |= (guint64)1 << 38;
    }
    return bits;
}

static int
accessible_interface(const AccessibleObject *object, const char *interface)
{
    if(strcmp(interface, ATSPI_ACCESSIBLE) == 0 || strcmp(interface, ATSPI_COMPONENT) == 0)
        return 1;
    if(strcmp(object->role, "application") == 0)
        return strcmp(interface, ATSPI_APPLICATION) == 0;
    if(strcmp(interface, ATSPI_ACTION) == 0)
        return (object->node.actions & AccessibilityActionActivate) != 0;
    if(strcmp(interface, ATSPI_TEXT) == 0)
        return strcmp(object->role, "textbox") == 0 || strcmp(object->role, "text") == 0;
    if(strcmp(interface, ATSPI_EDITABLE) == 0)
        return (object->node.actions & AccessibilityActionSetValue) != 0;
    return 0;
}

static const char *
accessible_bus_name(void)
{
    return g_dbus_connection_get_unique_name(accessibility_bridge->connection);
}

static GVariant *
accessible_reference(const char *path)
{
    return g_variant_new("(so)", accessible_bus_name(), path);
}

static AccessibleObject *
accessible_parent(const AccessibleObject *object)
{
    AccessibilityBridge *bridge = accessibility_bridge;
    if(object == bridge->root)
        return NULL;
    if(object == bridge->window)
        return bridge->root;
    if(strcmp(object->parent, ATSPI_WINDOW) == 0)
        return bridge->window;
    for(guint i = 0; i < bridge->children->len; i++) {
        AccessibleObject *candidate = bridge->children->pdata[i];
        if(strcmp(candidate->path, object->parent) == 0)
            return candidate;
    }
    return NULL;
}

static AccessibleObject *
accessible_child(const AccessibleObject *object, int index)
{
    AccessibilityBridge *bridge = accessibility_bridge;
    if(object == bridge->root)
        return index == 0 ? bridge->window : NULL;
    for(guint i = 0; i < bridge->children->len; i++) {
        AccessibleObject *child = bridge->children->pdata[i];
        if(child->index == index && strcmp(child->parent, object->path) == 0)
            return child;
    }
    return NULL;
}

static GVariant *
accessible_cache_item(const AccessibleObject *object)
{
    AccessibilityBridge *bridge = accessibility_bridge;
    GVariantBuilder interfaces;
    g_variant_builder_init(&interfaces, G_VARIANT_TYPE("as"));
    for(int i = 0; bridge->protocol->interfaces[i] != NULL; i++) {
        const char *name = bridge->protocol->interfaces[i]->name;
        if(accessible_interface(object, name))
            g_variant_builder_add(&interfaces, "s", name);
    }
    GVariant *parent = object == bridge->root
        ? g_variant_new("(so)", "org.a11y.atspi.Registry", ATSPI_ROOT)
        : accessible_reference(object == bridge->window ? ATSPI_ROOT : object->parent);
    int children = object->child_count;
    guint64 bits = accessible_states(object);
    guint32 states[] = {(guint32)bits, (guint32)(bits >> 32)};
    return g_variant_new("(@(so)@(so)@(so)ii@assus@au)",
        accessible_reference(object->path), accessible_reference(ATSPI_ROOT), parent,
        object->index, children, g_variant_builder_end(&interfaces), object->label,
        accessible_role(object), "",
        g_variant_new_fixed_array(G_VARIANT_TYPE_UINT32, states, 2, sizeof(guint32)));
}

static void
accessible_cache_update(const AccessibleObject *object)
{
    g_dbus_connection_emit_signal(accessibility_bridge->connection, NULL, ATSPI_CACHE_PATH,
        ATSPI_CACHE, "AddAccessible", g_variant_new("(@((so)(so)(so)iiassusau))",
            accessible_cache_item(object)), NULL);
}

static void
accessible_event(const char *path, const char *name, const char *detail,
                 int first, int second, GVariant *value)
{
    g_dbus_connection_emit_signal(accessibility_bridge->connection, NULL, path,
        "org.a11y.atspi.Event.Object", name,
        g_variant_new("(siiv@a{sv})", detail, first, second, value,
            g_variant_new_array(G_VARIANT_TYPE("{sv}"), NULL, 0)), NULL);
}

static Rectangle
accessible_bounds(const AccessibleObject *object, guint coordinates)
{
    Rectangle bounds = object->node.bounds;
    int toplevel = object == accessibility_bridge->root || object == accessibility_bridge->window;
    Vector2 position = IsWindowReady() ? GetWindowPosition() : (Vector2){0};
    if(coordinates == 0) {
        bounds.x += position.x;
        bounds.y += position.y;
    } else if(toplevel) {
        bounds.x = 0;
        bounds.y = 0;
    } else if(coordinates == 2) {
        AccessibleObject *parent = accessible_parent(object);
        if(parent != NULL && parent != accessibility_bridge->window) {
            bounds.x -= parent->node.bounds.x;
            bounds.y -= parent->node.bounds.y;
        }
    }
    return bounds;
}

static GVariant *
accessible_property(GDBusConnection *connection, const char *sender, const char *path,
                    const char *interface, const char *property, GError **error, gpointer data)
{
    AccessibleObject *object = data;
    AccessibilityBridge *bridge = accessibility_bridge;
    (void)connection;
    (void)sender;
    (void)path;
    if(object->retired) {
        g_set_error(error, G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_OBJECT, "Retired accessible object");
        return NULL;
    }
    if(strcmp(property, "version") == 0 || strcmp(property, "InterfaceVersion") == 0)
        return g_variant_new_uint32(1);
    if(strcmp(interface, ATSPI_ACCESSIBLE) == 0) {
        if(strcmp(property, "Name") == 0)
            return g_variant_new_string(object->label);
        if(strcmp(property, "Description") == 0 || strcmp(property, "HelpText") == 0)
            return g_variant_new_string("");
        if(strcmp(property, "Locale") == 0)
            return g_variant_new_string(g_getenv("LANG") != NULL ? g_getenv("LANG") : "C");
        if(strcmp(property, "AccessibleId") == 0) {
            char id[32];
            g_snprintf(id, sizeof(id), "%d", object->node.focus_id);
            return g_variant_new_string(id);
        }
        if(strcmp(property, "Parent") == 0) {
            if(object == bridge->root)
                return g_variant_new("(so)", "org.a11y.atspi.Registry", ATSPI_ROOT);
            return accessible_reference(object == bridge->window ? ATSPI_ROOT : object->parent);
        }
        if(strcmp(property, "ChildCount") == 0)
            return g_variant_new_int32(object->child_count);
    } else if(strcmp(interface, ATSPI_APPLICATION) == 0) {
        if(strcmp(property, "Id") == 0)
            return g_variant_new_int32(bridge->application_id);
        if(strcmp(property, "ToolkitName") == 0)
            return g_variant_new_string("Kryon");
        if(strcmp(property, "AtspiVersion") == 0)
            return g_variant_new_string("2.1");
        if(strcmp(property, "ToolkitVersion") == 0)
            return g_variant_new_string(KRYON_VERSION_STRING);
        if(strcmp(property, "Version") == 0)
            return g_variant_new_string("");
    } else if(strcmp(interface, ATSPI_TEXT) == 0) {
        if(strcmp(property, "CharacterCount") == 0)
            return g_variant_new_int32((int)g_utf8_strlen(accessible_text(object), -1));
        if(strcmp(property, "CaretOffset") == 0)
            return g_variant_new_int32(accessible_scalar_offset(object->value, object->node.selection_cursor));
    } else if(strcmp(interface, ATSPI_ACTION) == 0 && strcmp(property, "NActions") == 0) {
        return g_variant_new_int32(1);
    }
    g_set_error(error, G_DBUS_ERROR, G_DBUS_ERROR_UNKNOWN_PROPERTY, "Unknown accessible property");
    return NULL;
}

static gboolean
accessible_set_property(GDBusConnection *connection, const char *sender, const char *path,
                        const char *interface, const char *property, GVariant *value,
                        GError **error, gpointer data)
{
    (void)connection;
    (void)sender;
    (void)path;
    if(!((AccessibleObject *)data)->retired && data == accessibility_bridge->root && strcmp(interface, ATSPI_APPLICATION) == 0 &&
       strcmp(property, "Id") == 0 && g_variant_is_of_type(value, G_VARIANT_TYPE_INT32)) {
        accessibility_bridge->application_id = g_variant_get_int32(value);
        return TRUE;
    }
    g_set_error(error, G_DBUS_ERROR, G_DBUS_ERROR_PROPERTY_READ_ONLY, "Read-only accessible property");
    return FALSE;
}

static int
accessible_select(AccessibleObject *object, int start, int end)
{
    const char *text = accessible_text(object);
    glong length = g_utf8_strlen(text, -1);
    if(object->node.secure || start < 0 || end < 0 || start > length || end > length)
        return 0;
    return QueueAccessibilitySelection(object->node.focus_id, object->node.generation,
        (int)(g_utf8_offset_to_pointer(text, start) - text),
        (int)(g_utf8_offset_to_pointer(text, end) - text));
}

static GVariant *
accessible_text_range(const char *text, int offset, guint granularity)
{
    int length = (int)g_utf8_strlen(text, -1);
    if(offset < 0 || offset > length || granularity > 4)
        return NULL;
    if(offset == length)
        return g_variant_new("(sii)", "", offset, offset);
    PangoLogAttr *attributes = g_new0(PangoLogAttr, length + 1);
    pango_get_log_attrs(text, -1, -1, pango_language_get_default(), attributes, length + 1);
    int start = 0, end = length;
    const char *position = text;
    gunichar previous = 0;
    for(int index = 0; index <= length; index++) {
        int boundary = granularity == 0 ? attributes[index].is_cursor_position :
                       granularity == 1 ? attributes[index].is_word_boundary :
                       granularity == 2 ? attributes[index].is_sentence_boundary :
                       index == 0 || previous == '\n';
        if(boundary) {
            if(index <= offset)
                start = index;
            else {
                end = index;
                break;
            }
        }
        if(index < length) {
            previous = g_utf8_get_char(position);
            position = g_utf8_next_char(position);
        }
    }
    g_free(attributes);
    const char *first = g_utf8_offset_to_pointer(text, start);
    char *range = g_strndup(first, g_utf8_offset_to_pointer(text, end) - first);
    GVariant *result = g_variant_new("(sii)", range, start, end);
    g_free(range);
    return result;
}

static GVariant *
accessible_call(AccessibleObject *object, const char *interface, const char *method, GVariant *parameters)
{
    AccessibilityBridge *bridge = accessibility_bridge;
    AccessibilityNode node = object->node;
    if(strcmp(interface, ATSPI_CACHE) == 0) {
        GVariantBuilder items;
        g_variant_builder_init(&items, G_VARIANT_TYPE("a((so)(so)(so)iiassusau)"));
        g_variant_builder_add_value(&items, accessible_cache_item(bridge->root));
        g_variant_builder_add_value(&items, accessible_cache_item(bridge->window));
        for(guint i = 0; i < bridge->children->len; i++)
            g_variant_builder_add_value(&items, accessible_cache_item(bridge->children->pdata[i]));
        return g_variant_new("(@a((so)(so)(so)iiassusau))", g_variant_builder_end(&items));
    }
    if(strcmp(interface, ATSPI_ACCESSIBLE) == 0) {
        if(strcmp(method, "GetChildren") == 0) {
            GVariantBuilder children;
            g_variant_builder_init(&children, G_VARIANT_TYPE("a(so)"));
            for(int i = 0; i < object->child_count; i++) {
                AccessibleObject *child = accessible_child(object, i);
                if(child != NULL)
                    g_variant_builder_add_value(&children, accessible_reference(child->path));
            }
            return g_variant_new("(@a(so))", g_variant_builder_end(&children));
        }
        if(strcmp(method, "GetChildAtIndex") == 0) {
            int index;
            g_variant_get(parameters, "(i)", &index);
            AccessibleObject *child = accessible_child(object, index);
            if(child != NULL)
                return g_variant_new("(@(so))", accessible_reference(child->path));
            return NULL;
        }
        if(strcmp(method, "GetIndexInParent") == 0)
            return g_variant_new("(i)", object->index);
        if(strcmp(method, "GetRole") == 0)
            return g_variant_new("(u)", accessible_role(object));
        if(strcmp(method, "GetRoleName") == 0 || strcmp(method, "GetLocalizedRoleName") == 0)
            return g_variant_new("(s)", object->role);
        if(strcmp(method, "GetState") == 0) {
            guint64 bits = accessible_states(object);
            guint32 states[] = {(guint32)bits, (guint32)(bits >> 32)};
            return g_variant_new("(@au)", g_variant_new_fixed_array(G_VARIANT_TYPE_UINT32, states, 2, sizeof(guint32)));
        }
        if(strcmp(method, "GetAttributes") == 0)
            return g_variant_new("(@a{ss})", g_variant_new_array(G_VARIANT_TYPE("{ss}"), NULL, 0));
        if(strcmp(method, "GetRelationSet") == 0)
            return g_variant_new("(@a(ua(so)))", g_variant_new_array(G_VARIANT_TYPE("(ua(so))"), NULL, 0));
        if(strcmp(method, "GetApplication") == 0)
            return g_variant_new("(@(so))", accessible_reference(ATSPI_ROOT));
        if(strcmp(method, "GetInterfaces") == 0) {
            GVariantBuilder interfaces;
            g_variant_builder_init(&interfaces, G_VARIANT_TYPE("as"));
            for(int i = 0; bridge->protocol->interfaces[i] != NULL; i++) {
                const char *name = bridge->protocol->interfaces[i]->name;
                if(accessible_interface(object, name))
                    g_variant_builder_add(&interfaces, "s", name);
            }
            return g_variant_new("(@as)", g_variant_builder_end(&interfaces));
        }
    } else if(strcmp(interface, ATSPI_APPLICATION) == 0) {
        if(strcmp(method, "GetLocale") == 0)
            return g_variant_new("(s)", g_getenv("LANG") != NULL ? g_getenv("LANG") : "C");
        if(strcmp(method, "GetApplicationBusAddress") == 0)
            return g_variant_new("(s)", "");
    } else if(strcmp(interface, ATSPI_COMPONENT) == 0) {
        guint coordinates = 1;
        int x = 0, y = 0;
        if(strcmp(method, "GetExtents") == 0 || strcmp(method, "GetPosition") == 0)
            g_variant_get(parameters, "(u)", &coordinates);
        if(strcmp(method, "Contains") == 0 || strcmp(method, "GetAccessibleAtPoint") == 0)
            g_variant_get(parameters, "(iiu)", &x, &y, &coordinates);
        if(coordinates > 2)
            return NULL;
        Rectangle bounds = accessible_bounds(object, coordinates);
        if(strcmp(method, "GetExtents") == 0)
            return g_variant_new("((iiii))", (int)bounds.x, (int)bounds.y, (int)bounds.width, (int)bounds.height);
        if(strcmp(method, "GetPosition") == 0)
            return g_variant_new("(ii)", (int)bounds.x, (int)bounds.y);
        if(strcmp(method, "GetSize") == 0)
            return g_variant_new("(ii)", (int)bounds.width, (int)bounds.height);
        if(strcmp(method, "Contains") == 0)
            return g_variant_new("(b)", CheckCollisionPointRec((Vector2){x, y}, bounds));
        if(strcmp(method, "GetAccessibleAtPoint") == 0) {
            Vector2 point = {x, y};
            if(coordinates == 0 && IsWindowReady()) {
                Vector2 position = GetWindowPosition();
                point.x -= position.x;
                point.y -= position.y;
            } else if(coordinates == 2) {
                AccessibleObject *parent = accessible_parent(object);
                if(parent != NULL && parent != bridge->root && parent != bridge->window) {
                    point.x += parent->node.bounds.x;
                    point.y += parent->node.bounds.y;
                }
            }
            for(int i = (int)bridge->children->len - 1; i >= 0; i--) {
                AccessibleObject *child = bridge->children->pdata[i];
                AccessibleObject *parent = accessible_parent(child);
                while(parent != NULL && parent != object)
                    parent = accessible_parent(parent);
                if(parent == object && CheckCollisionPointRec(point, child->node.bounds))
                    return g_variant_new("(@(so))", accessible_reference(child->path));
            }
            return g_variant_new("(@(so))", accessible_reference(CheckCollisionPointRec((Vector2){x, y}, bounds) ? object->path : ATSPI_NULL));
        }
        if(strcmp(method, "GrabFocus") == 0)
            return g_variant_new("(b)", QueueAccessibilityAction(node.focus_id, node.generation, AccessibilityActionFocus));
        if(strcmp(method, "GetLayer") == 0)
            return g_variant_new("(u)", 3u);
        if(strcmp(method, "GetMDIZOrder") == 0)
            return g_variant_new("(n)", -1);
        if(strcmp(method, "GetAlpha") == 0)
            return g_variant_new("(d)", 1.0);
    } else if(strcmp(interface, ATSPI_ACTION) == 0) {
        if(strcmp(method, "GetActions") == 0) {
            GVariantBuilder actions;
            g_variant_builder_init(&actions, G_VARIANT_TYPE("a(sss)"));
            g_variant_builder_add(&actions, "(sss)", "activate", "", "");
            return g_variant_new("(@a(sss))", g_variant_builder_end(&actions));
        }
        int index;
        g_variant_get(parameters, "(i)", &index);
        if(index != 0)
            return NULL;
        if(strcmp(method, "DoAction") == 0)
            return g_variant_new("(b)", QueueAccessibilityAction(node.focus_id, node.generation, AccessibilityActionActivate));
        return g_variant_new("(s)", strcmp(method, "GetKeyBinding") == 0 ? "" : "activate");
    } else if(strcmp(interface, ATSPI_EDITABLE) == 0) {
        const char *value;
        g_variant_get(parameters, "(&s)", &value);
        return g_variant_new("(b)", QueueAccessibilityValue(node.focus_id, node.generation, value));
    } else if(strcmp(interface, ATSPI_TEXT) == 0) {
        const char *text = accessible_text(object);
        int length = (int)g_utf8_strlen(text, -1);
        int start = 0, end = 0, index = 0;
        if(strcmp(method, "GetStringAtOffset") == 0) {
            guint granularity;
            g_variant_get(parameters, "(iu)", &start, &granularity);
            return accessible_text_range(text, start, granularity);
        }
        if(strcmp(method, "GetText") == 0 || strcmp(method, "GetCharacterAtOffset") == 0) {
            if(strcmp(method, "GetText") == 0)
                g_variant_get(parameters, "(ii)", &start, &end);
            else {
                g_variant_get(parameters, "(i)", &start);
                if(start < 0 || start >= length)
                    return NULL;
                return g_variant_new("(i)", (int)g_utf8_get_char(g_utf8_offset_to_pointer(text, start)));
            }
            if(end == -1)
                end = length;
            if(start < 0 || end < start || end > length)
                return NULL;
            const char *first = g_utf8_offset_to_pointer(text, start);
            const char *last = g_utf8_offset_to_pointer(text, end);
            char *range = g_strndup(first, last - first);
            GVariant *result = g_variant_new("(s)", range);
            g_free(range);
            return result;
        }
        start = accessible_scalar_offset(text, node.selection_anchor);
        end = accessible_scalar_offset(text, node.selection_cursor);
        if(strcmp(method, "GetNSelections") == 0)
            return g_variant_new("(i)", start != end && !node.secure);
        if(strcmp(method, "GetSelection") == 0) {
            g_variant_get(parameters, "(i)", &index);
            if(index != 0 || start == end || node.secure)
                return NULL;
            return g_variant_new("(ii)", MIN(start, end), MAX(start, end));
        }
        if(strcmp(method, "SetCaretOffset") == 0) {
            g_variant_get(parameters, "(i)", &start);
            return g_variant_new("(b)", accessible_select(object, start, start));
        }
        if(strcmp(method, "SetSelection") == 0) {
            g_variant_get(parameters, "(iii)", &index, &start, &end);
            return g_variant_new("(b)", index == 0 && accessible_select(object, start, end));
        }
        if(strcmp(method, "AddSelection") == 0) {
            int empty = start == end;
            g_variant_get(parameters, "(ii)", &start, &end);
            return g_variant_new("(b)", empty && accessible_select(object, start, end));
        }
        if(strcmp(method, "RemoveSelection") == 0) {
            g_variant_get(parameters, "(i)", &index);
            return g_variant_new("(b)", index == 0 && accessible_select(object, end, end));
        }
        if(strcmp(method, "GetDefaultAttributes") == 0)
            return g_variant_new("(@a{ss})", g_variant_new_array(G_VARIANT_TYPE("{ss}"), NULL, 0));
    }
    return NULL;
}

static void
accessible_method(GDBusConnection *connection, const char *sender, const char *path,
                  const char *interface, const char *method, GVariant *parameters,
                  GDBusMethodInvocation *invocation, gpointer data)
{
    (void)connection;
    (void)sender;
    (void)path;
    if(((AccessibleObject *)data)->retired) {
        g_dbus_method_invocation_return_dbus_error(invocation,
            "org.freedesktop.DBus.Error.UnknownObject", "Retired accessible object");
        return;
    }
    GVariant *result = accessible_call(data, interface, method, parameters);
    if(result != NULL)
        g_dbus_method_invocation_return_value(invocation, result);
    else
        g_dbus_method_invocation_return_dbus_error(invocation,
            "org.freedesktop.DBus.Error.InvalidArgs", "Invalid accessible index or range");
}

static const GDBusInterfaceVTable accessible_vtable = {
    .method_call = accessible_method,
    .get_property = accessible_property,
    .set_property = accessible_set_property
};

static void
accessible_register(AccessibleObject *object)
{
    AccessibilityBridge *bridge = accessibility_bridge;
    g_main_context_push_thread_default(bridge->context);
    for(int i = 0; bridge->protocol->interfaces[i] != NULL; i++) {
        GDBusInterfaceInfo *interface = bridge->protocol->interfaces[i];
        int supported = accessible_interface(object, interface->name);
        if(supported && object->registrations[i] == 0) {
            g_atomic_int_inc(&object->references);
            object->registrations[i] = g_dbus_connection_register_object(bridge->connection,
                object->path, interface, &accessible_vtable, object, accessible_unref, NULL);
            if(object->registrations[i] == 0)
                accessible_unref(object);
        } else if(!supported && object->registrations[i] != 0) {
            g_dbus_connection_unregister_object(bridge->connection, object->registrations[i]);
            object->registrations[i] = 0;
        }
    }
    g_main_context_pop_thread_default(bridge->context);
}

static void
accessible_clear(AccessibleObject *object)
{
    if(object == NULL)
        return;
    object->retired = 1;
    for(size_t i = 0; i < G_N_ELEMENTS(object->registrations); i++)
        if(object->registrations[i] != 0)
            g_dbus_connection_unregister_object(accessibility_bridge->connection, object->registrations[i]);
    accessible_unref(object);
}

static gpointer
accessibility_connect(gpointer data)
{
    AccessibilityBridge *bridge = data;
    char *address = g_strdup(bridge->address);
    if(address == NULL || *address == '\0') {
        GDBusConnection *session = g_bus_get_sync(G_BUS_TYPE_SESSION, bridge->cancel, NULL);
        if(session != NULL) {
            GVariant *reply = g_dbus_connection_call_sync(session, "org.a11y.Bus", "/org/a11y/bus",
                "org.a11y.Bus", "GetAddress", NULL, G_VARIANT_TYPE("(s)"),
                G_DBUS_CALL_FLAGS_NONE, 1000, bridge->cancel, NULL);
            if(reply != NULL) {
                g_free(address);
                g_variant_get(reply, "(s)", &address);
                g_variant_unref(reply);
            }
            g_object_unref(session);
        }
    }
    GDBusConnection *connection = NULL;
    if(address != NULL && *address != '\0')
        connection = g_dbus_connection_new_for_address_sync(address,
            G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT | G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION,
            NULL, bridge->cancel, NULL);
    g_free(address);
    g_mutex_lock(&bridge->mutex);
    bridge->connection = connection;
    bridge->ready = 1;
    g_mutex_unlock(&bridge->mutex);
    return NULL;
}

void
ui_accessibility_platform_start(const char *title)
{
    if(accessibility_bridge != NULL || g_strcmp0(g_getenv("NO_AT_BRIDGE"), "1") == 0 ||
       g_strcmp0(g_getenv("KRYON_ACCESSIBILITY"), "0") == 0)
        return;
    AccessibilityBridge *bridge = g_new0(AccessibilityBridge, 1);
    g_mutex_init(&bridge->mutex);
    bridge->context = g_main_context_new();
    bridge->cancel = g_cancellable_new();
    bridge->title = g_utf8_make_valid(title != NULL ? title : "Kryon", -1);
    bridge->address = g_strdup(g_getenv("AT_SPI_BUS_ADDRESS"));
    bridge->children = g_ptr_array_new();
    bridge->protocol = g_dbus_node_info_new_for_xml(accessibility_protocol, NULL);
    accessibility_bridge = bridge;
    bridge->worker = g_thread_new("accessibility", accessibility_connect, bridge);
}

int
ui_accessibility_platform_active(void)
{
    return accessibility_bridge != NULL && accessibility_bridge->root != NULL;
}

static void
accessibility_embedded(GObject *source, GAsyncResult *result, gpointer data)
{
    (void)data;
    GVariant *reply = g_dbus_connection_call_finish(G_DBUS_CONNECTION(source), result, NULL);
    if(reply != NULL)
        g_variant_unref(reply);
}

void
ui_accessibility_platform_pump(void)
{
    AccessibilityBridge *bridge = accessibility_bridge;
    if(bridge == NULL)
        return;
    g_mutex_lock(&bridge->mutex);
    int ready = bridge->ready && bridge->connection != NULL;
    g_mutex_unlock(&bridge->mutex);
    if(ready && bridge->root == NULL) {
        bridge->root = g_new0(AccessibleObject, 1);
        bridge->root->references = 1;
        bridge->root->path = g_strdup(ATSPI_ROOT);
        bridge->root->role = g_strdup("application");
        bridge->root->label = g_strdup(bridge->title);
        bridge->root->value = g_strdup("");
        bridge->root->index = -1;
        bridge->root->child_count = 1;
        bridge->window = g_new0(AccessibleObject, 1);
        bridge->window->references = 1;
        bridge->window->path = g_strdup(ATSPI_WINDOW);
        bridge->window->role = g_strdup("frame");
        bridge->window->label = g_strdup(bridge->title);
        bridge->window->value = g_strdup("");
        accessible_register(bridge->root);
        accessible_register(bridge->window);
        g_main_context_push_thread_default(bridge->context);
        g_atomic_int_inc(&bridge->root->references);
        bridge->cache_registration = g_dbus_connection_register_object(bridge->connection,
            ATSPI_CACHE_PATH, g_dbus_node_info_lookup_interface(bridge->protocol, ATSPI_CACHE),
            &accessible_vtable, bridge->root, accessible_unref, NULL);
        if(bridge->cache_registration == 0)
            accessible_unref(bridge->root);
        g_dbus_connection_call(bridge->connection, "org.a11y.atspi.Registry", ATSPI_ROOT,
            "org.a11y.atspi.Socket", "Embed", g_variant_new("(@(so))", accessible_reference(ATSPI_ROOT)),
            G_VARIANT_TYPE("((so))"), G_DBUS_CALL_FLAGS_NONE, 1000, bridge->cancel, accessibility_embedded, NULL);
        g_main_context_pop_thread_default(bridge->context);
    }
    for(int i = 0; i < 64 && g_main_context_pending(bridge->context); i++)
        g_main_context_iteration(bridge->context, FALSE);
}

void
ui_accessibility_platform_close(void)
{
    AccessibilityBridge *bridge = accessibility_bridge;
    if(bridge == NULL)
        return;
    g_cancellable_cancel(bridge->cancel);
    g_thread_join(bridge->worker);
    if(bridge->connection != NULL) {
        if(bridge->cache_registration != 0)
            g_dbus_connection_unregister_object(bridge->connection, bridge->cache_registration);
        for(guint i = 0; i < bridge->children->len; i++) {
            AccessibleObject *object = bridge->children->pdata[i];
            accessible_clear(object);
        }
        accessible_clear(bridge->root);
        accessible_clear(bridge->window);
        g_dbus_connection_flush_sync(bridge->connection, NULL, NULL);
        g_dbus_connection_close_sync(bridge->connection, NULL, NULL);
        g_object_unref(bridge->connection);
    }
    g_ptr_array_free(bridge->children, TRUE);
    g_dbus_node_info_unref(bridge->protocol);
    g_object_unref(bridge->cancel);
    g_free(bridge->title);
    g_free(bridge->address);
    while(g_main_context_pending(bridge->context))
        g_main_context_iteration(bridge->context, FALSE);
    g_main_context_unref(bridge->context);
    g_mutex_clear(&bridge->mutex);
    g_free(bridge);
    accessibility_bridge = NULL;
}

void
ui_accessibility_platform_publish(const AccessibilityNode *nodes, int count)
{
    AccessibilityBridge *bridge = accessibility_bridge;
    if(!ui_accessibility_platform_active())
        return;
    int active = IsWindowReady() && IsWindowFocused();
    if(active != bridge->active) {
        bridge->active = active;
        g_dbus_connection_emit_signal(bridge->connection, NULL, ATSPI_WINDOW,
            "org.a11y.atspi.Event.Window", active ? "Activate" : "Deactivate",
            g_variant_new("(siiv@a{sv})", "", 0, 0, g_variant_new_string(bridge->title),
                g_variant_new_array(G_VARIANT_TYPE("{sv}"), NULL, 0)), NULL);
    }
    GPtrArray *previous = bridge->children;
    GHashTable *previous_by_key = g_hash_table_new(g_str_hash, g_str_equal);
    GHashTable *id_counts = g_hash_table_new(g_direct_hash, g_direct_equal);
    GHashTable *semantic_keys = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    AccessibleObject **parents = g_new0(AccessibleObject *, count + 1);
    parents[0] = bridge->window;
    bridge->window->old_child_count = bridge->window->child_count;
    bridge->window->child_count = 0;
    for(guint i = 0; i < previous->len; i++) {
        AccessibleObject *object = previous->pdata[i];
        g_hash_table_insert(previous_by_key, object->key, object);
    }
    for(int i = 0; i < count; i++) {
        gpointer id = GINT_TO_POINTER(nodes[i].focus_id);
        int occurrences = GPOINTER_TO_INT(g_hash_table_lookup(id_counts, id));
        g_hash_table_insert(id_counts, id, GINT_TO_POINTER(occurrences + 1));
        if(nodes[i].key != 0) {
            char *key = g_strdup_printf("key:%" G_GUINT64_FORMAT ":%s", (guint64)nodes[i].key, nodes[i].role);
            occurrences = GPOINTER_TO_INT(g_hash_table_lookup(semantic_keys, key));
            g_hash_table_replace(semantic_keys, key, GINT_TO_POINTER(occurrences + 1));
        }
    }
    bridge->children = g_ptr_array_new();
    bridge->root->node.bounds = (Rectangle){0, 0, GetScreenWidth(), GetScreenHeight()};
    bridge->window->node.bounds = bridge->root->node.bounds;
    for(int i = 0; i < count; i++) {
        AccessibilityNode node = nodes[i];
        AccessibleObject *parent = node.parent > 0 && node.parent <= (unsigned)i
            ? parents[node.parent] : bridge->window;
        parents[i+1] = parent;
        if(IsWindowReady() && !IsWindowFocused())
            node.focused = 0;
        if(strcmp(node.role, "main") == 0)
            continue;
        int unique = node.focus_id > 0 &&
            GPOINTER_TO_INT(g_hash_table_lookup(id_counts, GINT_TO_POINTER(node.focus_id))) == 1;
        char *semantic_key = g_strdup_printf("key:%" G_GUINT64_FORMAT ":%s", (guint64)node.key, node.role);
        char *key;
        if(unique)
            key = g_strdup_printf("focus:%d:%s", node.focus_id, node.role);
        else if(node.key != 0 && GPOINTER_TO_INT(g_hash_table_lookup(semantic_keys, semantic_key)) == 1)
            key = g_strdup(semantic_key);
        else
            key = g_strdup_printf("position:%s:%d:%s", parent->path, parent->child_count, node.role);
        g_free(semantic_key);
        if(!unique)
            node.actions = 0;
        AccessibleObject *object = g_hash_table_lookup(previous_by_key, key);
        if(object != NULL)
            previous->pdata[object->position] = NULL;
        int created = object == NULL;
        if(created) {
            object = g_new0(AccessibleObject, 1);
            object->references = 1;
            object->key = key;
            object->path = g_strdup_printf("/org/a11y/atspi/accessible/node_%" G_GUINT64_FORMAT, ++bridge->serial);
            object->role = g_strdup(node.role);
        } else {
            g_free(key);
        }
        AccessibilityNode old = object->node;
        char *old_value = object->value;
        char *old_label = object->label;
        char *old_parent = object->parent;
        object->parent = g_strdup(parent->path);
        object->old_child_count = object->child_count;
        object->child_count = 0;
        parents[i+1] = object;
        object->node = node;
        object->label = g_utf8_make_valid(node.label != NULL ? node.label : "", -1);
        object->value = g_utf8_make_valid(!node.secure && node.value != NULL ? node.value : "", -1);
        object->node.label = object->label;
        object->node.value = object->value;
        object->node.role = object->role;
        if(node.secure) {
            object->node.selection_anchor = 0;
            object->node.selection_cursor = 0;
        }
        int old_index = object->index;
        object->position = (int)bridge->children->len;
        object->index = parent->child_count++;
        g_ptr_array_add(bridge->children, object);
        accessible_register(object);
        int moved = !created && (old_index != object->index || strcmp(old_parent, object->parent) != 0);
        if(created || moved || strcmp(old_label, object->label) != 0 ||
           old.focused != node.focused || old.checked != node.checked ||
           old.disabled != node.disabled || old.read_only != node.read_only ||
           old.actions != node.actions || old.secure != node.secure)
            object->cache_dirty = 1;
        if(moved)
            accessible_event(old_parent, "ChildrenChanged", "remove", old_index, 0, accessible_reference(object->path));
        if(created || moved)
            accessible_event(object->parent, "ChildrenChanged", "add", object->index, 0, accessible_reference(object->path));
        if(old.focused != node.focused)
            accessible_event(object->path, "StateChanged", "focused", node.focused, 0, g_variant_new_int32(0));
        if(old.checked != node.checked)
            accessible_event(object->path, "StateChanged", "checked", node.checked, 0, g_variant_new_int32(0));
        if(old.disabled != node.disabled)
            accessible_event(object->path, "StateChanged", "enabled", !node.disabled, 0, g_variant_new_int32(0));
        if(!created && strcmp(old_label, object->label) != 0)
            accessible_event(object->path, "PropertyChange", "accessible-name", 0, 0, g_variant_new_string(object->label));
        if(!created && !node.secure && strcmp(old_value, object->value) != 0) {
            accessible_event(object->path, "TextChanged", "delete", 0, (int)g_utf8_strlen(old_value, -1), g_variant_new_string(old_value));
            accessible_event(object->path, "TextChanged", "insert", 0, (int)g_utf8_strlen(object->value, -1), g_variant_new_string(object->value));
        }
        if(!node.secure && (old.selection_anchor != node.selection_anchor || old.selection_cursor != node.selection_cursor)) {
            accessible_event(object->path, "TextCaretMoved", "", accessible_scalar_offset(object->value, node.selection_cursor), 0, g_variant_new_int32(0));
            accessible_event(object->path, "TextSelectionChanged", "", 0, 0, g_variant_new_int32(0));
        }
        g_free(old_label);
        g_free(old_value);
        g_free(old_parent);
    }
    for(guint i = 0; i < previous->len; i++) {
        AccessibleObject *object = previous->pdata[i];
        if(object != NULL) {
            g_dbus_connection_emit_signal(bridge->connection, NULL, ATSPI_CACHE_PATH,
                ATSPI_CACHE, "RemoveAccessible", g_variant_new("(@(so))", accessible_reference(object->path)), NULL);
            accessible_event(object->path, "StateChanged", "defunct", 1, 0, g_variant_new_int32(0));
            accessible_event(object->parent, "ChildrenChanged", "remove", object->index, 0, accessible_reference(object->path));
            accessible_clear(object);
        }
    }
    if(bridge->window->old_child_count != bridge->window->child_count)
        accessible_cache_update(bridge->window);
    for(guint i = 0; i < bridge->children->len; i++) {
        AccessibleObject *object = bridge->children->pdata[i];
        if(object->cache_dirty || object->old_child_count != object->child_count)
            accessible_cache_update(object);
        object->cache_dirty = 0;
    }
    g_free(parents);
    g_hash_table_destroy(semantic_keys);
    g_hash_table_destroy(previous_by_key);
    g_hash_table_destroy(id_counts);
    g_ptr_array_free(previous, TRUE);
}

#else
void ui_accessibility_platform_start(const char *title) { (void)title; }
void ui_accessibility_platform_close(void) {}
void ui_accessibility_platform_pump(void) {}
int ui_accessibility_platform_active(void) { return 0; }
void ui_accessibility_platform_publish(const AccessibilityNode *nodes, int count)
{
    (void)nodes;
    (void)count;
}
#endif
