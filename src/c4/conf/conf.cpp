#include "c4/conf/conf.hpp"
#include <c4/error.hpp>
#include <c4/memory_resource.hpp>
#include <c4/fs/fs.hpp>
#include <c4/format.hpp>

#if 1 || defined(NDEBUG)
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
struct maybe_path_eq_yml : public path_eq_yml
{
    maybe_path_eq_yml(csubstr spec) : path_eq_yml("=")
    {
        spec = spec.unquoted();
        if(spec.count('='))
        {
            path_eq_yml parsed = {spec};
            tree_path = parsed.tree_path;
            yml = parsed.yml.unquoted();
        }
        else
        {
            tree_path = "";
            yml = spec;
        }
    }
};

csubstr _get_leaf_key(csubstr path) noexcept
{
    size_t dot_pos = path.last_of('.');
    return dot_pos == csubstr::npos ? path : path.right_of(dot_pos);
}
csubstr _get_root_key(csubstr path) noexcept
{
    size_t dot_pos = path.first_of('.');
    return dot_pos == csubstr::npos ? path : path.left_of(dot_pos);
}
} // namespace


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

Workspace::Workspace(yml::Tree *output, yml::Tree *t)
    : m_wsbuf(output->callbacks())
    , m_ws(t ? t : &m_wsbuf)
    , m_output(output)
    , m_load_started(false)
    , m_arena_when_load_started()
    , m_dir_scratch()
    , m_dir_entry_list()
{
}

Workspace::~Workspace()
{
    _release(&m_dir_entry_list.names);
    _release(&m_dir_entry_list.arena);
    _release(&m_dir_scratch);
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
    _dbg("allocating arena: " << sz << "B: " << m_output->arena().len << "B-->" << m_output->arena().len + sz << "B / " << m_output->arena_capacity() << 'B');
    C4_CHECK(m_output->arena_size() + sz <= m_output->arena_capacity());
    substr ret = m_output->alloc_arena(sz);
    C4_CHECK(m_arena_when_load_started.str == m_output->arena().str);
    return ret;
}

void Workspace::_reserve_arena(size_t sz) const
{
    size_t arena_cap = m_output->arena_capacity();
    size_t arena_req = arena_cap + sz;
    _dbg("reserving arena: " << sz << "B: " << arena_cap << "B-->" << arena_req << "B");
    m_output->reserve_arena(arena_req);
}

void Workspace::prepare_add_dir(csubstr tree_path, const char *dirname)
{
    if(tree_path.not_empty()) { _dbg("preparing add directory: " << tree_path << "=" << dirname); }
    else { _dbg("preparing add directory to root: " << dirname); }
    C4_CHECK(!m_load_started);
    auto noop = [](fs::VisitedFile const&){ return 0; };
    // ensure the scratch has enough space for all the existing
    // filenames in the dir
    if(!m_dir_scratch.required_size)
        m_dir_scratch.required_size = 256;
    bool ok;
    do
    {
        _ensure(&m_dir_scratch);
        ok = c4::fs::walk_entries(dirname, noop, &m_dir_scratch, this);
    } while(!m_dir_scratch.valid());
    C4_CHECK(ok);
    // now prepare for each file in the dir
    auto file_visitor = [](fs::VisitedFile const& vf){
        ((Workspace *)vf.user_data)->prepare_add_file(vf.name);
        return 0;
    };
    ok = c4::fs::walk_entries(dirname, file_visitor, &m_dir_scratch, this);
    C4_CHECK(ok);
    // accomodate also the directory name
    _reserve_arena(tree_path.len + 2u + strlen(dirname));
}

void Workspace::prepare_add_dir(const char *dirname)
{
    prepare_add_dir("", dirname);
}

void Workspace::prepare_add_file(csubstr tree_path, const char *filename)
{
    if(tree_path.not_empty()) { _dbg("preparing add file: " << tree_path << "=" << filename); }
    else { _dbg("preparing add file to root: " << filename); }
    C4_CHECK(!m_load_started);
    _reserve_arena(tree_path.len + 2u + strlen(filename) + 2u + fs::file_size(filename));
}

void Workspace::prepare_add_file(const char *filename)
{
    prepare_add_file("", filename);
}

void Workspace::prepare_add_conf(csubstr tree_path, csubstr conf_yml)
{
    C4_CHECK(!m_load_started);
    _reserve_arena(tree_path.len + 2u + conf_yml.len);
}

void Workspace::prepare_add_conf(csubstr tree_path_eq_conf_yml)
{
    auto specs = path_eq_yml(tree_path_eq_conf_yml);
    prepare_add_conf(specs.tree_path, specs.yml);
}

void Workspace::_parse_yml(csubstr filename, substr yml)
{
    // ensure the conf yml is already in the destination tree
    C4_CHECK(yml.is_sub(m_output->arena()));
    C4_CHECK(!yml.is_sub(m_ws->arena()));
    m_ws->clear(); // does not clear the arena
    m_ws->clear_arena();
    yml::parse_in_place(filename, yml, m_ws);
}

void Workspace::_parse_yml(csubstr filename, csubstr yml)
{
    if(yml.is_sub(m_output->arena()))
    {
        substr arena = m_output->arena();
        size_t pos = (size_t)(yml.str - arena.str);
        substr yml_copy = arena.sub(pos, yml.len);
        C4_ASSERT(yml_copy.str == yml.str);
        C4_ASSERT(yml_copy.len == yml.len);
        _parse_yml(filename, yml_copy);
    }
    else
    {
        substr yml_copy = _alloc_arena(yml.len);
        size_t used = c4::cat(yml_copy, yml);
        C4_CHECK(used == yml.len);
        _parse_yml(filename, yml_copy);
    }
}

// ensure root is not a doc
void Workspace::_remdoc(yml::Tree *t)
{
    C4_CHECK(!t->is_stream(t->root_id()));
    t->_rem_flags(t->root_id(), c4::yml::DOC);
}

// ensure root has a key
void Workspace::_askeyx(yml::Tree *t, csubstr key)
{
    size_t root = t->root_id();
    C4_CHECK(!t->has_key(root));
    C4_CHECK(t->is_val(root) || t->has_children(root));
    C4_CHECK(!t->has_other_siblings(root));
    yml::NodeData *C4_RESTRICT old_data = t->_p(root);
    yml::NodeData saved = *old_data;
    old_data->m_type = yml::MAP;
    if(saved.m_type.is_doc())
        if(saved.m_type.is_val())
            saved.m_type = saved.m_type & ~yml::DOC;
    size_t newid = t->append_child(root);
    yml::NodeData *C4_RESTRICT new_data = t->_p(newid);
    *new_data = saved;
    new_data->m_type = new_data->m_type | yml::KEY;
    new_data->m_key.scalar = key;
    new_data->m_parent = root;
    old_data->m_first_child = newid;
    old_data->m_last_child = newid;
    if(saved.m_last_child != yml::NONE)
        t->_p(saved.m_last_child)->m_next_sibling = yml::NONE;
    for(size_t ch = t->first_child(newid); ch != yml::NONE; ch = t->next_sibling(ch))
    {
        C4_ASSERT(t->_p(ch)->m_parent == root);
        t->_p(ch)->m_parent = newid;
    }
}

template<class CharType>
void Workspace::_add_conf(csubstr filename, csubstr dst_path, basic_substring<CharType> conf_yml)
{
    auto _setup_yml_as_val = [this, filename](basic_substring<CharType> yml){
        _parse_yml(filename, yml);
        _remdoc(m_ws);
        _dbg("src_tree");_pr(*m_ws);
        return m_ws->root_id();
    };
    auto _setup_yml_as_keyval = [this, filename](csubstr key, basic_substring<CharType> yml){
        _parse_yml(filename, yml);
        _askeyx(m_ws, key);
        _remdoc(m_ws);
        _dbg("src_tree");_pr(*m_ws);
        size_t keyconf_node = m_ws->first_child(m_ws->root_id());
        return keyconf_node;
    };

    // now find the node where we should insert
    _dbg("dst_tree"); _pr(*m_output);
    if(dst_path.empty())
    {
        _parse_yml(filename, conf_yml);
        _dbg("src_tree"); _pr(*m_ws);
        _dbg("merging at root");
        m_output->merge_with(m_ws, m_ws->root_id(), m_output->root_id());
    }
    else
    {
        _dbg("dst_path=" << dst_path);
        auto result = m_output->lookup_path(dst_path);
        _dbg("result: resolved='" << result.resolved() << "' vs unresolved='" << result.unresolved() << "'");
        if( ! result)
        {
            _dbg("modifying...");
            if(dst_path.trimr(" \t").ends_with(']'))
            {
                _dbg("no key!");
                size_t conf_node = _setup_yml_as_val(conf_yml);
                m_output->lookup_path_or_modify(m_ws, conf_node, dst_path);
            }
            else
            {
                csubstr res = result.resolved();
                _dbg("res='" << res << "'");
                csubstr rem;
                if(res.empty()) // do not use the ternary operator to expose potential coverage misses
                {
                    //C4_ERROR("crl");
                    rem = _get_root_key(result.unresolved());
                }
                else
                {
                    rem = _get_leaf_key(result.unresolved());
                }
                _dbg("key='" << dst_path << "' rem='" << rem << "'");
                size_t keyconf_node = _setup_yml_as_keyval(rem, conf_yml);
                m_output->lookup_path_or_modify(m_ws, keyconf_node, dst_path);
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
                m_output->merge_with(m_ws, conf_node, result.target);
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
                _dbg("conf=" << keyconf_node << "(" << m_ws->type_str(keyconf_node) << ")" << "\n" << *m_ws);
                _dbg("conf=\n" << *m_ws);
                m_output->merge_with(m_ws, keyconf_node, result.target);
            }
        }
    }
    _dbg("outputtree=\n" << *m_output);_pr(*m_output);
}

void Workspace::add_dir(csubstr tree_path, const char *dirname)
{
    if(tree_path.not_empty()) { _dbg("adding directory: " << tree_path << "=" << dirname); }
    else { _dbg("adding directory to root: " << dirname); }

    m_dir_scratch.required_size = 256;
    bool ok;
    do
    {
        _ensure(&m_dir_scratch);
        ok = c4::fs::list_entries(dirname, &m_dir_entry_list, &m_dir_scratch);
    } while(!m_dir_scratch.valid());
    C4_CHECK(m_dir_scratch.valid());
    if(!ok)
    {
        _ensure(&m_dir_entry_list.names);
        _ensure(&m_dir_entry_list.arena);
        ok = c4::fs::list_entries(dirname, &m_dir_entry_list, &m_dir_scratch);
    }
    C4_CHECK(ok);
    C4_CHECK(m_dir_entry_list.valid());
    C4_CHECK(m_dir_scratch.valid());
    m_dir_entry_list.sort();
    for(const char *filename : m_dir_entry_list)
    {
        add_file(tree_path, filename);
    }
}

void Workspace::add_dir(const char *dirname)
{
    csubstr rootpath = "";
    add_dir(rootpath, dirname);
}

void Workspace::add_file(csubstr tree_path, const char *filename_)
{
    if(tree_path.not_empty()) { _dbg("adding file: " << tree_path << "=" << filename_); }
    else { _dbg("adding file to root: " << filename_); }
    _load_started();
    C4_CHECK(fs::is_file(filename_));
    // copy the file contents into the tree arena
    size_t filesz = fs::file_size(filename_);
    substr file_contents = _alloc_arena(filesz); // must be substr, not csubstr!
    size_t actualsz = fs::file_get_contents(filename_, file_contents.str, file_contents.len);
    C4_CHECK(actualsz == filesz);
    // now parse the yaml content into the work tree
    _add_conf(to_csubstr(filename_), tree_path, file_contents);
}

void Workspace::add_file(const char *filename)
{
    csubstr rootpath = "";
    add_file(rootpath, filename);
}

void Workspace::add_conf(csubstr path_eq_conf_yml)
{
    auto specs = path_eq_yml(path_eq_conf_yml);
    add_conf(specs.tree_path, specs.yml);
}

void Workspace::add_conf(csubstr dst_path, csubstr conf_yml)
{
    _load_started();
    _add_conf("", dst_path, conf_yml);
}

void Workspace::apply_opts(ParsedOpt const* args_, size_t num_args)
{
    // prepare everything first
    ParsedOpt const* C4_RESTRICT args = args_;
    for(size_t iarg = 0; iarg < num_args; ++iarg)
    {
        ParsedOpt const& arg = args[iarg];
        switch(arg.action)
        {
        case ConfigAction::set_node:
            prepare_add_conf(arg.target, arg.payload);
            break;
        case ConfigAction::load_file:
            C4_ASSERT(strlen(arg.payload.data()) == arg.payload.len);
            prepare_add_file(arg.target, arg.payload.data());
            break;
        case ConfigAction::load_dir:
            C4_ASSERT(strlen(arg.payload.data()) == arg.payload.len);
            prepare_add_dir(arg.target, arg.payload.data());
            break;
        case ConfigAction::callback:
            break;
        default:
            C4_ERROR("unknown action");
        }
    }
    // now we can apply
    for(size_t iarg = 0; iarg < num_args; ++iarg)
    {
        ParsedOpt const& arg = args[iarg];
        switch(arg.action)
        {
        case ConfigAction::set_node:
            add_conf(arg.target, arg.payload);
            break;
        case ConfigAction::load_file:
            add_file(arg.target, arg.payload.data());
            break;
        case ConfigAction::load_dir:
            add_dir(arg.target, arg.payload.data());
            break;
        case ConfigAction::callback:
            arg.callback(*m_output, arg.payload);
            break;
        default:
            C4_ERROR("unknown action");
        }
    }
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

size_t parse_opts(int *argc, char ***argv,
                  ConfigActionSpec const* specs, size_t num_specs,
                  ParsedOpt *opt_args, size_t opt_args_size)
{
    auto getspec = [specs, num_specs](csubstr a) -> ConfigActionSpec const* {
        for(size_t ispec = 0; ispec < num_specs; ++ispec)
            if(specs[ispec].matches(a))
                return specs + ispec;
        return nullptr;
    };
    auto getarg = [&argc, &argv](int i) -> csubstr { C4_CHECK(i < *argc); return to_csubstr((*argv)[i]); };
    auto check_next_arg = [&](int iarg) -> bool {
        _dbg("arg[" << iarg << "]: expect one more argument");
        if(*argc < iarg + 2)
            return false;
        if(getarg(iarg + 1).begins_with('-')) // the next argument must not start with '-'
            return false;
        return true;
    };
    auto get_optional_arg = [&](int iarg, ConfigActionSpec const* spec, csubstr *optional_arg) -> bool {
        if(spec->accepts_optional_arg())
        {
            if(*argc < iarg + 2)
                return false;
            csubstr next_arg = getarg(iarg + 1);
            if(next_arg.begins_with('-')) // the next argument must not start with '-'
                return false;
            _dbg("arg[" << iarg << "]=" << getarg(iarg) << ": optional arg provided: " << next_arg);
            if(optional_arg)
                *optional_arg = next_arg;
        }
        else
        {
            if(spec->dummyname.empty())
            {
                C4_ASSERT(*argc >= iarg + 1);
                _dbg("arg[" << iarg << "]=" << getarg(iarg) << ": no arg expected");
                return false;
            }
            else
            {
                C4_ASSERT(*argc >= iarg + 2);
                csubstr next_arg = getarg(iarg + 1);
                C4_ASSERT(!next_arg.begins_with('-'));
                _dbg("arg[" << iarg << "]=" << getarg(iarg) << ": expected arg: " << next_arg);
                if(optional_arg)
                    *optional_arg = next_arg;
            }
        }
        return true;
    };
    size_t num_opt_args = 0;
    // if the output buffer size is insufficient, just report the
    // needed size, and do not change the input arguments buffer
    for(int iarg = 0; iarg < *argc; ++iarg)
    {
        ConfigActionSpec const* spec = getspec(getarg(iarg));
        if(!spec)
            continue;
        _dbg("arg[" << iarg << "]=" << getarg(iarg) << ": found spec: " << spec->optshort << "/" << spec->optlong);
        ++num_opt_args;
        // but report argerror ASAP
        switch(spec->action)
        {
        case ConfigAction::load_file:
        case ConfigAction::load_dir:
        case ConfigAction::set_node:
            if(!check_next_arg(iarg))
                return argerror;
            ++iarg;
            break;
        case ConfigAction::callback:
        {
            if(get_optional_arg(iarg, spec, nullptr))
                ++iarg;
            break;
        }
        default:
            C4_ERROR("unknown action");
        }
    }
    // now we know the arguments are sane
    if(num_opt_args > opt_args_size)
    {
        _dbg("require " << num_opt_args << " args, but space only for " << opt_args_size);
        return num_opt_args;
    }
    // ok, now we know we have enough size
    num_opt_args = 0;
    int filtered_argc = 0;
    for(int iarg = 0; iarg < *argc; /*do nothing here*/)
    {
        ConfigActionSpec const* spec = getspec(getarg(iarg));
        if(!spec)
        {
            _dbg("arg[" << iarg << "]=" << getarg(iarg) << ": no spec found");
            C4_ASSERT(filtered_argc <= iarg);
            if(filtered_argc != iarg)
                (*argv)[filtered_argc] = (*argv)[iarg];
            ++filtered_argc;
            ++iarg;
            continue;
        }
        _dbg("arg[" << iarg << "]=" << getarg(iarg) << ": found spec: " << spec->optshort << "/" << spec->optlong);
        switch(spec->action)
        {
        case ConfigAction::load_file:
        case ConfigAction::load_dir:
        case ConfigAction::set_node:
        {
            C4_ASSERT(check_next_arg(iarg));
            C4_ASSERT(opt_args && num_opt_args < opt_args_size);
            maybe_path_eq_yml parsed_spec(getarg(iarg + 1));
            opt_args[num_opt_args].action = spec->action;
            opt_args[num_opt_args].target = parsed_spec.tree_path;
            opt_args[num_opt_args].payload = parsed_spec.yml;
            opt_args[num_opt_args].callback = spec->callback;
            iarg += 2;
            break;
        }
        case ConfigAction::callback:
        {
            C4_ASSERT(opt_args && num_opt_args < opt_args_size);
            opt_args[num_opt_args].action = spec->action;
            opt_args[num_opt_args].target = {};
            opt_args[num_opt_args].payload = {};
            opt_args[num_opt_args].callback = spec->callback;
            bool needs_arg = get_optional_arg(iarg, spec, &opt_args[num_opt_args].payload);
            iarg += 1 + needs_arg;
            break;
        }
        default:
            C4_ERROR("unknown action");
        }
        ++num_opt_args;
    }
    _dbg("require " << num_opt_args);
    for(int iarg = filtered_argc; iarg < *argc; ++iarg)
        (*argv)[iarg] = nullptr;
    *argc = filtered_argc;
    return num_opt_args;
}

#undef _dbg

} // namespace conf
} // namespace c4
