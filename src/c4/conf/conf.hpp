#ifndef C4_CONF_HPP_
#define C4_CONF_HPP_

#include <c4/yml/yml.hpp>
#include <c4/fs/fs.hpp>
#include <c4/format.hpp>

#include <iostream>

namespace c4 {
namespace conf {

/** Provides a workspace tree to for reuse when loading newer files to
 * merge. */
struct Workspace
{
    yml::Tree   m_tree_ws;
    yml::Tree * m_tree;

    Workspace(yml::Tree *t=nullptr)
        : m_tree_ws()
        , m_tree(t ? t : &m_tree_ws)
    {
    }

    bool load_file(csubstr s, yml::Tree *output);

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

    /** the nodes in the final conf tree will be pointing at the arena
     * of the ws tree. Ensure no arena reallocation occurs while
     * parsing any of the confs. */
    template <class ConfIt>
    void reserve_arena_for_confs(ConfIt begin, ConfIt end)
    {
        // count the total size of the confs
        size_t sz = 0u;
        for( ; begin != end; ++begin)
            sz += to_csubstr(*begin).len;
        // to be absolutely certain, use twice the size
        sz *= size_t(2);
        // now reserve the arena in the ws tree
        m_tree->reserve_arena(sz);
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
inline bool add_files(It begin, It end, yml::Tree *tree, Workspace *workspace=nullptr)
{
    auto impl = [&](Workspace *ws){
        ws->prepare(begin, end, tree);
        bool ret = false;
        for(It it = begin; it != end; ++it)
            ret |= add_file(c4::to_csubstr(*it), tree, ws);
        return ret;
    };

    if(workspace)
    {
        return impl(workspace);
    }
    else
    {
        Workspace tmp;
        return impl(&tmp);
    }
}

/** load a sequence of files, with the filenames given in a container */
template <class Container>
inline bool add_files(Container const& filenames, yml::Tree *tree, Workspace *ws=nullptr)
{
    return add_files(filenames.begin(), filenames.end(), tree, ws);
}


//-----------------------------------------------------------------------------

void add_conf(csubstr path, csubstr conf_yml, yml::Tree *tree, Workspace *ws);
void add_conf(csubstr path_eq_conf_yml, yml::Tree *tree, Workspace *ws);


} // namespace conf
} // namespace c4

#endif // C4_CONF_HPP_
