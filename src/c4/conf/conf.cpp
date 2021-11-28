#include "c4/conf/conf.hpp"
#include "c4/error.hpp"
#include "c4/memory_resource.hpp"
#include <c4/fs/fs.hpp>
#include <c4/format.hpp>

#define _C4CONF_VERBOSE
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
    : m_wsbuf(output->allocator())
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
    substr ret = m_output->alloc_arena(sz);
    C4_CHECK(m_arena_when_load_started.str == m_output->arena().str);
    return ret;
}

void Workspace::prepare_add_dir(csubstr tree_path, const char *dirname)
{
    C4_CHECK(!m_load_started);
    auto noop = [](fs::VisitedFile const&){ return 0; };
    // ensure the scratch has enough space for all the existing
    // filenames in the dir
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
        Workspace *this_ = (Workspace *)vf.user_data;
        this_->prepare_add_file(vf.name);
        return 0;
    };
    ok = c4::fs::walk_entries(dirname, file_visitor, &m_dir_scratch, this);
    C4_CHECK(ok);
    // accomodate also the directory name
    size_t req_filecontents = tree_path.len + 2u + strlen(dirname);
    size_t arena_cap = m_output->arena_capacity();
    size_t arena_req = arena_cap + req_filecontents;
    m_output->reserve_arena(arena_req);
}

void Workspace::prepare_add_dir(const char *dirname)
{
    prepare_add_dir("", dirname);
}

void Workspace::prepare_add_file(csubstr tree_path, const char *filename)
{
    C4_CHECK(!m_load_started);
    size_t req_filecontents = tree_path.len + fs::file_size(filename) + 2u;
    size_t arena_cap = m_output->arena_capacity();
    size_t arena_req = arena_cap + req_filecontents;
    m_output->reserve_arena(arena_req);
}

void Workspace::prepare_add_file(const char *filename)
{
    C4_CHECK(!m_load_started);
    size_t req_filecontents = fs::file_size(filename) + 1u;
    size_t arena_cap = m_output->arena_capacity();
    size_t arena_req = arena_cap + req_filecontents;
    m_output->reserve_arena(arena_req);
}

void Workspace::prepare_add_conf(csubstr tree_path, csubstr conf_yml)
{
    C4_CHECK(!m_load_started);
    size_t req_conf = tree_path.len + conf_yml.len + 2u;
    size_t arena_cap = m_output->arena_capacity();
    size_t arena_req = arena_cap + req_conf;
    m_output->reserve_arena(arena_req);
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
    yml::parse(filename, yml, m_ws);
}

void Workspace::_parse_yml(csubstr filename, csubstr yml)
{
    if(yml.is_sub(m_output->arena()))
    {
        substr arena = m_output->arena();
        size_t pos = yml.str - arena.str;
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

template<class CharType>
void Workspace::_add_conf(csubstr filename, csubstr dst_path, basic_substring<CharType> conf_yml)
{
    auto _parse = [this, filename](substr yml) -> size_t {
        C4_STATIC_ASSERT((std::is_same<decltype(yml), substr>::value));
        _parse_yml(filename, yml);
        // ensure this is not a doc
        C4_CHECK(!m_ws->is_stream(m_ws->root_id()));
        m_ws->_rem_flags(m_ws->root_id(), c4::yml::DOC);
        _dbg("src_tree");_pr(*m_ws);
        return m_ws->root_id();
    };
    auto _setup_yml_as_val = [this, &_parse](csubstr val){
        size_t required = val.len;
        substr yml_copy = _alloc_arena(required);
        size_t used = c4::cat(yml_copy, val);
        C4_CHECK(used == required);
        return _parse(yml_copy);
    };
    auto _setup_yml_as_keyval = [this, &_parse](csubstr key, csubstr val){
        size_t required = key.len + 2 + val.len;
        substr yml_copy = _alloc_arena(required);
        size_t used = c4::cat(yml_copy, key, ": ", val);
        C4_CHECK(used == required);
        size_t root_node = _parse(yml_copy);
        size_t keyconf_node = m_ws->first_child(root_node);
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
                csubstr rem = res.empty() ? _get_root_key(result.unresolved()) : _get_leaf_key(res);
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

void Workspace::apply_opts(OptArg const* args_, size_t num_args)
{
    // prepare
    OptArg const* C4_RESTRICT args = args_;
    for(size_t iarg = 0; iarg < num_args; ++iarg)
    {
        OptArg const& arg = args[iarg];
        switch(arg.action)
        {
        case Opt::set_node:
            prepare_add_conf(arg.target, arg.payload);
            break;
        case Opt::load_file:
            C4_ASSERT(strlen(arg.payload.data()) == arg.payload.len);
            prepare_add_file(arg.payload.data());
            break;
        case Opt::load_dir:
            C4_ASSERT(strlen(arg.payload.data()) == arg.payload.len);
            prepare_add_dir(arg.payload.data());
            break;
        default:
            C4_ERROR("unknown action");
        }
    }
    // apply
    for(size_t iarg = 0; iarg < num_args; ++iarg)
    {
        OptArg const& arg = args[iarg];
        switch(arg.action)
        {
        case Opt::set_node:
            add_conf(arg.target, arg.payload);
            break;
        case Opt::load_file:
            add_file(arg.payload.data());
            break;
        case Opt::load_dir:
            add_dir(arg.payload.data());
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
                  OptSpec const* specs, size_t num_specs,
                  OptArg *opt_args, size_t opt_args_size)
{
    auto getarg = [&argc, &argv](int i) -> csubstr { return to_csubstr((*argv)[i]); };
    auto getspec = [specs, num_specs](csubstr a) -> OptSpec const* {
        for(size_t ispec = 0; ispec < num_specs; ++ispec)
            if(a == specs[ispec].optshort || a == specs[ispec].optlong)
                return specs + ispec;
        return nullptr;
    };
    int filtered_argc = 0;
    size_t num_opt_args = 0;
    for(int iarg = 0; iarg < *argc; /*do nothing here*/)
    {
        OptSpec const* spec = getspec(getarg(iarg));
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
        case Opt::set_node:
        {
            C4_ASSERT(spec->num_expected_args == 2u);
            if(*argc < iarg + 2)
                return argerror;
            if(opt_args && num_opt_args < opt_args_size)
            {
                opt_args[num_opt_args].action = spec->action;
                opt_args[num_opt_args].target = getarg(iarg + 1);
                opt_args[num_opt_args].payload = getarg(iarg + 2);
            }
            iarg += 3;
            break;
        }
        case Opt::load_file:
        case Opt::load_dir:
        {
            C4_ASSERT(spec->num_expected_args == 1u);
            if(*argc < iarg + 1)
                return argerror;
            if(opt_args && num_opt_args < opt_args_size)
            {
                opt_args[num_opt_args].action = spec->action;
                opt_args[num_opt_args].target = {};
                opt_args[num_opt_args].payload = getarg(iarg + 1);
            }
            iarg += 2;
            break;
        }
        default:
            C4_ERROR("unknown action");
        }
        ++num_opt_args;
    }
    for(int iarg = filtered_argc; iarg < *argc; ++iarg)
        (*argv)[iarg] = nullptr;
    *argc = filtered_argc;
    return num_opt_args;
}

#undef _dbg

} // namespace conf
} // namespace c4
