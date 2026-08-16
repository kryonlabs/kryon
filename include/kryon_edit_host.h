#ifndef KRYON_EDIT_HOST_H
#define KRYON_EDIT_HOST_H

#include "app_host.h"
#include "kryon_node.h"

#define KRYON_EDIT_HOST_ABI_VERSION 1

typedef struct KryonEditHost {
    void *userdata;
    int (*node_count)(void *userdata);
    int (*node)(void *userdata, int index, KryonNode *out);
    int (*select_node)(void *userdata, const char *node_id);
    int (*node_at)(void *userdata, Vector2 point, KryonNode *out);
    int (*property_count)(void *userdata, const char *node_id);
    int (*property)(void *userdata, const char *node_id, int index,
                    PropertySpec *spec, PropertyValue *value);
    int (*set_property)(void *userdata, const KryonNodeEdit *edit);
    int (*move_node)(void *userdata, const char *node_id, float dx, float dy);
    int (*resize_node)(void *userdata, const char *node_id, float dw, float dh);
    int (*create_node)(void *userdata, const char *parent_id,
                       const char *type, Rectangle bounds, KryonNode *out);
    int (*delete_node)(void *userdata, const char *node_id);
    int (*save)(void *userdata, char *status, int status_size);
} KryonEditHost;

typedef KryonEditHost *(*GetKryonEditHostCallback)(AppHost *host);

#endif /* KRYON_EDIT_HOST_H */
