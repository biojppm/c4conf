#include "c4/conf/conf.hpp"

//#define _C4CONF_VERBOSE
#ifndef _C4CONF_VERBOSE
#define _dbg(...)
#define _pr(tree)
#else
#include <iostream>
#include <c4/yml/detail/print.hpp>
#define _pr(...) c4::yml::print_tree(__VA_ARGS__)
#define _dbg(...) std::cout << __FILE__ << ':' << __LINE__ << ": " << __VA_ARGS__ << std::endl
#endif

namespace c4 {
namespace conf {

namespace {
csubstr _get_leaf_key(csubstr path)
{
    size_t dot_pos = path.last_of('.');
    csubstr rem = dot_pos == csubstr::npos ? path : path.right_of(dot_pos);
    return rem;
}
csubstr _get_root_key(csubstr path)
{
    size_t dot_pos = path.first_of('.');
    csubstr rem = dot_pos == csubstr::npos ? path : path.left_of(dot_pos);
    return rem;
}

size_t _setup_conf_with_key(yml::Tree *wstree, csubstr key)
{
    size_t conf_node = wstree->root_id();
    // move the whole contents of the conf node
    // into a newly created parent which must be a map, so
    // that we can set the proper key; this is not as trivial
    // as it sounds, so here we're going to misuse
    // some of the Tree API to achieve this:
    _dbg("conf=" << conf_node << "(container=" << wstree->is_container(conf_node) << ")");
    const size_t num_nodes = wstree->size();
    _dbg("conf=" << conf_node << "(" << wstree->type_str(conf_node) << ") " << num_nodes << "ch"); _pr(*wstree);
    wstree->set_root_as_stream();
    conf_node = num_nodes; // reverse engineering FTW
    _dbg("conf=" << conf_node << "(" << wstree->type_str(conf_node) << ") " << num_nodes << "ch"); _pr(*wstree);
    C4_ASSERT(!wstree->is_root(conf_node));
    C4_ASSERT(wstree->is_doc(conf_node));
    size_t docmap_node = wstree->append_sibling(conf_node);
    wstree->to_map(docmap_node, c4::yml::DOC);
    size_t keyconf_node = wstree->append_child(docmap_node);
    wstree->_add_flags(keyconf_node, c4::yml::KEY | wstree->type(conf_node)); // this is needed
    wstree->duplicate_contents(conf_node, keyconf_node);
    wstree->_add_flags(keyconf_node, c4::yml::KEY | wstree->type(conf_node)); // ... and this is needed
    wstree->set_key(keyconf_node, key);
    wstree->_rem_flags(keyconf_node, c4::yml::DOC);
    return keyconf_node;
}
} // namespace

bool Workspace::load_file(csubstr s, yml::Tree *output)
{
    C4_ASSERT(output);
    size_t pos = output->arena_pos();
    // filesystem functions use libc functions which cannot
    // deal with substrings so create a cstr
    output->copy_to_arena(s);
    output->copy_to_arena("\0");
    const char *file = output->arena().str + pos;
    if (!fs::path_exists(file))
        return false;
    // copy the file contents into the tree arena
    size_t filesz = fs::file_get_contents(file, nullptr, 0);
    substr file_contents = output->alloc_arena(filesz);
    file = output->arena().str + pos; // WATCHOUT: the arena may have been reallocated
    fs::file_get_contents(file, file_contents.str, file_contents.len);
    // now parse the yaml content into the tree
    m_tree->clear();
    yml::parse(s, file_contents, m_tree);
    return true;
}

void add_conf(csubstr path_eq_conf_yml, yml::Tree *tree, Workspace *ws)
{
    C4_CHECK(path_eq_conf_yml.count('=') == 1);
    _dbg("spec=" << path_eq_conf_yml);
    size_t pos = path_eq_conf_yml.find('=');
    csubstr dst_path = path_eq_conf_yml.left_of(pos);
    csubstr conf_yml = path_eq_conf_yml.right_of(pos);
    _dbg("dst_path=" << dst_path << "  conf_yml=" << conf_yml);
    add_conf(dst_path, conf_yml, tree, ws);
}

void add_conf(csubstr dst_path, csubstr conf_yml, yml::Tree *tree, Workspace *ws)
{
    C4_ASSERT(ws != nullptr);
    C4_ASSERT(ws->m_tree != nullptr);
    //
    // clear but ensure the existing arena is kept
    yml::Tree *wstree = ws->m_tree;
    csubstr arena_before = wstree->arena();
    wstree->clear(); // does not clear the arena
    csubstr arena_after = wstree->arena();
    C4_CHECK(arena_after.str == arena_before.str);
    C4_CHECK(arena_after.len == arena_before.len);
    //
    // parse the yaml in the conf into the ws tree
    yml::parse(conf_yml, wstree);
    // it is likely the destination tree is pointing into the arena of
    // the wstree. so we need to ensure that no arena relocation
    // occured while parsing the yaml from the conf. To prevent
    // relocations from happening, call
    // Workspace::reserve_arena_for_confs() prior to this.
    arena_after = wstree->arena();
    C4_CHECK(arena_after.str == arena_before.str);
    C4_CHECK(arena_after.len >= arena_before.len);

    // now find the node where we should insert
    size_t conf_node = wstree->root_id();
    wstree->_rem_flags(conf_node, c4::yml::DOC);
    _dbg("dst_path=" << dst_path);
    _dbg("conf");_pr(*wstree);
    _dbg("dst_tree");_pr(*tree);
    auto result = tree->lookup_path(dst_path);
    _dbg("result: resolved='" << result.resolved() << "' vs unresolved='" << result.unresolved() << "'");
    if( ! result)
    {
        _dbg("modifying...");
        if(dst_path.ends_with(']'))
        {
            _dbg("no key!");
            tree->lookup_path_or_modify(wstree, conf_node, dst_path);
        }
        else
        {
            csubstr res = result.resolved();
            csubstr rem = res.empty() ? _get_root_key(result.unresolved()) : _get_leaf_key(result.resolved());
            size_t keyconf_node = _setup_conf_with_key(wstree, rem);
            tree->lookup_path_or_modify(wstree, keyconf_node, dst_path);
        }
    }
    else
    {
        _dbg("merging...");
        // we only have the value of the conf; we need to make it look
        // like the destination node, so if it needs a key, we need to
        // add the appropriate key (eg, foo.bar.baz implies key must
        // be baz)
        if(!tree->has_key(result.target))
        {
            // no key is needed, just do it.
            _dbg("no key!");
            tree->merge_with(wstree, conf_node, result.target);
        }
        else
        {
            C4_CHECK(!dst_path.ends_with(']'));
            // get the appropriate key
            csubstr rem = _get_leaf_key(dst_path);
            _dbg("key='" << dst_path << "' rem='" << rem << "'");
            // ensure we have that key with the conf
            size_t keyconf_node = _setup_conf_with_key(wstree, rem);
            // finally we have the conf ready to merge with the
            // destination node
            _dbg("conf=" << keyconf_node << "(" << wstree->type_str(keyconf_node) << ")" << "\n" << *wstree);
            _dbg("conf=\n" << *wstree);
            tree->merge_with(wstree, keyconf_node, result.target);
        }
    }
    _pr(*tree);
    _dbg("outputtree=\n" << *tree);
}

#undef _dbg

} // namespace conf
} // namespace c4
