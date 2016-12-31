/* Test/demo to see what information can be extracted
 * from aravis parsed xml (genicam) tree.
 */
#include <stdexcept>
#include <stdio.h>

#include "ghelper.h"

namespace {

void inspect_node(ArvDomNode *node, int indent)
{
    GErrorHelper err;
    const char *tag = arv_dom_node_get_node_name(node);

    printf("%*s", indent, tag);

    if(ARV_IS_GC_FEATURE_NODE(node)) {
        const char *name = arv_gc_feature_node_get_name(ARV_GC_FEATURE_NODE(node));
        printf(" %s", name);
    }

    /* order of tests is important as, eg.
     *    ENUMERATION is also INTEGER and STRING
     *    BOOLEAN is also INTEGER
     */

    if(ARV_IS_GC_ENUMERATION(node)) { // <Enumeration>
        ArvGcEnumeration *feat = ARV_GC_ENUMERATION(node);
        printf(" enum %s (%d)",
               arv_gc_enumeration_get_string_value(feat, err.get()),
               (int)arv_gc_enumeration_get_int_value(feat, err.get()));

        const GSList *entries = arv_gc_enumeration_get_entries(feat);
        if(entries) {
            for(const GSList *it = entries; it; it = it->next) {
                ArvGcEnumEntry *ent = ARV_GC_ENUM_ENTRY(it->data);
                if(!ent) continue;

                printf(" %s:%d,",
                       arv_gc_feature_node_get_name(ARV_GC_FEATURE_NODE(ent)),
                       (int)arv_gc_enum_entry_get_value(ent, err.get())
                       );
            }
        }

    } else if(ARV_IS_GC_BOOLEAN(node)) { // <Boolean>
        printf(" bool %d",
               arv_gc_boolean_get_value(ARV_GC_BOOLEAN(node), err.get()));

    } else if(ARV_IS_GC_COMMAND(node)) { // <Command>
        printf(" command");

    } else if(ARV_IS_GC_INTEGER(node)) { // <Integer>, <IntReg>, or <MaskedIntReg>
        printf(" integer %d",
               (int)arv_gc_integer_get_value(ARV_GC_INTEGER(node), err.get()));

    } else if(ARV_IS_GC_FLOAT(node)) { // <Float> or <FloatReg>
        printf(" float %f",
               arv_gc_float_get_value(ARV_GC_FLOAT(node), err.get()));

    } else if(ARV_IS_GC_STRING(node)) {  // <StringReg>
        printf(" string %s",
               arv_gc_string_get_value(ARV_GC_STRING(node), err.get()));

    } else if(ARV_IS_GC_FEATURE_NODE(node)) { // unhandled feature node type?
        printf(" feature %s",
               arv_gc_feature_node_get_value_as_string(ARV_GC_FEATURE_NODE(node), err.get()));

    } else { // non-feature node
        const char * val = arv_dom_node_get_node_value(node);
        if(val)
            printf(" \"%s\"", val);
    }

    if(err) {
        printf("(Error: %s)\n", err->message);
    } else {
        printf("\n");
    }

    ArvDomNodeList* nodes(arv_dom_node_get_child_nodes(node));

    for(unsigned i=0, len=arv_dom_node_list_get_length(nodes);
        i < len; i++)
    {
        ArvDomNode *cnode = arv_dom_node_list_get_item(nodes, i);
        if(!cnode) continue;

        inspect_node(cnode, indent+4);
    }
}

} // namespace

int main(int argc, char *argv[])
{
    try {
        if(argc<2) {
            fprintf(stderr, "Usage: %s <camera_name>\n", argv[0]);
            return 2;
        }

        // Search for camera name and build ArvCamera if found
        GWrapper<ArvCamera> cam(arv_camera_new(argv[1]));
        // extract borrowed references
        ArvDevice* dev(arv_camera_get_device(cam));
        ArvGc* gc(arv_device_get_genicam(dev));

        // Note that ArvGc derives from ArvDomDocument -> ArvDomNode -> GObject
        inspect_node(ARV_DOM_NODE(gc), 0);

        return 0;
    } catch(std::exception& e) {
        fprintf(stderr, "Unhandled exception: %s\n", e.what());
        return 2;
    }
}
