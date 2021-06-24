#ifndef C4_CONF_HPP_
#define C4_CONF_HPP_

#include <c4/yml/yml.hpp>
#include <c4/fs/fs.hpp>
#include <c4/format.hpp>

namespace c4 {
namespace conf {

/** Provide a workspace tree to load newer files to merge.
 * When loading multiple files, an object of this class may be
 * provided as a workspace to allow reuse of the tree.. */
struct Workspace
{
    yml::Tree   m_tree_ws;
    yml::Tree * m_tree;

    Workspace(yml::Tree *t=nullptr)
        : m_tree_ws()
        , m_tree(t ? t : &m_tree_ws)
    {
    }

    bool load_file(csubstr s, yml::Tree *output)
    {
        C4_ASSERT(output);
        size_t pos = output->arena_pos();
        // filesystem functions use libc functions which cannot
        // deal with substrings so create a cstr
        output->copy_to_arena(s);
        output->copy_to_arena("\0");
        const char* file = output->arena().str + pos;
        if( ! fs::path_exists(file)) return false;
        // copy the file contents into the tree arena
        size_t filesz = fs::file_get_contents(file, nullptr, 0);
        substr file_contents = output->alloc_arena(filesz);
        file = output->arena().str + pos; // WATCHOUT the arena may have been reallocated
        fs::file_get_contents(file, file_contents.str, file_contents.len);
        // now parse the yaml content into the tree
        m_tree->clear();
        yml::parse(s, file_contents, m_tree);
        return true;
    }

    template <class It>
    void prepare(It begin, It end, yml::Tree *output) const
    {
        // we have no guarantees that the filenames
        // are zero-terminated, as (regrettably) required
        // by filesystem functions.
        size_t fns = 0; // max filename size
        for(It it = begin; it != end; ++it)
        {
            csubstr filename = to_csubstr(*it);
            fns = filename.len > fns ? filename.len : fns;
        }
        output->reserve_arena(fns + 1);
        substr arena = output->m_arena;
        C4_ASSERT(output->m_arena.len >= fns + 1);
        size_t count = 0;
        for(It it = begin; it != end; ++it)
        {
            csubstr filename = to_csubstr(*it);
            count += filename.len + 1;
            c4::cat(arena, filename, '\0');
            count += fs::file_size(arena.str);
        }
        output->reserve_arena(count);
    }


};


//-----------------------------------------------------------------------------

/** load the given @p file_path and merge with the existing @p tree,
    using an existing @p workspace */
inline bool add_file(csubstr file_path, yml::Tree *tree, Workspace *workspace)
{
    C4_ASSERT(workspace != nullptr);
    if( ! workspace->load_file(file_path, tree))
        return false;
    size_t root = tree->root_id();
    tree->merge_with(workspace->m_tree, workspace->m_tree->root_id(), root);
    return true;
}

/** load the given @p file_path and merge with the existing @p tree,
    instantiating a temporary @p workspace */
inline bool add_file(csubstr file_path, yml::Tree *tree)
{
    Workspace ws;
    return add_file(file_path, tree, &ws);
}


//-----------------------------------------------------------------------------

/** load a sequence of files */
template <class It>
inline bool add_files(It begin, It end, yml::Tree *tree, Workspace *ws=nullptr)
{
    if(ws)
    {
        ws->prepare(begin, end, tree);
        bool ret = false;
        for(It it = begin; it != end; ++it)
            ret |= add_file(c4::to_csubstr(*it), tree, ws);
        return ret;
    }
    else
    {
        Workspace tmp;
        tmp.prepare(begin, end, tree);
        bool ret = false;
        for(It it = begin; it != end; ++it)
            ret |= add_file(c4::to_csubstr(*it), tree, &tmp);
        return ret;
    }
}

/** load a sequence of files, with the names given in a container with multiple filenames */
template <class Container>
inline bool add_files(Container const& container, yml::Tree *tree, Workspace *ws=nullptr)
{
    return add_files(container.begin(), container.end(), tree, ws);
}


//-----------------------------------------------------------------------------

inline bool add_conf(csubstr key, csubstr val, yml::Tree *tree, Workspace *ws)
{
    C4_ASSERT(ws != nullptr);
    C4_ASSERT(ws->m_tree != nullptr);
    ws->m_tree->clear();
    yml::parse(key, val, ws->m_tree);
    auto lookup_result = tree->lookup_path(key);
    if(!lookup_result)
        return false;
    C4_CHECK(lookup_result == true);
    tree->merge_with(ws->m_tree, ws->m_tree->root_id(), lookup_result.target);
    return true;
}


} // namespace conf
} // namespace c4

#endif // C4_CONF_HPP_
