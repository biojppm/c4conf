#include "c4/conf/conf.hpp"
#include <c4/fs/fs.hpp>
#include <c4/format.hpp>

//#define _C4CONF_VERBOSE
#ifndef _C4CONF_VERBOSE
#define _pr(...)
#define _dbg(...)
#else
#include <iostream>
#include <c4/yml/detail/print.hpp>
#define _pr(...) c4::yml::print_tree(__VA_ARGS__)
#define _dbg(...) std::cout << __FILE__ << ':' << __LINE__ << ": " << __VA_ARGS__ << std::endl
#endif

namespace c4 {
namespace conf {

namespace {
struct path_eq_yml
{
    /** path.to.foo.bar[0]=.....yml.... */
    path_eq_yml(csubstr spec)
    {
        C4_CHECK(spec.count('=') == 1);
        _dbg("spec=" << spec);
        size_t pos = spec.find('=');
        tree_path = spec.left_of(pos);
        yml = spec.right_of(pos);
        _dbg("dst_path=" << tree_path << "  conf_yml=" << yml);
    }
    csubstr tree_path;
    csubstr yml;
};

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
} // namespace


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

Workspace::Workspace(yml::Tree *output, yml::Tree *t)
    : m_tree_ws()
    , m_tree(t ? t : &m_tree_ws)
    , m_output(output)
    , m_load_started(false)
    , m_arena_when_load_started()
{
    C4_CHECK(output);
}

void Workspace::_load_started()
{
    const bool is_first_load = !m_load_started;
    m_load_started = true;
    if(is_first_load)
        m_arena_when_load_started = m_output->arena();
    else
        C4_CHECK(m_arena_when_load_started.str == m_output->arena().str);
}

substr Workspace::_alloc_arena(size_t sz) const
{
    substr ret = m_output->alloc_arena(sz);
    C4_CHECK(m_arena_when_load_started.str == m_output->arena().str);
    return ret;
}

void Workspace::prepare_add_file(const char *filename)
{
    C4_CHECK(!m_load_started);
    const size_t req_filecontents = c4::fs::file_size(filename) + 1;
    const size_t arena_cap = m_output->arena_capacity();
    const size_t arena_req = arena_cap + req_filecontents;
    m_output->reserve_arena(arena_req);
}

void Workspace::prepare_add_conf(csubstr tree_path, csubstr conf_yml)
{
    C4_CHECK(!m_load_started);
    const size_t req_conf = tree_path.len + conf_yml.len + 2;
    const size_t arena_cap = m_output->arena_capacity();
    const size_t arena_req = arena_cap + req_conf;
    m_output->reserve_arena(arena_req);
}

void Workspace::prepare_add_conf(csubstr tree_path_eq_conf_yml)
{
    auto specs = path_eq_yml(tree_path_eq_conf_yml);
    prepare_add_conf(specs.tree_path, specs.yml);
}

bool Workspace::add_file(const char *filename)
{
    _load_started();
    C4_CHECK(fs::is_file(filename));
    // copy the file contents into the tree arena
    size_t filesz = fs::file_size(filename);
    substr file_contents = _alloc_arena(filesz);
    size_t actualsz = fs::file_get_contents(filename, file_contents.str, file_contents.len);
    C4_CHECK(actualsz == filesz);
    // now parse the yaml content into the work tree
    m_tree->clear();
    yml::parse(to_csubstr(filename), file_contents, m_tree);
    m_output->merge_with(m_tree, m_tree->root_id(), m_output->root_id());
    return true;
}

void Workspace::add_conf(csubstr path_eq_conf_yml)
{
    auto specs = path_eq_yml(path_eq_conf_yml);
    add_conf(specs.tree_path, specs.yml);
}

void Workspace::add_conf(csubstr dst_path, csubstr conf_yml)
{
    auto _parse_yml = [this](substr yml) -> size_t {
        C4_STATIC_ASSERT((std::is_same<decltype(yml), substr>::value));
        // parse the yaml in the conf into the ws tree
        yml::parse(yml, m_tree);
        // ensure this is not a doc
        C4_CHECK(!m_tree->is_stream(m_tree->root_id()));
        m_tree->_rem_flags(m_tree->root_id(), c4::yml::DOC);
        _dbg("conf");_pr(*m_tree);
        return m_tree->root_id();
    };
    auto _setup_yml_as_val = [this, &_parse_yml](csubstr val){
        size_t required = val.len;
        substr yml_copy = _alloc_arena(required);
        size_t used = c4::cat(yml_copy, val);
        C4_CHECK(used == required);
        return _parse_yml(yml_copy);
    };
    auto _setup_yml_as_keyval = [this, &_parse_yml](csubstr key, csubstr val){
        size_t required = key.len + 2 + val.len;
        substr yml_copy = _alloc_arena(required);
        size_t used = c4::cat(yml_copy, key, ": ", val);
        C4_CHECK(used == required);
        size_t root_node = _parse_yml(yml_copy);
        size_t keyconf_node = m_tree->first_child(root_node);
        return keyconf_node;
    };

    _load_started();
    yml::Tree *wstree = m_tree;
    wstree->clear(); // does not clear the arena
    wstree->clear_arena();

    // now find the node where we should insert
    _dbg("dst_tree");_pr(*m_output);
    _dbg("dst_path=" << dst_path);
    auto result = m_output->lookup_path(dst_path);
    _dbg("result: resolved='" << result.resolved() << "' vs unresolved='" << result.unresolved() << "'");
    if( ! result)
    {
        _dbg("modifying...");
        if(dst_path.ends_with(']'))
        {
            _dbg("no key!");
            size_t conf_node = _setup_yml_as_val(conf_yml);
            m_output->lookup_path_or_modify(wstree, conf_node, dst_path);
        }
        else
        {
            csubstr res = result.resolved();
            csubstr rem = res.empty() ? _get_root_key(result.unresolved()) : _get_leaf_key(res);
            _dbg("key='" << dst_path << "' rem='" << rem << "'");
            size_t keyconf_node = _setup_yml_as_keyval(rem, conf_yml);
            m_output->lookup_path_or_modify(wstree, keyconf_node, dst_path);
        }
    }
    else
    {
        _dbg("merging...");
        // we only have the value of the conf; we need to make it look
        // like the destination node, so if it needs a key, we need to
        // add the appropriate key (eg, foo.bar.baz implies key must
        // be baz)
        if(!m_output->has_key(result.target))
        {
            // no key is needed, just do it.
            _dbg("no key!");
            size_t conf_node = _setup_yml_as_val(conf_yml);
            m_output->merge_with(wstree, conf_node, result.target);
        }
        else
        {
            C4_CHECK(!dst_path.ends_with(']'));
            // get the appropriate key
            csubstr rem = _get_leaf_key(dst_path);
            _dbg("key='" << dst_path << "' rem='" << rem << "'");
            // ensure we have that key with the conf
            size_t keyconf_node = _setup_yml_as_keyval(rem, conf_yml);
            // finally we have the conf ready to merge with the
            // destination node
            _dbg("conf=" << keyconf_node << "(" << wstree->type_str(keyconf_node) << ")" << "\n" << *wstree);
            _dbg("conf=\n" << *wstree);
            m_output->merge_with(wstree, keyconf_node, result.target);
        }
    }
    _pr(*m_output);
    _dbg("outputtree=\n" << *m_output);
}

#undef _dbg

} // namespace conf
} // namespace c4
