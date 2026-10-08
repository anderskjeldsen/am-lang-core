#include <libc/core.h>
#include <Am/Lang/GC.h>
#include <libc/core_inline_functions.h>
#include <Am/Lang/Array.h>
#include <libc/gc_graph.h>
#ifdef __APPLE__
#include <malloc/malloc.h>
#endif
// Enumerate proven owning edges: properties, Array elements and the one local
// reference held by each interface wrapper. Unknown native owners stay roots.
static void gc_owned_edges(cycle_graph *graph, aobject *obj) {
    aclass *cls=obj->class_ptr;
    if(cls->statics->type==interface) {
        cycle_edge(graph,obj->object_properties.iface_reference.implementation_object);
        return;
    }
    if(cls->statics->type!=class) return;
    if(cls->object_property_count) {
        property *props=obj->object_properties.class_object_properties.properties;
        for(int i=0;i<cls->properties_count;i++)
            if(!__is_primitive(props[i].nullable_value)) cycle_edge(graph,props[i].nullable_value.value.object_value);
    }
    if(cls->mark_children==(void*)Am_Lang_Array__native_mark_children_0) {
        array_holder *array=(array_holder*)((char*)obj+sizeof(aobject));
        if(array->ctype==object_type) {
            aobject **items=(aobject**)(array+1);
            for(unsigned i=0;i<array->size;i++) cycle_edge(graph,items[i]);
        } else if(array->ctype==any_type) {
            nullable_value *items=(nullable_value*)(array+1);
            for(unsigned i=0;i<array->size;i++) if(!__is_primitive(items[i])) cycle_edge(graph,items[i].value.object_value);
        }
    }
}
static void gc_clear_edge(nullable_value *slot) {
    if(!__is_primitive(*slot) && slot->value.object_value) {
        aobject *old=slot->value.object_value;
        slot->value.object_value=NULL;
        __decrease_property_reference_count(old);
    }
}
static void gc_clear_owned_edges(aobject *obj) {
    aclass *cls=obj->class_ptr;
    if(cls->statics->type!=class) return;
    if(cls->object_property_count) {
        property *props=obj->object_properties.class_object_properties.properties;
        for(int i=0;i<cls->properties_count;i++) gc_clear_edge(&props[i].nullable_value);
    }
    if(cls->mark_children==(void*)Am_Lang_Array__native_mark_children_0) {
        array_holder *array=(array_holder*)((char*)obj+sizeof(aobject));
        if(array->ctype==object_type) {
            aobject **items=(aobject**)(array+1);
            for(unsigned i=0;i<array->size;i++) if(items[i]) {
                aobject *old=items[i]; items[i]=NULL; __decrease_property_reference_count(old);
            }
        } else if(array->ctype==any_type) {
            nullable_value *items=(nullable_value*)(array+1);
            for(unsigned i=0;i<array->size;i++) gc_clear_edge(&items[i]);
        }
    }
}
function_result Am_Lang_GC_collect_0() {
    function_result result={.has_return_value=true};
    aisolate *iso=__amlc_isolate();
    if(iso->first_detached_object) return result;
    cycle_graph graph;
    // allocation_count also covers implementations held only through interface
    // wrappers (they have local references, so are absent from first_object).
    if(!cycle_init(&graph,(size_t)iso->allocation_count)) { cycle_dispose(&graph); return result; }
    graph.count=0;
    size_t i=0;
    for(aobject *obj=iso->first_object;obj;obj=obj->next)
        cycle_add(&graph,graph.count++,obj,(long)obj->reference_count+obj->property_reference_count);
    for(i=0;i<graph.count;i++) {
        aobject *obj=graph.nodes[i].object;
        if(obj->class_ptr->statics->type==interface) {
            aobject *impl=obj->object_properties.iface_reference.implementation_object;
            if(impl && !impl->immortal && !cycle_contains(&graph,impl))
                cycle_add(&graph,graph.count++,impl,(long)impl->reference_count+impl->property_reference_count);
        }
    }
    size_t count=graph.count;
    for(i=0;i<count;i++) gc_owned_edges(&graph,graph.nodes[i].object);
    // Native finalizers other than Array/String are not part of this collector's
    // ownership contract. Retain them and everything reachable from them.
    for(i=0;i<count;i++) {
        aobject *obj=graph.nodes[i].object;
        if(obj->immortal || obj->pending_deallocation ||
           (obj->class_ptr->release && obj->class_ptr->mark_children!=(void*)Am_Lang_Array__native_mark_children_0 && strcmp(obj->class_ptr->name,"Am.Lang.String")))
            graph.nodes[i].references=graph.nodes[i].internal+1;
    }
    cycle_roots(&graph);
    for(i=0;i<graph.queued;i++) gc_owned_edges(&graph,graph.nodes[graph.queue[i]].object);
    size_t live_bytes=0;
#ifdef __APPLE__
    if(getenv("AM_GC_TRACE")) for(i=0;i<count;i++) if(graph.nodes[i].live) {
        aobject *obj=graph.nodes[i].object;
        if(!obj->chunked) live_bytes+=malloc_size(obj);
    }
#endif
    long reclaimed=0;
    // Pin every unreachable candidate before clearing anything. Normal ARC
    // setters unlink properties; normal ARC release runs finalizers. Interface
    // edges remain intact: the wrapper's ordinary destructor releases them.
    // Pins protect implementations regardless of wrapper destruction order.
    // No manual detachment/free, list surgery or live-object mutation is needed.
    for(i=0;i<count;i++) if(!graph.nodes[i].live) {
        __increase_reference_count(graph.nodes[i].object); reclaimed++;
    }
    for(i=0;i<count;i++) if(!graph.nodes[i].live) gc_clear_owned_edges(graph.nodes[i].object);
    for(i=0;i<count;i++) if(!graph.nodes[i].live) __decrease_reference_count(graph.nodes[i].object);
    cycle_dispose(&graph);
    if(getenv("AM_GC_TRACE")) fprintf(stderr,"GC isolate=%p examined=%zu reclaimed=%ld remaining=%d live_bytes=%zu\n",(void*)iso,count,reclaimed,iso->allocation_count,live_bytes);
    result.return_value.value.long_value=reclaimed;
    return result;
}

function_result Am_Lang_GC__native_init_0(aobject *const self) { return (function_result){0}; }
function_result Am_Lang_GC__native_release_0(aobject *const self) { return (function_result){0}; }
function_result Am_Lang_GC__native_mark_children_0(aobject *const self) { return (function_result){0}; }
